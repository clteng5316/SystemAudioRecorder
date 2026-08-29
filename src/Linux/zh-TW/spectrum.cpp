/*
 * SystemAudioRecorder - spectrum analyzer
 * Linux port of the same 4096-point FFT / Hann / 75% overlap design.
 */
#include "spectrum.h"
#include <algorithm>
#include <cmath>
#include <cstring>

static constexpr double kPi = 3.14159265358979323846;
static const std::array<float, SPECTRUM_BAND_COUNT> g_bandCenter = {
    50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f, 200.0f,
    250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f,
    1250.0f, 1600.0f, 2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f,
    6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f
};

const std::array<float, SPECTRUM_BAND_COUNT> &SpectrumGetBandCenters() { return g_bandCenter; }

static float clamp_db(float v) {
    return std::max(-90.0f, std::min(0.0f, v));
}

static void fft_in_place(float *re, float *im, int n) {
    int j = 0;
    for (int i = 1; i < n; ++i) {
        int bit = n >> 1;
        while (j & bit) { j ^= bit; bit >>= 1; }
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int len = 2; len <= n; len <<= 1) {
        const int half = len >> 1;
        const double angle = -2.0 * kPi / static_cast<double>(len);
        const float wlenRe = static_cast<float>(std::cos(angle));
        const float wlenIm = static_cast<float>(std::sin(angle));
        for (int base = 0; base < n; base += len) {
            float wRe = 1.0f, wIm = 0.0f;
            for (int k = 0; k < half; ++k) {
                const int u = base + k, v = base + k + half;
                const float vr = re[v] * wRe - im[v] * wIm;
                const float vi = re[v] * wIm + im[v] * wRe;
                const float ur = re[u], ui = im[u];
                re[u] = ur + vr; im[u] = ui + vi;
                re[v] = ur - vr; im[v] = ui - vi;
                const float nextRe = wRe * wlenRe - wIm * wlenIm;
                wIm = wRe * wlenIm + wIm * wlenRe;
                wRe = nextRe;
            }
        }
    }
}

static void analyze_locked(SpectrumState *state) {
    const int oldest = state->ringWrite;
    double sumSq = 0.0, windowEnergy = 0.0;
    for (int i = 0; i < SPECTRUM_FFT_SIZE; ++i) {
        int idx = oldest + i;
        if (idx >= SPECTRUM_FFT_SIZE) idx -= SPECTRUM_FFT_SIZE;
        const float x = state->ring[idx];
        sumSq += static_cast<double>(x) * x;
        state->re[i] = x * state->window[i];
        state->im[i] = 0.0f;
        windowEnergy += static_cast<double>(state->window[i]) * state->window[i];
    }
    state->rmsDb = clamp_db(static_cast<float>(20.0 * std::log10(std::sqrt(sumSq / SPECTRUM_FFT_SIZE) + 1e-12)));
    fft_in_place(state->re.data(), state->im.data(), SPECTRUM_FFT_SIZE);

    for (int i = 0; i < SPECTRUM_BAND_COUNT; ++i) {
        const double low = (i == 0)
            ? g_bandCenter[i] / std::sqrt(g_bandCenter[i + 1] / g_bandCenter[i])
            : std::sqrt(g_bandCenter[i - 1] * g_bandCenter[i]);
        const double high = (i == SPECTRUM_BAND_COUNT - 1)
            ? g_bandCenter[i] * std::sqrt(g_bandCenter[i] / g_bandCenter[i - 1])
            : std::sqrt(g_bandCenter[i] * g_bandCenter[i + 1]);
        int firstBin = static_cast<int>(std::ceil(low * SPECTRUM_FFT_SIZE / state->sampleRate));
        int lastBin  = static_cast<int>(std::floor(high * SPECTRUM_FFT_SIZE / state->sampleRate));
        firstBin = std::max(1, firstBin);
        lastBin = std::min(SPECTRUM_FFT_SIZE / 2 - 1, lastBin);
        if (lastBin < firstBin) lastBin = firstBin;
        double powerSum = 0.0;
        int powerCount = 0;
        for (int b = firstBin; b <= lastBin; ++b) {
            const double pr = state->re[b], pi = state->im[b];
            powerSum += pr * pr + pi * pi;
            ++powerCount;
        }
        float db = -90.0f;
        if (powerCount > 0 && windowEnergy > 0.0) {
            const double normalized = (powerSum / powerCount) / windowEnergy;
            db = clamp_db(static_cast<float>(10.0 * std::log10(normalized + 1e-18)));
        }
        const float old = state->smoothDb[i];
        state->smoothDb[i] = (db > old) ? old * 0.30f + db * 0.70f : old * 0.88f + db * 0.12f;
        if (state->smoothDb[i] >= state->peakDb[i]) {
            state->peakDb[i] = state->smoothDb[i]; state->peakHold[i] = 18;
        } else if (state->peakHold[i] > 0) {
            --state->peakHold[i];
        } else {
            state->peakDb[i] = std::max(state->smoothDb[i], state->peakDb[i] - 0.75f);
        }
    }

    float maxDb = -90.0f, maxHz = 0.0f;
    for (int i = 2; i < SPECTRUM_FFT_SIZE / 2; ++i) {
        const float hz = static_cast<float>(i) * state->sampleRate / SPECTRUM_FFT_SIZE;
        if (hz < 40.0f || hz > 18000.0f) continue;
        const double pr = state->re[i], pi = state->im[i];
        const float db = clamp_db(static_cast<float>(10.0 * std::log10((pr * pr + pi * pi) / (windowEnergy + 1e-18) + 1e-18)));
        if (db > maxDb) { maxDb = db; maxHz = hz; }
    }
    state->dominantHz = maxHz;
    state->framesAnalyzed += SPECTRUM_HOP_SIZE;
}

bool SpectrumInitialize(SpectrumState *state, uint32_t sampleRate) {
    if (!state || sampleRate == 0) return false;
    std::lock_guard<std::mutex> guard(state->lock);
    state->initialized = true;
    state->sampleRate = sampleRate;
    for (int i = 0; i < SPECTRUM_FFT_SIZE; ++i)
        state->window[i] = 0.5f - 0.5f * static_cast<float>(std::cos(2.0 * kPi * i / (SPECTRUM_FFT_SIZE - 1)));
    state->smoothDb.fill(-90.0f); state->peakDb.fill(-90.0f); state->peakHold.fill(0);
    return true;
}

void SpectrumReset(SpectrumState *state, uint32_t sampleRate) {
    if (!state || !state->initialized) return;
    std::lock_guard<std::mutex> guard(state->lock);
    state->ring.fill(0.0f); state->ringWrite = 0; state->ringCount = 0; state->sinceLastFft = 0;
    state->sampleRate = sampleRate; state->rmsDb = -90.0f; state->dominantHz = 0.0f; state->framesAnalyzed = 0;
    state->smoothDb.fill(-90.0f); state->peakDb.fill(-90.0f); state->peakHold.fill(0);
}

void SpectrumPushMono(SpectrumState *state, const float *samples, size_t count) {
    if (!state || !samples || count == 0 || !state->initialized) return;
    std::lock_guard<std::mutex> guard(state->lock);
    for (size_t i = 0; i < count; ++i) {
        state->ring[state->ringWrite++] = samples[i];
        if (state->ringWrite >= SPECTRUM_FFT_SIZE) state->ringWrite = 0;
        if (state->ringCount < SPECTRUM_FFT_SIZE) ++state->ringCount;
        if (++state->sinceLastFft >= SPECTRUM_HOP_SIZE && state->ringCount == SPECTRUM_FFT_SIZE) {
            state->sinceLastFft = 0; analyze_locked(state);
        }
    }
}

void SpectrumGetSnapshot(SpectrumState *state, SpectrumSnapshot *snapshot) {
    if (!snapshot) return;
    *snapshot = SpectrumSnapshot{};
    if (!state || !state->initialized) return;
    std::lock_guard<std::mutex> guard(state->lock);
    snapshot->bandDb = state->smoothDb; snapshot->peakDb = state->peakDb;
    snapshot->rmsDb = state->rmsDb; snapshot->dominantHz = state->dominantHz;
    snapshot->sampleRate = state->sampleRate; snapshot->framesAnalyzed = state->framesAnalyzed;
}
