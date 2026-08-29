/*
 * SystemAudioRecorder - spectrum analyzer
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */

#ifndef SYSTEM_AUDIO_RECORDER_SPECTRUM_H
#define SYSTEM_AUDIO_RECORDER_SPECTRUM_H
#include "public.h"
#define SPECTRUM_FFT_SIZE 4096
#define SPECTRUM_HOP_SIZE 1024
#define SPECTRUM_BAND_COUNT 26
typedef struct _SPECTRUM_SNAPSHOT
{
    float     bandDb[SPECTRUM_BAND_COUNT];
    float     peakDb[SPECTRUM_BAND_COUNT];
    float     rmsDb;
    float     dominantHz;
    ULONG     sampleRate;
    ULONGLONG framesAnalyzed;
} SPECTRUM_SNAPSHOT;
typedef struct _SPECTRUM_STATE
{
    CRITICAL_SECTION lock;
    BOOL             initialized;
    ULONG            sampleRate;
    float            ring[SPECTRUM_FFT_SIZE];
    int              ringWrite;
    int              ringCount;
    int              sinceLastFft;
    float            window[SPECTRUM_FFT_SIZE];
    float            re[SPECTRUM_FFT_SIZE];
    float            im[SPECTRUM_FFT_SIZE];
    float            smoothDb[SPECTRUM_BAND_COUNT];
    float            peakDb[SPECTRUM_BAND_COUNT];
    int              peakHold[SPECTRUM_BAND_COUNT];
    float            rmsDb;
    float            dominantHz;
    ULONGLONG        framesAnalyzed;
} SPECTRUM_STATE;
BOOL SpectrumInitialize(SPECTRUM_STATE *state, ULONG sampleRate);
void SpectrumReset(SPECTRUM_STATE *state, ULONG sampleRate);
void SpectrumDestroy(SPECTRUM_STATE *state);
void SpectrumPushMono(SPECTRUM_STATE *state, const float *samples, UINT count);
void SpectrumGetSnapshot(SPECTRUM_STATE *state, SPECTRUM_SNAPSHOT *snapshot);
const float *SpectrumGetBandCenters(void);
#endif