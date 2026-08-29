# SystemAudioRecorder Linux Reference Port

**Version:** 2.0-beta.1-linux-port  
**Author:** Teng Chuan-Liang  
**Interface:** English (United States) terminal / CLI

> This is a new Linux reference port based on the module layout and feature concepts of the Windows SystemAudioRecorder project. No verifiable earlier Linux SystemAudioRecorder source was available, so this directory is not a revision of an older Linux implementation.

## Architecture mapping

| Windows version | Linux reference port |
|---|---|
| WASAPI loopback capture | PulseAudio/PipeWire-Pulse monitor capture through FFmpeg |
| Win32 GUI | Terminal CLI |
| 4096-point FFT / Hann / 75% overlap | Same |
| External FFmpeg MP3 conversion | External FFmpeg MP3 conversion |
| External yt-dlp + FFmpeg download | External yt-dlp + FFmpeg download |
| Download queue | In-memory FIFO worker queue |

## System-audio capture

Most current Linux desktops use PipeWire. PipeWire can provide a PulseAudio-compatible server, allowing standard PulseAudio client tools such as `pactl` to continue working. This reference port uses `pactl` to locate the default sink and its monitor source, then uses FFmpeg's PulseAudio input to capture that monitor source.

Capture path:

```text
default sink monitor
        |
        v
FFmpeg -f pulse
        |
        v
signed 16-bit stereo PCM / 48 kHz
        |
        +--> 4096-point FFT spectrum
        |
        +--> optional WAV -> MP3 recording
```

## Dependencies

Required:

- C++17 compiler (`g++` tested)
- `make`
- `ffmpeg` with PulseAudio input (`-f pulse`) support
- `pactl`
- `yt-dlp` only for URL download functions

Typical Ubuntu/Debian package installation:

```bash
sudo apt install build-essential ffmpeg pulseaudio-utils
```

Install `yt-dlp` using your distribution's package or the installation method recommended by the upstream project.

## Build

```bash
make
```

The executable is created as:

```text
SystemAudioRecorder
```

Run:

```bash
./SystemAudioRecorder
```

Show command help directly:

```bash
./SystemAudioRecorder --help
```

## Commands

```text
start                         Start system-audio capture
stop                          Stop capture
spectrum                      Show the current spectrum
record <file.mp3> [gain]      Start MP3 recording (default gain: 6 dB)
stop-record                   Stop recording and convert to MP3
add-audio <url>               Add an audio download to the queue
add-video <url>               Add a video download to the queue
queue                         Show the download queue
status                        Show capture status
help                          Show command help
quit                          Exit
```

## External tools

This source package does not bundle FFmpeg, `pactl`, or `yt-dlp`. They are launched as separate external programs and remain governed by their own licenses.

## URL download notice

Use URL downloading only when the download method is permitted by applicable law and by the applicable source platform or service terms. The program does not implement DRM circumvention, paid-content unlocking, browser-cookie/session extraction, login/paywall bypass, or access-control bypass.

## Limitations

- This is a CLI reference port, not a pixel-for-pixel Linux reproduction of the Windows Win32 GUI.
- System-audio capture depends on a PulseAudio-compatible server and an available monitor source. Headless systems or unusual PipeWire configurations may require adjustment.
- The download queue exists only in memory and is not preserved after the program exits.
- This version has been compile-tested, but actual monitor-source capture depends on the target Linux audio environment and should be verified on the destination system.

## License status

This package is a pre-release development archive. See `LICENSE.txt` before public redistribution.
