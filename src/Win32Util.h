#pragma once

// Small, dependency-free helpers used across the no-Qt build: wide<->UTF-8
// string conversion (Win32 API wants wchar_t*, libtorrent wants UTF-8
// std::string) and a couple of numeric-formatting helpers that replace
// what QLocale::formattedDataSize() used to do.

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include <windows.h>

#include <string>
#include <cstdint>

namespace win {

inline std::wstring utf8ToWide(const std::string& s)
{
    if (s.empty())
        return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len);
    return out;
}

inline std::string wideToUtf8(const std::wstring& s)
{
    if (s.empty())
        return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), len, nullptr, nullptr);
    return out;
}

// Human-readable byte size, e.g. "4.15 MB" / "812 KB" / "0 B". Matches the
// binary (1024-based) convention QLocale::formattedDataSize() used.
inline std::wstring formatBytes(int64_t bytes)
{
    if (bytes <= 0)
        return L"-";
    static const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    wchar_t buf[64];
    if (unit == 0)
        swprintf(buf, 64, L"%.0f %s", value, units[unit]);
    else
        swprintf(buf, 64, L"%.2f %s", value, units[unit]);
    return buf;
}

inline std::wstring formatSpeed(int bps)
{
    if (bps <= 0)
        return L"-";
    return formatBytes(bps) + L"/s";
}

// --- DPI scaling -----------------------------------------------------------
// The manifest declares the app per-monitor-DPI aware, so Windows no longer
// stretches it: every pixel size in the code goes through scale(), and the
// shared font is rebuilt when the DPI changes.
inline int& dpiRef() { static int d = 96; return d; }
inline int dpi() { return dpiRef(); }
inline int scale(int px) { return MulDiv(px, dpiRef(), 96); }

// DPI of the monitor a window is on (Windows 10 1607+), else the system DPI.
inline int queryDpi(HWND hwnd)
{
    using Fn = UINT(WINAPI*)(HWND);
    static Fn fn = reinterpret_cast<Fn>(reinterpret_cast<void*>(
        GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow")));
    if (fn && hwnd) {
        const UINT d = fn(hwnd);
        if (d)
            return static_cast<int>(d);
    }
    HDC dc = GetDC(nullptr);
    const int d = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(nullptr, dc);
    return d > 0 ? d : 96;
}

inline HFONT makeFont(int pixels, int weight)
{
    return CreateFontW(-MulDiv(pixels, dpiRef(), 96), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

// Shared UI font (Segoe UI has full Unicode arrow/glyph coverage; the raw
// DEFAULT_GUI_FONT stock object maps to whatever minimal bitmap font a
// given Windows install falls back to, which isn't guaranteed to cover
// the arrow glyphs the status bar uses). One GDI object per DPI value.
inline HFONT& uiFontRef() { static HFONT f = nullptr; return f; }
inline HFONT uiFont()
{
    if (!uiFontRef())
        uiFontRef() = makeFont(14, FW_NORMAL);
    return uiFontRef();
}

// Switches the scale and returns the previous font; the caller re-applies
// uiFont() to its controls and then deletes the returned handle.
inline HFONT setDpi(int newDpi)
{
    dpiRef() = newDpi;
    HFONT old = uiFontRef();
    uiFontRef() = nullptr;
    return old;
}

// Per-user data folder %LOCALAPPDATA%\LiteTorrent (created on demand).
inline std::wstring dataDir()
{
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    std::wstring dir = (n > 0 && n < MAX_PATH) ? std::wstring(buf) : std::wstring();
    if (dir.empty()) {
        n = GetTempPathW(MAX_PATH, buf);
        dir = (n > 0 && n < MAX_PATH) ? std::wstring(buf) : L".";
        while (!dir.empty() && (dir.back() == L'\\' || dir.back() == L'/')) dir.pop_back();
    }
    dir += L"\\LiteTorrent";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

inline std::wstring formatEta(int64_t remainingBytes, int downloadRateBps)
{
    if (remainingBytes <= 0 || downloadRateBps <= 0)
        return L"-";

    const int64_t totalSecs = remainingBytes / downloadRateBps;
    const int64_t hours = totalSecs / 3600;
    const int64_t minutes = (totalSecs % 3600) / 60;
    const int64_t seconds = totalSecs % 60;

    wchar_t buf[32];
    if (hours > 0)
        swprintf(buf, 32, L"%lld:%02lld:%02lld", (long long)hours, (long long)minutes, (long long)seconds);
    else
        swprintf(buf, 32, L"%lld:%02lld", (long long)minutes, (long long)seconds);
    return buf;
}

} // namespace win
