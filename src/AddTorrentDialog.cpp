#include "AddTorrentDialog.h"
#include "AppSettings.h"
#include "Localization.h"

#include <commdlg.h>
#include <shobjidl.h>
#include <shlobj.h>

namespace {

constexpr int IDC_MAGNET_EDIT = 101;
constexpr int IDC_CHOOSE_FILE = 102;
constexpr int IDC_FOLDER_STATIC = 103;
constexpr int IDC_CHANGE_FOLDER = 104;
constexpr int IDC_ADD = 105;
constexpr int IDC_CANCEL = 106;

constexpr wchar_t kClassName[] = L"LiteTorrentAddDialog";

struct DialogState
{
    HWND hwnd = nullptr;
    HWND magnetEdit = nullptr;
    HWND chooseFileBtn = nullptr;
    HWND folderStatic = nullptr;
    HWND changeFolderBtn = nullptr;
    HWND addBtn = nullptr;
    HWND cancelBtn = nullptr;
    std::wstring saveDir;
    bool accepted = false;
    bool done = false;
};

HFONT smallFont()
{
    static HFONT font = nullptr;
    static int fontDpi = 0;
    if (!font || fontDpi != win::dpi()) {
        font = win::makeFont(11, FW_NORMAL); // old one (if any) leaks once per DPI change: negligible
        fontDpi = win::dpi();
    }
    return font;
}

int S(int v) { return win::scale(v); }

void pickFile(DialogState* st)
{
    const Localization& loc = Localization::instance();
    wchar_t path[MAX_PATH] = L"";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = st->hwnd;
    ofn.lpstrFilter = L"Torrent\0*.torrent\0\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    std::wstring title = loc.t(Localization::OpenTorrentDialogTitle);
    ofn.lpstrTitle = title.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&ofn))
        SetWindowTextW(st->magnetEdit, path);

    SetFocus(st->magnetEdit);
}

void pickFolder(DialogState* st)
{
    IFileDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                 IID_PPV_ARGS(&dialog))))
        return;

    DWORD opts = 0;
    dialog->GetOptions(&opts);
    dialog->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);

    IShellItem* startItem = nullptr;
    if (SUCCEEDED(SHCreateItemFromParsingName(st->saveDir.c_str(), nullptr, IID_PPV_ARGS(&startItem)))) {
        dialog->SetFolder(startItem);
        startItem->Release();
    }

    if (SUCCEEDED(dialog->Show(st->hwnd))) {
        IShellItem* result = nullptr;
        if (SUCCEEDED(dialog->GetResult(&result))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                st->saveDir = path;
                SetWindowTextW(st->folderStatic, st->saveDir.c_str());
                CoTaskMemFree(path);

                // Remember it immediately, regardless of whether the
                // dialog is ultimately confirmed or cancelled -- same
                // behavior as before.
                AppSettings settings;
                settings.setDownloadDir(st->saveDir);
            }
            result->Release();
        }
    }
    dialog->Release();
}

void confirmAdd(DialogState* st)
{
    wchar_t buf[4096];
    GetWindowTextW(st->magnetEdit, buf, 4096);
    std::wstring source = buf;
    // Trim surrounding whitespace.
    size_t b = source.find_first_not_of(L" \t\r\n");
    size_t e = source.find_last_not_of(L" \t\r\n");
    source = (b == std::wstring::npos) ? L"" : source.substr(b, e - b + 1);

    if (source.empty()) {
        SetFocus(st->magnetEdit);
        return;
    }

    st->accepted = true;
    st->done = true;
}

LRESULT CALLBACK dialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    DialogState* st = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
    case WM_CREATE: {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        st = reinterpret_cast<DialogState*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(st));
        st->hwnd = hwnd;

        const Localization& loc = Localization::instance();
        HINSTANCE hInst = cs->hInstance;

        auto makeStatic = [&](const std::wstring& text, int x, int y, int w, int h, HFONT font = nullptr) {
            HWND h2 = CreateWindowExW(0, L"STATIC", text.c_str(), WS_CHILD | WS_VISIBLE,
                                       x, y, w, h, hwnd, nullptr, hInst, nullptr);
            SendMessageW(h2, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : win::uiFont()), TRUE);
            return h2;
        };

        makeStatic(loc.t(Localization::AddDialogMagnetLabel), S(20), S(16), S(380), S(18));

        st->magnetEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
            S(20), S(38), S(400), S(24), hwnd, reinterpret_cast<HMENU>(IDC_MAGNET_EDIT), hInst, nullptr);
        SendMessageW(st->magnetEdit, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

        st->chooseFileBtn = CreateWindowExW(0, L"BUTTON", loc.t(Localization::AddDialogChooseFile).c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            S(20), S(72), S(400), S(28), hwnd, reinterpret_cast<HMENU>(IDC_CHOOSE_FILE), hInst, nullptr);
        SendMessageW(st->chooseFileBtn, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

        makeStatic(loc.t(Localization::AddDialogSaveToLabel), S(20), S(114), S(380), S(18));

        st->folderStatic = CreateWindowExW(WS_EX_CLIENTEDGE, L"STATIC", st->saveDir.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_PATHELLIPSIS | SS_NOTIFY,
            S(20), S(136), S(300), S(24), hwnd, reinterpret_cast<HMENU>(IDC_FOLDER_STATIC), hInst, nullptr);
        SendMessageW(st->folderStatic, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

        st->changeFolderBtn = CreateWindowExW(0, L"BUTTON", loc.t(Localization::AddDialogChangeButton).c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            S(326), S(136), S(94), S(24), hwnd, reinterpret_cast<HMENU>(IDC_CHANGE_FOLDER), hInst, nullptr);
        SendMessageW(st->changeFolderBtn, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

        makeStatic(loc.t(Localization::AddDialogHint), S(20), S(168), S(400), S(32), smallFont());

        st->cancelBtn = CreateWindowExW(0, L"BUTTON", loc.t(Localization::AddDialogCancel).c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            S(236), S(216), S(90), S(28), hwnd, reinterpret_cast<HMENU>(IDC_CANCEL), hInst, nullptr);
        SendMessageW(st->cancelBtn, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

        st->addBtn = CreateWindowExW(0, L"BUTTON", loc.t(Localization::AddDialogConfirm).c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            S(330), S(216), S(90), S(28), hwnd, reinterpret_cast<HMENU>(IDC_ADD), hInst, nullptr);
        SendMessageW(st->addBtn, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

        SetFocus(st->magnetEdit);
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_CHOOSE_FILE: pickFile(st); return 0;
        case IDC_CHANGE_FOLDER: pickFolder(st); return 0;
        case IDC_ADD: confirmAdd(st); return 0;
        case IDC_CANCEL: st->done = true; return 0;
        }
        return 0;
    case WM_CLOSE:
        st->done = true;
        return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { st->done = true; return 0; }
        if (wParam == VK_RETURN) { confirmAdd(st); return 0; }
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

void registerClassOnce(HINSTANCE hInst)
{
    static bool registered = false;
    if (registered)
        return;
    WNDCLASSW wc{};
    wc.lpfnWndProc = dialogProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    wc.lpszClassName = kClassName;
    RegisterClassW(&wc);
    registered = true;
}

} // namespace

bool showAddTorrentDialog(HWND owner, HINSTANCE hInst, AddTorrentResult* result)
{
    registerClassOnce(hInst);

    AppSettings settings;
    DialogState st;
    st.saveDir = settings.downloadDir();

    const Localization& loc = Localization::instance();

    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    const int w = S(440), h = S(284);
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - w) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - h) / 2;

    EnableWindow(owner, FALSE);

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kClassName, loc.t(Localization::AddDialogTitle).c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, w, h, owner, nullptr, hInst, &st);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (!st.done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (st.accepted && result) {
        wchar_t buf[4096];
        GetWindowTextW(st.magnetEdit, buf, 4096); // must read before DestroyWindow below
        result->source = buf;
        size_t b = result->source.find_first_not_of(L" \t\r\n");
        size_t e = result->source.find_last_not_of(L" \t\r\n");
        result->source = (b == std::wstring::npos) ? L"" : result->source.substr(b, e - b + 1);
        result->saveDir = st.saveDir;
    }

    DestroyWindow(hwnd);
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);

    return st.accepted;
}
