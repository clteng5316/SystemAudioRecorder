# SystemAudioRecorder Linux Reference Port

Version 2.0-beta.1-linux-port. This is a new Linux reference port that mirrors the Windows project's module boundaries and core feature ideas; no prior Linux SystemAudioRecorder source was available to modify.

Capture path: default PulseAudio/PipeWire-Pulse sink monitor -> FFmpeg `-f pulse` -> signed 16-bit stereo PCM -> spectrum / optional WAV-to-MP3 recorder.

Build with `make`; run `./SystemAudioRecorder --help`.
