# SystemAudioRecorder

SystemAudioRecorder is a pre-release system-audio spectrum analyzer and recorder with a Windows x64 GUI implementation, a Linux CLI reference port, and a separately distributed Linux GUI edition.

**Current release line:** `v2.0-beta.1`  
**Author:** Teng Chuan-Liang

## Editions

| Platform | Language | Interface | Source availability |
|---|---|---|---|
| Windows x64 | English (en-US) | Win32 GUI | Public: `src/Windows/en-US` |
| Windows x64 | Traditional Chinese (zh-TW) | Win32 GUI | Public: `src/Windows/zh-TW` |
| Linux x64 | English (en-US) | Terminal / CLI | Public: `src/Linux/en-US` |
| Linux x64 | Traditional Chinese (zh-TW) | Terminal / CLI | Public: `src/Linux/zh-TW` |
| Linux x64 | English (en-US) | GUI | Binary release only; source not public |
| Linux x64 | Traditional Chinese (zh-TW) | GUI | Binary release only; source not public |

## Source code availability

Source code for the Windows editions and Linux CLI reference port is available in this repository.

The Linux GUI edition is distributed as a binary release only. Its source code is not included in this public repository or in GitHub's automatically generated Source code archives.

For Linux GUI source-code inquiries, licensing discussions, or development-related requests, please contact:

`clteng5316@gmail.com`

Access to the Linux GUI source code is subject to the copyright holder's approval and applicable terms.

## Highlights

### Windows GUI

- Captures the default Windows playback stream with WASAPI loopback.
- Displays a real-time 4096-point FFT spectrum using a Hann window and 75% overlap.
- Records PCM to a temporary WAV file and converts it to MP3 with external FFmpeg.
- Supports optional URL media download with external `yt-dlp` + FFmpeg.
- Provides an in-memory FIFO download queue with progress, reorder, and cancel controls.
- Supports recording-gain and download-directory selection.

### Linux CLI reference port

The Linux CLI version is a reference port based on the Windows project's module layout and feature concepts. It is not a revision of an older Linux SystemAudioRecorder codebase.

- Uses a PulseAudio-compatible monitor source through FFmpeg for system-audio capture.
- Works with common PipeWire desktop setups through the PulseAudio compatibility layer.
- Uses the same 4096-point FFT / Hann window / 75% overlap design.
- Provides MP3 recording and optional URL-download functions through external tools.
- Uses a terminal / CLI interface.

### Linux GUI edition

The Linux GUI edition is provided separately from the Linux CLI reference port.

- Uses a graphical user interface.
- Does not replace the Linux CLI edition.
- English and Traditional Chinese binary packages are provided separately.
- Linux GUI source code is not included in the public repository.

## Repository layout

```text
SystemAudioRecorder
└─ src
   ├─ Windows
   │  ├─ en-US
   │  └─ zh-TW
   └─ Linux
      ├─ en-US
      └─ zh-TW
```

The `src/Linux` directories contain the public Linux CLI reference-port source code. Linux GUI source code is not included in this repository.

Each public source edition contains its own detailed `README.md`, build files, notices, and source code.

## Build overview

### Windows x64

The Windows source targets Windows 7+ x64 and uses the Windows Driver Kit 7 legacy `build.exe` toolchain.

Typical build command:

```bat
build -cZg
```

See the README inside the selected Windows language directory for details.

### Linux CLI

Typical dependencies include:

- C++17 compiler (`g++` tested)
- `make`
- `ffmpeg` with PulseAudio input support
- `pactl`
- `yt-dlp` only for URL-download functions

Build:

```bash
make
```

Run:

```bash
./SystemAudioRecorder
```

See the README inside the selected Linux language directory for platform-specific details.

### Linux GUI

Prebuilt Linux GUI packages are distributed through GitHub Releases.

The Linux GUI source code is not part of the public repository. For source-code or licensing inquiries, contact:

`clteng5316@gmail.com`

## External tools

Third-party helper binaries are not bundled with the public source tree or release packages unless explicitly stated.

- Windows MP3 recording: `ffmpeg.exe`
- Windows URL media functions: `yt-dlp.exe` + `ffmpeg.exe`
- Linux system-audio capture: FFmpeg + `pactl`
- Linux URL media functions: `yt-dlp` + FFmpeg

These tools remain governed by their own licenses.

## URL-download notice

Use URL-download functions only when the download method is permitted by applicable law and by the relevant source platform or service terms.

SystemAudioRecorder does not implement DRM circumvention, paid-content unlocking, browser-cookie/session extraction, login/paywall bypass, or access-control bypass.

## Linux CLI port note

The Linux CLI reference port was created on 2026-08-29. Its module layout intentionally mirrors the cleaned Windows project (`main`, `audio`, `spectrum`, `ffmpeg`, `downloader`, `queue`, and `public`).

To keep build dependencies small, the Linux CLI port does not directly link against libpulse or libpipewire development libraries; FFmpeg performs PulseAudio input and the application reads raw PCM through a pipe.

## Version notes

`v2.0-beta.1` includes:

- SystemAudioRecorder as the public product name.
- English and Traditional Chinese documentation.
- Windows GUI editions for en-US and zh-TW.
- Linux CLI reference-port editions for en-US and zh-TW.
- Linux GUI binary editions for en-US and zh-TW.
- Cleanup of stale previous-name branding and old pre-release version labels.
- Capture-thread restart cleanup.
- Downloader-worker shutdown synchronization.
- Download-queue initialization checks.
- Compact spectrum MP3 status rendering fixes.

## License status

This project is currently a pre-release development package.

No open-source redistribution license is granted by this README. Copyright remains with Teng Chuan-Liang unless and until a repository-level license states otherwise.

---

## 繁體中文說明

SystemAudioRecorder 是預發佈的系統音訊頻譜分析與錄音工具，目前包含 Windows x64 GUI、Linux CLI 參考移植版，以及另外提供的 Linux GUI 執行版本。

### 版本

- Windows x64 GUI：英文（en-US）與繁體中文（zh-TW），原始碼已公開於本 Repository。
- Linux CLI：英文（en-US）與繁體中文（zh-TW），CLI 參考移植版原始碼已公開於本 Repository。
- Linux GUI：英文（en-US）與繁體中文（zh-TW），僅提供執行檔，不公開 GUI 原始碼。

### Linux GUI 原始碼

Linux GUI 本次僅提供執行檔，不包含於公開 Repository 或 GitHub 自動產生的 Source code ZIP / TAR.GZ 中。

如需 Linux GUI 原始碼、洽談授權或其他開發相關需求，請聯絡：

`clteng5316@gmail.com`

是否提供原始碼及相關使用條件，須經著作權人同意。

### Windows GUI

- 使用 WASAPI loopback 擷取 Windows 預設播放裝置的系統音訊。
- 即時顯示 4096 點 FFT 頻譜，使用 Hann window 與 75% overlap。
- 透過外部 FFmpeg 將暫存 WAV 轉為 MP3。
- 可選用 `yt-dlp` + FFmpeg 進行 URL 媒體下載。
- 提供記憶體內 FIFO 下載佇列。

### Linux CLI 參考移植版

Linux CLI 是依 Windows 版的模組配置與功能概念重新建立的參考移植版，並不是舊 Linux 原始碼的修正版。

它使用 PulseAudio 相容 monitor source、FFmpeg 與 `pactl` 進行系統音訊擷取，介面為終端機 CLI。

### Linux GUI

Linux GUI 與 CLI 分開提供，不取代 CLI 參考移植版。

Linux GUI 使用圖形使用者介面；本次公開 Release 僅提供英文與繁體中文執行檔，不公開 GUI 原始碼。

詳細建置方式、限制與使用說明，請參閱各公開原始碼語言目錄內的 `README.md`，以及 GitHub Release 中各 binary 套件附帶的 `README.txt`。
