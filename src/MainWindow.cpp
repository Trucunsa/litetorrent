#include "MainWindow.h"
#include "AddTorrentDialog.h"
#include "Localization.h"

#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>

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

    m_hwnd = CreateWindowExW(WS_EX_ACCEPTFILES, kClassName, L"LiteTorrent",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 980, 620,
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
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(m_hwnd, msg, wParam, lParam);
}

void MainWindow::onCreate(HWND hwnd)
{
    m_hwnd = hwnd;
    DragAcceptFiles(hwnd, TRUE);

    m_addBtn = makeButton(hwnd, m_hInst, L"", ID_ADD, BS_OWNERDRAW);
    m_pauseResumeBtn = makeButton(hwnd, m_hInst, L"", ID_PAUSE_RESUME);
    m_deleteBtn = makeButton(hwnd, m_hInst, L"", ID_DELETE);
    m_openFolderBtn = makeButton(hwnd, m_hInst, L"", ID_OPEN_FOLDER);

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

    Localization::instance().onLanguageChanged = [this] { retranslateUi(); };

    m_session.setDownloadLimitKBs(m_settings.downloadLimitKBs());
    m_session.setUploadLimitKBs(m_settings.uploadLimitKBs());

    retranslateUi();
    updateToolbarEnabledState();
    updateStatusBar();
    layoutControls();

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

    const int margin = 10;
    int x = margin;
    const int toolbarY = 8;
    const int btnH = 28;

    MoveWindow(m_addBtn, x, toolbarY, 96, btnH, TRUE); x += 96 + 14;
    MoveWindow(m_pauseResumeBtn, x, toolbarY, 116, btnH, TRUE); x += 116 + 6;
    MoveWindow(m_deleteBtn, x, toolbarY, 96, btnH, TRUE); x += 96 + 6;
    MoveWindow(m_openFolderBtn, x, toolbarY, 84, btnH, TRUE);

    // Language toggle, pinned to the top-right corner.
    const int langBtnW = 32, langBtnH = 22;
    int langX = w - margin - langBtnW;
    MoveWindow(m_langEnBtn, langX, toolbarY + 3, langBtnW, langBtnH, TRUE);
    langX -= langBtnW + 2;
    MoveWindow(m_langRuBtn, langX, toolbarY + 3, langBtnW, langBtnH, TRUE);

    const int listY = toolbarY + btnH + 10;
    const int bottomRowH = 30;
    const int listH = h - listY - bottomRowH - margin;
    m_list.layout(margin, listY, w - margin * 2, std::max(listH, 60));

    const int bottomY = h - bottomRowH - 4;
    const int checkW = 320;
    MoveWindow(m_shutdownCheck, w - margin - checkW, bottomY + 4, checkW, 20, TRUE);
    MoveWindow(m_statusStatic, margin, bottomY + 4, w - margin * 2 - checkW - 10, 20, TRUE);
}

void MainWindow::onCommand(int id, int notifyCode, HWND ctrl)
{
    (void)ctrl;
    switch (id) {
    case ID_ADD: if (notifyCode == BN_CLICKED) onAddClicked(); break;
    case ID_PAUSE_RESUME: if (notifyCode == BN_CLICKED) onPauseResumeClicked(); break;
    case ID_DELETE: if (notifyCode == BN_CLICKED) onDeleteClicked(); break;
    case ID_OPEN_FOLDER: if (notifyCode == BN_CLICKED) onOpenFolderClicked(); break;
    case ID_LANG_RU: if (notifyCode == BN_CLICKED) onLanguageButtonClicked(false); break;
    case ID_LANG_EN: if (notifyCode == BN_CLICKED) onLanguageButtonClicked(true); break;
    case ID_SHUTDOWN_CHECK:
        if (notifyCode == BN_CLICKED) {
            const bool checked = SendMessageW(m_shutdownCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            m_settings.setShutdownWhenFinished(checked);
            if (checked)
                checkShutdownCondition();
            else
                m_shutdown.cancel();
        }
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
    if (!m_session.addTorrent(source, saveDir, &error)) {
        MessageBoxW(m_hwnd, error.c_str(), Localization::instance().t(Localization::CouldntAddTorrentTitle).c_str(),
                    MB_OK | MB_ICONWARNING);
        return;
    }
    // Adding a torrent while a shutdown countdown is running should cancel it --
    // the user clearly isn't done yet.
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

    // TASKDIALOG-free custom-button prompt via a small modal MessageBox
    // substitute: MessageBox's own Yes/No/Cancel labels are OS-localized,
    // which drifts out of sync with the app's own RU/EN toggle, so build
    // the "Yes, with files / No / Cancel" choice explicitly with
    // MB_YESNOCANCEL but relabel by hand isn't possible via MessageBoxW --
    // so route Yes = delete files, No = keep files, and phrase the
    // question text itself to spell out what each button means.
    std::wstring text = loc.t(Localization::RemoveFilesQuestion) + L"\n\n" +
        loc.t(Localization::YesButton) + L" / " + loc.t(Localization::NoButton);

    const int choice = MessageBoxW(m_hwnd, text.c_str(), loc.t(Localization::RemoveFilesQuestionTitle).c_str(),
                                    MB_YESNOCANCEL | MB_ICONQUESTION);
    if (choice != IDYES && choice != IDNO)
        return;

    m_session.removeTorrent(hash, choice == IDYES);
}

void MainWindow::onOpenFolderClicked()
{
    ShellExecuteW(m_hwnd, L"open", m_settings.downloadDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::updateToolbarEnabledState()
{
    const TorrentInfo* info = m_list.selected();
    const bool hasSelection = info != nullptr;

    EnableWindow(m_pauseResumeBtn, hasSelection);
    EnableWindow(m_deleteBtn, hasSelection);
    EnableWindow(m_openFolderBtn, hasSelection);

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
    if (m_session.allDownloadsFinished(m_settings.waitForSeedingToFinish()))
        m_shutdown.arm();
    else
        m_shutdown.cancel();
}

void MainWindow::onCountdownStarted(int seconds)
{
    std::wstring text = Localization::instance().t(Localization::ShuttingDownMessage);
    const size_t pos = text.find(L"@SECONDS@");
    if (pos != std::wstring::npos) text.replace(pos, 9, std::to_wstring(seconds));
    SetWindowTextW(m_statusStatic, text.c_str());
}

void MainWindow::onCountdownTick(int secondsLeft)
{
    std::wstring text = Localization::instance().t(Localization::ShuttingDownMessageWithHint);
    const size_t pos = text.find(L"@SECONDS@");
    if (pos != std::wstring::npos) text.replace(pos, 9, std::to_wstring(secondsLeft));
    SetWindowTextW(m_statusStatic, text.c_str());
}

void MainWindow::onCountdownCancelled()
{
    updateStatusBar();
}
