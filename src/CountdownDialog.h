#pragma once

#include "Win32Util.h"

#include <functional>
#include <string>

// The always-on-top "Shutting down in N seconds" window with a big Cancel
// button, shown while the shutdown countdown runs. Not modal: the main
// window's timer keeps driving the countdown and calls setText()/close().
class CountdownDialog
{
public:
    ~CountdownDialog() { close(); }

    void show(HWND owner, HINSTANCE hInst, const std::wstring& title,
              const std::wstring& text, const std::wstring& cancelLabel,
              std::function<void()> onCancel);
    void setText(const std::wstring& text);
    void close();
    bool isOpen() const { return m_hwnd != nullptr; }

private:
    static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    HWND m_hwnd = nullptr;
    HWND m_text = nullptr;
    HWND m_button = nullptr;
    HFONT m_font = nullptr;
    std::function<void()> m_onCancel;
};
