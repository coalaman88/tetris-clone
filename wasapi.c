#define WIN32_LEAN_AND_MEAN
#define COBJMACROS

#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <uuids.h>
#include <avrt.h>
#pragma warning(push, 0)
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#pragma warning(pop)

#include "basic.h"

#pragma comment (lib, "avrt")
#pragma comment (lib, "ole32")
#pragma comment (lib, "onecore")
#pragma comment (lib, "mfplat")
#pragma comment (lib, "mfreadwrite")

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator,  0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioClient,         0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioClient3,        0x7ed4ee07, 0x8e67, 0x4cd4, 0x8c, 0x1a, 0x2b, 0x7a, 0x59, 0x87, 0xad, 0x42);
DEFINE_GUID(IID_IAudioRenderClient,   0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);

typedef struct {
	// public part
	
	// describes sampleBuffer format
	WAVEFORMATEX* bufferFormat;

	// use these values only between LockBuffer/UnlockBuffer calls
	void* sampleBuffer;  // ringbuffer for interleaved samples, no need to handle wrapping
	size_t sampleCount;  // how big is buffer in samples
	size_t playCount;    // how many samples were actually used for playback since previous LockBuffer call

	// private
	IAudioClient* client;
	HANDLE event;
	HANDLE thread;
	LONG stop;
	LONG lock;
	BYTE* buffer1;
	BYTE* buffer2;
	UINT32 outSize;              // output buffer size in bytes
	UINT32 rbSize;               // ringbuffer size, always power of 2
	UINT32 bufferUsed;           // how many samples are used from buffer
	BOOL bufferFirstLock;        // true when BufferLock is used at least once
	volatile LONG rbReadOffset;  // offset to read from buffer
	volatile LONG rbLockOffset;  // offset up to what buffer is currently being used
	volatile LONG rbWriteOffset; // offset up to what buffer is filled
} WasapiAudio;

typedef struct {
	i16* samples;
	size_t count;
	size_t pos;
	bool loop;
} Sound;

Sound SampleMusic; // @Removeme

DWORD roundup_two_power(DWORD value){
    DWORD mask = 0x80000000;
    i32 index  = 31;
    while(index > 0 && !(value & mask)){
        mask >>= 1;
        index--;
    }
    assert(index < 31);
    return 1U << (index + 1);
}

static void lock_audio(LONG *lock){
	// loop while audio->lock != FALSE
	while(InterlockedCompareExchange(lock, TRUE, FALSE) != FALSE){
		// wait while audio->lock == locked
		LONG locked = FALSE;
		WaitOnAddress(lock, &locked, sizeof(locked), INFINITE);
	}
	// now audio->lock == TRUE
}

static void unlock_audio(LONG *lock){
	// audio->lock = FALSE
	InterlockedExchange(lock, FALSE);
	WakeByAddressSingle(lock);
}

static void WA_LockBuffer(WasapiAudio* audio){
	UINT32 bytesPerSample = audio->bufferFormat->nBlockAlign;
	UINT32 rbSize  = audio->rbSize;
	UINT32 outSize = audio->outSize;

	lock_audio(&audio->lock);

	UINT32 readOffset  = audio->rbReadOffset;
	UINT32 lockOffset  = audio->rbLockOffset;
	UINT32 writeOffset = audio->rbWriteOffset;

	// how many bytes are used in buffer by reader = [read, lock) range
	UINT32 usedSize = lockOffset - readOffset;

	// make sure there are samples available for one wasapi buffer submission
	// so in case audio thread needs samples before UnlockBuffer is called, it can get some 
	if(usedSize < outSize){
		// how many bytes available in current buffer = [read, write) range
		UINT32 availSize = writeOffset - readOffset;

		// if [read, lock) is smaller than outSize buffer, then increase lock to [read, read+outSize) range
		usedSize = min(outSize, availSize);
		audio->rbLockOffset = lockOffset = readOffset + usedSize;
	}

	// how many bytes can be written to buffer
	UINT32 writeSize = rbSize - usedSize;

	// reset write marker to beginning of lock offset (can start writing there)
	audio->rbWriteOffset = lockOffset;

	// reset play sample count, use 0 for playCount when LockBuffer is called first time
	audio->playCount = audio->bufferFirstLock ? 0 : audio->bufferUsed;
	audio->bufferFirstLock = FALSE;
	audio->bufferUsed = 0;

	unlock_audio(&audio->lock);

	// buffer offset/size where to write
	// safe to write in [write, read) range, because reading happen in [read, lock) range (lock==write)
	audio->sampleBuffer = audio->buffer1 + (lockOffset & (rbSize - 1));
	audio->sampleCount  = writeSize / bytesPerSample;
}

static void WA_UnlockBuffer(WasapiAudio* audio, size_t writtenSamples){
	UINT32 bytesPerSample = audio->bufferFormat->nBlockAlign;
	size_t writeSize = writtenSamples * bytesPerSample;

	// advance write offset to allow reading new samples
	InterlockedAdd(&audio->rbWriteOffset, (LONG)writeSize);
}

static void S_Mix(float* outSamples, size_t outSampleCount, float volume, const Sound* sound)
{
	const short* inSamples = sound->samples;
	size_t inPos = sound->pos;
	size_t inCount = sound->count;
	bool inLoop = sound->loop;

	for (size_t i = 0; i < outSampleCount; i++)
	{
		if (inLoop)
		{
			if (inPos == inCount)
			{
				// reset looping sound back to start
				inPos = 0;
			}
		}
		else
		{
			if (inPos >= inCount)
			{
				// non-looping sounds stops playback when done
				break;
			}
		}

		float sample = inSamples[inPos++] * (1.f / 32768.f);
		outSamples[0] += volume * sample;
		outSamples[1] += volume * sample;
		outSamples += 2;
	}
}

static DWORD CALLBACK wasapi_audio_thread(LPVOID arg){
	WasapiAudio* audio = arg;

	HRESULT result;
	DWORD task;
	HANDLE handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task); // Setting high priority on windows vista.
	assert(handle);

	IAudioClient* client = audio->client;

	IAudioRenderClient* playback;
	result = IAudioClient_GetService(client, &IID_IAudioRenderClient, (LPVOID*)&playback);
	assert(result == S_OK);

	// get audio buffer size in samples
	UINT32 bufferSamples;
	result = IAudioClient_GetBufferSize(client, &bufferSamples);
	assert(result == S_OK);

	// start the playback
	result = IAudioClient_Start(client);
	assert(result == S_OK);

	UINT32 bytesPerSample = audio->bufferFormat->nBlockAlign;
	UINT32 rbMask = audio->rbSize - 1;
	BYTE* input = audio->buffer1;

	while (WaitForSingleObject(audio->event, INFINITE) == WAIT_OBJECT_0){
		if (InterlockedExchange(&audio->stop, FALSE))
			break;

		UINT32 paddingSamples;
		result = IAudioClient_GetCurrentPadding(client, &paddingSamples);
		assert(result == S_OK);

		// get output buffer from WASAPI
		BYTE* output;
		UINT32 maxOutputSamples = bufferSamples - paddingSamples;
		result = IAudioRenderClient_GetBuffer(playback, maxOutputSamples, &output);
		assert(result == S_OK);

		lock_audio(&audio->lock);

		UINT32 readOffset = audio->rbReadOffset;
		UINT32 writeOffset = audio->rbWriteOffset;

		// how many bytes available to read from ringbuffer
		UINT32 availableSize = writeOffset - readOffset;

		// how many samples available
		UINT32 availableSamples = availableSize / bytesPerSample;

		// will use up to max that's possible to output
		UINT32 useSamples = min(availableSamples, maxOutputSamples);

		// how many bytes to use
		UINT32 useSize = useSamples * bytesPerSample;

		// lock range [read, lock) that memcpy will read from below
		audio->rbLockOffset = readOffset + useSize;

		// will always submit required amount of samples, but if there's not enough to use, then submit silence
		UINT32 submitCount = useSamples ? useSamples : maxOutputSamples;
		DWORD flags = useSamples ? 0 : AUDCLNT_BUFFERFLAGS_SILENT;

		// remember how many samples are submitted
		audio->bufferUsed += submitCount;

		unlock_audio(&audio->lock);

		// copy bytes to output
		// safe to do it outside lock_audio/Unlock, because nobody will overwrite [read, lock) interval
		memcpy(output, input + (readOffset & rbMask), useSize);

		// advance read offset up to lock position, allows writing to [read, lock) interval
		InterlockedAdd(&audio->rbReadOffset, useSize);

		// submit output buffer to WASAPI
		result = IAudioRenderClient_ReleaseBuffer(playback, submitCount, flags);
		assert(result == S_OK);
	}

	// stop the playback
	result = IAudioClient_Stop(client);
	assert(result == S_OK);
	IAudioRenderClient_Release(playback);

	AvRevertMmThreadCharacteristics(handle);
	return 0;
}

static void S_Update(Sound* sound, size_t samples){
	sound->pos += samples;
	if(sound->loop){
		sound->pos %= sound->count;
	} else {
		sound->pos = min(sound->pos, sound->count);
	}
}

void update_sound(WasapiAudio *audio){
	WA_LockBuffer(audio);

	// write at least 100msec of samples into buffer (or whatever space available, whichever is smaller)
	// this is max amount of time you expect code will take until the next iteration of loop
	// if code will take more time then you'll hear discontinuity as buffer will be filled with silence
	size_t sampleRate = audio->bufferFormat->nSamplesPerSec;
	size_t writeCount = min(sampleRate/10, audio->sampleCount);

	// alternatively you can write as much as "audio.sampleCount" to fully fill the buffer (~1 second)
	// then you can try to increase delay below to 900+ msec, it still should sound fine
	//writeCount = audio.sampleCount;

	// advance sound playback positions
	size_t playCount = audio->playCount;
	S_Update(&SampleMusic, playCount);

	// initialize output with 0.0f
	float* output = audio->sampleBuffer;
	u32 bytesPerSample = audio->bufferFormat->nBlockAlign;
	memset(output, 0, writeCount * bytesPerSample);

	// mix sounds into output
	S_Mix(output, writeCount, 1.0f, &SampleMusic);

	WA_UnlockBuffer(audio, writeCount);
}

// loads any supported sound file, and resamples to mono 16-bit audio with specified sample rate
static Sound S_Load(const WCHAR* path, size_t sampleRate){
	HRESULT result;
	Sound sound = { NULL, 0, 0, false };
	result = MFStartup(MF_VERSION, MFSTARTUP_LITE);
	assert(result == S_OK);

	IMFSourceReader* reader;
	result = MFCreateSourceReaderFromURL(path, NULL, &reader);
	assert(result == S_OK);

	// read only first audio stream
	result = IMFSourceReader_SetStreamSelection(reader, (DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
	assert(result == S_OK);
	result = IMFSourceReader_SetStreamSelection(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE);
	assert(result == S_OK);

	const size_t kChannelCount = 1;
	const WAVEFORMATEXTENSIBLE format = {
		.Format = {
			.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
			.nChannels = (WORD)kChannelCount,
			.nSamplesPerSec = (WORD)sampleRate,
			.nAvgBytesPerSec = (DWORD)(sampleRate * kChannelCount * sizeof(short)),
			.nBlockAlign = (WORD)(kChannelCount * sizeof(short)),
			.wBitsPerSample = (WORD)(8 * sizeof(short)),
			.cbSize = sizeof(format) - sizeof(format.Format),
		},
		.Samples.wValidBitsPerSample = 8 * sizeof(short),
		.dwChannelMask = SPEAKER_FRONT_CENTER,
		.SubFormat = MEDIASUBTYPE_PCM,
	};

	// Media Foundation in Windows 8+ allows reader to convert output to different format than native
	IMFMediaType* type;
	result = MFCreateMediaType(&type);
	assert(result == S_OK);
	result = MFInitMediaTypeFromWaveFormatEx(type, &format.Format, sizeof(format));
	assert(result == S_OK);
	result = IMFSourceReader_SetCurrentMediaType(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, type);
	assert(result == S_OK);
	IMFMediaType_Release(type);

	size_t used = 0;
	size_t capacity = 0;

	while(true){
		IMFSample* sample;
		DWORD flags = 0;
		result = IMFSourceReader_ReadSample(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, NULL, &sample);
		if(FAILED(result))
			break;

		if(flags & MF_SOURCE_READERF_ENDOFSTREAM)
			break;
		assert(flags == 0);

		IMFMediaBuffer* buffer;
		result = IMFSample_ConvertToContiguousBuffer(sample, &buffer);
		assert(result == S_OK);

		BYTE* data;
		DWORD size;
		result = IMFMediaBuffer_Lock(buffer, &data, NULL, &size);
		assert(result == S_OK);
		{
			size_t avail = capacity - used;
			if (avail < size)
			{
				sound.samples = realloc(sound.samples, capacity += 64 * 1024);
			}
			memcpy((char*)sound.samples + used, data, size);
			used += size;
		}
		result = IMFMediaBuffer_Unlock(buffer);
		assert(result == S_OK);

		IMediaBuffer_Release(buffer);
		IMFSample_Release(sample);
	}

	IMFSourceReader_Release(reader);

	MFShutdown();

	sound.pos = sound.count = used / format.Format.nBlockAlign;
	return sound;
}

void init_wasapi(WasapiAudio* audio){
	HRESULT result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    assert(result == S_OK);

	IMMDeviceEnumerator* enumerator;
	result = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (LPVOID*)&enumerator);
    assert(result == S_OK);

	IMMDevice* device;
	result = IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, eRender, eConsole, &device);
    assert(result == S_OK);
	IMMDeviceEnumerator_Release(enumerator);

    IAudioClient* client;
	result = IMMDevice_Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, (LPVOID*)&client);
    assert(result == S_OK);
	IMMDevice_Release(device);

    // mock data
    WORD channels_n = 2;
    WORD sample_rate = 48000;
    DWORD channel_mask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;


	WAVEFORMATEXTENSIBLE formatEx = {
		.Format = {
			.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
			.nChannels = channels_n,
			.nSamplesPerSec = sample_rate,
			.nAvgBytesPerSec = sample_rate * channels_n * sizeof(f32),
			.nBlockAlign = channels_n * sizeof(f32),
			.wBitsPerSample = (WORD)(8 * sizeof(f32)),
			.cbSize = sizeof(formatEx) - sizeof(formatEx.Format),
		},
		.Samples.wValidBitsPerSample = 8 * sizeof(f32),
		.dwChannelMask = channel_mask,
		.SubFormat = MEDIASUBTYPE_IEEE_FLOAT,
	};

    REFERENCE_TIME duration;
    result = IAudioClient_GetDevicePeriod(client, &duration, NULL);
    assert(result == S_OK);

    const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    result = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, flags, duration, 0, &formatEx.Format, NULL);
    assert(result == S_OK);

    WAVEFORMATEX *format;
    result = IAudioClient_GetMixFormat(client, &format);
    assert(result == S_OK);

    UINT32 bufferSamples;
	result = IAudioClient_GetBufferSize(client, &bufferSamples);
    assert(result == S_OK);
	DWORD out_size = bufferSamples * formatEx.Format.nBlockAlign;

    // Thanks @mmozeiko for this neat example!

	// setup event handle to wait on
    HANDLE event = CreateEventW(NULL, FALSE, FALSE, NULL);
	result = IAudioClient_SetEventHandle(client, event);
    assert(result == S_OK);

	// use at least 64KB or 1 second whichever is larger, and round upwards to pow2 for ringbuffer
	DWORD rbSize = roundup_two_power(max(64 * 1024, formatEx.Format.nAvgBytesPerSec));

	// reserve virtual address placeholder for 2x size for magic ringbuffer
	char* placeholder1 = VirtualAlloc2(NULL, NULL, 2 * rbSize, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
	char* placeholder2 = placeholder1 + rbSize;
	assert(placeholder1);

	// split allocated address space in half
	VirtualFree(placeholder1, rbSize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);

	// create page-file backed section for buffer
	HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, rbSize, NULL);
	assert(section);

    // map same section into both addresses
	void* view1 = MapViewOfFile3(section, NULL, placeholder1, 0, rbSize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
	void* view2 = MapViewOfFile3(section, NULL, placeholder2, 0, rbSize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
	assert(view1 && view2);

	audio->client = client;
	audio->bufferFormat = format;
	audio->outSize = out_size;
	audio->event = event;
	audio->sampleBuffer = NULL;
	audio->sampleCount = 0;
	audio->playCount = 0;
	audio->buffer1 = view1;
	audio->buffer2 = view2;
	audio->rbSize = rbSize;
	audio->bufferUsed = 0;
	audio->bufferFirstLock = TRUE;
	audio->rbReadOffset = 0;
	audio->rbLockOffset = 0;
	audio->rbWriteOffset = 0;
	InterlockedExchange(&audio->stop, FALSE);
	InterlockedExchange(&audio->lock, FALSE);
	audio->thread = CreateThread(NULL, 0, &wasapi_audio_thread, audio, 0, NULL);

	// this is ok, actual memory will be freed only when it is unmapped
	VirtualFree(placeholder1, 0, MEM_RELEASE);
	VirtualFree(placeholder2, 0, MEM_RELEASE);
	CloseHandle(section);
}