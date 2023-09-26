#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <uuids.h>

#include "basic.h"

#pragma comment (lib, "avrt")
#pragma comment (lib, "ole32")
#pragma comment (lib, "onecore")

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator,  0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioClient,         0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioClient3,        0x7ed4ee07, 0x8e67, 0x4cd4, 0x8c, 0x1a, 0x2b, 0x7a, 0x59, 0x87, 0xad, 0x42);
DEFINE_GUID(IID_IAudioRenderClient,   0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);

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

void init_wasapi(void){
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


	WAVEFORMATEXTENSIBLE formatEx =
	{
		.Format =
		{
			.wFormatTag = WAVE_FORMAT_EXTENSIBLE,
			.nChannels = channels_n,
			.nSamplesPerSec = sample_rate,
			.nAvgBytesPerSec = sample_rate * channels_n * sizeof(float),
			.nBlockAlign = channels_n * sizeof(float),
			.wBitsPerSample = (WORD)(8 * sizeof(float)),
			.cbSize = sizeof(formatEx) - sizeof(formatEx.Format),
		},
		.Samples.wValidBitsPerSample = 8 * sizeof(float),
		.dwChannelMask = channel_mask,
		.SubFormat = MEDIASUBTYPE_IEEE_FLOAT,
	};

    REFERENCE_TIME duration;
    result = IAudioClient_GetDevicePeriod(client, &duration, NULL);
    assert(result == S_OK);

    const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    result = IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED, flags, duration, 0, &formatEx.Format, NULL);
    assert(result == S_OK);

    //WAVEFORMATEX *format = &formatEx.Format;
    //result = IAudioClient_GetMixFormat(client, &format);
    //assert(result == S_OK);

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

    // TODO @incomplete

	// this is ok, actual memory will be freed only when it is unmapped
	VirtualFree(placeholder1, 0, MEM_RELEASE);
	VirtualFree(placeholder2, 0, MEM_RELEASE);
	CloseHandle(section);
}