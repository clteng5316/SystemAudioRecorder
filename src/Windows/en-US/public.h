/*
 * SystemAudioRecorder
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */

#ifndef SYSTEM_AUDIO_RECORDER_PUBLIC_H
#define SYSTEM_AUDIO_RECORDER_PUBLIC_H

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef WINVER
#define WINVER 0x0601
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include "wasapi.h"
#include <commdlg.h>
#include <strsafe.h>
#include <ks.h>
#include <ksmedia.h>
#include <math.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
#define SAFE_RELEASE(p) do { if ((p) != NULL) { (p)->Release(); (p) = NULL; } } while (0)

#endif
