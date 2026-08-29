# Changelog

## 2.0-beta.1

### Added

- SystemAudioRecorder public product name.
- English and Traditional Chinese documentation.
- Windows x64 GUI source editions for `en-US` and `zh-TW`.
- Linux CLI reference-port source editions for `en-US` and `zh-TW`.
- Linux GUI binary editions for `en-US` and `zh-TW`.
- Separate Linux CLI and Linux GUI release packages.

### Source availability

- Windows GUI source code is available in this repository.
- Linux CLI reference-port source code is available in this repository.
- Linux GUI source code is not included in the public repository or public release.
- For Linux GUI source-code inquiries, licensing discussions, or development-related requests, contact `clteng5316@gmail.com`.

### Fixed

- Removed stale previous-name branding and old pre-release version labels.
- Aligned README source-file names with the actual source packages.
- Reap a completed capture thread before restarting after a capture error.
- Wait for downloader worker shutdown before destroying shared state.
- Check downloader initialization before creating the download queue.
- Keep the compact spectrum MP3 status on one rendered line.

### Linux CLI reference port

- New Linux CLI reference port created on 2026-08-29.
- Module layout mirrors the cleaned Windows project: `main`, `audio`, `spectrum`, `ffmpeg`, `downloader`, `queue`, and `public`.
- System-audio capture uses FFmpeg PulseAudio input and reads raw PCM through a pipe.
- The CLI port avoids direct libpulse/libpipewire development-library linkage to keep build dependencies smaller.
- English and Traditional Chinese terminal / CLI editions are provided separately.

### Linux GUI edition

- Added separate Linux GUI editions for `en-US` and `zh-TW`.
- Linux GUI does not replace the Linux CLI reference port; both are distributed separately.
- Linux GUI is distributed as binary-only release packages.
- Linux GUI source code is not included in GitHub's automatically generated Source code archives because it is not committed to the public repository.

### Release packages

- Windows x64 GUI:
  - `SystemAudioRecorder-v2.0-beta.1-Windows-x64-en-US.zip`
  - `SystemAudioRecorder-v2.0-beta.1-Windows-x64-zh-TW.zip`
- Linux x64 CLI:
  - `SystemAudioRecorder-v2.0-beta.1-Linux-x64-CLI-en-US.tar.gz`
  - `SystemAudioRecorder-v2.0-beta.1-Linux-x64-CLI-zh-TW.tar.gz`
- Linux x64 GUI:
  - `SystemAudioRecorder-v2.0-beta.1-Linux-x64-GUI-en-US.tar.gz`
  - `SystemAudioRecorder-v2.0-beta.1-Linux-x64-GUI-zh-TW.tar.gz`

### Known limitations

- Windows WDK7 build verification has been performed outside the earlier Linux-only review environment.
- Linux system-audio capture depends on the target PulseAudio/PipeWire-compatible audio environment.
- Helper-program behavior depends on separately installed FFmpeg and yt-dlp versions.
- Download queues are kept in memory and are not persisted after program exit.
- This is a beta pre-release and is not marked as production-ready.
