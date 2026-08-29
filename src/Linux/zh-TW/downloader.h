/* SystemAudioRecorder - Linux URL downloader */
#ifndef SYSTEM_AUDIO_RECORDER_LINUX_DOWNLOADER_H
#define SYSTEM_AUDIO_RECORDER_LINUX_DOWNLOADER_H
#include "public.h"
enum class DownloadMode { Video, Audio };
struct DownloadResult { bool success{false}; int exitCode{-1}; std::string message; };
bool DownloaderToolsAvailable(std::string *message);
DownloadResult DownloaderRun(const std::string &url, DownloadMode mode, const std::string &outputDirectory);
#endif
