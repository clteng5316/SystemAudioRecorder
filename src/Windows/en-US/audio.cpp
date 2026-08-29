/*
 * SystemAudioRecorder - WASAPI audio capture
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */
#include "audio.h"
typedef HANDLE (WINAPI *PFN_AV_SET_MM_THREAD_CHARACTERISTICS_W)(LPCWSTR, LPDWORD);
typedef BOOL (WINAPI *PFN_AV_REVERT_MM_THREAD_CHARACTERISTICS)(HANDLE);
static HMODULE load_system_dll(LPCWSTR dllName)
{
    WCHAR systemDir[MAX_PATH] = {0};
    WCHAR fullPath[MAX_PATH]  = {0};
    UINT  length;
    if (dllName == NULL || dllName[0] == L'\0')
        return NULL;
    length = GetSystemDirectoryW(systemDir, ARRAY_COUNT(systemDir));
    if (length == 0 || length >= ARRAY_COUNT(systemDir))
        return NULL;
    if (FAILED(StringCchPrintfW(fullPath, ARRAY_COUNT(fullPath), L"%s\\%s", systemDir, dllName)))
    {
        return NULL;
    };
    return LoadLibraryW(fullPath);
};
static void set_status_error(AUDIO_ENGINE *engine, LPCWSTR text)
{
    if (engine == NULL)
        return;
    EnterCriticalSection(&engine->lock);
    StringCchCopyW(engine->status.lastError, ARRAY_COUNT(engine->status.lastError), text != NULL ? text : L"Unknown audio error");
    LeaveCriticalSection(&engine->lock);
};
static BOOL subformat_is_wave_tag(const GUID *subFormat, WORD tag)
{
    static const BYTE tail[8] = {0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71};
    if (subFormat == NULL) return FALSE;
    if (subFormat->Data1 != (ULONG)tag || subFormat->Data2 != 0x0000 || subFormat->Data3 != 0x0010) return FALSE;
    return memcmp(subFormat->Data4, tail, sizeof(tail)) == 0;
};
static BOOL wave_is_float(const WAVEFORMATEX *wfx)
{
    if (wfx == NULL)
        return FALSE;
    if (wfx->wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
        return TRUE;
    if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wfx->cbSize >= 22)
    {
        const WAVEFORMATEXTENSIBLE *ex = (const WAVEFORMATEXTENSIBLE *)wfx;
        return subformat_is_wave_tag(&ex->SubFormat, WAVE_FORMAT_IEEE_FLOAT);
    };
    return FALSE;
};
static BOOL wave_is_pcm(const WAVEFORMATEX *wfx)
{
    if (wfx == NULL)
        return FALSE;
    if (wfx->wFormatTag == WAVE_FORMAT_PCM)
        return TRUE;
    if (wfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wfx->cbSize >= 22)
    {
        const WAVEFORMATEXTENSIBLE *ex = (const WAVEFORMATEXTENSIBLE *)wfx;
        return subformat_is_wave_tag(&ex->SubFormat, WAVE_FORMAT_PCM);
    };
    return FALSE;
};
static float sample_to_float(const BYTE *base, UINT frame, UINT channel, const WAVEFORMATEX *wfx, BOOL isFloat)
{
    const BYTE *p;
    UINT        sample;
    if (wfx == NULL || base == NULL || wfx->nChannels == 0)
        return 0.0f;
    sample = wfx->nBlockAlign / wfx->nChannels;
    p = base + (SIZE_T)frame * wfx->nBlockAlign + (SIZE_T)channel * sample;
    if (isFloat && sample == 4)
    {
        float v;
        RtlCopyMemory(&v, p, sizeof(v));
        if (v > 1.0f) v = 1.0f;
        if (v < -1.0f) v = -1.0f;
        return v;
    };
    if (wave_is_pcm(wfx))
    {
        if (sample == 2)
        {
            short v;
            RtlCopyMemory(&v, p, sizeof(v));
            return (float)v / 32768.0f;
        };
        if (sample == 3)
        {
            LONG v = (LONG)((ULONG)p[0] | ((ULONG)p[1] << 8) | ((ULONG)p[2] << 16));
            if ((v & 0x00800000L) != 0)
                v |= (LONG)0xFF000000L;
            return (float)v / 8388608.0f;
        };
        if (sample == 4)
        {
            LONG v;
            RtlCopyMemory(&v, p, sizeof(v));
            return (float)((double)v / 2147483648.0);
        };
        if (sample == 1)
        {
            return ((float)p[0] - 128.0f) / 128.0f;
        }
    }
    return 0.0f;
};
static short float_to_s16(float v)
{
    LONG n;
    if (v > 1.0f)
        v = 1.0f;
    if (v < -1.0f)
        v = -1.0f;
    n = (LONG)(v * 32767.0f);
    if (n > 32767)
        n = 32767;
    if (n < -32768)
        n = -32768;
    return (short)n;
};
static BOOL process_packet(AUDIO_ENGINE *engine, const BYTE *data, UINT frames, DWORD flags, const WAVEFORMATEX *wfx, BOOL isFloat)
{
    float  monoStack[2048]   = {0};
    short  stereoStack[4096] = {0};
    float *mono              = monoStack;
    short *stereo            = stereoStack;
    BOOL   monoHeap          = FALSE;
    BOOL   stereoHeap        = FALSE;
    UINT   i;
    UINT   channels;
    BOOL   ok = TRUE;
    if (engine == NULL || wfx == NULL || frames == 0)
        return TRUE;
    channels = wfx->nChannels;
    if (channels == 0)
        return FALSE;
    if (frames > ARRAY_COUNT(monoStack))
    {
        mono = (float *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)frames * sizeof(float));
        if (mono == NULL) return FALSE;
        monoHeap = TRUE;
    };
    if ((SIZE_T)frames * 2 > ARRAY_COUNT(stereoStack))
    {
        stereo = (short *)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)frames * 2 * sizeof(short));
        if (stereo == NULL)
        {
            if (monoHeap) HeapFree(GetProcessHeap(), 0, mono);
            return FALSE;
        };
        stereoHeap = TRUE;
    };
    for (i = 0; i < frames; ++i)
    {
        float left;
        float right;
        if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || data == NULL)
        {
            left = 0.0f;
            right = 0.0f;
        }
        else
        {
            left = sample_to_float(data, i, 0, wfx, isFloat);
            if (channels >= 2)
                right = sample_to_float(data, i, 1, wfx, isFloat);
            else
                right = left;
        };
        mono[i]           = (left + right) * 0.5f;
        stereo[i * 2]     = float_to_s16(left);
        stereo[i * 2 + 1] = float_to_s16(right);
    };
    SpectrumPushMono(&engine->spectrum, mono, frames);
    EnterCriticalSection(&engine->recorderLock);
    {
        FFMPEG_MP3_STATUS status;
        FfmpegMp3GetStatus(&engine->mp3, &status);
        if (status.recording)
        {
            if (!FfmpegMp3WriteStereo16(&engine->mp3, stereo, frames))
            {
                FfmpegMp3Stop(&engine->mp3);
                FfmpegMp3GetStatus(&engine->mp3, &status);
                EnterCriticalSection(&engine->lock);
                StringCchCopyW(engine->status.lastError, ARRAY_COUNT(engine->status.lastError), status.message);
                StringCchCopyW(engine->status.recorderMessage, ARRAY_COUNT(engine->status.recorderMessage), status.message);
                engine->status.recording  = status.recording;
                engine->status.converting = status.converting;
                LeaveCriticalSection(&engine->lock);
                ok = FALSE;
            };
        };
    };
    LeaveCriticalSection(&engine->recorderLock);
    EnterCriticalSection(&engine->lock);
    engine->status.capturedFrames += frames;
    LeaveCriticalSection(&engine->lock);
    if (stereoHeap)
        HeapFree(GetProcessHeap(), 0, stereo);
    if (monoHeap)
        HeapFree(GetProcessHeap(), 0, mono);
    return ok;
};
static DWORD WINAPI capture_thread(LPVOID context)
{
    AUDIO_ENGINE *engine = (AUDIO_ENGINE *)context;
    HRESULT       hr;
    PFN_AV_SET_MM_THREAD_CHARACTERISTICS_W  pAvSetMmThreadCharacteristicsW   = NULL;
    PFN_AV_REVERT_MM_THREAD_CHARACTERISTICS pAvRevertMmThreadCharacteristics = NULL;
    IMMDeviceEnumerator *enumerator = NULL;
    IMMDevice           *device     = NULL;
    IAudioClient        *client     = NULL;
    IAudioCaptureClient *capture    = NULL;
    WAVEFORMATEX        *wfx        = NULL;
    HANDLE  mmcss      = NULL;
    DWORD   taskIndex  = 0;
    HMODULE avrtModule = NULL;
    BOOL    isFloat    = FALSE;
    BOOL    isPcm      = FALSE;
    BOOL    started    = FALSE;
    BOOL    init       = FALSE;
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr))
    {
        set_status_error(engine, L"CoInitializeEx failed.");
        goto Exit;
    };
    init = TRUE;
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void **)&enumerator);
    if (FAILED(hr))
    {
        set_status_error(engine, L"Unable to create MMDeviceEnumerator.");
        goto Exit;
    };
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr))
    {
        set_status_error(engine, L"No default playback endpoint is available.");
        goto Exit;
    };
    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL, (void **)&client);
    if (FAILED(hr))
    {
        set_status_error(engine, L"Unable to activate IAudioClient.");
        goto Exit;
    };
    hr = client->GetMixFormat(&wfx);
    if (FAILED(hr) || wfx == NULL)
    {
        set_status_error(engine, L"Unable to read the endpoint mix format.");
        goto Exit;
    };
    isFloat = wave_is_float(wfx);
    isPcm   = wave_is_pcm(wfx);
    if (!isFloat && !isPcm)
    {
        set_status_error(engine, L"The playback endpoint uses an unsupported sample format.");
        goto Exit;
    };
    SpectrumReset(&engine->spectrum, wfx->nSamplesPerSec);
    EnterCriticalSection(&engine->lock);
    engine->status.sampleRate     = wfx->nSamplesPerSec;
    engine->status.channels       = wfx->nChannels;
    engine->status.bitsPerSample  = wfx->wBitsPerSample;
    engine->status.isFloat        = isFloat;
    engine->status.capturedFrames = 0;
    StringCchCopyW(engine->status.lastError, ARRAY_COUNT(engine->status.lastError), L"OK");
    LeaveCriticalSection(&engine->lock);
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, wfx, NULL);
    if (FAILED(hr))
    {
        set_status_error(engine, L"WASAPI loopback initialization failed.");
        goto Exit;
    };
    hr = client->GetService(__uuidof(IAudioCaptureClient), (void **)&capture);
    if (FAILED(hr))
    {
        set_status_error(engine, L"Unable to obtain IAudioCaptureClient.");
        goto Exit;
    };
    avrtModule = load_system_dll(L"avrt.dll");
    if (avrtModule != NULL)
    {
        pAvSetMmThreadCharacteristicsW = (PFN_AV_SET_MM_THREAD_CHARACTERISTICS_W) GetProcAddress(avrtModule, "AvSetMmThreadCharacteristicsW");
        pAvRevertMmThreadCharacteristics = (PFN_AV_REVERT_MM_THREAD_CHARACTERISTICS) GetProcAddress(avrtModule, "AvRevertMmThreadCharacteristics");
        if (pAvSetMmThreadCharacteristicsW != NULL)
        {
            mmcss = pAvSetMmThreadCharacteristicsW(L"Audio", &taskIndex);
        };
    };
    hr = client->Start();
    if (FAILED(hr))
    {
        set_status_error(engine, L"Unable to start WASAPI loopback capture.");
        goto Exit;
    };
    started = TRUE;
    EnterCriticalSection(&engine->lock);
    engine->status.running = TRUE;
    LeaveCriticalSection(&engine->lock);
    while (InterlockedCompareExchange(&engine->stopRequested, 0, 0) == 0)
    {
        UINT frame = 0;
        Sleep(8);
        hr = capture->GetNextPacketSize(&frame);
        if (FAILED(hr))
        {
            set_status_error(engine, L"GetNextPacketSize failed.");
            break;
        };
        while (frame != 0 && InterlockedCompareExchange(&engine->stopRequested, 0, 0) == 0)
        {
            BYTE *data   = NULL;
            UINT  frames = 0;
            DWORD flags  = 0;
            hr = capture->GetBuffer(&data, &frames, &flags, NULL, NULL);
            if (FAILED(hr))
            {
                set_status_error(engine, L"IAudioCaptureClient::GetBuffer failed.");
                break;
            };
            process_packet(engine, data, frames, flags, wfx, isFloat);
            hr = capture->ReleaseBuffer(frames);
            if (FAILED(hr))
            {
                set_status_error(engine, L"IAudioCaptureClient::ReleaseBuffer failed.");
                break;
            };
            hr = capture->GetNextPacketSize(&frame);
            if (FAILED(hr))
            {
                set_status_error(engine, L"GetNextPacketSize failed.");
                break;
            };
        };
    };
Exit:
    if (started && client != NULL)
        client->Stop();
    EnterCriticalSection(&engine->lock);
    engine->status.running = FALSE;
    LeaveCriticalSection(&engine->lock);
    if (mmcss != NULL && pAvRevertMmThreadCharacteristics != NULL)
    {
        pAvRevertMmThreadCharacteristics(mmcss);
    };
    if (avrtModule != NULL)
    {
        FreeLibrary(avrtModule);
        avrtModule = NULL;
    };
    if (wfx != NULL)
        CoTaskMemFree(wfx);
#pragma warning(disable:4127)
    SAFE_RELEASE(capture);
    SAFE_RELEASE(client);
    SAFE_RELEASE(device);
    SAFE_RELEASE(enumerator);
#pragma warning(default:4127)
    if (init)
        CoUninitialize();
    return 0;
};
BOOL AudioEngineInitialize(AUDIO_ENGINE *engine)
{
    if (engine == NULL)
        return FALSE;
    RtlZeroMemory(engine, sizeof(*engine));
    InitializeCriticalSection(&engine->lock);
    InitializeCriticalSection(&engine->recorderLock);
    if (!FfmpegMp3Initialize(&engine->mp3))
    {
        DeleteCriticalSection(&engine->recorderLock);
        DeleteCriticalSection(&engine->lock);
        return FALSE;
    };
    if (!SpectrumInitialize(&engine->spectrum, 48000))
    {
        FfmpegMp3Destroy(&engine->mp3);
        DeleteCriticalSection(&engine->recorderLock);
        DeleteCriticalSection(&engine->lock);
        return FALSE;
    };
    engine->initialized = TRUE;
    StringCchCopyW(engine->status.lastError, ARRAY_COUNT(engine->status.lastError), L"Ready");
    return TRUE;
};
void AudioEngineDestroy(AUDIO_ENGINE *engine)
{
    if (engine == NULL || !engine->initialized)
        return;
    AudioEngineStop(engine);
    FfmpegMp3Destroy(&engine->mp3);
    SpectrumDestroy(&engine->spectrum);
    DeleteCriticalSection(&engine->recorderLock);
    DeleteCriticalSection(&engine->lock);
    engine->initialized = FALSE;
};
BOOL AudioEngineStart(AUDIO_ENGINE *engine)
{
    if (engine == NULL || !engine->initialized)
        return FALSE;
    if (engine->thread != NULL)
    {
        /* A capture thread may have ended after a device/WASAPI error.
         * Reap a completed thread so Start can create a fresh one. */
        if (WaitForSingleObject(engine->thread, 0) == WAIT_OBJECT_0)
        {
            CloseHandle(engine->thread);
            engine->thread = NULL;
        }
        else
        {
            return TRUE;
        };
    };
    InterlockedExchange(&engine->stopRequested, 0);
    engine->thread = CreateThread(NULL, 0, capture_thread, engine, 0, NULL);
    if (engine->thread == NULL)
    {
        set_status_error(engine, L"Unable to create capture thread.");
        return FALSE;
    };
    return TRUE;
};
void AudioEngineStop(AUDIO_ENGINE *engine)
{
    HANDLE thread;
    ULONG  sampleRate;
    if (engine == NULL || !engine->initialized)
        return;
    thread = engine->thread;
    if (thread != NULL)
    {
        InterlockedExchange(&engine->stopRequested, 1);
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        engine->thread = NULL;
    };
    AudioEngineStopMp3(engine);
    EnterCriticalSection(&engine->lock);
    sampleRate = engine->status.sampleRate;
    if (sampleRate == 0)
        sampleRate = 48000;
    LeaveCriticalSection(&engine->lock);
    SpectrumReset(&engine->spectrum, sampleRate);
};
BOOL AudioEngineStartMp3(AUDIO_ENGINE *engine, LPCWSTR path, LONG bitrateKbps, LONG gainMode)
{
    FFMPEG_MP3_STATUS status;
    BOOL              ok;
    ULONG             rate;
    if (engine == NULL || path == NULL)
        return FALSE;
    EnterCriticalSection(&engine->lock);
    rate = engine->status.sampleRate;
    LeaveCriticalSection(&engine->lock);
    if (rate == 0)
    {
        set_status_error(engine, L"Start loopback capture before recording MP3.");
        return FALSE;
    };
    EnterCriticalSection(&engine->recorderLock);
    ok = FfmpegMp3Start(&engine->mp3, path, (LONG)rate, bitrateKbps, gainMode);
    FfmpegMp3GetStatus(&engine->mp3, &status);
    LeaveCriticalSection(&engine->recorderLock);
    EnterCriticalSection(&engine->lock);
    engine->status.recording  = status.recording;
    engine->status.converting = status.converting;
    StringCchCopyW(engine->status.recorderMessage, ARRAY_COUNT(engine->status.recorderMessage), status.message);
    if (ok)
    {
        StringCchCopyW(engine->status.recordPath, ARRAY_COUNT(engine->status.recordPath), path);
        StringCchCopyW(engine->status.lastError, ARRAY_COUNT(engine->status.lastError), L"OK");
    }
    else
    {
        engine->status.recordPath[0] = L'\0';
        StringCchCopyW(engine->status.lastError, ARRAY_COUNT(engine->status.lastError), status.message);
    };
    LeaveCriticalSection(&engine->lock);
    return ok;
};
void AudioEngineStopMp3(AUDIO_ENGINE *engine)
{
    FFMPEG_MP3_STATUS status;
    if (engine == NULL || !engine->initialized)
        return;
    EnterCriticalSection(&engine->recorderLock);
    FfmpegMp3Stop(&engine->mp3);
    FfmpegMp3GetStatus(&engine->mp3, &status);
    LeaveCriticalSection(&engine->recorderLock);
    EnterCriticalSection(&engine->lock);
    engine->status.recording = status.recording;
    engine->status.converting = status.converting;
    StringCchCopyW(engine->status.recorderMessage, ARRAY_COUNT(engine->status.recorderMessage), status.message);
    LeaveCriticalSection(&engine->lock);
};
void AudioEngineGetStatus(AUDIO_ENGINE *engine, AUDIO_ENGINE_STATUS *status)
{
    FFMPEG_MP3_STATUS st;
    if (status == NULL)
        return;
    RtlZeroMemory(status, sizeof(*status));
    if (engine == NULL || !engine->initialized)
        return;
    EnterCriticalSection(&engine->lock);
    *status = engine->status;
    LeaveCriticalSection(&engine->lock);
    FfmpegMp3GetStatus(&engine->mp3, &st);
    status->recording  = st.recording;
    status->converting = st.converting;
    StringCchCopyW(status->recorderMessage, ARRAY_COUNT(status->recorderMessage), st.message);
    if (st.targetPath[0] != L'\0')
    {
        StringCchCopyW(status->recordPath, ARRAY_COUNT(status->recordPath), st.targetPath);
    };
};
void AudioEngineGetSpectrum(AUDIO_ENGINE *engine, SPECTRUM_SNAPSHOT *snapshot)
{
    if (engine == NULL || snapshot == NULL)
        return;
    SpectrumGetSnapshot(&engine->spectrum, snapshot);
};