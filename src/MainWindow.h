#pragma once

#include "Win32Util.h"
#include "SessionManager.h"
#include "ShutdownManager.h"
#include "AppSettings.h"
#include "TorrentListView.h"
#include "CountdownDialog.h"
#include <vector>
#include <shellapi.h> // HDROP

// The app's single window. Plain Win32: no Qt, no moc, no signals/slots --
// every reaction (button clicks, selection changes, timer ticks) is
// dispatched from one WndProc, and SessionManager/ShutdownManager are
// driven by std::function callbacks exactly as they were in the Qt build.
// Window class of the main window; the second process looks it up to hand
// over a .torrent / magnet argument (see main.cpp).
constexpr wchar_t kMainWindowClass[] = L"LiteTorrentMainWindow";
// dwData tag of the WM_COPYDATA message that carries that argument.
constexpr ULONG_PTR kCopyDataOpen = 0x4C544F50; // "LTOP"

class MainWindow
{
public:
    bool create(HINSTANCE hInst);
    void show(int nCmdShow);

    HWND hwnd() const { return m_hwnd; }

    // For a .torrent path (or magnet link) handed to the process on the
    // command line -- Windows launches the app this way when a .torrent
    // file is double-clicked or opened with "Open with -> LiteTorrent".
    void openFromCommandLine(const std::wstring& source);

private:
    static LRESULT CALLBACK staticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT wndProc(UINT msg, WPARAM wParam, LPARAM lParam);

    void onCreate(HWND hwnd);
    void onSize(int width, int height);
    void onCommand(int id, int notifyCode, HWND ctrl);
    LRESULT onNotify(NMHDR* hdr);
    void onDropFiles(HDROP hDrop);
    bool onDrawItem(DRAWITEMSTRUCT* dis);

    void onDpiChanged(int newDpi, const RECT* suggested);
    void onCopyData(const COPYDATASTRUCT* cds);
    void onDeferred();
    void flushState(int timeoutMs);

    void onAddClicked();
    void onPauseResumeClicked();
    void onDeleteClicked();
    void onOpenFolderClicked();
    void onTorrentsChanged();
    void onLanguageButtonClicked(bool english);

    void onCountdownStarted(int seconds);
    void onCountdownTick(int secondsLeft);
    void onCountdownCancelled();

    void addSource(const std::wstring& source, const std::wstring& saveDir);
    void updateStatusBar();
    void updateToolbarEnabledState();
    void retranslateUi();
    void checkShutdownCondition();
    void layoutControls();

    HWND m_hwnd = nullptr;
    HINSTANCE m_hInst = nullptr;

    SessionManager m_session;
    ShutdownManager m_shutdown;
    AppSettings m_settings;

    TorrentListView m_list;
    CountdownDialog m_countdown;

    // Work queued from callbacks that must not show UI or add torrents
    // while libtorrent's alert list is being walked.
    std::vector<std::wstring> m_pendingOpen;
    std::vector<std::pair<bool, std::wstring>> m_pendingErrors; // {isDuplicate, text}

    HWND m_addBtn = nullptr;
    HWND m_pauseResumeBtn = nullptr;
    HWND m_deleteBtn = nullptr;
    HWND m_openFolderBtn = nullptr;

    HWND m_langRuBtn = nullptr;
    HWND m_langEnBtn = nullptr;

    HWND m_statusStatic = nullptr;
    HWND m_shutdownCheck = nullptr;
};
