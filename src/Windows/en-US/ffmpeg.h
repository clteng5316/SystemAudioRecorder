/*
 * SystemAudioRecorder - FFmpeg MP3 integration
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */
#ifndef SYSTEM_AUDIO_RECORDER_MP3_FFMPEG_H
#define SYSTEM_AUDIO_RECORDER_MP3_FFMPEG_H
#include "public.h"
#define MP3_GAIN_MODE_0DB              0
#define MP3_GAIN_MODE_3DB              1
#define MP3_GAIN_MODE_6DB              2
#define MP3_GAIN_MODE_9DB              3
#define MP3_GAIN_MODE_12DB             4
#define MP3_GAIN_MODE_AUTO_NORMALIZE   5
typedef struct _FFMPEG_MP3_STATUS
{
    BOOL  initialized;
    BOOL  recording;
    BOOL  converting;
    BOOL  success;
    DWORD exitCode;
    WCHAR targetPath[MAX_PATH];
    WCHAR temporaryWavePath[MAX_PATH];
    WCHAR ffmpegPath[MAX_PATH];
    WCHAR message[256];
} FFMPEG_MP3_STATUS;
typedef struct _FFMPEG_MP3_WRITER
{
    CRITICAL_SECTION lock;
    BOOL             lockInitialized;
    HANDLE           waveFile;
    HANDLE           convertThread;
    BOOL             recording;
    BOOL             converting;
    BOOL             success;
    DWORD            exitCode;
    LONG             sampleRate;
    LONG             bitrateKbps;
    LONG             gainMode;
    ULONGLONG        dataBytes;
    WCHAR            targetPath[MAX_PATH];
    WCHAR            temporaryWavePath[MAX_PATH];
    WCHAR            ffmpegPath[MAX_PATH];
    WCHAR            message[256];
} FFMPEG_MP3_WRITER;
BOOL FfmpegMp3Initialize(FFMPEG_MP3_WRITER *writer);
void FfmpegMp3Destroy(FFMPEG_MP3_WRITER *writer);
BOOL FfmpegMp3Start(FFMPEG_MP3_WRITER *writer, LPCWSTR path, LONG sampleRate, LONG bitrateKbps, LONG gainMode);
BOOL FfmpegMp3WriteStereo16(FFMPEG_MP3_WRITER *writer, const short *samples, UINT frames);
BOOL FfmpegMp3Stop(FFMPEG_MP3_WRITER *writer);
void FfmpegMp3GetStatus(FFMPEG_MP3_WRITER *writer, FFMPEG_MP3_STATUS *status);
#endif