/*
 * SystemAudioRecorder - FFmpeg MP3 integration
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */
#include "ffmpeg.h"
#define WAV_HEADER_BYTES   44UL
#define WAV_MAX_DATA_BYTES 0xFFFFFFD0UL
static BOOL file_exists(LPCWSTR path)
{
    DWORD attr;
    if (path == NULL || path[0] == L'\0')
        return FALSE;
    attr = GetFileAttributesW(path);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return FALSE;
    return ((attr & FILE_ATTRIBUTE_DIRECTORY) == 0);
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
static BOOL find_ffmpeg(WCHAR *path, DWORD count)
{
    WCHAR directory[MAX_PATH] = {0};
    WCHAR candidate[MAX_PATH] = {0};
    if (path == NULL || count == 0)
        return FALSE;
    path[0] = L'\0';
    if (!get_module_directory(directory, ARRAY_COUNT(directory)))
        return FALSE;
    if (SUCCEEDED(StringCchPrintfW(candidate, ARRAY_COUNT(candidate), L"%s\\tools\\ffmpeg.exe", directory)) && file_exists(candidate))
    {
        return SUCCEEDED(StringCchCopyW(path, count, candidate));
    };
    if (SUCCEEDED(StringCchPrintfW(candidate, ARRAY_COUNT(candidate), L"%s\\ffmpeg.exe", directory)) && file_exists(candidate))
    {
        return SUCCEEDED(StringCchCopyW(path, count, candidate));
    };
    return FALSE;
};
static void set_message_locked(FFMPEG_MP3_WRITER *writer, LPCWSTR message)
{
    if (writer == NULL)
        return;
    StringCchCopyW(writer->message, ARRAY_COUNT(writer->message), message != NULL ? message : L"Unknown recorder error");
};
static BOOL write_all(HANDLE file, const BYTE *data, DWORD bytes)
{
    DWORD offset;
    DWORD written;

    if (file == INVALID_HANDLE_VALUE || data == NULL)
        return FALSE;
    offset = 0;
    while (offset < bytes)
    {
        written = 0;
        if (!WriteFile(file, data + offset, bytes - offset, &written, NULL))
            return FALSE;
        if (written == 0)
            return FALSE;
        offset += written;
    };
    return TRUE;
};
static void put_u16(BYTE *p, WORD value)
{
    p[0] = (BYTE)(value & 0xFF);
    p[1] = (BYTE)((value >> 8) & 0xFF);
};
static void put_u32(BYTE *p, DWORD value)
{
    p[0] = (BYTE)(value & 0xFF);
    p[1] = (BYTE)((value >> 8) & 0xFF);
    p[2] = (BYTE)((value >> 16) & 0xFF);
    p[3] = (BYTE)((value >> 24) & 0xFF);
};
static void build_wave_header(BYTE *header, LONG sampleRate, DWORD dataBytes)
{
    DWORD rate;
    DWORD size;
    RtlZeroMemory(header, WAV_HEADER_BYTES);
    CopyMemory(header + 0, "RIFF", 4);
    size = 36UL + dataBytes;
    put_u32(header + 4, size);
    CopyMemory(header + 8, "WAVE", 4);
    CopyMemory(header + 12, "fmt ", 4);
    put_u32(header + 16, 16);
    put_u16(header + 20, 1);
    put_u16(header + 22, 2);
    put_u32(header + 24, (DWORD)sampleRate);
    rate = (DWORD)sampleRate * 4UL;
    put_u32(header + 28, rate);
    put_u16(header + 32, 4);
    put_u16(header + 34, 16);
    CopyMemory(header + 36, "data", 4);
    put_u32(header + 40, dataBytes);
};
static BOOL finalize_wave_locked(FFMPEG_MP3_WRITER *writer)
{
    BYTE  header[WAV_HEADER_BYTES] = {0};
    DWORD position;
    if (writer == NULL || writer->waveFile == INVALID_HANDLE_VALUE)
        return FALSE;
    if (writer->dataBytes > WAV_MAX_DATA_BYTES)
    {
        set_message_locked(writer, L"Recording exceeded the classic WAV 4-GB size limit.");
        return FALSE;
    };
    build_wave_header(header, writer->sampleRate, (DWORD)writer->dataBytes);
    SetLastError(NO_ERROR);
    position = SetFilePointer(writer->waveFile, 0, NULL, FILE_BEGIN);
    if (position == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
    {
        set_message_locked(writer, L"Unable to seek to the WAV header.");
        return FALSE;
    };
    if (!write_all(writer->waveFile, header, WAV_HEADER_BYTES))
    {
        set_message_locked(writer, L"Unable to finalize the WAV header.");
        return FALSE;
    };
    FlushFileBuffers(writer->waveFile);
    return TRUE;
};
static LONG normalize_gain_mode(LONG gainMode)
{
    switch (gainMode)
    {
        case MP3_GAIN_MODE_0DB:
        case MP3_GAIN_MODE_3DB:
        case MP3_GAIN_MODE_6DB:
        case MP3_GAIN_MODE_9DB:
        case MP3_GAIN_MODE_12DB:
        case MP3_GAIN_MODE_AUTO_NORMALIZE:
            return gainMode;
        default:
            return MP3_GAIN_MODE_6DB;
    };
};
static LPCWSTR gain_filter_for_mode(LONG gainMode)
{
    switch (gainMode)
    {
        case MP3_GAIN_MODE_3DB:
            return L"volume=3dB,alimiter=limit=0.95";
        case MP3_GAIN_MODE_6DB:
            return L"volume=6dB,alimiter=limit=0.95";
        case MP3_GAIN_MODE_9DB:
            return L"volume=9dB,alimiter=limit=0.95";
        case MP3_GAIN_MODE_12DB:
            return L"volume=12dB,alimiter=limit=0.95";
        case MP3_GAIN_MODE_AUTO_NORMALIZE:
            return L"loudnorm=I=-16:LRA=11:TP=-1.5";
        case MP3_GAIN_MODE_0DB:
        default:
            return NULL;
    };
};
static DWORD WINAPI convert_thread(LPVOID parameter)
{
    STARTUPINFOW        st;
    PROCESS_INFORMATION psInfo;
    FFMPEG_MP3_WRITER  *writer;
    WCHAR               ffmpeg[MAX_PATH];
    WCHAR               input[MAX_PATH];
    WCHAR               output[MAX_PATH];
    LONG                bitrate;
    LONG                sampleRate;
    LONG                gainMode;
    LPCWSTR             filter;
    WCHAR               command[4 * MAX_PATH + 768];
    BOOL                created;
    DWORD               exitCode;
    HRESULT             hr;
    writer = (FFMPEG_MP3_WRITER *)parameter;
    if (writer == NULL)
        return 1;
    EnterCriticalSection(&writer->lock);
    StringCchCopyW(ffmpeg, ARRAY_COUNT(ffmpeg), writer->ffmpegPath);
    StringCchCopyW(input, ARRAY_COUNT(input), writer->temporaryWavePath);
    StringCchCopyW(output, ARRAY_COUNT(output), writer->targetPath);
    bitrate    = writer->bitrateKbps;
    sampleRate = writer->sampleRate;
    gainMode   = writer->gainMode;
    LeaveCriticalSection(&writer->lock);
    filter = gain_filter_for_mode(gainMode);
    if (filter != NULL)
    {
        hr = StringCchPrintfW(command, ARRAY_COUNT(command), L"\"%s\" -hide_banner -loglevel error -nostdin -y "
                                                             L"-i \"%s\" -vn -af \"%s\" -ar %ld -codec:a libmp3lame -b:a %ldk \"%s\"",
                                                                                         ffmpeg, input, filter, sampleRate, bitrate, output);
    }
    else
    {
        hr = StringCchPrintfW(command, ARRAY_COUNT(command), L"\"%s\" -hide_banner -loglevel error -nostdin -y "
                                                             L"-i \"%s\" -vn -ar %ld -codec:a libmp3lame -b:a %ldk \"%s\"", ffmpeg, input, sampleRate, bitrate, output);
    };
    if (FAILED(hr))
    {
        EnterCriticalSection(&writer->lock);
        writer->converting = FALSE;
        writer->success    = FALSE;
        writer->exitCode   = ERROR_BUFFER_OVERFLOW;
        set_message_locked(writer, L"The FFmpeg MP3 conversion command is too long. The temporary WAV was retained.");
        LeaveCriticalSection(&writer->lock);
        return ERROR_BUFFER_OVERFLOW;
    };
    RtlZeroMemory(&st, sizeof(st));
    st.cb = sizeof(st);
    RtlZeroMemory(&psInfo, sizeof(psInfo));
    created = CreateProcessW(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &st, &psInfo);
    if (!created)
    {
        DWORD error = GetLastError();
        EnterCriticalSection(&writer->lock);
        writer->converting = FALSE;
        writer->success    = FALSE;
        writer->exitCode   = error;
        StringCchPrintfW(writer->message, ARRAY_COUNT(writer->message),
                         L"Unable to start ffmpeg.exe (Windows error %lu). Use a standalone/static FFmpeg build. The temporary WAV was retained.", error);
        LeaveCriticalSection(&writer->lock);
        return error;
    };
    CloseHandle(psInfo.hThread);
    WaitForSingleObject(psInfo.hProcess, INFINITE);
    exitCode = 1;
    GetExitCodeProcess(psInfo.hProcess, &exitCode);
    CloseHandle(psInfo.hProcess);
    EnterCriticalSection(&writer->lock);
    writer->exitCode   = exitCode;
    writer->converting = FALSE;
    writer->success    = (exitCode == 0);
    if (exitCode == 0)
    {
        DeleteFileW(writer->temporaryWavePath);
        StringCchPrintfW(writer->message, ARRAY_COUNT(writer->message), L"MP3 conversion complete: %s", writer->targetPath);
    }
    else
    {
        StringCchPrintfW(writer->message, ARRAY_COUNT(writer->message),
                         L"FFmpeg MP3 conversion failed (exit code %lu). Use a standalone/static ffmpeg.exe with MP3 encoding support. Temporary WAV retained: %s",
                         exitCode, writer->temporaryWavePath);
    };
    LeaveCriticalSection(&writer->lock);

    return exitCode;
};
static void reap_conversion_thread_locked(FFMPEG_MP3_WRITER *writer)
{
    DWORD wait;
    if (writer == NULL || writer->convertThread == NULL)
        return;
    wait = WaitForSingleObject(writer->convertThread, 0);
    if (wait == WAIT_OBJECT_0)
    {
        CloseHandle(writer->convertThread);
        writer->convertThread = NULL;
    };
};
BOOL FfmpegMp3Initialize(FFMPEG_MP3_WRITER *writer)
{
    if (writer == NULL)
        return FALSE;
    RtlZeroMemory(writer, sizeof(*writer));
    InitializeCriticalSection(&writer->lock);
    writer->lockInitialized = TRUE;
    writer->waveFile        = INVALID_HANDLE_VALUE;
    writer->exitCode        = STILL_ACTIVE;
    set_message_locked(writer, L"MP3 recorder ready. Requires only tools\\ffmpeg.exe.");
    return TRUE;
};
void FfmpegMp3Destroy(FFMPEG_MP3_WRITER *writer)
{
    HANDLE thread;
    if (writer == NULL || !writer->lockInitialized)
        return;
    FfmpegMp3Stop(writer);
    EnterCriticalSection(&writer->lock);
    thread = writer->convertThread;
    LeaveCriticalSection(&writer->lock);
    if (thread != NULL)
    {
        WaitForSingleObject(thread, INFINITE);
    };
    EnterCriticalSection(&writer->lock);
    reap_conversion_thread_locked(writer);
    if (writer->waveFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(writer->waveFile);
        writer->waveFile = INVALID_HANDLE_VALUE;
    };
    LeaveCriticalSection(&writer->lock);
    DeleteCriticalSection(&writer->lock);
    writer->lockInitialized = FALSE;
};
BOOL FfmpegMp3Start(FFMPEG_MP3_WRITER *writer, LPCWSTR path, LONG sampleRate, LONG bitrateKbps, LONG gainMode)
{
    BYTE    header[WAV_HEADER_BYTES] = {0};
    WCHAR   ffmpeg[MAX_PATH]         = {0};
    HRESULT hr;
    BOOL    ok;

    if (writer == NULL || !writer->lockInitialized || path == NULL || path[0] == L'\0' || sampleRate <= 0)
    {
        return FALSE;
    };
    if (!find_ffmpeg(ffmpeg, ARRAY_COUNT(ffmpeg)))
    {
        EnterCriticalSection(&writer->lock);
        set_message_locked(writer, L"ffmpeg.exe was not found. Put a standalone/static ffmpeg.exe in the tools folder beside SystemAudioRecorder.exe.");
        LeaveCriticalSection(&writer->lock);
        return FALSE;
    };
    EnterCriticalSection(&writer->lock);
    reap_conversion_thread_locked(writer);
    if (writer->recording)
    {
        set_message_locked(writer, L"A recording is already active.");
        LeaveCriticalSection(&writer->lock);
        return FALSE;
    };
    if (writer->converting || writer->convertThread != NULL)
    {
        set_message_locked(writer, L"The previous recording is still being converted to MP3.");
        LeaveCriticalSection(&writer->lock);
        return FALSE;
    };
    hr = StringCchPrintfW(writer->temporaryWavePath, ARRAY_COUNT(writer->temporaryWavePath), L"%s.recording.wav", path);
    if (FAILED(hr))
    {
        set_message_locked(writer, L"The MP3 output path is too long to create a temporary WAV file.");
        LeaveCriticalSection(&writer->lock);
        return FALSE;
    };
    StringCchCopyW(writer->targetPath, ARRAY_COUNT(writer->targetPath), path);
    StringCchCopyW(writer->ffmpegPath, ARRAY_COUNT(writer->ffmpegPath), ffmpeg);
    writer->sampleRate  = sampleRate;
    writer->bitrateKbps = bitrateKbps > 0 ? bitrateKbps : 192;
    writer->gainMode    = normalize_gain_mode(gainMode);
    writer->dataBytes   = 0;
    writer->success     = FALSE;
    writer->exitCode    = STILL_ACTIVE;
    writer->waveFile     = CreateFileW(writer->temporaryWavePath,
                                       GENERIC_READ | GENERIC_WRITE,
                                       FILE_SHARE_READ,
                                       NULL,
                                       CREATE_ALWAYS,
                                       FILE_ATTRIBUTE_NORMAL,
                                       NULL);
    if (writer->waveFile == INVALID_HANDLE_VALUE)
    {
        set_message_locked(writer, L"Unable to create the temporary WAV file for MP3 recording.");
        LeaveCriticalSection(&writer->lock);
        return FALSE;
    };
    build_wave_header(header, sampleRate, 0);
    ok = write_all(writer->waveFile, header, WAV_HEADER_BYTES);
    if (!ok)
    {
        CloseHandle(writer->waveFile);
        writer->waveFile = INVALID_HANDLE_VALUE;
        DeleteFileW(writer->temporaryWavePath);
        set_message_locked(writer, L"Unable to write the temporary WAV header.");
        LeaveCriticalSection(&writer->lock);
        return FALSE;
    };
    writer->recording = TRUE;
    set_message_locked(writer, L"Recording PCM to temporary WAV. Stop Record will convert it to MP3 with ffmpeg.exe.");
    LeaveCriticalSection(&writer->lock);
    return TRUE;
};
BOOL FfmpegMp3WriteStereo16(FFMPEG_MP3_WRITER *writer, const short *samples, UINT frames)
{
    SIZE_T byteCountSize;
    DWORD  byteCount;
    BOOL   ok;
    if (writer == NULL || !writer->lockInitialized || samples == NULL || frames == 0)
        return FALSE;
    byteCountSize = (SIZE_T)frames * 2U * sizeof(short);
    if (byteCountSize > 0xFFFFFFFFUL)
        return FALSE;
    byteCount = (DWORD)byteCountSize;
    EnterCriticalSection(&writer->lock);
    if (!writer->recording || writer->waveFile == INVALID_HANDLE_VALUE)
    {
        LeaveCriticalSection(&writer->lock);
        return FALSE;
    };
    if (writer->dataBytes + (ULONGLONG)byteCount > (ULONGLONG)WAV_MAX_DATA_BYTES)
    {
        set_message_locked(writer, L"Recording reached the classic WAV 4-GB limit.");
        ok = FALSE;
    }
    else
    {
        ok = write_all(writer->waveFile, (const BYTE *)samples, byteCount);
        if (ok)
        {
            writer->dataBytes += (ULONGLONG)byteCount;
        }
        else
        {
            set_message_locked(writer, L"Writing the temporary WAV recording failed.");
        };
    };
    LeaveCriticalSection(&writer->lock);
    return ok;
};
BOOL FfmpegMp3Stop(FFMPEG_MP3_WRITER *writer)
{
    BOOL finalized;
    HANDLE thread;
    if (writer == NULL || !writer->lockInitialized)
        return FALSE;
    EnterCriticalSection(&writer->lock);
    reap_conversion_thread_locked(writer);
    if (!writer->recording)
    {
        BOOL alreadyConverting = writer->converting;
        LeaveCriticalSection(&writer->lock);
        return alreadyConverting;
    };
    finalized = finalize_wave_locked(writer);

    if (writer->waveFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(writer->waveFile);
        writer->waveFile = INVALID_HANDLE_VALUE;
    };
    writer->recording = FALSE;
    if (!finalized || writer->dataBytes == 0)
    {
        if (writer->dataBytes == 0)
        {
            set_message_locked(writer, L"No audio samples were recorded; MP3 conversion was not started.");
            DeleteFileW(writer->temporaryWavePath);
        };
        writer->success = FALSE;
        writer->converting = FALSE;
        LeaveCriticalSection(&writer->lock);
        return FALSE;
    };
    writer->converting = TRUE;
    writer->success    = FALSE;
    writer->exitCode   = STILL_ACTIVE;
    set_message_locked(writer, L"Recording stopped. Converting the temporary WAV to MP3 with ffmpeg.exe...");
    thread = CreateThread(NULL, 0, convert_thread, writer, 0, NULL);
    if (thread == NULL)
    {
        writer->converting = FALSE;
        writer->exitCode   = GetLastError();
        StringCchPrintfW(writer->message, ARRAY_COUNT(writer->message),
                                          L"Unable to create the FFmpeg conversion thread (Windows error %lu). Temporary WAV retained.", writer->exitCode);
        LeaveCriticalSection(&writer->lock);
        return FALSE;
    };
    writer->convertThread = thread;
    LeaveCriticalSection(&writer->lock);
    return TRUE;
};
void FfmpegMp3GetStatus(FFMPEG_MP3_WRITER *writer, FFMPEG_MP3_STATUS *status)
{
    if (status == NULL)
        return;
    RtlZeroMemory(status, sizeof(*status));
    if (writer == NULL || !writer->lockInitialized)
        return;
    EnterCriticalSection(&writer->lock);
    reap_conversion_thread_locked(writer);
    status->initialized = TRUE;
    status->recording   = writer->recording;
    status->converting  = writer->converting;
    status->success     = writer->success;
    status->exitCode    = writer->exitCode;
    StringCchCopyW(status->targetPath, ARRAY_COUNT(status->targetPath), writer->targetPath);
    StringCchCopyW(status->temporaryWavePath, ARRAY_COUNT(status->temporaryWavePath), writer->temporaryWavePath);
    StringCchCopyW(status->ffmpegPath, ARRAY_COUNT(status->ffmpegPath), writer->ffmpegPath);
    StringCchCopyW(status->message, ARRAY_COUNT(status->message), writer->message);
    LeaveCriticalSection(&writer->lock);
};