/*
 * SystemAudioRecorder - URL downloader integration
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */

#include "downloader.h"
static BOOL file_exists(LPCWSTR path)
{
    DWORD attributes;
    if (path == NULL || path[0] == L'\0')
        return FALSE;
    attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return FALSE;
    return ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
};
static BOOL directory_exists(LPCWSTR path)
{
    DWORD attributes;
    if (path == NULL || path[0] == L'\0')
        return FALSE;
    attributes = GetFileAttributesW(path);
    if (attributes == INVALID_FILE_ATTRIBUTES)
        return FALSE;
    return ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
};
static void set_message(DOWNLOADER_STATE *state, LPCWSTR text);
static BOOL ensure_output_directory(DOWNLOADER_STATE *state)
{
    WCHAR directory[MAX_PATH];
    DWORD error;
    if (state == NULL || !state->lockInitialized)
        return FALSE;
    EnterCriticalSection(&state->lock);
    StringCchCopyW(directory, ARRAY_COUNT(directory), state->outputDirectory);
    LeaveCriticalSection(&state->lock);
    if (directory_exists(directory))
        return TRUE;
    if (CreateDirectoryW(directory, NULL))
        return TRUE;
    error = GetLastError();
    if (error == ERROR_ALREADY_EXISTS && directory_exists(directory))
        return TRUE;
    set_message(state, L"下載資料夾不存在，而且無法建立。");
    return FALSE;
};
static BOOL get_module_directory(WCHAR *path, DWORD count)
{
    DWORD length;
    DWORD i;
    if (path == NULL || count < 4)
        return FALSE;
    path[0] = L'\0';
    length  = GetModuleFileNameW(NULL, path, count);
    if (length == 0 || length >= count)
        return FALSE;
    i = length;
    while (i > 0)
    {
        --i;
        if (path[i] == L'\\' || path[i] == L'/')
        {
            path[i] = L'\0';
            return TRUE;
        };
    };
    return FALSE;
};
static BOOL get_parent_directory(LPCWSTR filePath, WCHAR *directory, size_t count)
{
    size_t length;
    size_t i;
    if (filePath == NULL || directory == NULL || count == 0)
        return FALSE;
    if (FAILED(StringCchCopyW(directory, count, filePath)))
        return FALSE;
    length = wcslen(directory);
    i      = length;
    while (i > 0)
    {
        --i;
        if (directory[i] == L'\\' || directory[i] == L'/')
        {
            directory[i] = L'\0';
            return TRUE;
        };
    };
    return FALSE;
};
static BOOL find_helper(LPCWSTR exeDirectory, LPCWSTR fileName, WCHAR *path, size_t count)
{
    if (FAILED(StringCchPrintfW(path, count, L"%s\\tools\\%s", exeDirectory, fileName)))
        return FALSE;
    if (file_exists(path))
        return TRUE;
    if (FAILED(StringCchPrintfW(path, count, L"%s\\%s", exeDirectory, fileName)))
        return FALSE;
    if (file_exists(path))
        return TRUE;
    path[0] = L'\0';
    return FALSE;
};
static void set_message_locked(DOWNLOADER_STATE *state, LPCWSTR text)
{
    if (state == NULL || text == NULL)
        return;
    StringCchCopyW(state->message, ARRAY_COUNT(state->message), text);
};
static void set_message(DOWNLOADER_STATE *state, LPCWSTR text)
{
    if (state == NULL || !state->lockInitialized)
        return;
    EnterCriticalSection(&state->lock);
    set_message_locked(state, text);
    LeaveCriticalSection(&state->lock);
};
static BOOL is_diagnostic_error_line(LPCWSTR text)
{
    if (text == NULL)
        return FALSE;
    if (wcsstr(text, L"ERROR:") != NULL)
        return TRUE;
    if (wcsstr(text, L"Error:") != NULL)
        return TRUE;
    if (wcsstr(text, L"Usage:") != NULL)
        return TRUE;
    if (wcsstr(text, L"unknown option") != NULL)
        return TRUE;
    if (wcsstr(text, L"no such option") != NULL)
        return TRUE;
    return FALSE;
};
static int parse_progress_percent(LPCWSTR text)
{
    const WCHAR *percent;
    const WCHAR *p;
    const WCHAR *begin;
    int value;
    if (text == NULL || wcsstr(text, L"[download]") == NULL)
        return -1;
    percent = wcschr(text, L'%');
    if (percent == NULL)
        return -1;
    begin = percent;
    while (begin > text)
    {
        WCHAR ch = begin[-1];
        if ((ch >= L'0' && ch <= L'9') || ch == L'.')
            --begin;
        else
            break;
    };
    if (begin == percent)
        return -1;
    value = 0;
    p     = begin;
    while (p < percent && *p >= L'0' && *p <= L'9')
    {
        value = value * 10 + (int)(*p - L'0');
        ++p;
    };
    if (value < 0)
        value = 0;
    if (value > 100)
        value = 100;
    return value;
};
static void copy_name_segment(LPCWSTR begin, LPCWSTR end, WCHAR *name, size_t count)
{
    LPCWSTR base;
    LPCWSTR p;
    size_t  length;
    if (name == NULL || count == 0)
        return;
    name[0] = L'\0';
    if (begin == NULL || end == NULL || end <= begin)
        return;
    while (begin < end && (*begin == L' ' || *begin == L'\t' || *begin == L'\"'))
        ++begin;
    while (end > begin && (end[-1] == L' ' || end[-1] == L'\t' || end[-1] == L'\r' || end[-1] == L'\n' || end[-1] == L'\"'))
        --end;

    base = begin;
    for (p = begin; p < end; ++p)
    {
        if (*p == L'\\' || *p == L'/')
            base = p + 1;
    };
    length = (size_t)(end - base);
    if (length >= count)
        length = count - 1;
    if (length > 0)
        RtlCopyMemory(name, base, length * sizeof(WCHAR));
    name[length] = L'\0';
};
static BOOL parse_download_display_name(LPCWSTR text, WCHAR *name, size_t count)
{
    LPCWSTR begin;
    LPCWSTR end;
    LPCWSTR suffix;
    if (name == NULL || count == 0)
        return FALSE;
    name[0] = L'\0';
    if (text == NULL)
        return FALSE;
    begin = wcsstr(text, L"[ExtractAudio] Destination: ");
    if (begin != NULL)
    {
        begin += wcslen(L"[ExtractAudio] Destination: ");
        end    = begin + wcslen(begin);
        copy_name_segment(begin, end, name, count);
        return name[0] != L'\0';
    };
    begin = wcsstr(text, L"[Merger] Merging formats into ");
    if (begin != NULL)
    {
        begin += wcslen(L"[Merger] Merging formats into ");
        while (*begin == L' ' || *begin == L'\"')
            ++begin;
        end = begin + wcslen(begin);
        if (end > begin && end[-1] == L'\"')
            --end;
        copy_name_segment(begin, end, name, count);
        return name[0] != L'\0';
    };
    begin = wcsstr(text, L"[download] Destination: ");
    if (begin != NULL)
    {
        begin += wcslen(L"[download] Destination: ");
        end    = begin + wcslen(begin);
        copy_name_segment(begin, end, name, count);
        return name[0] != L'\0';
    };
    begin  = wcsstr(text, L"[download] ");
    suffix = wcsstr(text, L" has already been downloaded");
    if (begin != NULL && suffix != NULL && suffix > begin)
    {
        begin += wcslen(L"[download] ");
        copy_name_segment(begin, suffix, name, count);
        return name[0] != L'\0';
    };
    return FALSE;
};
static void set_output_message(DOWNLOADER_STATE *state, LPCWSTR text)
{
    BOOL  diagnostic;
    BOOL  haveName;
    int   progress;
    WCHAR parsedName[DOWNLOADER_NAME_MAX];

    if (state == NULL || !state->lockInitialized || text == NULL)
        return;
    diagnostic = is_diagnostic_error_line(text);
    progress   = parse_progress_percent(text);
    haveName   = parse_download_display_name(text, parsedName, ARRAY_COUNT(parsedName));
    EnterCriticalSection(&state->lock);
    if (progress >= 0)
        state->progressPercent = progress;
    if (haveName)
        StringCchCopyW(state->displayName, ARRAY_COUNT(state->displayName), parsedName);
    if (diagnostic)
    {
        state->haveDiagnosticError = TRUE;
        set_message_locked(state, text);
    }
    else if (!state->haveDiagnosticError)
    {
        set_message_locked(state, text);
    };
    LeaveCriticalSection(&state->lock);
};
static BOOL validate_url(LPCWSTR url)
{
    size_t i;
    if (url == NULL)
        return FALSE;
    if (wcsncmp(url, L"https://", 8) != 0 && wcsncmp(url, L"http://", 7) != 0)
        return FALSE;
    for (i = 0; url[i] != L'\0'; ++i)
    {
        if (url[i] == L'\"' || url[i] == L'\r' || url[i] == L'\n')
            return FALSE;
        if (i + 1 >= DOWNLOADER_URL_MAX)
            return FALSE;
    };
    return (i > 8);
};
static void update_from_output(DOWNLOADER_STATE *state, const char *buffer, DWORD length)
{
    char  line[1024] = {0};
    WCHAR wide[1024] = {0};
    DWORD start;
    DWORD i;
    DWORD copyLength;
    int   converted;

    if (state == NULL || buffer == NULL || length == 0)
        return;
    start = 0;
    for (i = 0; i < length; ++i)
    {
        if (buffer[i] == '\r' || buffer[i] == '\n')
        {
            if (i > start)
            {
                copyLength = i - start;
                if (copyLength >= (DWORD)ARRAY_COUNT(line))
                    copyLength = (DWORD)ARRAY_COUNT(line) - 1;
                RtlCopyMemory(line, buffer + start, copyLength);
                line[copyLength] = '\0';

                converted = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, line, (int)copyLength, wide, (int)ARRAY_COUNT(wide) - 1);
                if (converted <= 0)
                {
                    converted = MultiByteToWideChar(CP_ACP, 0, line, (int)copyLength, wide, (int)ARRAY_COUNT(wide) - 1);
                };
                if (converted > 0)
                {
                    wide[converted] = L'\0';
                    set_output_message(state, wide);
                };
            };
            start = i + 1;
        };
    };
    if (start < length)
    {
        copyLength = length - start;
        if (copyLength >= (DWORD)ARRAY_COUNT(line))
            copyLength = (DWORD)ARRAY_COUNT(line) - 1;
        RtlCopyMemory(line, buffer + start, copyLength);
        line[copyLength] = '\0';
        converted = MultiByteToWideChar(CP_UTF8, 0, line, (int)copyLength, wide, (int)ARRAY_COUNT(wide) - 1);
        if (converted > 0)
        {
            wide[converted] = L'\0';
            set_output_message(state, wide);
        };
    };
};
static DWORD WINAPI downloader_thread(LPVOID parameter)
{
    DOWNLOADER_STATE   *state;
    SECURITY_ATTRIBUTES security;
    STARTUPINFOW        st;
    PROCESS_INFORMATION psInfo;
    HANDLE readPipe;
    HANDLE writePipe;
    WCHAR command[8192];
    WCHAR mpegdir[MAX_PATH];
    WCHAR url[DOWNLOADER_URL_MAX];
    WCHAR ytDlp[MAX_PATH];
    WCHAR mpegpath[MAX_PATH];
    WCHAR outdir[MAX_PATH];
    int   mode;
    BOOL  created;
    DWORD readBytes;
    char  output[1024];
    DWORD exitCode;
    HRESULT hr;
    state = (DOWNLOADER_STATE *)parameter;
    if (state == NULL)
        return 1;
    readPipe  = NULL;
    writePipe = NULL;
    exitCode  = ERROR_GEN_FAILURE;
    RtlZeroMemory(&psInfo, sizeof(psInfo));
    EnterCriticalSection(&state->lock);
    StringCchCopyW(url, ARRAY_COUNT(url), state->url);
    StringCchCopyW(ytDlp, ARRAY_COUNT(ytDlp), state->ytDlpPath);
    StringCchCopyW(mpegpath, ARRAY_COUNT(mpegpath), state->ffmpegPath);
    StringCchCopyW(outdir, ARRAY_COUNT(outdir), state->outputDirectory);
    mode = state->mode;
    LeaveCriticalSection(&state->lock);
    if (!get_parent_directory(mpegpath, mpegdir, ARRAY_COUNT(mpegdir)))
    {
        set_message(state, L"無法判斷 FFmpeg 所在資料夾。");
        exitCode = ERROR_PATH_NOT_FOUND;
        goto Finished;
    };
    if (mode == DOWNLOAD_MODE_AUDIO)
    {
        hr = StringCchPrintfW(command, ARRAY_COUNT(command), L"\"%s\" --no-playlist --newline --ffmpeg-location \"%s\" "
                                                             L"-x --audio-format mp3 --audio-quality 0 \"%s\"", ytDlp, mpegdir, url);
    }
    else
    {
        hr = StringCchPrintfW(command, ARRAY_COUNT(command), L"\"%s\" --no-playlist --newline --ffmpeg-location \"%s\" "
                                                             L"-f \"bestvideo+bestaudio/best\" --merge-output-format mp4 \"%s\"", ytDlp, mpegdir, url);
    };
    if (FAILED(hr))
    {
        set_message(state, L"下載命令過長。");
        exitCode = ERROR_BUFFER_OVERFLOW;
        goto Finished;
    };
    RtlZeroMemory(&security, sizeof(security));
    security.nLength        = sizeof(security);
    security.bInheritHandle = TRUE;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0))
    {
        exitCode = GetLastError();
        set_message(state, L"無法建立下載器輸出管線。");
        goto Finished;
    };
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
    RtlZeroMemory(&st, sizeof(st));
    st.cb         = sizeof(st);
    st.dwFlags    = STARTF_USESTDHANDLES;
    st.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    st.hStdOutput = writePipe;
    st.hStdError  = writePipe;
    set_message(state, mode == DOWNLOAD_MODE_AUDIO ? L"開始下載音訊..." : L"開始下載影片...");
    created = CreateProcessW(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, outdir, &st, &psInfo);
    CloseHandle(writePipe);
    writePipe = NULL;
    if (!created)
    {
        WCHAR errorText[256];
        exitCode = GetLastError();
        StringCchPrintfW(errorText, ARRAY_COUNT(errorText), L"無法啟動 yt-dlp.exe（Windows 錯誤 %lu）。", exitCode);
        set_message(state, errorText);
        goto Finished;
    };
    EnterCriticalSection(&state->lock);
    state->process = psInfo.hProcess;
    LeaveCriticalSection(&state->lock);
    CloseHandle(psInfo.hThread);
    psInfo.hThread = NULL;
    for (;;)
    {
        if (!ReadFile(readPipe, output, (DWORD)sizeof(output), &readBytes, NULL) || readBytes == 0)
            break;
        update_from_output(state, output, readBytes);
    };
    CloseHandle(readPipe);
    readPipe = NULL;
    WaitForSingleObject(psInfo.hProcess, INFINITE);
    exitCode = ERROR_GEN_FAILURE;
    GetExitCodeProcess(psInfo.hProcess, &exitCode);
    CloseHandle(psInfo.hProcess);
    psInfo.hProcess = NULL;
    EnterCriticalSection(&state->lock);
    state->process  = NULL;
    state->exitCode = exitCode;
    state->success  = (exitCode == 0);
    if (exitCode == 0)
    {
        state->progressPercent = 100;
        StringCchPrintfW(state->message, ARRAY_COUNT(state->message), L"下載完成。儲存位置：%s", state->outputDirectory);
    }
    else
    {
        WCHAR prior[DOWNLOADER_TEXT_MAX];
        StringCchCopyW(prior, ARRAY_COUNT(prior), state->message);
        StringCchPrintfW(state->message, ARRAY_COUNT(state->message), L"下載失敗（yt-dlp 結束碼 %lu）。%s", exitCode, prior);
    };
    state->busy = FALSE;
    LeaveCriticalSection(&state->lock);
    return 0;
Finished:
    if (writePipe != NULL)
        CloseHandle(writePipe);
    if (readPipe != NULL)
        CloseHandle(readPipe);
    if (psInfo.hThread != NULL)
        CloseHandle(psInfo.hThread);
    if (psInfo.hProcess != NULL)
        CloseHandle(psInfo.hProcess);
    EnterCriticalSection(&state->lock);
    state->process  = NULL;
    state->success  = FALSE;
    state->exitCode = exitCode;
    state->busy     = FALSE;
    LeaveCriticalSection(&state->lock);
    return exitCode;
};
BOOL DownloaderRefreshTools(DOWNLOADER_STATE *state)
{
    WCHAR ytDlp[MAX_PATH]  = {0};
    WCHAR ffmpeg[MAX_PATH] = {0};
    BOOL  ytdlp;
    BOOL  mpeg;
    if (state == NULL || !state->lockInitialized)
        return FALSE;
    ytdlp = find_helper(state->exeDirectory, L"yt-dlp.exe", ytDlp, ARRAY_COUNT(ytDlp));
    mpeg  = find_helper(state->exeDirectory, L"ffmpeg.exe", ffmpeg, ARRAY_COUNT(ffmpeg));
    EnterCriticalSection(&state->lock);
    if (ytdlp)
        StringCchCopyW(state->ytDlpPath, ARRAY_COUNT(state->ytDlpPath), ytDlp);
    else
        state->ytDlpPath[0] = L'\0';
    if (mpeg)
        StringCchCopyW(state->ffmpegPath, ARRAY_COUNT(state->ffmpegPath), ffmpeg);
    else
        state->ffmpegPath[0] = L'\0';
    LeaveCriticalSection(&state->lock);
    return (ytdlp && mpeg);
};
BOOL DownloaderSetOutputDirectory(DOWNLOADER_STATE *state, LPCWSTR directory)
{
    if (state == NULL || !state->lockInitialized || directory == NULL || directory[0] == L'\0')
        return FALSE;
    if (!directory_exists(directory))
    {
        set_message(state, L"選取的下載資料夾不存在或不是有效資料夾。");
        return FALSE;
    };
    EnterCriticalSection(&state->lock);
    if (state->busy)
    {
        StringCchCopyW(state->message, ARRAY_COUNT(state->message), L"下載進行中不能變更下載資料夾。");
        LeaveCriticalSection(&state->lock);
        return FALSE;
    };
    if (FAILED(StringCchCopyW(state->outputDirectory, ARRAY_COUNT(state->outputDirectory), directory)))
    {
        LeaveCriticalSection(&state->lock);
        return FALSE;
    };
    StringCchCopyW(state->message, ARRAY_COUNT(state->message), L"下載資料夾已變更。");
    state->exitCode = STILL_ACTIVE;
    state->success  = FALSE;
    LeaveCriticalSection(&state->lock);
    return TRUE;
};
BOOL DownloaderInitialize(DOWNLOADER_STATE *state)
{
    if (state == NULL)
        return FALSE;
    RtlZeroMemory(state, sizeof(*state));
    InitializeCriticalSection(&state->lock);
    state->lockInitialized = TRUE;
    state->exitCode        = STILL_ACTIVE;
    if (!get_module_directory(state->exeDirectory, ARRAY_COUNT(state->exeDirectory)))
    {
        set_message(state, L"無法判斷應用程式所在資料夾。");
        return FALSE;
    };
    if (FAILED(StringCchPrintfW(state->outputDirectory, ARRAY_COUNT(state->outputDirectory), L"%s\\downloads", state->exeDirectory)))
    {
        set_message(state, L"下載資料夾路徑過長。");
        return FALSE;
    };
    DownloaderRefreshTools(state);
    set_message(state, L"URL 下載器就緒；yt-dlp.exe 與 ffmpeg.exe 為外部輔助工具。");
    return TRUE;
};
void DownloaderDestroy(DOWNLOADER_STATE *state)
{
    HANDLE process;
    HANDLE thread;
    if (state == NULL || !state->lockInitialized)
        return;
    EnterCriticalSection(&state->lock);
    process = state->process;
    thread = state->thread;
    if (process != NULL)
    {
        TerminateProcess(process, ERROR_CANCELLED);
    };
    LeaveCriticalSection(&state->lock);
    if (thread != NULL)
    {
        /* The worker can be blocked in ReadFile on the redirected output pipe.
         * Cancel that synchronous I/O and wait for the worker before destroying
         * the critical section/state it still references. */
        CancelSynchronousIo(thread);
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
    };
    DeleteCriticalSection(&state->lock);
    RtlZeroMemory(state, sizeof(*state));
};
BOOL DownloaderStart(DOWNLOADER_STATE *state, LPCWSTR url, int mode)
{
    HANDLE oldThread;
    HANDLE thread;
    DWORD  threadId;
    if (state == NULL || !state->lockInitialized || !validate_url(url))
    {
        if (state != NULL && state->lockInitialized)
            set_message(state, L"請輸入有效的 http:// 或 https:// URL，且不要包含引號或換行。");
        return FALSE;
    };
    if (mode != DOWNLOAD_MODE_VIDEO && mode != DOWNLOAD_MODE_AUDIO)
        return FALSE;
    DownloaderRefreshTools(state);
    EnterCriticalSection(&state->lock);
    if (state->busy)
    {
        LeaveCriticalSection(&state->lock);
        set_message(state, L"目前已有下載正在進行。");
        return FALSE;
    };
    if (state->ytDlpPath[0] == L'\0')
    {
        LeaveCriticalSection(&state->lock);
        set_message(state, L"找不到 yt-dlp.exe。請將它放在 SystemAudioRecorder.exe 旁的 tools 資料夾。");
        return FALSE;
    };
    if (state->ffmpegPath[0] == L'\0')
    {
        LeaveCriticalSection(&state->lock);
        set_message(state, L"找不到 ffmpeg.exe。請將它放在 SystemAudioRecorder.exe 旁的 tools 資料夾。");
        return FALSE;
    }

    LeaveCriticalSection(&state->lock);
    if (!ensure_output_directory(state))
        return FALSE;
    EnterCriticalSection(&state->lock);
    if (state->busy)
    {
        LeaveCriticalSection(&state->lock);
        set_message(state, L"目前已有下載正在進行。");
        return FALSE;
    };
    oldThread                  = state->thread;
    state->thread              = NULL;
    state->busy                = TRUE;
    state->success             = FALSE;
    state->haveDiagnosticError = FALSE;
    state->exitCode            = STILL_ACTIVE;
    state->mode                = mode;
    state->progressPercent     = 0;
    state->displayName[0]      = L'\0';
    StringCchCopyW(state->url, ARRAY_COUNT(state->url), url);
    StringCchCopyW(state->message, ARRAY_COUNT(state->message), mode == DOWNLOAD_MODE_AUDIO ? L"正在準備音訊下載..." : L"正在準備影片下載...");
    LeaveCriticalSection(&state->lock);

    if (oldThread != NULL)
        CloseHandle(oldThread);
    thread = CreateThread(NULL, 0, downloader_thread, state, 0, &threadId);
    if (thread == NULL)
    {
        EnterCriticalSection(&state->lock);
        state->busy    = FALSE;
        state->success = FALSE;
        StringCchCopyW(state->message, ARRAY_COUNT(state->message), L"無法建立下載工作執行緒。");
        LeaveCriticalSection(&state->lock);
        return FALSE;
    };
    EnterCriticalSection(&state->lock);
    state->thread = thread;
    LeaveCriticalSection(&state->lock);
    return TRUE;
};
void DownloaderGetStatus(DOWNLOADER_STATE *state, DOWNLOADER_STATUS *status)
{
    if (status == NULL)
        return;
    RtlZeroMemory(status, sizeof(*status));
    if (state == NULL || !state->lockInitialized)
        return;
    EnterCriticalSection(&state->lock);
    status->initialized     = TRUE;
    status->busy            = state->busy;
    status->success         = state->success;
    status->exitCode        = state->exitCode;
    status->mode            = state->mode;
    status->progressPercent = state->progressPercent;
    StringCchCopyW(status->displayName, ARRAY_COUNT(status->displayName), state->displayName);
    StringCchCopyW(status->message, ARRAY_COUNT(status->message), state->message);
    StringCchCopyW(status->outputDirectory, ARRAY_COUNT(status->outputDirectory), state->outputDirectory);
    StringCchCopyW(status->ytDlpPath, ARRAY_COUNT(status->ytDlpPath), state->ytDlpPath);
    StringCchCopyW(status->ffmpegPath, ARRAY_COUNT(status->ffmpegPath), state->ffmpegPath);
    LeaveCriticalSection(&state->lock);
};