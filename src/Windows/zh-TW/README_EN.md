# SystemAudioRecorder 2.0-beta.1

Windows x64 system-audio spectrum analyzer, MP3 recorder, and optional URL media downloader.

**Author:** Teng Chuan-Liang  
**Platform:** Windows 7+ x64  
**Build toolchain:** Windows Driver Kit 7 / legacy `build.exe`

> This archive contains source code only. Rebuild the executable after source changes.

## Features

- Captures the default Windows playback stream with WASAPI loopback.
- Displays a real-time 4096-point FFT spectrum using a Hann window and 75% overlap.
- Records PCM to a temporary WAV file, then converts it to MP3 with external `ffmpeg.exe`.
- Optionally downloads URL media with external `yt-dlp.exe` + `ffmpeg.exe`.
- Provides an in-memory FIFO download queue with progress, reorder, and cancel controls.
- Lets the user choose the recording gain and download directory.

## Project layout

```text
main.cpp        Win32 UI and application lifetime
audio.cpp/.h    WASAPI loopback capture
spectrum.cpp/.h FFT and spectrum state
ffmpeg.cpp/.h   WAV recording and FFmpeg MP3 conversion
downloader.cpp/.h yt-dlp / FFmpeg process integration
queue.cpp/.h    Download queue UI
wasapi.h        Minimal WASAPI declarations for WDK7 builds
public.h        Shared Windows declarations
version.rc      Version metadata
sources         WDK7 build target
```

## Optional helper programs

Helper binaries are not included in the source archive. Put compatible standalone executables in:

```text
SystemAudioRecorder\
├─ SystemAudioRecorder.exe
└─ tools\
   ├─ ffmpeg.exe
   └─ yt-dlp.exe
```

| Feature | Required helper |
|---|---|
| WASAPI capture / spectrum | none |
| MP3 recording | `ffmpeg.exe` |
| URL audio/video | `yt-dlp.exe` + `ffmpeg.exe` |

## Build

Open a Windows 7 x64 Free Build Environment and run:

```bat
build -cZg
```

The target name in `sources` is `SystemAudioRecorder`.

## URL-download notice

Use the URL feature only when the download method is allowed by applicable law and by the source platform/service terms. The application does not implement DRM circumvention, paid-content unlocking, browser-cookie import, account/session extraction, login/paywall bypass, or access-control bypass.

See `CONTENT_DOWNLOAD_NOTICE.md` and `THIRD_PARTY_NOTICES.md`.

## Known limitations

- Windows-only capture backend (WASAPI loopback).
- URL-media compatibility depends on the separately installed yt-dlp/FFmpeg versions and the source service.
- The download queue is in memory and is not persisted after exit.
- Classic WAV temporary recording is limited to approximately 4 GB.
- This source-cleanup archive was statically reviewed here, but the WDK7 Windows build was not executed in this Linux review environment.

## License status

This is a pre-release development package. Copyright remains with Teng Chuan-Liang. `LICENSE.txt` currently does not grant a public redistribution license; select one before publishing source or binaries as an open-source project.
