/* SystemAudioRecorder - Linux system-audio capture */
#ifndef SYSTEM_AUDIO_RECORDER_LINUX_AUDIO_H
#define SYSTEM_AUDIO_RECORDER_LINUX_AUDIO_H
#include "public.h"
#include "spectrum.h"
#include "ffmpeg.h"

struct AudioStatus {
    bool running{false};
    uint32_t sampleRate{48000};
    uint16_t channels{2};
    std::string monitorSource;
    std::string message{"就緒"};
};

struct AudioEngine {
    std::mutex lock;
    std::thread worker;
    std::atomic<bool> stopRequested{false};
    std::atomic<pid_t> ffmpegPid{-1};
    AudioStatus status;
    SpectrumState spectrum;
    Mp3Recorder recorder;
};

bool AudioEngineInitialize(AudioEngine *engine);
bool AudioEngineStart(AudioEngine *engine);
void AudioEngineStop(AudioEngine *engine);
AudioStatus AudioEngineGetStatus(AudioEngine *engine);
bool AudioEngineStartMp3(AudioEngine *engine, const std::string &path, int bitrateKbps, int gainDb);
bool AudioEngineStopMp3(AudioEngine *engine);

#endif
