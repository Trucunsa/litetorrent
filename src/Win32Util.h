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
// --- Display scaling ---------------------------------------------------------
// The manifest declares the app per-monitor DPI aware, so Windows does not
// stretch the window for us: every pixel measurement in the UI code goes
// through px(), and fonts are created for the current DPI.

inline int& currentDpi()
{
    static int dpi = 96;
    return dpi;
}

// Converts a size designed at 100 % scaling to the current monitor's scale.
inline int px(int designPixels)
{
    return MulDiv(designPixels, currentDpi(), 96);
}

struct UiFonts
{
    HFONT normal = nullptr;
    HFONT small_ = nullptr;
    HFONT large = nullptr;
    int dpi = 0;
};

inline UiFonts& uiFonts()
{
    static UiFonts f;
    if (f.dpi != currentDpi()) {
        // (Old fonts are deleted by setDpi() after every control has
        // been switched to the new ones.)
        auto make = [](int size, int weight) {
            return CreateFontW(-px(size), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        };
        f.normal = make(14, FW_NORMAL);
        f.small_ = make(11, FW_NORMAL);
        f.large = make(18, FW_SEMIBOLD);
        f.dpi = currentDpi();
    }
    return f;
}

// Segoe UI has full coverage for the arrows the status bar uses; the stock
// DEFAULT_GUI_FONT is not guaranteed to.
inline HFONT uiFont() { return uiFonts().normal; }
inline HFONT uiSmallFont() { return uiFonts().small_; }
inline HFONT uiLargeFont() { return uiFonts().large; }

inline BOOL CALLBACK applyFontProc(HWND child, LPARAM font)
{
    SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
    return TRUE;
}

// Switches to a new DPI: new fonts for every child of `top`, old ones freed.
inline void setDpi(HWND top, int dpi)
{
    if (dpi <= 0 || dpi == currentDpi())
        return;
    const UiFonts old = uiFonts();
    currentDpi() = dpi;
    const HFONT font = uiFont();
    if (top)
        EnumChildWindows(top, applyFontProc, reinterpret_cast<LPARAM>(font));
    if (old.normal) DeleteObject(old.normal);
    if (old.small_) DeleteObject(old.small_);
    if (old.large) DeleteObject(old.large);
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
