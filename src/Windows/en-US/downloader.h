/*
 * SystemAudioRecorder - URL downloader integration
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */

#ifndef SYSTEMAUDIORECORDER_DOWNLOADER_H
#define SYSTEMAUDIORECORDER_DOWNLOADER_H
#include "public.h"
#define DOWNLOADER_URL_MAX 2048
#define DOWNLOADER_TEXT_MAX 768
#define DOWNLOADER_NAME_MAX 512
#define DOWNLOAD_MODE_VIDEO 1
#define DOWNLOAD_MODE_AUDIO 2
typedef struct _DOWNLOADER_STATUS
{
    BOOL  initialized;
    BOOL  busy;
    BOOL  success;
    DWORD exitCode;
    int   mode;
    int   progressPercent;
    WCHAR displayName[DOWNLOADER_NAME_MAX];
    WCHAR message[DOWNLOADER_TEXT_MAX];
    WCHAR outputDirectory[MAX_PATH];
    WCHAR ytDlpPath[MAX_PATH];
    WCHAR ffmpegPath[MAX_PATH];
} DOWNLOADER_STATUS;
typedef struct _DOWNLOADER_STATE
{
    CRITICAL_SECTION lock;
    BOOL             lockInitialized;
    HANDLE           thread;
    HANDLE           process;
    BOOL             busy;
    BOOL             success;
    BOOL             haveDiagnosticError;
    DWORD            exitCode;
    int              mode;
    int              progressPercent;
    WCHAR            displayName[DOWNLOADER_NAME_MAX];
    WCHAR            url[DOWNLOADER_URL_MAX];
    WCHAR            message[DOWNLOADER_TEXT_MAX];
    WCHAR            exeDirectory[MAX_PATH];
    WCHAR            outputDirectory[MAX_PATH];
    WCHAR            ytDlpPath[MAX_PATH];
    WCHAR            ffmpegPath[MAX_PATH];
} DOWNLOADER_STATE;
BOOL DownloaderInitialize(DOWNLOADER_STATE *state);
void DownloaderDestroy(DOWNLOADER_STATE *state);
BOOL DownloaderStart(DOWNLOADER_STATE *state, LPCWSTR url, int mode);
void DownloaderGetStatus(DOWNLOADER_STATE *state, DOWNLOADER_STATUS *status);
BOOL DownloaderRefreshTools(DOWNLOADER_STATE *state);
BOOL DownloaderSetOutputDirectory(DOWNLOADER_STATE *state, LPCWSTR directory);
#endif