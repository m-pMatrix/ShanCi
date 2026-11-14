/* ---------------------------------------------------------------------------
 * Bootstrapper.c - 单文件启动器（自解压）
 *
 * 本程序不依赖 Qt、不依赖 C++ 运行库（以 -static 编译），仅依赖 Windows API
 * 与 zlib。运行流程：
 *   1. 读取自身 RCDATA 资源中的压缩包（由 tools/package.py 生成）；
 *   2. 若目标目录版本不匹配，则解压到 %LOCALAPPDATA%\VocabTutor\app；
 *      （用户数据 data\vocab.db 等不在压缩包内，不会被覆盖）
 *   3. 启动 app\VocabTutor.exe，工作目录设为该目录。
 *
 * 压缩包格式（小端）：
 *   magic       : 4 字节 'V','T','P','K'
 *   fmtVersion  : u32
 *   tagLen      : u16 + tag(tagLen 字节, UTF-8, 用于判断是否需要重新解压)
 *   fileCount   : u32
 *   每个文件:
 *     pathLen   : u16 + path(pathLen 字节, UTF-8, 以 '/' 分隔)
 *     compLen   : u64 (zlib 压缩后长度)
 *     rawLen    : u64 (原始长度)
 *     crc32     : u32
 *     data      : compLen 字节 (zlib 格式，可直接用 zlib 的 uncompress 解压)
 * ------------------------------------------------------------------------- */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "zlib.h"

#define PAYLOAD_RESID 101
#define APP_DIR_NAME  L"ShanCi"
#define APP_EXE_NAME  L"ShanCi.exe"
#define TAG_FILE_NAME L"\\.payload-tag"
#define MAX_WPATH     32768

static HWND g_hwnd = NULL;
static HWND g_status = NULL;

/* ------------------------------ 小工具 ------------------------------ */

static void pump_messages(void) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

static void set_status(const wchar_t *text) {
    if (g_status) SetWindowTextW(g_status, text);
    pump_messages();
}

static void fail(const wchar_t *text) {
    MessageBoxW(NULL, text, L"启动失败", MB_ICONERROR | MB_OK);
    if (g_hwnd) DestroyWindow(g_hwnd);
}

/* 递归创建目录 */
static void make_dirs(const wchar_t *fullPath) {
    wchar_t buf[MAX_WPATH];
    size_t n = wcslen(fullPath);
    wchar_t *p;
    if (n == 0 || n >= MAX_WPATH) return;
    wcsncpy(buf, fullPath, MAX_WPATH - 1);
    buf[MAX_WPATH - 1] = 0;
    for (p = buf + 1; *p; ++p) {
        if (*p == L'\\' || *p == L'/') {
            wchar_t c = *p;
            *p = 0;
            CreateDirectoryW(buf, NULL);
            *p = c;
        }
    }
    CreateDirectoryW(buf, NULL);
}

static void normalize_slashes(wchar_t *s) {
    for (; *s; ++s)
        if (*s == L'/') *s = L'\\';
}

static int utf8_to_wide(const char *src, int srcLen, wchar_t *dst, int dstChars) {
    return MultiByteToWideChar(CP_UTF8, 0, src, srcLen, dst, dstChars);
}

static int write_file(const wchar_t *path, const unsigned char *data, size_t len) {
    HANDLE h;
    const unsigned char *p = data;
    size_t remaining = len;
    h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    while (remaining > 0) {
        DWORD chunk = (remaining > 0x10000000u) ? 0x10000000u : (DWORD)remaining;
        DWORD written = 0;
        if (!WriteFile(h, p, chunk, &written, NULL) || written == 0) {
            CloseHandle(h);
            return 0;
        }
        p += written;
        remaining -= written;
    }
    CloseHandle(h);
    return 1;
}

static int read_text_file(const wchar_t *path, char *buf, int bufSize) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD read = 0;
    BOOL ok;
    if (h == INVALID_HANDLE_VALUE) return -1;
    ok = ReadFile(h, buf, (DWORD)bufSize - 1, &read, NULL);
    CloseHandle(h);
    if (!ok) return -1;
    buf[read] = 0;
    return (int)read;
}

/* --------------------------- 小端读取 --------------------------- */
static unsigned int rd_u16(const unsigned char *p) { return (unsigned int)p[0] | ((unsigned int)p[1] << 8); }
static unsigned int rd_u32(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}
static unsigned long long rd_u64(const unsigned char *p) {
    unsigned long long v = 0;
    int i;
    for (i = 7; i >= 0; --i) v = (v << 8) | (unsigned long long)p[i];
    return v;
}

/* ------------------------------ 主逻辑 ------------------------------ */

static void compose_target_dir(wchar_t *out, size_t outChars) {
    wchar_t local[MAX_WPATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_WPATH);
    if (n == 0 || n >= MAX_WPATH) {
        GetModuleFileNameW(NULL, local, (DWORD)MAX_WPATH);
        {
            wchar_t *slash = wcsrchr(local, L'\\');
            if (slash) *slash = 0;
        }
    }
    swprintf(out, outChars, L"%ls\\%ls\\app", local, APP_DIR_NAME);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nCmdShow) {
    HRSRC hrs;
    HGLOBAL hg;
    const unsigned char *payload;
    const unsigned char *p, *end;
    DWORD payloadSize;
    unsigned int fmtVersion, tagLen, fileCount, i;
    char tag[256];
    wchar_t target[MAX_WPATH], tagPath[MAX_WPATH], exePath[MAX_WPATH], cmd[MAX_WPATH];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    int needExtract = 1;
    char existingTag[256];
    WNDCLASSEXW wc;
    int sw, sh;

    (void)hPrev; (void)lpCmdLine; (void)nCmdShow;

    /* --- 提示窗口 --- */
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"ShanCiBoot";
    RegisterClassExW(&wc);

    sw = GetSystemMetrics(SM_CXSCREEN);
    sh = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"闪词",
                             WS_POPUP | WS_BORDER | WS_CAPTION,
                             (sw - 420) / 2, (sh - 150) / 2, 420, 150,
                             NULL, NULL, hInst, NULL);
    if (g_hwnd) {
        g_status = CreateWindowExW(0, L"STATIC", L"正在准备运行环境，请稍候…",
                                   WS_CHILD | WS_VISIBLE | SS_CENTER,
                                   20, 40, 370, 60, g_hwnd, NULL, hInst, NULL);
        ShowWindow(g_hwnd, SW_SHOW);
        UpdateWindow(g_hwnd);
        pump_messages();
    }

    /* --- 定位资源 --- */
    hrs = FindResourceW(NULL, MAKEINTRESOURCEW(PAYLOAD_RESID), RT_RCDATA);
    if (!hrs) { fail(L"未找到内置运行环境数据，文件可能已损坏。"); return 1; }
    payloadSize = SizeofResource(NULL, hrs);
    hg = LoadResource(NULL, hrs);
    payload = (const unsigned char *)LockResource(hg);
    if (!payload || payloadSize < 14) { fail(L"内置运行环境数据无效。"); return 1; }

    /* --- 解析头部 --- */
    p = payload;
    end = payload + payloadSize;
    if (!(p[0] == 'V' && p[1] == 'T' && p[2] == 'P' && p[3] == 'K')) {
        fail(L"运行环境数据格式错误。");
        return 1;
    }
    p += 4;
    if (p + 6 > end) { fail(L"运行环境数据已损坏。"); return 1; }
    fmtVersion = rd_u32(p);
    (void)fmtVersion;
    p += 4;
    tagLen = rd_u16(p);
    p += 2;
    if (p + tagLen + 4 > end || tagLen >= sizeof(tag)) { fail(L"运行环境数据已损坏。"); return 1; }
    memcpy(tag, p, tagLen);
    tag[tagLen] = 0;
    p += tagLen;
    fileCount = rd_u32(p);
    p += 4;

    /* --- 目标目录 --- */
    compose_target_dir(target, MAX_WPATH);
    swprintf(tagPath, MAX_WPATH, L"%ls%ls", target, TAG_FILE_NAME);
    swprintf(exePath, MAX_WPATH, L"%ls\\%ls", target, APP_EXE_NAME);

    if (GetFileAttributesW(exePath) != INVALID_FILE_ATTRIBUTES &&
        read_text_file(tagPath, existingTag, sizeof(existingTag)) > 0 &&
        strcmp(existingTag, tag) == 0) {
        needExtract = 0;
    }

    if (needExtract) {
        make_dirs(target);
        for (i = 0; i < fileCount; ++i) {
            unsigned int pathLen;
            unsigned long long compLen, rawLen;
            wchar_t rel[MAX_WPATH], fullPath[MAX_WPATH], parent[MAX_WPATH];
            int wideChars;
            unsigned char *raw;

            if (p + 2 > end) { fail(L"运行环境数据已损坏（条目头）。"); return 1; }
            pathLen = rd_u16(p);
            p += 2;
            if (p + pathLen + 20 > end) { fail(L"运行环境数据已损坏（路径）。"); return 1; }
            wideChars = utf8_to_wide((const char *)p, (int)pathLen, rel, MAX_WPATH - 1);
            if (wideChars <= 0) { fail(L"运行环境数据路径解析失败。"); return 1; }
            rel[wideChars] = 0;
            p += pathLen;
            compLen = rd_u64(p); p += 8;
            rawLen = rd_u64(p); p += 8;
            p += 4; /* crc32（校验可选） */
            if (p + compLen > end) { fail(L"运行环境数据已损坏（内容）。"); return 1; }

            swprintf(fullPath, MAX_WPATH, L"%ls\\%ls", target, rel);
            normalize_slashes(fullPath);

            wcsncpy(parent, fullPath, MAX_WPATH - 1);
            parent[MAX_WPATH - 1] = 0;
            {
                wchar_t *slash = wcsrchr(parent, L'\\');
                if (slash) { *slash = 0; make_dirs(parent); }
            }

            if (rawLen == 0) {
                if (!write_file(fullPath, (const unsigned char *)"", 0)) {
                    fail(L"写入文件失败，请检查磁盘空间或权限。");
                    return 1;
                }
                p += compLen;
                continue;
            }

            raw = (unsigned char *)malloc((size_t)rawLen);
            if (!raw) { fail(L"内存不足。"); return 1; }
            {
                uLongf destLen = (uLongf)rawLen;
                int rc = uncompress(raw, &destLen, (const Bytef *)p, (uLong)compLen);
                if (rc != Z_OK || destLen != (uLongf)rawLen) {
                    free(raw);
                    fail(L"解压内置运行环境失败。");
                    return 1;
                }
                if (!write_file(fullPath, raw, (size_t)destLen)) {
                    free(raw);
                    fail(L"写入文件失败，请检查磁盘空间或权限。");
                    return 1;
                }
            }
            free(raw);
            p += compLen;

            if ((i & 31) == 0 && g_status) {
                wchar_t msg[128];
                swprintf(msg, 128, L"正在准备运行环境… %u%%",
                         (unsigned int)(((unsigned long long)(i + 1) * 100) / fileCount));
                set_status(msg);
            }
        }
        write_file(tagPath, (const unsigned char *)tag, strlen(tag));
    }

    /* --- 启动主程序 --- */
    set_status(L"正在启动…");
    swprintf(cmd, MAX_WPATH, L"\"%ls\"", exePath);

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, 0, NULL, target, &si, &pi)) {
        DWORD err = GetLastError();
        wchar_t msg[1024];
        swprintf(msg, 1024, L"启动主程序失败（错误码 %lu）。\n\n路径：%ls",
                 (unsigned long)err, exePath);
        fail(msg);
        return 1;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (g_hwnd) DestroyWindow(g_hwnd);
    return 0;
}
