/*
 * SystemAudioRecorder - WASAPI audio capture
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */

#ifndef SYSTEM_AUDIO_RECORDER_AUDIO_CORE_H
#define SYSTEM_AUDIO_RECORDER_AUDIO_CORE_H
#include "public.h"
#include "spectrum.h"
#include "ffmpeg.h"
typedef struct _AUDIO_ENGINE_STATUS
{
    BOOL      running;
    BOOL      recording;
    BOOL      converting;
    ULONG     sampleRate;
    USHORT    channels;
    USHORT    bitsPerSample;
    BOOL      isFloat;
    ULONGLONG capturedFrames;
    WCHAR     lastError[256];
    WCHAR     recorderMessage[256];
    WCHAR     recordPath[MAX_PATH];
} AUDIO_ENGINE_STATUS;
typedef struct _AUDIO_ENGINE
{
    CRITICAL_SECTION    lock;
    CRITICAL_SECTION    recorderLock;
    BOOL                initialized;
    volatile LONG       stopRequested;
    HANDLE              thread;
    SPECTRUM_STATE      spectrum;
    FFMPEG_MP3_WRITER   mp3;
    AUDIO_ENGINE_STATUS status;
} AUDIO_ENGINE;
BOOL AudioEngineInitialize(AUDIO_ENGINE *engine);
void AudioEngineDestroy(AUDIO_ENGINE *engine);
BOOL AudioEngineStart(AUDIO_ENGINE *engine);
void AudioEngineStop(AUDIO_ENGINE *engine);
BOOL AudioEngineStartMp3(AUDIO_ENGINE *engine, LPCWSTR path, LONG bitrateKbps, LONG gainMode);
void AudioEngineStopMp3(AUDIO_ENGINE *engine);
void AudioEngineGetStatus(AUDIO_ENGINE *engine, AUDIO_ENGINE_STATUS *status);
void AudioEngineGetSpectrum(AUDIO_ENGINE *engine, SPECTRUM_SNAPSHOT *snapshot);
#endif