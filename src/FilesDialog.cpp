#include "FilesDialog.h"
#include "Localization.h"

#include <commctrl.h>
#include <algorithm>

namespace {

constexpr wchar_t kClassName[] = L"LiteTorrentFilesDialog";
constexpr int IDC_LIST = 101;
constexpr int IDC_ALL = 102;
constexpr int IDC_NONE = 103;
constexpr int IDC_SUMMARY = 104;

struct State
{
    std::vector<TorrentFileEntry>* files = nullptr;
    HWND hwnd = nullptr;
    HWND list = nullptr;
    HWND summary = nullptr;
    HWND ok = nullptr;
    bool accepted = false;
    bool done = false;
    bool filling = false; // suppress change notifications while populating
};

int S(int v) { return win::px(v); }

void updateSummary(State* st)
{
    int64_t total = 0;
    int count = 0;
    const int n = ListView_GetItemCount(st->list);
    for (int i = 0; i < n; ++i) {
        if (ListView_GetCheckState(st->list, i)) {
            total += (*st->files)[static_cast<size_t>(i)].size;
            ++count;
        }
    }
    std::wstring text = Localization::instance().t(Localization::FilesSummary);
    const size_t c = text.find(L"@COUNT@");
    if (c != std::wstring::npos) text.replace(c, 7, std::to_wstring(count));
    const size_t a = text.find(L"@ALL@");
    if (a != std::wstring::npos) text.replace(a, 5, std::to_wstring(n));
    const size_t z = text.find(L"@SIZE@");
    if (z != std::wstring::npos) text.replace(z, 6, count ? win::formatBytes(total) : L"0");
    SetWindowTextW(st->summary, text.c_str());
    EnableWindow(st->ok, count > 0); // downloading nothing at all makes no sense
}

void setAll(State* st, bool checked)
{
    const int n = ListView_GetItemCount(st->list);
    st->filling = true;
    for (int i = 0; i < n; ++i)
        ListView_SetCheckState(st->list, i, checked);
    st->filling = false;
    updateSummary(st);
}

void layout(State* st)
{
    RECT rc; GetClientRect(st->hwnd, &rc);
    const int w = rc.right, h = rc.bottom;
    const int m = S(12), bh = S(28), bw = S(100);
    MoveWindow(st->list, m, m, w - 2 * m, h - 2 * m - bh - S(10), TRUE);
    const int y = h - m - bh;
    MoveWindow(GetDlgItem(st->hwnd, IDC_ALL), m, y, S(90), bh, TRUE);
    MoveWindow(GetDlgItem(st->hwnd, IDC_NONE), m + S(96), y, S(90), bh, TRUE);
    MoveWindow(st->summary, m + S(196), y + S(5), w - 2 * m - S(196) - 2 * bw - S(16), S(20), TRUE);
    MoveWindow(GetDlgItem(st->hwnd, IDCANCEL), w - m - bw, y, bw, bh, TRUE);
    MoveWindow(st->ok, w - m - 2 * bw - S(8), y, bw, bh, TRUE);
    // Name column takes what the size column leaves.
    RECT lr; GetClientRect(st->list, &lr);
    ListView_SetColumnWidth(st->list, 1, S(90));
    ListView_SetColumnWidth(st->list, 0, std::max<int>(S(120), lr.right - S(90)));
}

HWND makeButton(State* st, HINSTANCE hInst, Localization::Key key, int id, DWORD style = BS_PUSHBUTTON)
{
    HWND b = CreateWindowExW(0, L"BUTTON", Localization::instance().t(key).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | style, 0, 0, 0, 0, st->hwnd,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
    SendMessageW(b, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);
    return b;
}

LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    State* st = reinterpret_cast<State*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = reinterpret_cast<State*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->hwnd = hwnd;
        const Localization& loc = Localization::instance();

        st->list = CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_LIST)), cs->hInstance, nullptr);
        ListView_SetExtendedListViewStyle(st->list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SendMessageW(st->list, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

        std::wstring colName = loc.t(Localization::FilesColName), colSize = loc.t(Localization::ColSize);
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = S(400); col.pszText = colName.data();
        ListView_InsertColumn(st->list, 0, &col);
        col.mask |= LVCF_FMT; col.fmt = LVCFMT_RIGHT;
        col.cx = S(90); col.pszText = colSize.data();
        ListView_InsertColumn(st->list, 1, &col);

        st->filling = true;
        for (size_t i = 0; i < st->files->size(); ++i) {
            const TorrentFileEntry& f = (*st->files)[i];
            LVITEMW item{};
            item.mask = LVIF_TEXT;
            item.iItem = static_cast<int>(i);
            item.pszText = const_cast<LPWSTR>(f.path.c_str());
            ListView_InsertItem(st->list, &item);
            std::wstring size = win::formatBytes(f.size);
            ListView_SetItemText(st->list, static_cast<int>(i), 1, size.data());
            ListView_SetCheckState(st->list, static_cast<int>(i), f.wanted);
        }
        st->filling = false;

        makeButton(st, cs->hInstance, Localization::FilesSelectAll, IDC_ALL);
        makeButton(st, cs->hInstance, Localization::FilesSelectNone, IDC_NONE);
        st->summary = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
            0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_SUMMARY)), cs->hInstance, nullptr);
        SendMessageW(st->summary, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);
        st->ok = makeButton(st, cs->hInstance, Localization::FilesOk, IDOK, BS_DEFPUSHBUTTON);
        makeButton(st, cs->hInstance, Localization::AddDialogCancel, IDCANCEL);

        layout(st);
        updateSummary(st);
        SetFocus(st->list);
        return 0;
    }
    case WM_SIZE:
        if (st && st->list) layout(st);
        return 0;
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = S(560);
        mmi->ptMinTrackSize.y = S(260);
        return 0;
    }
    case WM_NOTIFY: {
        auto* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (st && hdr->idFrom == IDC_LIST && hdr->code == LVN_ITEMCHANGED && !st->filling) {
            auto* nm = reinterpret_cast<NMLISTVIEW*>(lParam);
            if (nm->uChanged & LVIF_STATE)
                updateSummary(st);
        }
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_ALL: setAll(st, true); return 0;
        case IDC_NONE: setAll(st, false); return 0;
        case IDOK: {
            if (!IsWindowEnabled(st->ok))
                return 0;
            const int n = ListView_GetItemCount(st->list);
            for (int i = 0; i < n; ++i)
                (*st->files)[static_cast<size_t>(i)].wanted = ListView_GetCheckState(st->list, i) != FALSE;
            st->accepted = true;
            st->done = true;
            return 0;
        }
        case IDCANCEL: st->done = true; return 0;
        }
        return 0;
    case WM_CLOSE:
        st->done = true;
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

bool showFilesDialog(HWND owner, HINSTANCE hInst, const std::wstring& torrentName,
                     std::vector<TorrentFileEntry>& files)
{
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

    State st;
    st.files = &files;

    RECT r{};
    GetWindowRect(owner, &r);
    const int w = S(680), h = S(440);
    const int x = r.left + ((r.right - r.left) - w) / 2;
    const int y = r.top + ((r.bottom - r.top) - h) / 2;

    std::wstring title = Localization::instance().t(Localization::FilesDialogTitle);
    if (!torrentName.empty())
        title += L" — " + torrentName;

    EnableWindow(owner, FALSE);
    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kClassName, title.c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, x, y, w, h, owner, nullptr, hInst, &st);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE && GetAncestor(msg.hwnd, GA_ROOT) == hwnd) {
            st.done = true;
            continue;
        }
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (msg.message == WM_QUIT)
        PostQuitMessage(static_cast<int>(msg.wParam)); // let the main loop see it

    DestroyWindow(hwnd);
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
    return st.accepted;
}
