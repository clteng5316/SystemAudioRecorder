/* SystemAudioRecorder - spectrum analyzer */
#ifndef SYSTEM_AUDIO_RECORDER_LINUX_SPECTRUM_H
#define SYSTEM_AUDIO_RECORDER_LINUX_SPECTRUM_H

#include "public.h"
#include <array>

constexpr int SPECTRUM_FFT_SIZE = 4096;
constexpr int SPECTRUM_HOP_SIZE = 1024;
constexpr int SPECTRUM_BAND_COUNT = 26;

struct SpectrumSnapshot {
    std::array<float, SPECTRUM_BAND_COUNT> bandDb{};
    std::array<float, SPECTRUM_BAND_COUNT> peakDb{};
    float rmsDb{-90.0f};
    float dominantHz{0.0f};
    uint32_t sampleRate{0};
    uint64_t framesAnalyzed{0};
};

struct SpectrumState {
    std::mutex lock;
    bool initialized{false};
    uint32_t sampleRate{0};
    std::array<float, SPECTRUM_FFT_SIZE> ring{};
    int ringWrite{0};
    int ringCount{0};
    int sinceLastFft{0};
    std::array<float, SPECTRUM_FFT_SIZE> window{};
    std::array<float, SPECTRUM_FFT_SIZE> re{};
    std::array<float, SPECTRUM_FFT_SIZE> im{};
    std::array<float, SPECTRUM_BAND_COUNT> smoothDb{};
    std::array<float, SPECTRUM_BAND_COUNT> peakDb{};
    std::array<int, SPECTRUM_BAND_COUNT> peakHold{};
    float rmsDb{-90.0f};
    float dominantHz{0.0f};
    uint64_t framesAnalyzed{0};
};

bool SpectrumInitialize(SpectrumState *state, uint32_t sampleRate);
void SpectrumReset(SpectrumState *state, uint32_t sampleRate);
void SpectrumPushMono(SpectrumState *state, const float *samples, size_t count);
void SpectrumGetSnapshot(SpectrumState *state, SpectrumSnapshot *snapshot);
const std::array<float, SPECTRUM_BAND_COUNT> &SpectrumGetBandCenters();

#endif
