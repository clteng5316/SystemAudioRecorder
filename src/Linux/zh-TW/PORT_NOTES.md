# Linux port notes

- New reference port created 2026-08-29.
- No existing Linux SystemAudioRecorder source was found in the provided archive or retrievable prior project files.
- Module layout intentionally mirrors the cleaned Windows project: `main`, `audio`, `spectrum`, `ffmpeg`, `downloader`, `queue`, `public`.
- Capture is intentionally implemented without linking libpulse/libpipewire development libraries; FFmpeg performs PulseAudio input and the app reads raw PCM through a pipe.
- This keeps build dependencies small but means runtime capture requires an FFmpeg build with PulseAudio input support.
