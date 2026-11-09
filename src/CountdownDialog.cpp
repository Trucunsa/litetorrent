#include "CountdownDialog.h"

namespace {
constexpr wchar_t kClassName[] = L"LiteTorrentCountdown";
constexpr int ID_CANCEL = 1;
}

LRESULT CALLBACK CountdownDialog::proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = reinterpret_cast<CountdownDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_NCCREATE:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams));
        break;
    case WM_COMMAND:
        if (self && LOWORD(wParam) == ID_CANCEL) {
            auto cb = self->m_onCancel; // the callback may close (destroy) this very window
            if (cb) cb();
            return 0;
        }
        break;
    case WM_CLOSE: // the title-bar X also means "cancel"
        if (self) {
            auto cb = self->m_onCancel;
            if (cb) cb();
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, GetSysColorBrush(COLOR_BTNFACE));
        return 1;
    }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void CountdownDialog::show(HWND owner, HINSTANCE hInst, const std::wstring& title,
                           const std::wstring& text, const std::wstring& cancelLabel,
                           std::function<void()> onCancel)
{
    if (m_hwnd) {
        setText(text);
        return;
    }
    m_onCancel = std::move(onCancel);

    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = proc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
        wc.lpszClassName = kClassName;
        wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(100));
        RegisterClassW(&wc);
        registered = true;
    }

    const int w = win::scale(440), h = win::scale(190);
    RECT r{};
    if (owner && !IsIconic(owner))
        GetWindowRect(owner, &r);
    else {
        r.right = GetSystemMetrics(SM_CXSCREEN);
        r.bottom = GetSystemMetrics(SM_CYSCREEN);
    }
    const int x = r.left + ((r.right - r.left) - w) / 2;
    const int y = r.top + ((r.bottom - r.top) - h) / 2;

    m_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_DLGMODALFRAME, kClassName, title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, w, h, owner, nullptr, hInst, this);
    if (!m_hwnd)
        return;

    m_font = win::makeFont(16, FW_SEMIBOLD);
    m_text = CreateWindowExW(0, L"STATIC", text.c_str(), WS_CHILD | WS_VISIBLE | SS_CENTER,
        win::scale(20), win::scale(24), w - win::scale(40) - 2 * GetSystemMetrics(SM_CXFIXEDFRAME),
        win::scale(56), m_hwnd, nullptr, hInst, nullptr);
    SendMessageW(m_text, WM_SETFONT, reinterpret_cast<WPARAM>(m_font), TRUE);

    m_button = CreateWindowExW(0, L"BUTTON", cancelLabel.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        win::scale(110), win::scale(92), win::scale(200), win::scale(36),
        m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CANCEL)), hInst, nullptr);
    SendMessageW(m_button, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

    ShowWindow(m_hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_button);
    MessageBeep(MB_ICONEXCLAMATION);
}

void CountdownDialog::setText(const std::wstring& text)
{
    if (m_text)
        SetWindowTextW(m_text, text.c_str());
}

void CountdownDialog::close()
{
    if (m_hwnd) {
        HWND h = m_hwnd;
        m_hwnd = nullptr;
        m_text = nullptr;
        m_button = nullptr;
        DestroyWindow(h);
    }
    if (m_font) {
        DeleteObject(m_font);
        m_font = nullptr;
    }
}
