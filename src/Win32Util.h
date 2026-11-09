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

// H:MM:SS (or M:SS under an hour) -- numeric, so it needs no translation.
// Shared UI font (Segoe UI has full Unicode arrow/glyph coverage; the raw
// DEFAULT_GUI_FONT stock object maps to whatever minimal bitmap font a
// given Windows install falls back to, which isn't guaranteed to cover
// the ↓/↑ glyphs the status bar uses). Created once, lives for the
// process's lifetime -- a single GDI font object is negligible.
inline HFONT uiFont()
{
    static HFONT font = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    return font;
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
