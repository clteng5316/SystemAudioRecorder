# Changelog

## 2.0-beta.1

### Added

- SystemAudioRecorder public product name.
- English and Traditional Chinese documentation.
- Windows x64 source editions for `en-US` and `zh-TW`.
- Linux `en-US` and `zh-TW` reference ports based on the cleaned Windows module layout.

### Fixed

- Removed stale previous-name branding and old pre-release version labels.
- Aligned README source-file names with the actual source packages.
- Reap a completed capture thread before restarting after a capture error.
- Wait for downloader worker shutdown before destroying shared state.
- Check downloader initialization before creating the download queue.
- Keep the compact spectrum MP3 status on one rendered line.

### Linux reference port

- New Linux reference port created on 2026-08-29.
- Module layout mirrors the cleaned Windows project: `main`, `audio`, `spectrum`, `ffmpeg`, `downloader`, `queue`, and `public`.
- System-audio capture uses FFmpeg PulseAudio input and reads raw PCM through a pipe.
- The port avoids direct libpulse/libpipewire development-library linkage to keep build dependencies smaller.

### Known limitations

- Windows WDK7 build verification has now been performed outside the earlier Linux-only review environment.
- Linux system-audio capture still depends on the target PulseAudio/PipeWire-compatible monitor-source environment.
- Helper-program behavior depends on separately installed FFmpeg and yt-dlp versions.
- Download queues are kept in memory and are not persisted after program exit.
