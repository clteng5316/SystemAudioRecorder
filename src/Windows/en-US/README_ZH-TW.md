# SystemAudioRecorder 2.0-beta.1

Windows x64 系統音訊頻譜分析、MP3 錄音與選用 URL 媒體下載工具。

**作者：** Teng Chuan-Liang  
**平台：** Windows 7+ x64  
**建置工具：** Windows Driver Kit 7 / 傳統 `build.exe`

> 本壓縮檔只保留原始碼。修改原始碼後，請重新建置執行檔。

## 功能

- 透過 WASAPI loopback 擷取 Windows 預設播放裝置的系統音訊。
- 即時顯示 4096 點 FFT 頻譜，使用 Hann window 與 75% overlap。
- 先將 PCM 寫入暫存 WAV，再透過外部 `ffmpeg.exe` 轉成 MP3。
- 可選用外部 `yt-dlp.exe` + `ffmpeg.exe` 下載 URL 媒體。
- 提供記憶體內 FIFO 下載佇列，可顯示進度、調整順序與取消。
- 可選擇錄音增益與下載資料夾。

## 專案結構

```text
main.cpp        Win32 介面與應用程式生命週期
audio.cpp/.h    WASAPI loopback 擷取
spectrum.cpp/.h FFT 與頻譜狀態
ffmpeg.cpp/.h   WAV 錄音與 FFmpeg MP3 轉換
downloader.cpp/.h yt-dlp / FFmpeg 程序整合
queue.cpp/.h    下載佇列介面
wasapi.h        WDK7 建置使用的最小 WASAPI 宣告
public.h        共用 Windows 宣告
version.rc      版本資訊
sources         WDK7 建置目標
```

## 選用外部工具

原始碼壓縮檔不包含第三方執行檔。請將相容版本放在：

```text
SystemAudioRecorder\
├─ SystemAudioRecorder.exe
└─ tools\
   ├─ ffmpeg.exe
   └─ yt-dlp.exe
```

| 功能 | 需要的外部工具 |
|---|---|
| WASAPI 擷取 / 頻譜 | 無 |
| MP3 錄音 | `ffmpeg.exe` |
| URL 音訊 / 影片 | `yt-dlp.exe` + `ffmpeg.exe` |

## 建置

開啟 Windows 7 x64 Free Build Environment，執行：

```bat
build -cZg
```

`sources` 內的目標名稱為 `SystemAudioRecorder`。

## URL 下載提醒

僅在適用法律以及來源平台／服務條款允許下載方式時使用此功能。本程式不實作 DRM 規避、付費內容解鎖、瀏覽器 Cookie 匯入、帳號／工作階段擷取、登入／付費牆繞過或存取控制繞過。

詳見 `CONTENT_DOWNLOAD_NOTICE.md` 與 `THIRD_PARTY_NOTICES.md`。

## 已知限制

- 系統音訊擷取後端僅支援 Windows（WASAPI loopback）。
- URL 媒體相容性取決於使用者另外安裝的 yt-dlp／FFmpeg 版本及來源服務。
- 下載佇列只存在記憶體，程式結束後不保留。
- 傳統 WAV 暫存錄音約有 4 GB 上限。
- 本次已做靜態檢查，但目前環境是 Linux，無法在此直接執行 WDK7 Windows 建置。

## 授權狀態

目前是預發佈開發版本，著作權仍屬 Teng Chuan-Liang。`LICENSE.txt` 目前沒有授予公開再散布權；若要將原始碼或執行檔正式公開為開源專案，請先選定公開授權條款。
