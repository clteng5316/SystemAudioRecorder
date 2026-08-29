/* SystemAudioRecorder - Linux WAV/MP3 recorder */
#ifndef SYSTEM_AUDIO_RECORDER_LINUX_FFMPEG_H
#define SYSTEM_AUDIO_RECORDER_LINUX_FFMPEG_H
#include "public.h"

struct Mp3Recorder {
    std::mutex lock;
    FILE *wave{nullptr};
    bool recording{false};
    std::string targetPath;
    std::string tempPath;
    uint32_t sampleRate{48000};
    uint16_t channels{2};
    uint64_t dataBytes{0};
    int bitrateKbps{192};
    int gainDb{6};
    std::string message{"MP3 recorder ready"};
};

bool FfmpegMp3Start(Mp3Recorder *rec, const std::string &path, uint32_t sampleRate, int bitrateKbps, int gainDb);
bool FfmpegMp3Write(Mp3Recorder *rec, const int16_t *samples, size_t frames, uint16_t channels);
bool FfmpegMp3Stop(Mp3Recorder *rec);
std::string FfmpegMp3Message(Mp3Recorder *rec);
bool FfmpegAvailable();

#endif
