/*
 * SystemAudioRecorder - download queue
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */
#ifndef SYSTEMAUDIORECORDER_DOWNLOAD_QUEUE_WIN_H
#define SYSTEMAUDIORECORDER_DOWNLOAD_QUEUE_WIN_H

#include "downloader.h"

BOOL DownloadQueueInitialize(HINSTANCE instance, DOWNLOADER_STATE *downloader);
void DownloadQueueDestroy(void);
void DownloadQueueShow(HWND owner);
void DownloadQueueTick(HWND owner);
void DownloadQueueGetCounts(int *waiting, int *active, int *completed, int *failed);

#endif
