/*
 * SystemAudioRecorder
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */
#include "audio.h"
#include "downloader.h"
#include "queue.h"
#define IDC_START       1001
#define IDC_STOP        1002
#define IDC_REC         1003
#define IDC_STOPREC     1004
#define IDC_ABOUT       1005
#define IDC_STATUS      1006
#define IDC_DOWNLOADQ   1007
#define IDC_GAIN        1012
#define UI_TIMER        1

static AUDIO_ENGINE     g_engine;
static DOWNLOADER_STATE g_downloader;
static HWND             g_start     = NULL;
static HWND             g_stop      = NULL;
static HWND             g_rec       = NULL;
static HWND             g_stopRec   = NULL;
static HWND             g_download  = NULL;
static HWND             g_about     = NULL;
static HWND             g_status    = NULL;
static HWND             g_gainLabel = NULL;
static HWND             g_gainCombo = NULL;
static HBITMAP          g_backBitmap    = NULL;
static HBITMAP          g_backOldBitmap = NULL;
static HFONT            g_font          = NULL;
static HFONT            g_bigFont       = NULL;
static HDC              g_backDc        = NULL;
static int  g_backWidth  = 0;
static int  g_backHeight = 0;
static UINT g_uiTick     = 0;
static void destroy_back_buffer(void)
{
    if (g_backDc != NULL)
    {
        if (g_backOldBitmap != NULL)
        {
            SelectObject(g_backDc, g_backOldBitmap);
            g_backOldBitmap = NULL;
        };
        if (g_backBitmap != NULL)
        {
            DeleteObject(g_backBitmap);
            g_backBitmap = NULL;
        };
        DeleteDC(g_backDc);
        g_backDc = NULL;
    };
    g_backWidth = 0;
    g_backHeight = 0;
};
static BOOL ensure_back_buffer(HDC referenceDc, int width, int height)
{
    HBITMAP newBitmap;
    HBITMAP oldBitmap;
    HDC     newDc;
    if (referenceDc == NULL || width <= 0 || height <= 0)
    {
        return FALSE;
    };
    if (g_backDc != NULL && g_backBitmap != NULL && g_backWidth == width && g_backHeight == height)
    {
        return TRUE;
    };
    destroy_back_buffer();
    newDc = CreateCompatibleDC(referenceDc);
    if (newDc == NULL)
    {
        return FALSE;
    };
    newBitmap = CreateCompatibleBitmap(referenceDc, width, height);
    if (newBitmap == NULL)
    {
        DeleteDC(newDc);
        return FALSE;
    };
    oldBitmap = (HBITMAP)SelectObject(newDc, newBitmap);
    if (oldBitmap == NULL)
    {
        DeleteObject(newBitmap);
        DeleteDC(newDc);
        return FALSE;
    };
    g_backDc        = newDc;
    g_backBitmap    = newBitmap;
    g_backOldBitmap = oldBitmap;
    g_backWidth     = width;
    g_backHeight    = height;
    return TRUE;
};
static RECT spectrum_invalidate_rect(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    rc.top = 72;
    if (rc.top > rc.bottom)
    {
        rc.top = rc.bottom;
    };
    return rc;
};
static float db_to_norm(float db)
{
    const float floorDb = -72.0f;
    if (db < floorDb)
        return 0.0f;
    if (db > 0.0f)
        return 1.0f;
    return (db - floorDb) / -floorDb;
};
static void format_frequency(float hz, WCHAR *text, size_t count)
{
    if (hz >= 1000.0f)
    {
        float khz = hz / 1000.0f;
        if (khz >= 10.0f)
            StringCchPrintfW(text, count, L"%.0fk", khz);
        else
            StringCchPrintfW(text, count, L"%.1fk", khz);
    }
    else
    {
        StringCchPrintfW(text, count, L"%.0f", hz);
    };
};
static void draw_text_center(HDC dc, const RECT *rc, LPCWSTR text, HFONT font, COLORREF color)
{
    HFONT old = NULL;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    if (font != NULL)
        old = (HFONT)SelectObject(dc, font);
    DrawTextW(dc, text, -1, (RECT *)rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (old != NULL)
        SelectObject(dc, old);
};
static void draw_spectrum(HDC dc, const RECT *client)
{
    SPECTRUM_SNAPSHOT   snap;
    AUDIO_ENGINE_STATUS status;
    RECT   panel = *client;
    RECT   top;
    RECT   bars;
    HBRUSH black;
    HPEN   gridPen;
    HPEN   oldPen;
    const float *centers   = SpectrumGetBandCenters();
    WCHAR        text[256] = {0};
    int          leftPad   = 28;
    int          rightPad  = 68;
    int          bottomPad = 52;
    int          gap       = 6;
    int          barStartX;
    int          groupWidth;
    int          availableWidth;
    int          barWidth;
    int          i;
    panel.left   += 16;
    panel.right  -= 16;
    panel.top    += 72;
    panel.bottom -= 16;
    black = CreateSolidBrush(RGB(10, 18, 22));
    FillRect(dc, &panel, black);
    DeleteObject(black);
    AudioEngineGetSpectrum(&g_engine, &snap);
    AudioEngineGetStatus(&g_engine, &status);
    top        = panel;
    top.left  += 16;
    top.right -= 16;
    top.top   += 8;
    top.bottom = top.top + 56;
    {
        RECT left  = top;
        RECT right = top;
        left.right = left.left + (top.right - top.left) / 2;
        right.left = left.right;
        StringCchPrintfW(text, ARRAY_COUNT(text), L"主峰 %.0f Hz", snap.dominantHz);
        draw_text_center(dc, &left, text, g_bigFont, RGB(75, 220, 245));
        StringCchPrintfW(text, ARRAY_COUNT(text), status.recording ? L"MP3 錄音中 | 192 kbps" : L"MP3 就緒");
        draw_text_center(dc, &right, text, g_bigFont, status.recording ? RGB(255, 90, 80) : RGB(180, 180, 180));
    };
    bars         = panel;
    bars.left   += leftPad;
    bars.right  -= rightPad;
    bars.top    += 78;
    bars.bottom -= bottomPad;
    gridPen = CreatePen(PS_DOT, 1, RGB(70, 82, 86));
    oldPen  = (HPEN)SelectObject(dc, gridPen);
    for (i = 0; i <= 6; ++i)
    {
        int y = bars.top + (bars.bottom - bars.top) * i / 6;
        MoveToEx(dc, bars.left, y, NULL);
        LineTo(dc, bars.right, y);
        StringCchPrintfW(text, ARRAY_COUNT(text), L"%d", -12 * i);
        {
            RECT tr = {bars.right + 8, y - 10, panel.right - 5, y + 10};
            draw_text_center(dc, &tr, text, g_font, RGB(190, 205, 210));
        };
    };
    SelectObject(dc, oldPen);
    DeleteObject(gridPen);
    availableWidth = bars.right - bars.left;
    barWidth       = (availableWidth - gap * (SPECTRUM_BAND_COUNT - 1)) / SPECTRUM_BAND_COUNT;
    if (barWidth > 14)
        barWidth = 14;
    if (barWidth < 2)
        barWidth = 2;
    groupWidth = barWidth * SPECTRUM_BAND_COUNT + gap * (SPECTRUM_BAND_COUNT - 1);
    barStartX  = bars.left;
    if (groupWidth < availableWidth)
        barStartX += (availableWidth - groupWidth) / 2;
    for (i = 0; i < SPECTRUM_BAND_COUNT; ++i)
    {
        HBRUSH   brush;
        COLORREF color;
        float    norm     = db_to_norm(snap.bandDb[i]);
        float    peakNorm = db_to_norm(snap.peakDb[i]);
        int      x        = barStartX + i * (barWidth + gap);
        int      h        = (int)((bars.bottom - bars.top) * norm);
        int      peakY    = bars.bottom - (int)((bars.bottom - bars.top) * peakNorm);
        RECT     br       = {x, bars.bottom - h, x + barWidth, bars.bottom};
        if (norm >= 0.88f)
            color = RGB(242, 82, 68);
        else if (norm >= 0.70f)
            color = RGB(245, 214, 62);
        else
            color = RGB(105, 230, 78);
        brush = CreateSolidBrush(color);
        FillRect(dc, &br, brush);
        DeleteObject(brush);
        {
            HPEN peakPen = CreatePen(PS_SOLID, 2, RGB(235, 245, 245));
            HPEN old = (HPEN)SelectObject(dc, peakPen);
            MoveToEx(dc, x, peakY, NULL);
            LineTo(dc, x + barWidth, peakY);
            SelectObject(dc, old);
            DeleteObject(peakPen);
        };
        if (i == 1 || i == 5 || i == 9 || i == 13 || i == 17 || i == 21 || i == 25)
        {
            RECT lr = {x - 12, bars.bottom + 8, x + barWidth + 12, bars.bottom + 34};
            format_frequency(centers[i], text, ARRAY_COUNT(text));
            draw_text_center(dc, &lr, text, g_font, RGB(205, 225, 230));
        };
    };
    {
        RECT footer = panel;
        footer.left  += 16;
        footer.top    = panel.bottom - 30;
        footer.bottom = panel.bottom - 4;
        StringCchPrintfW(text, ARRAY_COUNT(text), L"RMS %.1f dBFS    %lu Hz / %u 聲道 / %u bit%s    FFT 4096 + Hann + 75%% overlap",
                         snap.rmsDb, status.sampleRate, status.channels, status.bitsPerSample, status.isFloat ? L" 浮點" : L"");
        draw_text_center(dc, &footer, text, g_font, RGB(170, 195, 205));
    };
};
static LONG selected_record_gain_mode(void)
{
    LRESULT selection;
    LRESULT itemData;
    if (g_gainCombo == NULL)
        return MP3_GAIN_MODE_6DB;
    selection = SendMessageW(g_gainCombo, CB_GETCURSEL, 0, 0);
    if (selection == CB_ERR)
        return MP3_GAIN_MODE_6DB;
    itemData = SendMessageW(g_gainCombo, CB_GETITEMDATA, (WPARAM)selection, 0);
    if (itemData == CB_ERR)
        return MP3_GAIN_MODE_6DB;
    return (LONG)itemData;
};
static LPCWSTR record_gain_label(LONG gainMode)
{
    switch (gainMode)
    {
        case MP3_GAIN_MODE_0DB:
            return L"0 dB";
        case MP3_GAIN_MODE_3DB:
            return L"+3 dB";
        case MP3_GAIN_MODE_6DB:
            return L"+6 dB";
        case MP3_GAIN_MODE_9DB:
            return L"+9 dB";
        case MP3_GAIN_MODE_12DB:
            return L"+12 dB";
        case MP3_GAIN_MODE_AUTO_NORMALIZE:
            return L"自動正規化";
        default:
            return L"+6 dB";
    };
};
static void update_controls(void)
{
    AUDIO_ENGINE_STATUS audioStatus;
    WCHAR               audioText[900] = {0};
    WCHAR               text[1100]     = {0};
    int                 waiting;
    int                 active;
    int                 completed;
    int                 failed;
    AudioEngineGetStatus(&g_engine, &audioStatus);
    DownloadQueueGetCounts(&waiting, &active, &completed, &failed);
    UNREFERENCED_PARAMETER(completed);
    UNREFERENCED_PARAMETER(failed);
    EnableWindow(g_start, !audioStatus.running);
    EnableWindow(g_stop, audioStatus.running);
    EnableWindow(g_rec, audioStatus.running && !audioStatus.recording && !audioStatus.converting);
    EnableWindow(g_stopRec, audioStatus.recording);
    if (g_gainCombo != NULL)
    {
        BOOL enabled = (!audioStatus.recording && !audioStatus.converting);
        if (IsWindowEnabled(g_gainCombo) != enabled)
            EnableWindow(g_gainCombo, enabled);
    };
    if (audioStatus.converting)
        StringCchPrintfW(audioText, ARRAY_COUNT(audioText), L"MP3 轉換 | %s", audioStatus.recorderMessage);
    else if (audioStatus.recording)
    {
        StringCchPrintfW(audioText, ARRAY_COUNT(audioText), L"MP3 錄音：%s | 增益：%s",
                                                            audioStatus.recordPath, record_gain_label(selected_record_gain_mode()));
    }
    else if (audioStatus.running)
    {
        StringCchPrintfW(audioText, ARRAY_COUNT(audioText), L"正在擷取系統音訊 | %lu Hz，%u 聲道 | %s",
                                                            audioStatus.sampleRate, audioStatus.channels, audioStatus.lastError);
    }
    else
    {
        StringCchPrintfW(audioText, ARRAY_COUNT(audioText), L"已停止 | %s", audioStatus.lastError);
    };
    if (active > 0 || waiting > 0)
        StringCchPrintfW(text, ARRAY_COUNT(text), L"下載佇列：%d 執行中 / %d 等待 | %s", active, waiting, audioText);
    else
        StringCchCopyW(text, ARRAY_COUNT(text), audioText);
    SetWindowTextW(g_status, text);
};
static BOOL choose_mp3_path(HWND hwnd, WCHAR *path, DWORD count)
{
    OPENFILENAMEW ofn;
    if (path == NULL || count == 0)
        return FALSE;
    RtlZeroMemory(&ofn, sizeof(ofn));
    StringCchCopyW(path, count, L"system-audio.mp3");
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFilter = L"MP3 音訊 (*.mp3)\0*.mp3\0所有檔案 (*.*)\0*.*\0\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = count;
    ofn.lpstrDefExt = L"mp3";
    ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    return GetSaveFileNameW(&ofn);
};
static void layout(HWND hwnd)
{
    RECT rc;
    int  x;
    int  y;
    int  h;
    GetClientRect(hwnd, &rc);
    x = 10;
    y = 9;
    h = 28;
    MoveWindow(g_start, x, y, 88, h, TRUE); x += 94;
    MoveWindow(g_stop, x, y, 62, h, TRUE); x += 68;
    MoveWindow(g_rec, x, y, 94, h, TRUE); x += 100;
    MoveWindow(g_stopRec, x, y, 84, h, TRUE); x += 90;
    MoveWindow(g_gainLabel, x, y + 4, 70, 22, TRUE); x += 74;
    MoveWindow(g_gainCombo, x, y, 94, 150, TRUE); x += 102;
    MoveWindow(g_download, x, y, 88, h, TRUE);
    MoveWindow(g_about, rc.right - 68, y, 58, h, TRUE);
    MoveWindow(g_status, 10, 43, rc.right - 20, 22, TRUE);
};
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CREATE:
            g_start     = CreateWindowW(L"BUTTON", L"開始擷取", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)IDC_START, GetModuleHandleW(NULL), NULL);
            g_stop      = CreateWindowW(L"BUTTON", L"停止", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)IDC_STOP, GetModuleHandleW(NULL), NULL);
            g_rec       = CreateWindowW(L"BUTTON", L"錄製 MP3", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)IDC_REC, GetModuleHandleW(NULL), NULL);
            g_stopRec   = CreateWindowW(L"BUTTON", L"停止 MP3", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)IDC_STOPREC, GetModuleHandleW(NULL), NULL);
            g_gainLabel = CreateWindowW(L"STATIC", L"錄音增益：", WS_CHILD | WS_VISIBLE | SS_LEFT, 0,0,0,0, hwnd, NULL, GetModuleHandleW(NULL), NULL);
            g_gainCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                                        0,0,0,0, hwnd, (HMENU)IDC_GAIN, GetModuleHandleW(NULL), NULL);
            g_download  = CreateWindowW(L"BUTTON", L"下載佇列...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)IDC_DOWNLOADQ, GetModuleHandleW(NULL), NULL);
            g_about     = CreateWindowW(L"BUTTON", L"關於", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0,0,0,0, hwnd, (HMENU)IDC_ABOUT, GetModuleHandleW(NULL), NULL);
            g_status    = CreateWindowW(L"STATIC", L"就緒", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS, 0,0,0,0, hwnd, (HMENU)IDC_STATUS, GetModuleHandleW(NULL), NULL);
            if (g_gainCombo != NULL)
            {
                LRESULT item;
                item = SendMessageW(g_gainCombo, CB_ADDSTRING, 0, (LPARAM)L"0 dB");
                SendMessageW(g_gainCombo, CB_SETITEMDATA, (WPARAM)item, (LPARAM)MP3_GAIN_MODE_0DB);
                item = SendMessageW(g_gainCombo, CB_ADDSTRING, 0, (LPARAM)L"+3 dB");
                SendMessageW(g_gainCombo, CB_SETITEMDATA, (WPARAM)item, (LPARAM)MP3_GAIN_MODE_3DB);
                item = SendMessageW(g_gainCombo, CB_ADDSTRING, 0, (LPARAM)L"+6 dB（建議）");
                SendMessageW(g_gainCombo, CB_SETITEMDATA, (WPARAM)item, (LPARAM)MP3_GAIN_MODE_6DB);
                item = SendMessageW(g_gainCombo, CB_ADDSTRING, 0, (LPARAM)L"+9 dB");
                SendMessageW(g_gainCombo, CB_SETITEMDATA, (WPARAM)item, (LPARAM)MP3_GAIN_MODE_9DB);
                item = SendMessageW(g_gainCombo, CB_ADDSTRING, 0, (LPARAM)L"+12 dB");
                SendMessageW(g_gainCombo, CB_SETITEMDATA, (WPARAM)item, (LPARAM)MP3_GAIN_MODE_12DB);
                item = SendMessageW(g_gainCombo, CB_ADDSTRING, 0, (LPARAM)L"自動正規化");
                SendMessageW(g_gainCombo, CB_SETITEMDATA, (WPARAM)item, (LPARAM)MP3_GAIN_MODE_AUTO_NORMALIZE);
                SendMessageW(g_gainCombo, CB_SETCURSEL, 2, 0);
            };
            if (g_font != NULL)
            {
                SendMessageW(g_start, WM_SETFONT, (WPARAM)g_font, TRUE);
                SendMessageW(g_stop, WM_SETFONT, (WPARAM)g_font, TRUE);
                SendMessageW(g_rec, WM_SETFONT, (WPARAM)g_font, TRUE);
                SendMessageW(g_stopRec, WM_SETFONT, (WPARAM)g_font, TRUE);
                SendMessageW(g_gainLabel, WM_SETFONT, (WPARAM)g_font, TRUE);
                SendMessageW(g_gainCombo, WM_SETFONT, (WPARAM)g_font, TRUE);
                SendMessageW(g_download, WM_SETFONT, (WPARAM)g_font, TRUE);
                SendMessageW(g_about, WM_SETFONT, (WPARAM)g_font, TRUE);
                SendMessageW(g_status, WM_SETFONT, (WPARAM)g_font, TRUE);
            };
            layout(hwnd);
            SetTimer(hwnd, UI_TIMER, 33, NULL);
            update_controls();
            return 0;
        case WM_GETMINMAXINFO:
        {
            MINMAXINFO *mmi = (MINMAXINFO *)lParam;
            mmi->ptMinTrackSize.x = 700;
            mmi->ptMinTrackSize.y = 430;
        };
        return 0;
        case WM_SIZE:
            layout(hwnd);
            destroy_back_buffer();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        case WM_TIMER:
            if (wParam == UI_TIMER)
            {
                RECT spectrumRect = spectrum_invalidate_rect(hwnd);
                InvalidateRect(hwnd, &spectrumRect, FALSE);
                ++g_uiTick;
                if ((g_uiTick & 7U) == 0U)
                {
                    DownloadQueueTick(hwnd);
                    update_controls();
                };
            };
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_START:
                    if (!AudioEngineStart(&g_engine))
                    {
                        AUDIO_ENGINE_STATUS st;
                        AudioEngineGetStatus(&g_engine, &st);
                        MessageBoxW(hwnd, st.lastError, L"音訊擷取錯誤", MB_OK | MB_ICONERROR);
                    };
                    update_controls();
                    return 0;
                case IDC_STOP:
                {
                    RECT spectrumRect;
                    AudioEngineStop(&g_engine);
                    update_controls();
                    spectrumRect = spectrum_invalidate_rect(hwnd);
                    InvalidateRect(hwnd, &spectrumRect, FALSE);
                    UpdateWindow(hwnd);
                };
                return 0;
                case IDC_REC:
                {
                    WCHAR path[MAX_PATH] = {0};
                    if (choose_mp3_path(hwnd, path, (DWORD)ARRAY_COUNT(path)))
                    {
                        if (!AudioEngineStartMp3(&g_engine, path, 192, selected_record_gain_mode()))
                        {
                            AUDIO_ENGINE_STATUS st;
                            AudioEngineGetStatus(&g_engine, &st);
                            MessageBoxW(hwnd, st.lastError, L"MP3 轉換", MB_OK | MB_ICONERROR);
                        };
                        update_controls();
                    };
                };
                return 0;
                case IDC_STOPREC:
                    AudioEngineStopMp3(&g_engine);
                    update_controls();
                    return 0;
                case IDC_DOWNLOADQ:
                    DownloadQueueShow(hwnd);
                    return 0;
                case IDC_ABOUT:
                    MessageBoxW(hwnd, L"SystemAudioRecorder v2.0-beta.1\r\n"
                                      L"Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.\r\n\r\n"
                                      L"系統音訊：擷取 Windows 輸出音訊（WASAPI loopback）。\r\n"
                                      L"頻譜：4096 點 FFT、Hann window、75% overlap。\r\n"
                                      L"MP3：透過獨立／靜態 ffmpeg.exe 提供增益與正規化選項。\r\n"
                                      L"下載：使用 yt-dlp + ffmpeg 的單工 FIFO 佇列。\r\n"
                                      L"佇列項目只保留在記憶體中，程式結束後即清除。",
                                      L"關於 SystemAudioRecorder", MB_OK | MB_ICONINFORMATION);
                    return 0;
            };
            break;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            RECT        rc;
            HDC         dc = BeginPaint(hwnd, &ps);
            HBRUSH      bg;
            int         paintWidth;
            int         paintHeight;
            GetClientRect(hwnd, &rc);
            if (ensure_back_buffer(dc, rc.right - rc.left, rc.bottom - rc.top))
            {
                bg = CreateSolidBrush(RGB(225, 241, 252));
                if (bg != NULL)
                {
                    FillRect(g_backDc, &rc, bg);
                    DeleteObject(bg);
                };
                draw_spectrum(g_backDc, &rc);
                paintWidth  = ps.rcPaint.right - ps.rcPaint.left;
                paintHeight = ps.rcPaint.bottom - ps.rcPaint.top;
                if (paintWidth > 0 && paintHeight > 0)
                {
                    BitBlt(dc, ps.rcPaint.left, ps.rcPaint.top, paintWidth, paintHeight, g_backDc, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
                };
            }
            else
            {
                bg = CreateSolidBrush(RGB(225, 241, 252));
                if (bg != NULL)
                {
                    FillRect(dc, &rc, bg);
                    DeleteObject(bg);
                };
                draw_spectrum(dc, &rc);
            };
            EndPaint(hwnd, &ps);
        };
        return 0;
        case WM_DESTROY:
            KillTimer(hwnd, UI_TIMER);
            DownloadQueueDestroy();
            destroy_back_buffer();
            DownloaderDestroy(&g_downloader);
            AudioEngineDestroy(&g_engine);
            PostQuitMessage(0);
            return 0;
    };
    return DefWindowProcW(hwnd, msg, wParam, lParam);
};
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nShowCmd)
{
    WNDCLASSEXW       wc;
    HWND              hwnd;
    MSG               msg;
    NONCLIENTMETRICSW ncm;
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    ZeroMemory(&ncm, sizeof(ncm));
    ncm.cbSize = sizeof(ncm);
    if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
        g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    g_bigFont = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft JhengHei UI");
    if (!AudioEngineInitialize(&g_engine))
    {
        MessageBoxW(NULL, L"音訊引擎初始化失敗。",  L"SystemAudioRecorder", MB_OK | MB_ICONERROR);
        return 1;
    };
    if (!DownloaderInitialize(&g_downloader))
    {
        MessageBoxW(NULL, L"下載器初始化失敗。", L"SystemAudioRecorder", MB_OK | MB_ICONERROR);
        DownloaderDestroy(&g_downloader);
        AudioEngineDestroy(&g_engine);
        return 1;
    };
    if (!DownloadQueueInitialize(hInstance, &g_downloader))
    {
        DownloaderDestroy(&g_downloader);
        AudioEngineDestroy(&g_engine);
        return 1;
    };
    RtlZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = wndproc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"SystemAudioRecorderV20Beta1ZHTW";
    if (!RegisterClassExW(&wc))
        return 1;
    hwnd = CreateWindowExW(0, wc.lpszClassName, L"SystemAudioRecorder v2.0-beta.1 - 系統音訊頻譜",
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 780, 500, NULL, NULL, hInstance, NULL);
    if (hwnd == NULL)
        return 1;
    ShowWindow(hwnd, nShowCmd);
    UpdateWindow(hwnd);
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    };
    if (g_bigFont != NULL)
        DeleteObject(g_bigFont);
    if (g_font != NULL)
        DeleteObject(g_font);
    return (int)msg.wParam;
};