# SystemAudioRecorder Linux 參考移植版

**版本：** 2.0-beta.1-linux-port  
**作者：** Teng Chuan-Liang  
**介面：** Traditional Chinese terminal / CLI

> 這是一份依 Windows SystemAudioRecorder 的模組與功能概念重新建立的 Linux 參考移植版。現有資料中沒有可驗證的舊 Linux SystemAudioRecorder 原始碼，因此本目錄不是「舊 Linux 版修正版」。

## 對應架構

| Windows 版 | Linux 參考移植版 |
|---|---|
| WASAPI loopback | FFmpeg PulseAudio input + default sink monitor |
| Win32 GUI | 終端機 CLI |
| 4096-point FFT / Hann / 75% overlap | 相同 |
| ffmpeg.exe MP3 conversion | `ffmpeg` MP3 conversion |
| yt-dlp.exe + ffmpeg.exe | `yt-dlp` + `ffmpeg` |
| in-memory FIFO queue | in-memory FIFO queue |

目前主流 Linux 桌面通常使用 PipeWire；PipeWire 的 PulseAudio 相容伺服器可讓一般 PulseAudio client/tool（例如 `pactl`）繼續運作。因此此參考版用 `pactl` 找預設 sink/monitor，再由 FFmpeg 的 PulseAudio input 擷取該 monitor source。

## 依賴

- C++17 compiler (`g++` or compatible)
- `ffmpeg`，且該 build 需要 PulseAudio input (`-f pulse`) 支援
- `pactl`
- `yt-dlp`（只有 URL 下載功能需要）

Ubuntu/Debian 類系統常見套件名稱：

```bash
sudo apt install build-essential ffmpeg pulseaudio-utils
```

`yt-dlp` 請依發行版或官方建議方式安裝。

## 建置

```bash
make
./SystemAudioRecorder --help
```

## 指令

```text
start
stop
spectrum
record output.mp3 6
stop-record
add-audio https://...
add-video https://...
queue
status
quit
```

## 限制

- 本版是 CLI 參考移植，不是 Windows Win32 GUI 的逐像素移植。
- 系統音訊擷取依賴 PulseAudio 相容層與 monitor source；非桌面／特殊 PipeWire 設定可能需要調整。
- URL 下載功能僅應用於法律與來源平台／服務條款允許的內容與下載方式。
- 下載佇列只存在記憶體，程式結束後不保存。
