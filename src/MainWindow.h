#pragma once

#include "Win32Util.h"
#include "SessionManager.h"
#include "ShutdownManager.h"
#include "AppSettings.h"
#include "TorrentListView.h"
#include <shellapi.h> // HDROP

// The app's single window. Plain Win32: no Qt, no moc, no signals/slots --
// every reaction (button clicks, selection changes, timer ticks) is
// dispatched from one WndProc, and SessionManager/ShutdownManager are
// driven by std::function callbacks exactly as they were in the Qt build.
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

    HWND m_addBtn = nullptr;
    HWND m_pauseResumeBtn = nullptr;
    HWND m_deleteBtn = nullptr;
    HWND m_openFolderBtn = nullptr;

    HWND m_langRuBtn = nullptr;
    HWND m_langEnBtn = nullptr;

    HWND m_statusStatic = nullptr;
    HWND m_shutdownCheck = nullptr;
};
