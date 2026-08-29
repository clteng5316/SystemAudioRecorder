/*
 * SystemAudioRecorder
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */

#ifndef SYSTEM_AUDIO_RECORDER_WASAPI_COMPAT_H
#define SYSTEM_AUDIO_RECORDER_WASAPI_COMPAT_H

/*
 * Minimal WASAPI declarations used by SystemAudioRecorder.
 * This avoids a hard dependency on audioclient.h in WDK7 build.exe
 * environments where that SDK header is not on the active include path.
 */

#ifndef _REFERENCE_TIME_DEFINED
#define _REFERENCE_TIME_DEFINED
typedef LONGLONG REFERENCE_TIME;
#endif

typedef enum _SYSTEMAUDIORECORDER_AUDCLNT_SHAREMODE
{
    SYSTEMAUDIORECORDER_AUDCLNT_SHAREMODE_SHARED = 0,
    SYSTEMAUDIORECORDER_AUDCLNT_SHAREMODE_EXCLUSIVE = 1
} SYSTEMAUDIORECORDER_AUDCLNT_SHAREMODE;

#ifndef AUDCLNT_SHAREMODE_SHARED
#define AUDCLNT_SHAREMODE_SHARED SYSTEMAUDIORECORDER_AUDCLNT_SHAREMODE_SHARED
#endif

#ifndef AUDCLNT_SHAREMODE_EXCLUSIVE
#define AUDCLNT_SHAREMODE_EXCLUSIVE SYSTEMAUDIORECORDER_AUDCLNT_SHAREMODE_EXCLUSIVE
#endif

#ifndef AUDCLNT_STREAMFLAGS_LOOPBACK
#define AUDCLNT_STREAMFLAGS_LOOPBACK 0x00020000
#endif

#ifndef AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY
#define AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY 0x00000001
#endif

#ifndef AUDCLNT_BUFFERFLAGS_SILENT
#define AUDCLNT_BUFFERFLAGS_SILENT 0x00000002
#endif

#ifndef AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR
#define AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR 0x00000004
#endif

struct __declspec(uuid("1CB9AD4C-DBFA-4C32-B178-C2F568A703B2")) IAudioClient : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE Initialize(
        SYSTEMAUDIORECORDER_AUDCLNT_SHAREMODE ShareMode,
        DWORD StreamFlags,
        REFERENCE_TIME hnsBufferDuration,
        REFERENCE_TIME hnsPeriodicity,
        const WAVEFORMATEX *pFormat,
        LPCGUID AudioSessionGuid) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetBufferSize(UINT *pNumBufferFrames) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetStreamLatency(REFERENCE_TIME *phnsLatency) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentPadding(UINT *pNumPaddingFrames) = 0;

    virtual HRESULT STDMETHODCALLTYPE IsFormatSupported(
        SYSTEMAUDIORECORDER_AUDCLNT_SHAREMODE ShareMode,
        const WAVEFORMATEX *pFormat,
        WAVEFORMATEX **ppClosestMatch) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetMixFormat(WAVEFORMATEX **ppDeviceFormat) = 0;

    virtual HRESULT STDMETHODCALLTYPE GetDevicePeriod(
        REFERENCE_TIME *phnsDefaultDevicePeriod,
        REFERENCE_TIME *phnsMinimumDevicePeriod) = 0;

    virtual HRESULT STDMETHODCALLTYPE Start(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE Stop(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE Reset(void) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetEventHandle(HANDLE eventHandle) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetService(REFIID riid, void **ppv) = 0;
};

struct __declspec(uuid("C8ADBD64-E71E-48A0-A4DE-185C395CD317")) IAudioCaptureClient : public IUnknown
{
    virtual HRESULT STDMETHODCALLTYPE GetBuffer(
        BYTE **ppData,
        UINT *pNumFramesToRead,
        DWORD *pdwFlags,
        ULONGLONG *pu64DevicePosition,
        ULONGLONG *pu64QPCPosition) = 0;

    virtual HRESULT STDMETHODCALLTYPE ReleaseBuffer(UINT NumFramesRead) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetNextPacketSize(UINT *pNumFramesInNextPacket) = 0;
};

#endif
