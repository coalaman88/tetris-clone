#define WIN32_LEAN_AND_MEAN
#define COBJMACROS

#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <uuids.h>
#include <avrt.h>

#include "basic.h"
#include "engine.h"

#pragma comment (lib, "avrt")
#pragma comment (lib, "ole32")
#pragma comment (lib, "onecore")

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

typedef struct{
	WasapiAudio *internals;
	
	SoundState audio_stack[10];
	i32 audio_count;
} T_AudioPlayer;

T_AudioPlayer AudioState = {0};

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

static void S_Mix(f32* outSamples, size_t outSampleCount, const SoundState* sound){
	const i16* inSamples = sound->samples;
	size_t inPos = sound->pos;
	size_t inCount = sound->count;
	b32 inLoop = sound->loop;

	for (size_t i = 0; i < outSampleCount; i++){
		if (inLoop){
			if (inPos == inCount){
				// reset looping sound back to start
				inPos = 0;
			}
		}
		else {
			if (inPos >= inCount){
				// non-looping sounds stops playback when done
				break;
			}
		}

		f32 sample = inSamples[inPos++] * (1.f / 32768.f);
		outSamples[0] += sound->volume * sample;
		outSamples[1] += sound->volume * sample;
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

static void S_Update(SoundState* sound, size_t samples){
	sound->pos += samples;
	if(sound->loop){
		sound->pos %= sound->count;
	} else {
		sound->pos = min(sound->pos, sound->count);
	}
}

void update_sounds(T_AudioPlayer *state){
	WasapiAudio *audio = state->internals;
	WA_LockBuffer(audio);
	// write at least 100msec of samples into buffer (or whatever space available, whichever is smaller)
	// this is max amount of time you expect code will take until the next iteration of loop
	// if code will take more time then you'll hear discontinuity as buffer will be filled with silence
	size_t sampleRate = audio->bufferFormat->nSamplesPerSec;
	size_t writeCount = min(sampleRate/10, audio->sampleCount);

	// alternatively you can write as much as "audio.sampleCount" to fully fill the buffer (~1 second)
	// then you can try to increase delay below to 900+ msec, it still should sound fine
	//writeCount = audio.sampleCount;

	// initialize output with 0.0f
	f32* output = audio->sampleBuffer;
	u32 bytesPerSample = audio->bufferFormat->nBlockAlign;
	memset(output, 0, writeCount * bytesPerSample);

	// advance sound playback positions
	size_t playCount = audio->playCount;
	for(i32 i = 0; i < array_size(state->audio_stack); i++){
		SoundState *sound = &state->audio_stack[i];
		if(sound->pos >= sound->count) continue;
		S_Update(sound, playCount);
		S_Mix(output, writeCount, sound);
	}

	// mix sounds into output
	WA_UnlockBuffer(audio, writeCount);
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

	AudioState.internals = audio;
}

Sound load_sin_wave(size_t sample_rate, u32 bytes_per_sample, f64 seconds){
	assert(bytes_per_sample == sizeof(f32));
    size_t samples_count = (size_t)(sample_rate * seconds);
    size_t sample_buffer_size = bytes_per_sample / 2 * samples_count;
	
	Sound sound = {
		.samples = os_memory_alloc(sample_buffer_size),
		.count   = samples_count,
	};

    f32 theta = 0;
	f32 step = (f32)PI * 0.025f;
    f32 volume = .5f;
    for(i32 i = 0; i < sound.count; i++){
        i16 sample = (i16)roundf(sinf(theta) * 32767.f * volume);
        sound.samples[i] = sample;
        theta += step;
    }
	return sound;
}

#pragma pack(1)
typedef struct{
	// riff
	i32 riff_chunck_id;
	i32 chunck_size;
	i32 format;

	// fmt
	i32 format_chunck_id;
	i32 format_chunck_size;
	i16 audio_format;
	i16 channels_count;
	i32 sample_rate;
	i32 bytes_rate;
	i16 block_align;
	i16 bits_per_sample;

	// other_chunck_id
	// other_chunck_size;
	// ...
	// data chunck
	// i32 subchunk2_id;
	// i32 subchunk2_size;
	// i16 data[];
}WaveFile;

f32 lerp(f32 v0, f32 v1, f32 t) { // @Move
  return (1.0f - t) * v0 + t * v1;
}

Sound load_wave_file(const char *file_name){
	const WasapiAudio *audio = AudioState.internals;
	HANDLE file = CreateFileA(file_name, GENERIC_READ,  FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    i32 size;
    u8 *data = (u8*)read_whole_file(file, &size);
    CloseHandle(file);
    WaveFile *wave = (WaveFile*)data;
	assert(size > sizeof(WaveFile));
	assert(wave->riff_chunck_id   == 0x46464952); // "RIFF"
	assert(wave->format_chunck_id == 0x20746D66); // "tfm "
	assert(wave->bits_per_sample  == 16);

	u8 *chunck_p = (u8*)wave + sizeof(WaveFile);
	i32 original_samples_size = 0;
	i16 *original_samples = NULL;
	while(chunck_p < data + size - 4){
		i32 chunck_id   = *(i32*)(chunck_p + 0);
		i32 chunck_size = *(i32*)(chunck_p + 4);
		i16 *data_p     =  (i16*)(chunck_p + 8);
		chunck_p  += 8 + chunck_size;
		//printf("id: 0x%x\n", chunck_id);

		if(chunck_id == 0x61746164){ // "data"
			original_samples_size = chunck_size;
			original_samples = data_p;
			break;
		}
	}
	assert(original_samples);

    //printf("sample_rate: %d/%d\n", wave->sample_rate, audio->bufferFormat->nSamplesPerSec);
    //printf("bytes per sample: %d/%d\n", wave->bits_per_sample / 8, audio->bufferFormat->wBitsPerSample / 8);
    //printf("channels: %d\n", wave->channels_count);

	i32 sample_count;
	i16 *samples;
    i32 original_samples_count = original_samples_size / wave->channels_count / (wave->bits_per_sample / 8);
	if(wave->sample_rate != (i32)audio->bufferFormat->nSamplesPerSec){
		assert(wave->sample_rate < (i32)audio->bufferFormat->nSamplesPerSec);
		f32 time = (f32)original_samples_count / (f32)wave->sample_rate;
		sample_count = (i32)(time * audio->bufferFormat->nSamplesPerSec);
		samples = os_memory_alloc(sample_count * sizeof(i16));
		//printf("ratio: %f\n", (f32)sample_count / (f32)original_samples_count);

		// silly resampling
		for(i32 i = 0; i < sample_count; i++){
			f32 position = (f32)i / sample_count * (original_samples_count - 1);
			i32 p0  = (i32)floorf(position);
			i32 p1  = (i32)ceilf(position);
			f32 t   = position - (f32)p0;
			f32 ch0 = lerp((f32)original_samples[p0 * 2 + 0], (f32)original_samples[p1 * 2 + 0], t);
			f32 ch1 = lerp((f32)original_samples[p0 * 2 + 1], (f32)original_samples[p1 * 2 + 1], t);
			samples[i] = (i16)((ch0 + ch1) * .5f);
		}
	} else {
		sample_count = original_samples_count;
		samples = os_memory_alloc(original_samples_size);
		memcpy(samples, original_samples, original_samples_size);
	}

    VirtualFree(data, 0, MEM_RELEASE);

	Sound sound = {
		.samples = samples,
		.count   = sample_count,
	};

	return sound;
}

void play_sound(Sound sound, f32 volume, b32 in_loop){
	T_AudioPlayer *state = &AudioState;
	SoundState *free = NULL;
	for(i32 i = 0; i < array_size(state->audio_stack); i++){
		SoundState *s = &state->audio_stack[i];
		if(s->pos >= s->count){
			free = s;
			break;
		}
	}

	if(!free){
		printf("WARNING! no audio slot!\n"); // @debug
		return;
	}

	free->samples = sound.samples;
	free->count   = sound.count;
	free->loop    = in_loop;
	free->volume  = volume;
	free->pos     = 0;
}

f32 sound_length(Sound sound){
	return (f32)sound.count / (f32)AudioState.internals->bufferFormat->nSamplesPerSec;
}