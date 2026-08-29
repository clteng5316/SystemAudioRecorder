/*
 * SystemAudioRecorder - spectrum analyzer
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */
#include "spectrum.h"
static const float g_bandCenter[SPECTRUM_BAND_COUNT] =
{
    50.0f,   63.0f,   80.0f,    100.0f,   125.0f,  160.0f,  200.0f,
    250.0f,  315.0f,  400.0f,   500.0f,   630.0f,  800.0f,  1000.0f,
    1250.0f, 1600.0f, 2000.0f,  2500.0f,  3150.0f, 4000.0f, 5000.0f,
    6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f
};
const float *SpectrumGetBandCenters(void)
{
    return g_bandCenter;
};
static void fft_in_place(float *re, float *im, int n)
{
    int i;
    int j;
    int bit;
    int len;
    j = 0;
    for (i = 1; i < n; ++i)
    {
        bit = n >> 1;
        while ((j & bit) != 0)
        {
            j ^= bit;
            bit >>= 1;
        };
        j ^= bit;
        if (i < j)
        {
            float tr = re[i];
            float ti = im[i];
            re[i] = re[j];
            im[i] = im[j];
            re[j] = tr;
            im[j] = ti;
        };
    };
    for (len = 2; len <= n; len <<= 1)
    {
        int    half   = len >> 1;
        double angle  = -2.0 * M_PI / (double)len;
        float  wlenRe = (float)cos(angle);
        float  wlenIm = (float)sin(angle);
        int    base;

        for (base = 0; base < n; base += len)
        {
            float wRe = 1.0f;
            float wIm = 0.0f;
            int   k;
            for (k = 0; k < half; ++k)
            {
                int   u  = base + k;
                int   v  = base + k + half;
                float vr = re[v] * wRe - im[v] * wIm;
                float vi = re[v] * wIm + im[v] * wRe;
                float ur = re[u];
                float ui = im[u];
                float nextRe;
                re[u] = ur + vr;
                im[u] = ui + vi;
                re[v] = ur - vr;
                im[v] = ui - vi;
                nextRe = wRe * wlenRe - wIm * wlenIm;
                wIm    = wRe * wlenIm + wIm * wlenRe;
                wRe    = nextRe;
            };
        };
    };
};
static float clamp_db(float v)
{
    if (v < -90.0f)
        return -90.0f;
    if (v > 0.0f)
        return 0.0f;
    return v;
};
static void analyze_locked(SPECTRUM_STATE *state)
{
    int    i;
    int    oldest;
    double sumSq;
    double windowEnergy;
    float  maxDb;
    float  maxHz;
    oldest       = state->ringWrite;
    sumSq        = 0.0;
    windowEnergy = 0.0;
    for (i = 0; i < SPECTRUM_FFT_SIZE; ++i)
    {
        int   idx = oldest + i;
        float x;
        if (idx >= SPECTRUM_FFT_SIZE)
            idx -= SPECTRUM_FFT_SIZE;
        x            = state->ring[idx];
        sumSq       += (double)x * (double)x;
        state->re[i] = x * state->window[i];
        state->im[i] = 0.0f;
        windowEnergy += (double)state->window[i] * (double)state->window[i];
    };
    state->rmsDb = clamp_db((float)(20.0 * log10(sqrt(sumSq / (double)SPECTRUM_FFT_SIZE) + 1.0e-12)));
    fft_in_place(state->re, state->im, SPECTRUM_FFT_SIZE);
    maxDb = -90.0f;
    maxHz = 0.0f;
    for (i = 0; i < SPECTRUM_BAND_COUNT; ++i)
    {
        double low;
        double high;
        int    firstBin;
        int    lastBin;
        int    b;
        double powerSum;
        int    powerCount;
        float  db;
        float  old;
        if (i == 0)
            low = (double)g_bandCenter[i] / sqrt((double)g_bandCenter[i + 1] / (double)g_bandCenter[i]);
        else
            low = sqrt((double)g_bandCenter[i - 1] * (double)g_bandCenter[i]);
        if (i == SPECTRUM_BAND_COUNT - 1)
            high = (double)g_bandCenter[i] * sqrt((double)g_bandCenter[i] / (double)g_bandCenter[i - 1]);
        else
            high = sqrt((double)g_bandCenter[i] * (double)g_bandCenter[i + 1]);
        firstBin = (int)ceil(low * (double)SPECTRUM_FFT_SIZE / (double)state->sampleRate);
        lastBin  = (int)floor(high * (double)SPECTRUM_FFT_SIZE / (double)state->sampleRate);
        if (firstBin < 1)
            firstBin = 1;
        if (lastBin > SPECTRUM_FFT_SIZE / 2 - 1)
            lastBin = SPECTRUM_FFT_SIZE / 2 - 1;
        if (lastBin < firstBin)
            lastBin = firstBin;
        powerSum   = 0.0;
        powerCount = 0;
        for (b = firstBin; b <= lastBin; ++b)
        {
            double pr = (double)state->re[b];
            double pi = (double)state->im[b];
            powerSum += pr * pr + pi * pi;
            ++powerCount;
        };
        if (powerCount > 0 && windowEnergy > 0.0)
        {
            double normalized = (powerSum / (double)powerCount) / windowEnergy;
            db = clamp_db((float)(10.0 * log10(normalized + 1.0e-18)));
        }
        else
        {
            db = -90.0f;
        };
        old = state->smoothDb[i];
        if (db > old)
            state->smoothDb[i] = old * 0.30f + db * 0.70f;
        else
            state->smoothDb[i] = old * 0.88f + db * 0.12f;
        if (state->smoothDb[i] >= state->peakDb[i])
        {
            state->peakDb[i]   = state->smoothDb[i];
            state->peakHold[i] = 18;
        }
        else if (state->peakHold[i] > 0)
        {
            --state->peakHold[i];
        }
        else
        {
            state->peakDb[i] -= 0.75f;
            if (state->peakDb[i] < state->smoothDb[i])
                state->peakDb[i] = state->smoothDb[i];
        };
    };
    for (i = 2; i < SPECTRUM_FFT_SIZE / 2; ++i)
    {
        float  hz = ((float)i * (float)state->sampleRate) / (float)SPECTRUM_FFT_SIZE;
        double pr;
        double pi;
        float  db;
        if (hz < 40.0f || hz > 18000.0f)
            continue;
        pr = (double)state->re[i];
        pi = (double)state->im[i];
        db = clamp_db((float)(10.0 * log10((pr * pr + pi * pi) / (windowEnergy + 1.0e-18) + 1.0e-18)));
        if (db > maxDb)
        {
            maxDb = db;
            maxHz = hz;
        };
    };
    state->dominantHz = maxHz;
    state->framesAnalyzed += SPECTRUM_HOP_SIZE;
};
BOOL SpectrumInitialize(SPECTRUM_STATE *state, ULONG sampleRate)
{
    int i;
    if (state == NULL || sampleRate == 0)
        return FALSE;
    RtlZeroMemory(state, sizeof(*state));
    InitializeCriticalSection(&state->lock);
    state->initialized = TRUE;
    state->sampleRate  = sampleRate;
    for (i = 0; i < SPECTRUM_FFT_SIZE; ++i)
    {
        state->window[i] = 0.5f - 0.5f * (float)cos((2.0 * M_PI * (double)i) / (double)(SPECTRUM_FFT_SIZE - 1));
    };
    for (i = 0; i < SPECTRUM_BAND_COUNT; ++i)
    {
        state->smoothDb[i] = -90.0f;
        state->peakDb[i]   = -90.0f;
    };
    state->rmsDb = -90.0f;
    return TRUE;
};
void SpectrumReset(SPECTRUM_STATE *state, ULONG sampleRate)
{
    int i;
    if (state == NULL || !state->initialized)
        return;
    EnterCriticalSection(&state->lock);
    RtlZeroMemory(state->ring, sizeof(state->ring));
    state->ringWrite      = 0;
    state->ringCount      = 0;
    state->sinceLastFft   = 0;
    state->sampleRate     = sampleRate;
    state->rmsDb          = -90.0f;
    state->dominantHz     = 0.0f;
    state->framesAnalyzed = 0;
    for (i = 0; i < SPECTRUM_BAND_COUNT; ++i)
    {
        state->smoothDb[i] = -90.0f;
        state->peakDb[i] = -90.0f;
        state->peakHold[i] = 0;
    };
    LeaveCriticalSection(&state->lock);
};
void SpectrumDestroy(SPECTRUM_STATE *state)
{
    if (state == NULL || !state->initialized)
        return;
    DeleteCriticalSection(&state->lock);
    state->initialized = FALSE;
};
void SpectrumPushMono(SPECTRUM_STATE *state, const float *samples, UINT count)
{
    UINT i;
    if (state == NULL || samples == NULL || count == 0 || !state->initialized)
        return;
    EnterCriticalSection(&state->lock);
    for (i = 0; i < count; ++i)
    {
        state->ring[state->ringWrite] = samples[i];
        ++state->ringWrite;
        if (state->ringWrite >= SPECTRUM_FFT_SIZE)
            state->ringWrite = 0;
        if (state->ringCount < SPECTRUM_FFT_SIZE)
            ++state->ringCount;
        ++state->sinceLastFft;
        if (state->ringCount == SPECTRUM_FFT_SIZE && state->sinceLastFft >= SPECTRUM_HOP_SIZE)
        {
            state->sinceLastFft = 0;
            analyze_locked(state);
        };
    };
    LeaveCriticalSection(&state->lock);
};
void SpectrumGetSnapshot(SPECTRUM_STATE *state, SPECTRUM_SNAPSHOT *snapshot)
{
    int i;
    if (snapshot == NULL)
        return;
    RtlZeroMemory(snapshot, sizeof(*snapshot));
    if (state == NULL || !state->initialized)
        return;
    EnterCriticalSection(&state->lock);
    snapshot->sampleRate     = state->sampleRate;
    snapshot->rmsDb          = state->rmsDb;
    snapshot->dominantHz     = state->dominantHz;
    snapshot->framesAnalyzed = state->framesAnalyzed;
    for (i = 0; i < SPECTRUM_BAND_COUNT; ++i)
    {
        snapshot->bandDb[i] = state->smoothDb[i];
        snapshot->peakDb[i] = state->peakDb[i];
    };
    LeaveCriticalSection(&state->lock);
};