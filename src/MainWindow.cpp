#include "MainWindow.h"
#include "AddTorrentDialog.h"
#include "Localization.h"
#include "FilesDialog.h"
#include "Associations.h"

#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <iterator>

namespace {
constexpr wchar_t kClassName[] = L"LiteTorrentMainWindow";

constexpr int ID_ADD = 201;
constexpr int ID_PAUSE_RESUME = 202;
constexpr int ID_DELETE = 203;
constexpr int ID_OPEN_FOLDER = 204;
constexpr int ID_LANG_RU = 205;
constexpr int ID_LANG_EN = 206;
constexpr int ID_SHUTDOWN_CHECK = 207;
constexpr int ID_LISTVIEW = 208;
constexpr int ID_SHUTDOWN_CANCEL = 209;
constexpr int ID_FILES = 210;
constexpr int ID_MORE = 211;

// "More" menu: download limits 300+i, upload limits 400+i, association 500.
constexpr int ID_MENU_DOWN = 300;
constexpr int ID_MENU_UP = 400;
constexpr int ID_MENU_ASSOC = 500;
constexpr int kDownPresets[] = {0, 512, 1024, 2048, 5120, 10240, 20480};   // KiB/s
constexpr int kUpPresets[] = {0, 64, 128, 256, 512, 1024, 2048};           // KiB/s

constexpr UINT WM_APP_DEFERRED = WM_APP + 1;

constexpr wchar_t kPromptClassName[] = L"LiteTorrentShutdownPrompt";

constexpr int ID_TIMER_TICK = 1;

constexpr COLORREF kAccent = RGB(0x4F, 0x46, 0xE5);
constexpr COLORREF kAccentWeak = RGB(0xEE, 0xEE, 0xFD);
constexpr COLORREF kAccentText = RGB(0x43, 0x38, 0xCA);
constexpr COLORREF kTextTertiary = RGB(0x9A, 0xA0, 0xAC);
constexpr COLORREF kBorder = RGB(0xD4, 0xD4, 0xD8);

HWND makeButton(HWND parent, HINSTANCE hInst, const std::wstring& text, int id, DWORD extraStyle = 0)
{
    HWND h = CreateWindowExW(0, L"BUTTON", text.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | extraStyle,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);
    SendMessageW(h, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);
    return h;
}

// "2 MB/s", "512 KB/s" -- the same 1024-based units the list shows.
std::wstring limitLabel(int kib)
{
    if (kib >= 1024 && kib % 1024 == 0)
        return std::to_wstring(kib / 1024) + L" MB/s";
    return std::to_wstring(kib) + L" KB/s";
}

std::wstring withSeconds(Localization::Key key, int seconds)
{
    std::wstring text = Localization::instance().t(key);
    const size_t pos = text.find(L"@SECONDS@");
    if (pos != std::wstring::npos)
        text.replace(pos, 9, std::to_wstring(seconds));
    return text;
}

// The countdown window has no logic of its own: its button and its close
// box are both reported to the main window as "cancel the shutdown".
LRESULT CALLBACK promptProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_COMMAND:
    case WM_CLOSE:
        PostMessageW(GetWindow(hwnd, GW_OWNER), WM_COMMAND, MAKEWPARAM(ID_SHUTDOWN_CANCEL, BN_CLICKED), 0);
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
}

const wchar_t* MainWindow::className()
{
    return kClassName;
}

bool MainWindow::create(HINSTANCE hInst)
{
    m_hInst = hInst;

    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    WNDCLASSW wc{};
    wc.lpfnWndProc = staticWndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClassName;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(100));
    RegisterClassW(&wc);

    WNDCLASSW pc{};
    pc.lpfnWndProc = promptProc;
    pc.hInstance = hInst;
    pc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    pc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    pc.lpszClassName = kPromptClassName;
    pc.hIcon = wc.hIcon;
    RegisterClassW(&pc);

    // Best guess before the window exists; corrected in onCreate() for
    // the monitor the window actually lands on.
    win::currentDpi() = static_cast<int>(GetDpiForSystem());

    m_hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, kClassName, L"LiteTorrent",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, win::px(980), win::px(620),
        nullptr, nullptr, hInst, this);

    return m_hwnd != nullptr;
}

void MainWindow::show(int nCmdShow)
{
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
}

LRESULT CALLBACK MainWindow::staticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hwnd = hwnd;
    } else {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self)
        return self->wndProc(msg, wParam, lParam);
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT MainWindow::wndProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        onCreate(m_hwnd);
        return 0;
    case WM_SIZE:
        onSize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_COMMAND:
        onCommand(LOWORD(wParam), HIWORD(wParam), reinterpret_cast<HWND>(lParam));
        return 0;
    case WM_NOTIFY:
        return onNotify(reinterpret_cast<NMHDR*>(lParam));
    case WM_DRAWITEM:
        if (onDrawItem(reinterpret_cast<DRAWITEMSTRUCT*>(lParam)))
            return TRUE;
        return FALSE;
    case WM_DROPFILES:
        onDropFiles(reinterpret_cast<HDROP>(wParam));
        return 0;
    case WM_TIMER:
        if (wParam == ID_TIMER_TICK) {
            m_session.tick();
            if (m_shutdown.isActive())
                m_shutdown.tick();
        }
        return 0;
    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(m_hwnd, &rc);
        FillRect(reinterpret_cast<HDC>(wParam), &rc, GetSysColorBrush(COLOR_BTNFACE));
        return 1;
    }
    case WM_DPICHANGED: {
        // Moved to a monitor with a different scale factor (or the user
        // changed it): new fonts, Windows' suggested window size, relayout.
        const bool promptWasOpen = m_shutdownPrompt != nullptr;
        closeShutdownPrompt(); // it is rebuilt below at the new scale
        win::setDpi(m_hwnd, HIWORD(wParam));
        if (promptWasOpen && m_shutdown.isActive())
            showShutdownPrompt(m_shutdown.secondsLeft());
        const RECT* r = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(m_hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        layoutControls();
        InvalidateRect(m_hwnd, nullptr, TRUE);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = win::px(780);
        mmi->ptMinTrackSize.y = win::px(320);
        return 0;
    }
    case WM_COPYDATA: {
        // A second copy of the app was started (typically by opening a
        // .torrent in Explorer) and is handing us its argument.
        const auto* cds = reinterpret_cast<const COPYDATASTRUCT*>(lParam);
        bringToFront();
        if (cds && cds->dwData == 1 && cds->lpData && cds->cbData >= sizeof(wchar_t)) {
            const wchar_t* text = static_cast<const wchar_t*>(cds->lpData);
            const size_t maxLen = cds->cbData / sizeof(wchar_t);
            size_t len = 0;
            while (len < maxLen && text[len] != L'\0')
                ++len;
            // Added after this message returns, so the other copy is not
            // kept waiting behind a possible message box.
            if (len > 0) {
                m_pendingSources.emplace_back(text, len);
                postDeferred();
            }
        }
        return TRUE;
    }
    case WM_APP_DEFERRED:
        runDeferred();
        return 0;
    case WM_ENDSESSION:
        // Windows is shutting down or the user is logging off (this is
        // also how our own shutdown feature ends): save before we go.
        if (wParam)
            m_session.saveAllAndWait();
        return 0;
    case WM_DESTROY:
        KillTimer(m_hwnd, ID_TIMER_TICK);
        closeShutdownPrompt();
        m_session.saveAllAndWait();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void MainWindow::onCreate(HWND hwnd)
{
    m_hwnd = hwnd;
    win::setDpi(nullptr, static_cast<int>(GetDpiForWindow(hwnd)));
    DragAcceptFiles(hwnd, TRUE);

    m_addBtn = makeButton(hwnd, m_hInst, L"", ID_ADD, BS_OWNERDRAW);
    m_pauseResumeBtn = makeButton(hwnd, m_hInst, L"", ID_PAUSE_RESUME);
    m_deleteBtn = makeButton(hwnd, m_hInst, L"", ID_DELETE);
    m_openFolderBtn = makeButton(hwnd, m_hInst, L"", ID_OPEN_FOLDER);
    m_filesBtn = makeButton(hwnd, m_hInst, L"", ID_FILES);
    m_moreBtn = makeButton(hwnd, m_hInst, L"", ID_MORE);

    m_langRuBtn = makeButton(hwnd, m_hInst, L"RU", ID_LANG_RU, BS_OWNERDRAW);
    m_langEnBtn = makeButton(hwnd, m_hInst, L"EN", ID_LANG_EN, BS_OWNERDRAW);

    m_list.create(hwnd, m_hInst, ID_LISTVIEW);

    m_statusStatic = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT,
        0, 0, 0, 0, hwnd, nullptr, m_hInst, nullptr);
    SendMessageW(m_statusStatic, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

    m_shutdownCheck = CreateWindowExW(0, L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
        0, 0, 0, 0, hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SHUTDOWN_CHECK)), m_hInst, nullptr);
    SendMessageW(m_shutdownCheck, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

    // --- Wiring (plain std::function callbacks, same pattern as before) ---
    m_session.onTorrentsChanged = [this] { onTorrentsChanged(); };

    m_shutdown.onCountdownStarted = [this](int s) { onCountdownStarted(s); };
    m_shutdown.onCountdownTick = [this](int s) { onCountdownTick(s); };
    m_shutdown.onCountdownCancelled = [this] { onCountdownCancelled(); };
    // Last moment before ExitWindowsEx: make sure progress is on disk.
    m_shutdown.onShutdownInitiated = [this] {
        closeShutdownPrompt();
        m_session.saveAllAndWait();
    };

    // Called from inside the alert loop: a message box there would run a
    // nested message loop whose timer re-enters that very loop. Defer it.
    m_session.onAddFailed = [this](bool duplicate, const std::wstring& message) {
        m_pendingErrors.emplace_back(duplicate, message);
        postDeferred();
    };

    Localization::instance().onLanguageChanged = [this] { retranslateUi(); };

    m_session.setDownloadLimitKBs(m_settings.downloadLimitKBs());
    m_session.setUploadLimitKBs(m_settings.uploadLimitKBs());

    retranslateUi();
    updateToolbarEnabledState();
    updateStatusBar();
    layoutControls();

    m_session.restoreSavedTorrents();

    // .torrent / magnet handling: refresh the registration (the exe may have
    // moved since last time), or ask once on the very first start.
    const int assocState = m_settings.fileAssociation();
    if (assocState == 1) {
        assoc::enable();
    } else if (assocState < 0) {
        m_pendingAssocQuestion = true;
        postDeferred();
    }

    SetTimer(hwnd, ID_TIMER_TICK, 1000, nullptr);
}

void MainWindow::onSize(int width, int height)
{
    (void)width; (void)height;
    layoutControls();
}

void MainWindow::layoutControls()
{
    if (!m_hwnd)
        return;
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;

    using win::px;
    const int margin = px(10);
    int x = margin;
    const int toolbarY = px(8);
    const int btnH = px(28);

    MoveWindow(m_addBtn, x, toolbarY, px(96), btnH, TRUE); x += px(96 + 14);
    MoveWindow(m_pauseResumeBtn, x, toolbarY, px(116), btnH, TRUE); x += px(116 + 6);
    MoveWindow(m_deleteBtn, x, toolbarY, px(96), btnH, TRUE); x += px(96 + 6);
    MoveWindow(m_openFolderBtn, x, toolbarY, px(84), btnH, TRUE); x += px(84 + 6);
    MoveWindow(m_filesBtn, x, toolbarY, px(84), btnH, TRUE); x += px(84 + 14);
    MoveWindow(m_moreBtn, x, toolbarY, px(84), btnH, TRUE);

    // Language toggle, pinned to the top-right corner.
    const int langBtnW = px(32), langBtnH = px(22);
    int langX = w - margin - langBtnW;
    MoveWindow(m_langEnBtn, langX, toolbarY + px(3), langBtnW, langBtnH, TRUE);
    langX -= langBtnW + px(2);
    MoveWindow(m_langRuBtn, langX, toolbarY + px(3), langBtnW, langBtnH, TRUE);

    const int listY = toolbarY + btnH + px(10);
    const int bottomRowH = px(30);
    const int listH = h - listY - bottomRowH - margin;
    m_list.layout(margin, listY, w - margin * 2, std::max(listH, px(60)));

    const int bottomY = h - bottomRowH - px(4);
    const int checkW = px(320);
    MoveWindow(m_shutdownCheck, w - margin - checkW, bottomY + px(4), checkW, px(20), TRUE);
    MoveWindow(m_statusStatic, margin, bottomY + px(4), w - margin * 2 - checkW - px(10), px(20), TRUE);
}

void MainWindow::onCommand(int id, int notifyCode, HWND ctrl)
{
    (void)ctrl;
    switch (id) {
    case ID_ADD: if (notifyCode == BN_CLICKED) onAddClicked(); break;
    case ID_PAUSE_RESUME: if (notifyCode == BN_CLICKED) onPauseResumeClicked(); break;
    case ID_DELETE: if (notifyCode == BN_CLICKED) onDeleteClicked(); break;
    case ID_OPEN_FOLDER: if (notifyCode == BN_CLICKED) onOpenFolderClicked(); break;
    case ID_FILES: if (notifyCode == BN_CLICKED) onFilesClicked(); break;
    case ID_MORE: if (notifyCode == BN_CLICKED) onMoreClicked(); break;
    case ID_LANG_RU: if (notifyCode == BN_CLICKED) onLanguageButtonClicked(false); break;
    case ID_LANG_EN: if (notifyCode == BN_CLICKED) onLanguageButtonClicked(true); break;
    case ID_SHUTDOWN_CHECK:
        if (notifyCode == BN_CLICKED) {
            const bool checked = SendMessageW(m_shutdownCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            m_settings.setShutdownWhenFinished(checked);
            m_sawUnfinished = false;
            if (checked)
                checkShutdownCondition();
            else
                m_shutdown.cancel();
        }
        break;
    case ID_SHUTDOWN_CANCEL:
        cancelShutdownByUser();
        break;
    }
}

LRESULT MainWindow::onNotify(NMHDR* hdr)
{
    if (hdr->hwndFrom == m_list.hwnd()) {
        if (hdr->code == LVN_ITEMCHANGED) {
            updateToolbarEnabledState();
        } else if (hdr->code == NM_CUSTOMDRAW) {
            return m_list.handleCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW*>(hdr));
        }
    }
    return 0;
}

bool MainWindow::onDrawItem(DRAWITEMSTRUCT* dis)
{
    if (dis->CtlID != ID_ADD && dis->CtlID != ID_LANG_RU && dis->CtlID != ID_LANG_EN)
        return false;

    HDC dc = dis->hDC;
    RECT rc = dis->rcItem;
    const bool pressed = dis->itemState & ODS_SELECTED;
    const bool disabled = dis->itemState & ODS_DISABLED;

    wchar_t text[64];
    GetWindowTextW(dis->hwndItem, text, 64);

    SetBkMode(dc, TRANSPARENT);
    HFONT font = reinterpret_cast<HFONT>(SendMessageW(dis->hwndItem, WM_GETFONT, 0, 0));
    HFONT oldFont = font ? static_cast<HFONT>(SelectObject(dc, font)) : nullptr;

    if (dis->CtlID == ID_ADD) {
        COLORREF bg = disabled ? kTextTertiary : (pressed ? kAccentText : kAccent);
        HBRUSH brush = CreateSolidBrush(bg);
        FillRect(dc, &rc, brush);
        DeleteObject(brush);

        SetTextColor(dc, RGB(255, 255, 255));
        DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else {
        // Language toggle buttons: checked == currently-active language.
        const bool checked = (dis->CtlID == ID_LANG_RU)
            ? Localization::instance().language() == Localization::Ru
            : Localization::instance().language() == Localization::En;

        HBRUSH bgBrush = CreateSolidBrush(checked ? kAccentWeak : RGB(255, 255, 255));
        FillRect(dc, &rc, bgBrush);
        DeleteObject(bgBrush);
        HBRUSH borderBrush = CreateSolidBrush(kBorder);
        FrameRect(dc, &rc, borderBrush);
        DeleteObject(borderBrush);

        SetTextColor(dc, checked ? kAccentText : kTextTertiary);
        DrawTextW(dc, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    if (oldFont) SelectObject(dc, oldFont);
    return true;
}

void MainWindow::onDropFiles(HDROP hDrop)
{
    const UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    for (UINT i = 0; i < count; ++i) {
        wchar_t path[MAX_PATH];
        if (DragQueryFileW(hDrop, i, path, MAX_PATH))
            addSource(path, m_settings.downloadDir());
    }
    DragFinish(hDrop);
}

// --- Adding torrents -------------------------------------------------------

void MainWindow::openFromCommandLine(const std::wstring& source)
{
    addSource(source, m_settings.downloadDir());
}

void MainWindow::onAddClicked()
{
    AddTorrentResult result;
    if (showAddTorrentDialog(m_hwnd, m_hInst, &result))
        addSource(result.source, result.saveDir);
}

void MainWindow::addSource(const std::wstring& source, const std::wstring& saveDir)
{
    std::wstring error;
    bool duplicate = false;
    if (!m_session.addTorrent(source, saveDir, &error, &duplicate)) {
        const Localization& loc = Localization::instance();
        MessageBoxW(m_hwnd,
                    duplicate ? loc.t(Localization::DuplicateTorrent).c_str() : error.c_str(),
                    loc.t(Localization::CouldntAddTorrentTitle).c_str(),
                    MB_OK | (duplicate ? MB_ICONINFORMATION : MB_ICONWARNING));
        return;
    }
    // Adding a torrent while a shutdown countdown is running should cancel it --
    // the user clearly isn't done yet. The new torrent takes a moment to
    // show up in the list, so also forget that "everything was finished".
    m_sawUnfinished = false;
    m_shutdown.cancel();
}

// --- Toolbar actions on the selected row --------------------------------

void MainWindow::onPauseResumeClicked()
{
    const TorrentInfo* info = m_list.selected();
    if (!info)
        return;

    if (info->isPaused)
        m_session.resumeTorrent(info->infoHash);
    else
        m_session.pauseTorrent(info->infoHash);
    m_shutdown.cancel();
}

void MainWindow::onDeleteClicked()
{
    const TorrentInfo* info = m_list.selected();
    if (!info)
        return;
    const std::wstring hash = info->infoHash;

    const Localization& loc = Localization::instance();

    // A task dialog rather than MessageBox, so the buttons carry the app's
    // own RU/EN wording instead of whatever language Windows is set to.
    const std::wstring title = loc.t(Localization::RemoveFilesQuestionTitle);
    const std::wstring question = loc.t(Localization::RemoveFilesQuestion);
    const std::wstring yes = loc.t(Localization::YesButton);
    const std::wstring no = loc.t(Localization::NoButton);
    const std::wstring cancel = loc.t(Localization::AddDialogCancel);
    const TASKDIALOG_BUTTON buttons[] = {
        {IDYES, yes.c_str()},
        {IDNO, no.c_str()},
        {IDCANCEL, cancel.c_str()},
    };
    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = m_hwnd;
    cfg.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    cfg.pszWindowTitle = title.c_str();
    cfg.pszMainIcon = TD_WARNING_ICON;
    cfg.pszContent = question.c_str();
    cfg.cButtons = 3;
    cfg.pButtons = buttons;
    cfg.nDefaultButton = IDNO;

    int choice = IDCANCEL;
    if (FAILED(TaskDialogIndirect(&cfg, &choice, nullptr, nullptr)))
        return;
    if (choice != IDYES && choice != IDNO)
        return;

    m_session.removeTorrent(hash, choice == IDYES);
}

void MainWindow::onOpenFolderClicked()
{
    // The folder the selected torrent was actually saved to, which is not
    // necessarily the folder currently offered for new torrents.
    const TorrentInfo* info = m_list.selected();
    const std::wstring dir = (info && !info->savePath.empty()) ? info->savePath : m_settings.downloadDir();
    ShellExecuteW(m_hwnd, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::updateToolbarEnabledState()
{
    const TorrentInfo* info = m_list.selected();
    const bool hasSelection = info != nullptr;

    EnableWindow(m_pauseResumeBtn, hasSelection);
    EnableWindow(m_deleteBtn, hasSelection);
    EnableWindow(m_openFolderBtn, hasSelection);
    EnableWindow(m_filesBtn, hasSelection);

    const Localization& loc = Localization::instance();
    if (hasSelection) {
        SetWindowTextW(m_pauseResumeBtn,
            (info->isPaused ? loc.t(Localization::ResumeButton) : loc.t(Localization::PauseButton)).c_str());
    } else {
        SetWindowTextW(m_pauseResumeBtn, loc.t(Localization::PauseButton).c_str());
    }
}

// --- Task list updates -------------------------------------------------

void MainWindow::onTorrentsChanged()
{
    // TorrentListView::refresh() preserves the selected row by info hash
    // internally (see TorrentListView.cpp), same fix as the old Qt model
    // needed for its beginResetModel()/endResetModel() cycle.
    m_list.refresh(m_session.torrents());

    updateStatusBar();
    updateToolbarEnabledState();
    checkShutdownCondition();
}

void MainWindow::updateStatusBar()
{
    std::wstring down = win::formatSpeed(m_session.totalDownloadRateBps());
    std::wstring up = win::formatSpeed(m_session.totalUploadRateBps());
    // Show an active speed limit next to the speed it caps.
    auto withLimit = [](std::wstring& text, int kib) {
        if (kib <= 0)
            return;
        std::wstring tag = Localization::instance().t(Localization::LimitTag);
        const size_t p = tag.find(L"@LIMIT@");
        if (p != std::wstring::npos) tag.replace(p, 7, limitLabel(kib));
        text += L" " + tag;
    };
    withLimit(down, m_settings.downloadLimitKBs());
    withLimit(up, m_settings.uploadLimitKBs());

    std::wstring text = Localization::instance().t(Localization::StatusLine);
    const size_t downPos = text.find(L"@DOWN@");
    if (downPos != std::wstring::npos) text.replace(downPos, 6, down);
    const size_t upPos = text.find(L"@UP@");
    if (upPos != std::wstring::npos) text.replace(upPos, 4, up);
    const size_t countPos = text.find(L"@COUNT@");
    if (countPos != std::wstring::npos) text.replace(countPos, 7, std::to_wstring(m_session.torrents().size()));

    SetWindowTextW(m_statusStatic, text.c_str());

    const bool hasTasks = !m_session.torrents().empty();
    EnableWindow(m_shutdownCheck, hasTasks);
}

// --- Language -------------------------------------------------------------

void MainWindow::onLanguageButtonClicked(bool english)
{
    Localization::instance().setLanguage(english ? Localization::En : Localization::Ru);
}

void MainWindow::retranslateUi()
{
    const Localization& loc = Localization::instance();
    SetWindowTextW(m_hwnd, loc.t(Localization::AppTitle).c_str());

    SetWindowTextW(m_addBtn, loc.t(Localization::AddButton).c_str());
    SetWindowTextW(m_deleteBtn, loc.t(Localization::DeleteButton).c_str());
    SetWindowTextW(m_openFolderBtn, loc.t(Localization::OpenFolderButton).c_str());
    SetWindowTextW(m_filesBtn, loc.t(Localization::FilesButton).c_str());
    SetWindowTextW(m_moreBtn, loc.t(Localization::MoreButton).c_str());
    SetWindowTextW(m_shutdownCheck, loc.t(Localization::ShutdownCheckbox).c_str());

    InvalidateRect(m_langRuBtn, nullptr, TRUE);
    InvalidateRect(m_langEnBtn, nullptr, TRUE);

    m_list.retranslate();
    updateToolbarEnabledState(); // refreshes the Pause/Resume label too
    updateStatusBar();
}

// --- Shutdown-when-finished feature --------------------------------------

void MainWindow::checkShutdownCondition()
{
    if (SendMessageW(m_shutdownCheck, BM_GETCHECK, 0, 0) != BST_CHECKED)
        return;

    // Seeding never blocks shutdown by default -- only active downloads do.
    // A torrent that failed with an error counts as not finished.
    if (!m_session.allDownloadsFinished(m_settings.waitForSeedingToFinish())) {
        if (!m_session.torrents().empty())
            m_sawUnfinished = true;
        m_shutdown.cancel();
        return;
    }

    // Everything is finished. Only act if something was still downloading
    // after the box was ticked; ticking it over an already-complete list
    // must not switch the PC off.
    if (m_sawUnfinished)
        m_shutdown.arm();
}

void MainWindow::cancelShutdownByUser()
{
    SendMessageW(m_shutdownCheck, BM_SETCHECK, BST_UNCHECKED, 0);
    m_settings.setShutdownWhenFinished(false);
    m_sawUnfinished = false;
    m_shutdown.cancel();
    closeShutdownPrompt();
}

void MainWindow::onCountdownStarted(int seconds)
{
    SetWindowTextW(m_statusStatic, withSeconds(Localization::ShuttingDownMessage, seconds).c_str());
    showShutdownPrompt(seconds);
}

void MainWindow::onCountdownTick(int secondsLeft)
{
    SetWindowTextW(m_statusStatic, withSeconds(Localization::ShuttingDownMessageWithHint, secondsLeft).c_str());
    updateShutdownPrompt(secondsLeft);
}

void MainWindow::onCountdownCancelled()
{
    closeShutdownPrompt();
    updateStatusBar();
}

// --- The visible countdown window ------------------------------------------

void MainWindow::showShutdownPrompt(int seconds)
{
    if (m_shutdownPrompt)
        return;
    using win::px;
    const Localization& loc = Localization::instance();

    const int w = px(460), h = px(210);
    HMONITOR mon = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    const int x = mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - w) / 2;
    const int y = mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - h) / 2;

    // Topmost and centred on the screen: the whole point is that it cannot
    // be missed, even when LiteTorrent itself is minimised or covered.
    m_shutdownPrompt = CreateWindowExW(WS_EX_TOPMOST | WS_EX_DLGMODALFRAME, kPromptClassName,
        loc.t(Localization::ShutdownPromptTitle).c_str(),
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, w, h, m_hwnd, nullptr, m_hInst, nullptr);

    RECT rc; GetClientRect(m_shutdownPrompt, &rc);
    const int cw = rc.right, ch = rc.bottom;

    m_shutdownPromptText = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTER,
        px(16), px(22), cw - px(32), px(64), m_shutdownPrompt, nullptr, m_hInst, nullptr);
    SendMessageW(m_shutdownPromptText, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiLargeFont()), TRUE);

    const int bw = px(240), bh = px(38);
    m_shutdownPromptButton = CreateWindowExW(0, L"BUTTON", loc.t(Localization::ShutdownPromptCancel).c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        (cw - bw) / 2, ch - bh - px(20), bw, bh, m_shutdownPrompt,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SHUTDOWN_CANCEL)), m_hInst, nullptr);
    SendMessageW(m_shutdownPromptButton, WM_SETFONT, reinterpret_cast<WPARAM>(win::uiFont()), TRUE);

    updateShutdownPrompt(seconds);

    ShowWindow(m_shutdownPrompt, SW_SHOWNORMAL);
    SetForegroundWindow(m_shutdownPrompt);
    SetFocus(m_shutdownPromptButton);
    MessageBeep(MB_ICONWARNING);

    FLASHWINFO fi{sizeof(fi), m_hwnd, FLASHW_ALL | FLASHW_TIMERNOFG, 0, 0};
    FlashWindowEx(&fi);
}

void MainWindow::updateShutdownPrompt(int seconds)
{
    if (m_shutdownPromptText)
        SetWindowTextW(m_shutdownPromptText, withSeconds(Localization::ShutdownPromptText, seconds).c_str());
}

void MainWindow::closeShutdownPrompt()
{
    if (!m_shutdownPrompt)
        return;
    DestroyWindow(m_shutdownPrompt);
    m_shutdownPrompt = nullptr;
    m_shutdownPromptText = nullptr;
    m_shutdownPromptButton = nullptr;
}

void MainWindow::bringToFront()
{
    if (IsIconic(m_hwnd))
        ShowWindow(m_hwnd, SW_RESTORE);
    SetForegroundWindow(m_hwnd);
}

// --- Files, the "More" menu, file associations -----------------------------

void MainWindow::onFilesClicked()
{
    const TorrentInfo* info = m_list.selected();
    if (!info)
        return;
    const std::wstring hash = info->infoHash;
    const std::wstring name = info->name;
    const Localization& loc = Localization::instance();

    std::vector<TorrentFileEntry> files = m_session.files(hash);
    if (files.empty()) {
        MessageBoxW(m_hwnd, loc.t(Localization::FilesNotReady).c_str(),
                    loc.t(Localization::FilesDialogTitle).c_str(), MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (showFilesDialog(m_hwnd, m_hInst, name, files))
        m_session.setWantedFiles(hash, files);
}

void MainWindow::onMoreClicked()
{
    const Localization& loc = Localization::instance();

    auto buildLimitMenu = [](const int* presets, size_t count, int baseId, int current) {
        HMENU sub = CreatePopupMenu();
        bool matched = false;
        for (size_t i = 0; i < count; ++i) {
            const int kib = presets[i];
            const std::wstring label = kib == 0 ? Localization::instance().t(Localization::MenuNoLimit)
                                                : limitLabel(kib);
            UINT flags = MF_STRING;
            if (kib == current) { flags |= MF_CHECKED; matched = true; }
            AppendMenuW(sub, flags, static_cast<UINT_PTR>(baseId + static_cast<int>(i)), label.c_str());
            if (i == 0)
                AppendMenuW(sub, MF_SEPARATOR, 0, nullptr);
        }
        if (!matched && current > 0) {
            // A value set by hand in the registry: show it, ticked.
            AppendMenuW(sub, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(sub, MF_STRING | MF_CHECKED | MF_DISABLED, 0, limitLabel(current).c_str());
        }
        return sub;
    };

    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(
        buildLimitMenu(kDownPresets, std::size(kDownPresets), ID_MENU_DOWN, m_settings.downloadLimitKBs())),
        loc.t(Localization::MenuDownloadLimit).c_str());
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(
        buildLimitMenu(kUpPresets, std::size(kUpPresets), ID_MENU_UP, m_settings.uploadLimitKBs())),
        loc.t(Localization::MenuUploadLimit).c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (m_settings.fileAssociation() == 1 ? MF_CHECKED : 0),
                ID_MENU_ASSOC, loc.t(Localization::MenuOpenWith).c_str());

    RECT rc;
    GetWindowRect(m_moreBtn, &rc);
    const int id = static_cast<int>(TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                                   rc.left, rc.bottom, 0, m_hwnd, nullptr));
    DestroyMenu(menu); // also destroys the submenus
    if (id)
        onMenuCommand(id);
}

void MainWindow::onMenuCommand(int id)
{
    if (id >= ID_MENU_DOWN && id < ID_MENU_DOWN + static_cast<int>(std::size(kDownPresets))) {
        const int kib = kDownPresets[id - ID_MENU_DOWN];
        m_settings.setDownloadLimitKBs(kib);
        m_session.setDownloadLimitKBs(kib);
    } else if (id >= ID_MENU_UP && id < ID_MENU_UP + static_cast<int>(std::size(kUpPresets))) {
        const int kib = kUpPresets[id - ID_MENU_UP];
        m_settings.setUploadLimitKBs(kib);
        m_session.setUploadLimitKBs(kib);
    } else if (id == ID_MENU_ASSOC) {
        setFileAssociation(m_settings.fileAssociation() != 1);
    }
    updateStatusBar();
}

void MainWindow::setFileAssociation(bool on)
{
    m_settings.setFileAssociation(on ? 1 : 0);
    if (!on) {
        assoc::disable();
        return;
    }
    assoc::enable();
    if (!assoc::ownedByAnotherApp())
        return;

    // Windows keeps the user's own earlier choice; it can only be changed
    // in Settings. Say so, and offer to open the right page.
    const Localization& loc = Localization::instance();
    const std::wstring title = loc.t(Localization::AssocTitle);
    const std::wstring text = loc.t(Localization::AssocTakenText);
    const std::wstring open = loc.t(Localization::AssocOpenSettings);
    const std::wstring close = loc.t(Localization::AddDialogCancel);
    const TASKDIALOG_BUTTON buttons[] = {{IDYES, open.c_str()}, {IDCANCEL, close.c_str()}};
    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = m_hwnd;
    cfg.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    cfg.pszWindowTitle = title.c_str();
    cfg.pszMainIcon = TD_INFORMATION_ICON;
    cfg.pszContent = text.c_str();
    cfg.cButtons = 2;
    cfg.pButtons = buttons;
    cfg.nDefaultButton = IDYES;
    int choice = IDCANCEL;
    if (SUCCEEDED(TaskDialogIndirect(&cfg, &choice, nullptr, nullptr)) && choice == IDYES)
        assoc::openDefaultAppsSettings();
}

void MainWindow::askAboutFileAssociation()
{
    const Localization& loc = Localization::instance();
    const std::wstring title = loc.t(Localization::AssocTitle);
    const std::wstring text = loc.t(Localization::AssocQuestion);
    const std::wstring yes = loc.t(Localization::AssocYes);
    const std::wstring no = loc.t(Localization::AssocNo);
    const TASKDIALOG_BUTTON buttons[] = {{IDYES, yes.c_str()}, {IDNO, no.c_str()}};
    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = m_hwnd;
    cfg.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    cfg.pszWindowTitle = title.c_str();
    cfg.pszMainIcon = TD_INFORMATION_ICON;
    cfg.pszContent = text.c_str();
    cfg.cButtons = 2;
    cfg.pButtons = buttons;
    cfg.nDefaultButton = IDYES;
    int choice = IDNO;
    if (FAILED(TaskDialogIndirect(&cfg, &choice, nullptr, nullptr)))
        return; // ask again next time
    // Closing the question counts as "no": it is asked only once either way.
    setFileAssociation(choice == IDYES);
}

void MainWindow::postDeferred()
{
    if (m_deferredPosted)
        return;
    m_deferredPosted = true;
    PostMessageW(m_hwnd, WM_APP_DEFERRED, 0, 0);
}

void MainWindow::runDeferred()
{
    m_deferredPosted = false;

    std::vector<std::wstring> sources;
    sources.swap(m_pendingSources);
    for (const auto& s : sources)
        addSource(s, m_settings.downloadDir());

    std::vector<std::pair<bool, std::wstring>> errors;
    errors.swap(m_pendingErrors);
    const Localization& loc = Localization::instance();
    for (const auto& e : errors) {
        MessageBoxW(m_hwnd,
                    e.first ? loc.t(Localization::DuplicateTorrent).c_str() : e.second.c_str(),
                    loc.t(Localization::CouldntAddTorrentTitle).c_str(),
                    MB_OK | (e.first ? MB_ICONINFORMATION : MB_ICONWARNING));
    }

    if (m_pendingAssocQuestion) {
        m_pendingAssocQuestion = false;
        askAboutFileAssociation();
    }
}
