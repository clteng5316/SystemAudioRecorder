/*
 * SystemAudioRecorder - download queue
 * Copyright (C) 2026 Teng Chuan-Liang. All rights reserved.
 */
#include "queue.h"
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>

#define DQ_MAX_ITEMS 128
#define DQ_ID_URL     3101
#define DQ_ID_TYPE    3102
#define DQ_ID_ADD     3103
#define DQ_ID_FOLDER  3104
#define DQ_ID_BROWSE  3105
#define DQ_ID_LIST    3106
#define DQ_MENU_CUT   3201
#define DQ_MENU_COPY  3202
#define DQ_MENU_PASTE 3203
#define DQ_MENU_ALL   3204
#define DQ_MENU_CANCEL 3210
#define DQ_MENU_MOVE_UP 3211
#define DQ_MENU_MOVE_DOWN 3212
#define DQ_BALLOON_TIMER 3301
#define DQ_TRAY_ID 1

#define DQ_STATE_WAITING  1
#define DQ_STATE_ACTIVE   2
#define DQ_STATE_DONE     3
#define DQ_STATE_FAILED   4
#define DQ_STATE_CANCELED 5

typedef struct _DQ_ITEM
{
    WCHAR url[DOWNLOADER_URL_MAX];
    WCHAR name[DOWNLOADER_NAME_MAX];
    int mode;
    int state;
    int progress;
} DQ_ITEM;

static HINSTANCE g_instance = NULL;
static DOWNLOADER_STATE *g_downloader = NULL;
static HWND g_owner = NULL;
static HWND g_window = NULL;
static HWND g_urlLabel = NULL;
static HWND g_url = NULL;
static HWND g_type = NULL;
static HWND g_typeLabel = NULL;
static HWND g_add = NULL;
static HWND g_folderLabel = NULL;
static HWND g_folder = NULL;
static HWND g_browse = NULL;
static HWND g_list = NULL;
static WNDPROC g_urlOldProc = NULL;
static HFONT g_queueFont = NULL;
static DQ_ITEM g_items[DQ_MAX_ITEMS];
static int g_count = 0;
static int g_active = -1;
static BOOL g_noticeAccepted = FALSE;
static BOOL g_batchOpen = FALSE;
static int g_batchStart = 0;
static BOOL g_trayAdded = FALSE;

static BOOL valid_url(LPCWSTR url)
{
    size_t i;
    if (url == NULL) return FALSE;
    if (wcsncmp(url, L"https://", 8) != 0 &&
        wcsncmp(url, L"http://", 7) != 0)
        return FALSE;
    for (i = 0; url[i] != L'\0'; ++i)
    {
        if (url[i] == L'"' || url[i] == L'\r' || url[i] == L'\n')
            return FALSE;
        if (i + 1 >= DOWNLOADER_URL_MAX)
            return FALSE;
    }
    return (i > 8);
}

static BOOL confirm_notice(HWND owner)
{
    int answer;
    if (g_noticeAccepted) return TRUE;
    answer = MessageBoxW(owner,
                         L"URL downloads use external yt-dlp and FFmpeg. Use this feature only when the download method is permitted by law and by the source platform/service terms. No DRM, paid-content, cookie, login, or access-control bypass is provided. Continue?",
                         L"URL Download Notice",
                         MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (answer == IDYES)
    {
        g_noticeAccepted = TRUE;
        return TRUE;
    }
    return FALSE;
}

static BOOL clipboard_has_text(void)
{
    return IsClipboardFormatAvailable(CF_UNICODETEXT);
}

static BOOL paste_clipboard(HWND edit)
{
    HANDLE data;
    LPCWSTR src;
    WCHAR *copy;
    SIZE_T len;
    SIZE_T begin;
    SIZE_T end;
    SIZE_T i;
    SIZE_T out;

    if (edit == NULL || !clipboard_has_text()) return FALSE;
    if (!OpenClipboard(edit)) return FALSE;
    data = GetClipboardData(CF_UNICODETEXT);
    if (data == NULL)
    {
        CloseClipboard();
        return FALSE;
    }
    src = (LPCWSTR)GlobalLock(data);
    if (src == NULL)
    {
        CloseClipboard();
        return FALSE;
    }
    len = (SIZE_T)lstrlenW(src);
    copy = (WCHAR *)HeapAlloc(GetProcessHeap(), 0, (len + 1U) * sizeof(WCHAR));
    if (copy == NULL)
    {
        GlobalUnlock(data);
        CloseClipboard();
        return FALSE;
    }
    CopyMemory(copy, src, (len + 1U) * sizeof(WCHAR));
    GlobalUnlock(data);
    CloseClipboard();

    begin = 0;
    while (begin < len &&
           (copy[begin] == L' ' || copy[begin] == L'\t' ||
            copy[begin] == L'\r' || copy[begin] == L'\n'))
        ++begin;
    end = len;
    while (end > begin &&
           (copy[end - 1] == L' ' || copy[end - 1] == L'\t' ||
            copy[end - 1] == L'\r' || copy[end - 1] == L'\n'))
        --end;

    out = 0;
    for (i = begin; i < end; ++i)
        copy[out++] = copy[i];
    copy[out] = L'\0';

    if (copy[0] != L'\0')
    {
        SetFocus(edit);
        SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)copy);
    }
    HeapFree(GetProcessHeap(), 0, copy);
    return TRUE;
}

static void copy_selection(HWND edit, BOOL cut)
{
    DWORD start;
    DWORD end;
    int length;
    WCHAR *all;
    HGLOBAL memory;
    WCHAR *dest;
    SIZE_T chars;

    start = 0;
    end = 0;
    SendMessageW(edit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    if (end <= start) return;
    length = GetWindowTextLengthW(edit);
    if (length <= 0 || start >= (DWORD)length) return;
    if (end > (DWORD)length) end = (DWORD)length;

    all = (WCHAR *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                             ((SIZE_T)length + 1U) * sizeof(WCHAR));
    if (all == NULL) return;
    GetWindowTextW(edit, all, length + 1);
    chars = (SIZE_T)(end - start);
    memory = GlobalAlloc(GMEM_MOVEABLE, (chars + 1U) * sizeof(WCHAR));
    if (memory != NULL)
    {
        dest = (WCHAR *)GlobalLock(memory);
        if (dest != NULL)
        {
            CopyMemory(dest, all + start, chars * sizeof(WCHAR));
            dest[chars] = L'\0';
            GlobalUnlock(memory);
            if (OpenClipboard(edit))
            {
                EmptyClipboard();
                if (SetClipboardData(CF_UNICODETEXT, memory) != NULL)
                    memory = NULL;
                CloseClipboard();
            }
        }
        if (memory != NULL) GlobalFree(memory);
    }
    HeapFree(GetProcessHeap(), 0, all);
    if (cut) SendMessageW(edit, EM_REPLACESEL, TRUE, (LPARAM)L"");
}

static void show_url_menu(HWND edit, LPARAM lParam)
{
    POINT pt;
    HMENU menu;
    DWORD start;
    DWORD end;
    UINT selectedFlags;
    int command;

    pt.x = (LONG)(SHORT)LOWORD((DWORD_PTR)lParam);
    pt.y = (LONG)(SHORT)HIWORD((DWORD_PTR)lParam);
    if (pt.x == -1 && pt.y == -1)
    {
        RECT rc;
        GetWindowRect(edit, &rc);
        pt.x = rc.left + 12;
        pt.y = rc.top + 12;
    }
    start = 0;
    end = 0;
    SendMessageW(edit, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);

    menu = CreatePopupMenu();
    if (menu == NULL) return;
    selectedFlags = (end > start) ? MF_STRING : (MF_STRING | MF_GRAYED);
    AppendMenuW(menu, selectedFlags, DQ_MENU_CUT, L"Cut");
    AppendMenuW(menu, selectedFlags, DQ_MENU_COPY, L"Copy");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, clipboard_has_text() ? MF_STRING : (MF_STRING | MF_GRAYED),
                DQ_MENU_PASTE, L"Paste");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, DQ_MENU_ALL, L"Select All");
    command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, edit, NULL);
    DestroyMenu(menu);
    switch (command)
    {
    case DQ_MENU_CUT: copy_selection(edit, TRUE); break;
    case DQ_MENU_COPY: copy_selection(edit, FALSE); break;
    case DQ_MENU_PASTE: paste_clipboard(edit); break;
    case DQ_MENU_ALL: SendMessageW(edit, EM_SETSEL, 0, -1); break;
    }
}

static LRESULT CALLBACK url_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_KEYDOWN)
    {
        BOOL ctrl = ((GetKeyState(VK_CONTROL) & 0x8000) != 0);
        BOOL shift = ((GetKeyState(VK_SHIFT) & 0x8000) != 0);
        if (ctrl && (wParam == L'V' || wParam == L'v'))
        {
            paste_clipboard(hwnd);
            return 0;
        }
        if (shift && wParam == VK_INSERT)
        {
            paste_clipboard(hwnd);
            return 0;
        }
        if (ctrl && (wParam == L'C' || wParam == L'c'))
        {
            copy_selection(hwnd, FALSE);
            return 0;
        }
        if (ctrl && (wParam == L'X' || wParam == L'x'))
        {
            copy_selection(hwnd, TRUE);
            return 0;
        }
        if (ctrl && (wParam == L'A' || wParam == L'a'))
        {
            SendMessageW(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
    }
    else if (message == WM_CONTEXTMENU)
    {
        show_url_menu(hwnd, lParam);
        return 0;
    }
    if (g_urlOldProc != NULL)
        return CallWindowProcW(g_urlOldProc, hwnd, message, wParam, lParam);
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

static void list_set_text(int row, int column, LPCWSTR text)
{
    if (g_list != NULL)
        ListView_SetItemText(g_list, row, column, (LPWSTR)text);
}

static void refresh_list(void)
{
    int i;
    WCHAR number[16];
    WCHAR progress[64];
    LPCWSTR typeText;
    LPCWSTR doneText;
    LPCWSTR nameText;

    if (g_list == NULL) return;
    ListView_DeleteAllItems(g_list);

    for (i = 0; i < g_count; ++i)
    {
        LVITEMW item;
        ZeroMemory(&item, sizeof(item));
        item.mask = LVIF_TEXT;
        item.iItem = i;
        StringCchPrintfW(number, ARRAY_COUNT(number), L"%d", i + 1);
        item.pszText = number;
        ListView_InsertItem(g_list, &item);

        if (g_items[i].name[0] != L'\0') nameText = g_items[i].name;
        else if (g_items[i].state == DQ_STATE_ACTIVE) nameText = L"Resolving name...";
        else if (g_items[i].state == DQ_STATE_WAITING) nameText = L"Waiting for name";
        else nameText = L"-";
        list_set_text(i, 1, nameText);
        list_set_text(i, 2, g_items[i].url);

        typeText = (g_items[i].mode == DOWNLOAD_MODE_AUDIO) ? L"Audio" : L"Video";
        list_set_text(i, 3, typeText);

        if (g_items[i].state == DQ_STATE_WAITING)
            StringCchCopyW(progress, ARRAY_COUNT(progress), L"Waiting");
        else if (g_items[i].state == DQ_STATE_ACTIVE)
            StringCchPrintfW(progress, ARRAY_COUNT(progress), L"%d%%", g_items[i].progress);
        else if (g_items[i].state == DQ_STATE_DONE)
            StringCchCopyW(progress, ARRAY_COUNT(progress), L"100%");
        else if (g_items[i].state == DQ_STATE_CANCELED)
            StringCchCopyW(progress, ARRAY_COUNT(progress), L"Canceled");
        else
            StringCchCopyW(progress, ARRAY_COUNT(progress), L"Failed");
        list_set_text(i, 4, progress);

        if (g_items[i].state == DQ_STATE_DONE) doneText = L"Yes";
        else if (g_items[i].state == DQ_STATE_FAILED) doneText = L"Failed";
        else if (g_items[i].state == DQ_STATE_CANCELED) doneText = L"Canceled";
        else doneText = L"No";
        list_set_text(i, 5, doneText);
    }
}

static int first_waiting(void)
{
    int i;
    for (i = 0; i < g_count; ++i)
        if (g_items[i].state == DQ_STATE_WAITING)
            return i;
    return -1;
}

static int previous_waiting(int index)
{
    int i;
    for (i = index - 1; i >= 0; --i)
        if (g_items[i].state == DQ_STATE_WAITING) return i;
    return -1;
}

static int next_waiting(int index)
{
    int i;
    for (i = index + 1; i < g_count; ++i)
        if (g_items[i].state == DQ_STATE_WAITING) return i;
    return -1;
}

static void select_row(int row)
{
    if (g_list == NULL || row < 0 || row >= g_count) return;
    ListView_SetItemState(g_list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(g_list, row, LVIS_SELECTED | LVIS_FOCUSED,
                          LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(g_list, row, FALSE);
}

static void swap_waiting_items(int a, int b)
{
    DQ_ITEM temp;
    if (a < 0 || b < 0 || a >= g_count || b >= g_count) return;
    if (g_items[a].state != DQ_STATE_WAITING ||
        g_items[b].state != DQ_STATE_WAITING) return;
    temp = g_items[a];
    g_items[a] = g_items[b];
    g_items[b] = temp;
    refresh_list();
    select_row(b);
}

static int context_list_row(LPARAM lParam, POINT *screenPoint)
{
    int row = -1;
    if (screenPoint == NULL || g_list == NULL) return -1;
    screenPoint->x = (LONG)(SHORT)LOWORD((DWORD_PTR)lParam);
    screenPoint->y = (LONG)(SHORT)HIWORD((DWORD_PTR)lParam);
    if (screenPoint->x == -1 && screenPoint->y == -1)
    {
        RECT itemRect;
        row = ListView_GetNextItem(g_list, -1, LVNI_SELECTED);
        if (row >= 0 && ListView_GetItemRect(g_list, row, &itemRect, LVIR_BOUNDS))
        {
            screenPoint->x = itemRect.left + 24;
            screenPoint->y = itemRect.top + 12;
            ClientToScreen(g_list, screenPoint);
        }
    }
    else
    {
        POINT client = *screenPoint;
        LVHITTESTINFO hit;
        ScreenToClient(g_list, &client);
        ZeroMemory(&hit, sizeof(hit));
        hit.pt = client;
        row = ListView_HitTest(g_list, &hit);
    }
    if (row >= 0 && row < g_count) select_row(row);
    return row;
}

static void show_list_context_menu(HWND owner, LPARAM lParam)
{
    POINT pt;
    HMENU menu;
    int row;
    int prev;
    int next;
    int command;
    UNREFERENCED_PARAMETER(owner);

    row = context_list_row(lParam, &pt);
    if (row < 0 || row >= g_count) return;
    if (g_items[row].state != DQ_STATE_WAITING) return;

    prev = previous_waiting(row);
    next = next_waiting(row);
    menu = CreatePopupMenu();
    if (menu == NULL) return;
    AppendMenuW(menu, MF_STRING, DQ_MENU_CANCEL, L"Cancel Download");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, prev >= 0 ? MF_STRING : (MF_STRING | MF_GRAYED),
                DQ_MENU_MOVE_UP, L"Move Up");
    AppendMenuW(menu, next >= 0 ? MF_STRING : (MF_STRING | MF_GRAYED),
                DQ_MENU_MOVE_DOWN, L"Move Down");
    command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                             pt.x, pt.y, 0, g_list, NULL);
    DestroyMenu(menu);
    if (command == DQ_MENU_CANCEL)
    {
        g_items[row].state = DQ_STATE_CANCELED;
        g_items[row].progress = 0;
        refresh_list();
        select_row(row);
    }
    else if (command == DQ_MENU_MOVE_UP && prev >= 0)
        swap_waiting_items(row, prev);
    else if (command == DQ_MENU_MOVE_DOWN && next >= 0)
        swap_waiting_items(row, next);
}

static void delete_tray_icon(void)
{
    NOTIFYICONDATAW nid;
    if (!g_trayAdded || g_window == NULL) return;
    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_window;
    nid.uID = DQ_TRAY_ID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_trayAdded = FALSE;
}

static void show_batch_balloon(int videos, int audioCount,
                               int canceledCount, int failedCount)
{
    NOTIFYICONDATAW nid;
    WCHAR info[256];
    if (g_window == NULL) return;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_window;
    nid.uID = DQ_TRAY_ID;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    if (!g_trayAdded)
    {
        nid.uFlags = NIF_ICON | NIF_TIP;
        StringCchCopyW(nid.szTip, ARRAY_COUNT(nid.szTip), L"SystemAudioRecorder");
        if (Shell_NotifyIconW(NIM_ADD, &nid)) g_trayAdded = TRUE;
    }
    if (!g_trayAdded) return;

    ZeroMemory(&nid, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = g_window;
    nid.uID = DQ_TRAY_ID;
    nid.uFlags = NIF_INFO;
    StringCchCopyW(nid.szInfoTitle, ARRAY_COUNT(nid.szInfoTitle), L"SystemAudioRecorder Downloads Complete");
    if (canceledCount > 0 || failedCount > 0)
        StringCchPrintfW(info, ARRAY_COUNT(info), L"Queue complete: %d video(s), %d audio item(s); %d canceled, %d failed.",
                         videos, audioCount, canceledCount, failedCount);
    else
        StringCchPrintfW(info, ARRAY_COUNT(info), L"Queue complete: %d video(s), %d audio item(s).", videos, audioCount);
    StringCchCopyW(nid.szInfo, ARRAY_COUNT(nid.szInfo), info);
    nid.dwInfoFlags = NIIF_INFO;
    nid.uTimeout = 8000;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
    KillTimer(g_window, DQ_BALLOON_TIMER);
    SetTimer(g_window, DQ_BALLOON_TIMER, 12000, NULL);
}

static void maybe_notify_batch_complete(void)
{
    int i;
    int videos = 0;
    int audioCount = 0;
    int canceledCount = 0;
    int failedCount = 0;
    if (!g_batchOpen || g_active >= 0 || first_waiting() >= 0) return;
    for (i = g_batchStart; i < g_count; ++i)
    {
        if (g_items[i].state == DQ_STATE_DONE)
        {
            if (g_items[i].mode == DOWNLOAD_MODE_AUDIO) ++audioCount;
            else ++videos;
        }
        else if (g_items[i].state == DQ_STATE_CANCELED) ++canceledCount;
        else if (g_items[i].state == DQ_STATE_FAILED) ++failedCount;
    }
    show_batch_balloon(videos, audioCount, canceledCount, failedCount);
    g_batchOpen = FALSE;
}

static void start_next(HWND notifyOwner)
{
    DOWNLOADER_STATUS status;
    int index;

    if (g_active >= 0 || g_downloader == NULL) return;

    for (;;)
    {
        index = first_waiting();
        if (index < 0) return;

        g_items[index].state = DQ_STATE_ACTIVE;
        g_items[index].progress = 0;
        g_items[index].name[0] = L'\0';
        g_active = index;
        if (DownloaderStart(g_downloader, g_items[index].url, g_items[index].mode))
        {
            refresh_list();
            return;
        }

        DownloaderGetStatus(g_downloader, &status);
        g_items[index].state = DQ_STATE_FAILED;
        g_active = -1;
        refresh_list();
        MessageBoxW(notifyOwner, status.message, L"Unable to Start Download", MB_OK | MB_ICONERROR);
    }
}

static int CALLBACK browse_callback(HWND dialog, UINT message, LPARAM lParam, LPARAM data)
{
    UNREFERENCED_PARAMETER(lParam);
    if (message == BFFM_INITIALIZED && data != 0)
        SendMessageW(dialog, BFFM_SETSELECTION, TRUE, data);
    return 0;
}

static void browse_folder(HWND owner)
{
    DOWNLOADER_STATUS status;
    BROWSEINFOW bi;
    LPITEMIDLIST item;
    WCHAR selected[MAX_PATH];

    if (g_active >= 0)
    {
        MessageBoxW(owner, L"A download is running. Change the folder after the current item finishes.", L"Save to:", MB_OK | MB_ICONINFORMATION);
        return;
    }

    DownloaderGetStatus(g_downloader, &status);
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = owner;
    bi.lpszTitle = L"Choose the folder for downloaded video and audio";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    bi.lpfn = browse_callback;
    bi.lParam = (LPARAM)status.outputDirectory;
    item = SHBrowseForFolderW(&bi);
    if (item == NULL) return;
    selected[0] = L'\0';
    if (SHGetPathFromIDListW(item, selected))
    {
        if (DownloaderSetOutputDirectory(g_downloader, selected))
            SetWindowTextW(g_folder, selected);
        else
        {
            DownloaderGetStatus(g_downloader, &status);
            MessageBoxW(owner, status.message, L"Save to:", MB_OK | MB_ICONERROR);
        }
    }
    CoTaskMemFree(item);
}

static void add_item(HWND owner)
{
    WCHAR url[DOWNLOADER_URL_MAX];
    int length;
    LRESULT selection;

    if (!confirm_notice(owner)) return;
    if (g_count >= DQ_MAX_ITEMS)
    {
        MessageBoxW(owner, L"The download queue is full (maximum 128 items).", L"Download Queue", MB_OK | MB_ICONINFORMATION);
        return;
    }

    url[0] = L'\0';
    length = GetWindowTextW(g_url, url, (int)ARRAY_COUNT(url));
    if (length <= 0 || !valid_url(url))
    {
        MessageBoxW(owner, L"Enter a valid http:// or https:// URL.", L"Download Queue", MB_OK | MB_ICONINFORMATION);
        return;
    }

    if (!g_batchOpen)
    {
        g_batchOpen = TRUE;
        g_batchStart = g_count;
    }

    selection = SendMessageW(g_type, CB_GETCURSEL, 0, 0);
    ZeroMemory(&g_items[g_count], sizeof(g_items[g_count]));
    g_items[g_count].mode = (selection == 1) ? DOWNLOAD_MODE_AUDIO : DOWNLOAD_MODE_VIDEO;
    g_items[g_count].state = DQ_STATE_WAITING;
    g_items[g_count].progress = 0;
    StringCchCopyW(g_items[g_count].url, ARRAY_COUNT(g_items[g_count].url), url);
    ++g_count;
    SetWindowTextW(g_url, L"");
    SetFocus(g_url);
    refresh_list();
    start_next(owner);
}

static void layout(HWND hwnd)
{
    RECT rc;
    int width;
    int margin;
    int labelWidth;
    int gap;
    int rowHeight;
    int editX;
    int typeLabelX;
    int typeX;
    int addX;
    int browseX;
    int listTop;
    int listWidth;
    int fixedWidth;
    int flexible;
    int nameWidth;
    int urlWidth;

    GetClientRect(hwnd, &rc);
    width = rc.right - rc.left;
    margin = 12;
    labelWidth = 78;
    gap = 8;
    rowHeight = 28;
    editX = margin + labelWidth + gap;
    addX = width - margin - 112;
    typeX = addX - gap - 100;
    typeLabelX = typeX - gap - 52;

    MoveWindow(g_urlLabel, margin, 12, labelWidth, rowHeight, TRUE);
    MoveWindow(g_url, editX, 12, typeLabelX - gap - editX, rowHeight, TRUE);
    MoveWindow(g_typeLabel, typeLabelX, 12, 52, rowHeight, TRUE);
    MoveWindow(g_type, typeX, 12, 100, 160, TRUE);
    MoveWindow(g_add, addX, 12, 112, rowHeight, TRUE);

    browseX = width - margin - 92;
    MoveWindow(g_folderLabel, margin, 50, labelWidth, rowHeight, TRUE);
    MoveWindow(g_folder, editX, 50, browseX - gap - editX, rowHeight, TRUE);
    MoveWindow(g_browse, browseX, 50, 92, rowHeight, TRUE);

    listTop = 90;
    listWidth = width - margin * 2;
    MoveWindow(g_list, margin, listTop, listWidth,
               rc.bottom - listTop - margin, TRUE);

    if (g_list != NULL && listWidth > 500)
    {
        fixedWidth = 40 + 72 + 78 + 72 + 26;
        flexible = listWidth - fixedWidth;
        if (flexible < 260) flexible = 260;
        nameWidth = flexible * 42 / 100;
        urlWidth = flexible - nameWidth;
        ListView_SetColumnWidth(g_list, 0, 40);
        ListView_SetColumnWidth(g_list, 1, nameWidth);
        ListView_SetColumnWidth(g_list, 2, urlWidth);
        ListView_SetColumnWidth(g_list, 3, 72);
        ListView_SetColumnWidth(g_list, 4, 78);
        ListView_SetColumnWidth(g_list, 5, 72);
    }
}

static void add_columns(void)
{
    LVCOLUMNW column;
    ZeroMemory(&column, sizeof(column));
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.iSubItem = 0; column.cx = 40; column.pszText = L"#";
    ListView_InsertColumn(g_list, 0, &column);
    column.iSubItem = 1; column.cx = 250; column.pszText = L"Name";
    ListView_InsertColumn(g_list, 1, &column);
    column.iSubItem = 2; column.cx = 330; column.pszText = L"URL";
    ListView_InsertColumn(g_list, 2, &column);
    column.iSubItem = 3; column.cx = 72; column.pszText = L"Type";
    ListView_InsertColumn(g_list, 3, &column);
    column.iSubItem = 4; column.cx = 78; column.pszText = L"Progress";
    ListView_InsertColumn(g_list, 4, &column);
    column.iSubItem = 5; column.cx = 72; column.pszText = L"Completed";
    ListView_InsertColumn(g_list, 5, &column);
}

static LRESULT CALLBACK queue_proc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        {
            NONCLIENTMETRICSW ncm;
            DOWNLOADER_STATUS status;
            LONG_PTR oldProc;

            if (g_queueFont == NULL)
            {
                ZeroMemory(&ncm, sizeof(ncm));
                ncm.cbSize = sizeof(ncm);
                if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,
                                          sizeof(ncm), &ncm, 0))
                    g_queueFont = CreateFontIndirectW(&ncm.lfMessageFont);
            }

            g_urlLabel = CreateWindowW(L"STATIC", L"URL:",
                                     WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                     0, 0, 0, 0, hwnd, NULL, g_instance, NULL);
            g_url = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                    ES_LEFT | ES_AUTOHSCROLL,
                                    58, 12, 300, 28, hwnd, (HMENU)DQ_ID_URL,
                                    g_instance, NULL);
            SendMessageW(g_url, EM_SETREADONLY, FALSE, 0);
            SendMessageW(g_url, EM_SETLIMITTEXT,
                         (WPARAM)(DOWNLOADER_URL_MAX - 1), 0);
            SetLastError(NO_ERROR);
            oldProc = SetWindowLongPtrW(g_url, GWLP_WNDPROC, (LONG_PTR)url_proc);
            if (oldProc != 0 || GetLastError() == NO_ERROR)
                g_urlOldProc = (WNDPROC)oldProc;

            g_typeLabel = CreateWindowW(L"STATIC", L"Type:",
                                     WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                     0, 0, 0, 0, hwnd, NULL, g_instance, NULL);
            g_type = CreateWindowW(L"COMBOBOX", L"",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                   CBS_DROPDOWNLIST | WS_VSCROLL,
                                   0, 0, 0, 0, hwnd, (HMENU)DQ_ID_TYPE,
                                   g_instance, NULL);
            SendMessageW(g_type, CB_ADDSTRING, 0, (LPARAM)L"Video");
            SendMessageW(g_type, CB_ADDSTRING, 0, (LPARAM)L"Audio");
            SendMessageW(g_type, CB_SETCURSEL, 0, 0);

            g_add = CreateWindowW(L"BUTTON", L"Add to Queue",
                                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                  0, 0, 0, 0, hwnd, (HMENU)DQ_ID_ADD,
                                  g_instance, NULL);

            g_folderLabel = CreateWindowW(L"STATIC", L"Save to:",
                                        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
                                        0, 0, 0, 0, hwnd, NULL, g_instance, NULL);
            g_folder = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                       WS_CHILD | WS_VISIBLE | ES_READONLY | ES_AUTOHSCROLL,
                                       0, 0, 0, 0, hwnd, (HMENU)DQ_ID_FOLDER,
                                       g_instance, NULL);
            g_browse = CreateWindowW(L"BUTTON", L"Browse...",
                                     WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                     0, 0, 0, 0, hwnd, (HMENU)DQ_ID_BROWSE,
                                     g_instance, NULL);

            g_list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                     LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                                     0, 0, 0, 0, hwnd, (HMENU)DQ_ID_LIST,
                                     g_instance, NULL);
            ListView_SetExtendedListViewStyle(g_list,
                                              LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            add_columns();

            if (g_queueFont != NULL)
            {
                HWND header;

                SendMessageW(g_urlLabel, WM_SETFONT, (WPARAM)g_queueFont, TRUE);
                SendMessageW(g_url, WM_SETFONT, (WPARAM)g_queueFont, TRUE);
                SendMessageW(g_typeLabel, WM_SETFONT, (WPARAM)g_queueFont, TRUE);
                SendMessageW(g_type, WM_SETFONT, (WPARAM)g_queueFont, TRUE);
                SendMessageW(g_add, WM_SETFONT, (WPARAM)g_queueFont, TRUE);
                SendMessageW(g_folderLabel, WM_SETFONT, (WPARAM)g_queueFont, TRUE);
                SendMessageW(g_folder, WM_SETFONT, (WPARAM)g_queueFont, TRUE);
                SendMessageW(g_browse, WM_SETFONT, (WPARAM)g_queueFont, TRUE);
                SendMessageW(g_list, WM_SETFONT, (WPARAM)g_queueFont, TRUE);

                header = ListView_GetHeader(g_list);
                if (header != NULL)
                    SendMessageW(header, WM_SETFONT, (WPARAM)g_queueFont, TRUE);
            }

            DownloaderGetStatus(g_downloader, &status);
            SetWindowTextW(g_folder, status.outputDirectory);
            layout(hwnd);
            refresh_list();
        }
        return 0;

    case WM_GETMINMAXINFO:
        {
            MINMAXINFO *mmi = (MINMAXINFO *)lParam;
            mmi->ptMinTrackSize.x = 760;
            mmi->ptMinTrackSize.y = 420;
        }
        return 0;

    case WM_SIZE:
        layout(hwnd);
        return 0;

    case WM_CONTEXTMENU:
        if ((HWND)wParam == g_list)
        {
            show_list_context_menu(hwnd, lParam);
            return 0;
        }
        break;

    case WM_TIMER:
        if (wParam == DQ_BALLOON_TIMER)
        {
            KillTimer(hwnd, DQ_BALLOON_TIMER);
            delete_tray_icon();
            return 0;
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case DQ_ID_ADD:
            add_item(hwnd);
            return 0;
        case DQ_ID_BROWSE:
            browse_folder(hwnd);
            return 0;
        }
        break;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, DQ_BALLOON_TIMER);
        delete_tray_icon();
        if (g_url != NULL && g_urlOldProc != NULL)
        {
            SetWindowLongPtrW(g_url, GWLP_WNDPROC, (LONG_PTR)g_urlOldProc);
            g_urlOldProc = NULL;
        }
        g_window = NULL;
        g_urlLabel = NULL;
        g_url = NULL;
        g_type = NULL;
        g_typeLabel = NULL;
        g_add = NULL;
        g_folderLabel = NULL;
        g_folder = NULL;
        g_browse = NULL;
        g_list = NULL;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

BOOL DownloadQueueInitialize(HINSTANCE instance, DOWNLOADER_STATE *downloader)
{
    INITCOMMONCONTROLSEX common;
    WNDCLASSEXW wc;

    g_instance = instance;
    g_downloader = downloader;
    g_count = 0;
    g_active = -1;
    g_batchOpen = FALSE;
    g_batchStart = 0;
    g_trayAdded = FALSE;
    ZeroMemory(g_items, sizeof(g_items));

    ZeroMemory(&common, sizeof(common));
    common.dwSize = sizeof(common);
    common.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&common);

    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = queue_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SystemAudioRecorderDownloadQueueV20Beta1EN";
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        return FALSE;
    return TRUE;
}

void DownloadQueueDestroy(void)
{
    if (g_window != NULL)
        DestroyWindow(g_window);
    g_window = NULL;

    if (g_queueFont != NULL)
    {
        DeleteObject(g_queueFont);
        g_queueFont = NULL;
    }
}

void DownloadQueueShow(HWND owner)
{
    g_owner = owner;
    if (g_window == NULL)
    {
        g_window = CreateWindowExW(WS_EX_APPWINDOW,
                                   L"SystemAudioRecorderDownloadQueueV20Beta1EN",
                                   L"SystemAudioRecorder - Download Queue",
                                   WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                   CW_USEDEFAULT, CW_USEDEFAULT, 940, 540,
                                   owner, NULL, g_instance, NULL);
        if (g_window == NULL) return;
    }
    ShowWindow(g_window, SW_SHOWNORMAL);
    SetForegroundWindow(g_window);
    if (g_url != NULL) SetFocus(g_url);
}

void DownloadQueueTick(HWND owner)
{
    DOWNLOADER_STATUS status;
    int completedIndex;
    BOOL succeeded;

    if (g_downloader == NULL) return;

    if (g_active >= 0)
    {
        DownloaderGetStatus(g_downloader, &status);
        if (status.displayName[0] != L'\0' &&
            wcscmp(g_items[g_active].name, status.displayName) != 0)
            StringCchCopyW(g_items[g_active].name,
                           ARRAY_COUNT(g_items[g_active].name),
                           status.displayName);

        if (status.busy)
        {
            if (status.progressPercent >= 0 && status.progressPercent <= 100)
                g_items[g_active].progress = status.progressPercent;
            refresh_list();
        }
        else if (status.exitCode != STILL_ACTIVE)
        {
            completedIndex = g_active;
            succeeded = status.success;
            if (status.displayName[0] != L'\0')
                StringCchCopyW(g_items[completedIndex].name,
                               ARRAY_COUNT(g_items[completedIndex].name),
                               status.displayName);
            g_items[completedIndex].state = succeeded ? DQ_STATE_DONE : DQ_STATE_FAILED;
            if (succeeded) g_items[completedIndex].progress = 100;
            g_active = -1;
            refresh_list();
            start_next(owner);

            /* Keep failure diagnostic visible; successful items are silent. */
            if (!succeeded)
                MessageBoxW(g_window != NULL ? g_window : owner,
                            status.message, L"Download Failed", MB_OK | MB_ICONERROR);
            maybe_notify_batch_complete();
        }
    }
    else
    {
        start_next(owner);
        maybe_notify_batch_complete();
    }

    if (g_browse != NULL) EnableWindow(g_browse, g_active < 0);
}

void DownloadQueueGetCounts(int *waiting, int *active, int *completed, int *failed)
{
    int i;
    int w = 0;
    int a = 0;
    int c = 0;
    int f = 0;

    for (i = 0; i < g_count; ++i)
    {
        if (g_items[i].state == DQ_STATE_WAITING) ++w;
        else if (g_items[i].state == DQ_STATE_ACTIVE) ++a;
        else if (g_items[i].state == DQ_STATE_DONE) ++c;
        else if (g_items[i].state == DQ_STATE_FAILED) ++f;
    }
    if (waiting != NULL) *waiting = w;
    if (active != NULL) *active = a;
    if (completed != NULL) *completed = c;
    if (failed != NULL) *failed = f;
}
