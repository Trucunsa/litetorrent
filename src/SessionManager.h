#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>

#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>

// Snapshot of a single torrent's state, refreshed every tick from
// libtorrent alerts. Plain data so the UI can copy it cheaply.
struct TorrentInfo
{
    lt::torrent_handle handle;
    std::wstring name;
    std::wstring infoHash;
    int64_t totalSize = 0;
    int64_t downloaded = 0;
    float progress = 0.0f;           // 0..1
    int downloadRateBps = 0;
    int uploadRateBps = 0;
    int numPeers = 0;
    int numSeeds = 0;
    std::wstring state;              // human-readable: "Downloading", "Seeding", "Paused"...
    bool isFinished = false;         // download complete (may still be seeding)
    bool isPaused = false;
    bool hasError = false;
    std::wstring errorMessage;
    std::wstring savePath;           // folder this torrent's files live in
    bool isQueued = false;           // waiting its turn in the engine's queue
    bool isChecking = false;         // verifying data already on disk
    bool needsMetadata = false;      // magnet link still fetching its file list
};

// Owns the single libtorrent::session for the process. All add/remove/
// pause/resume calls and the periodic alert pump go through here; the UI
// never touches libtorrent directly.
//
// No Qt anywhere in this build: the caller (MainWindow) drives tick() from
// its own Win32 WM_TIMER handler and subscribes to changes via plain
// std::function callbacks.
class SessionManager
{
public:
    explicit SessionManager();
    ~SessionManager();

    // Accepts either a magnet link or a path to a local .torrent file.
    // On failure returns false; *duplicateOut is set when the reason is
    // that this torrent is already in the list.
    bool addTorrent(const std::wstring& source, const std::wstring& saveDir,
                    std::wstring* errorOut = nullptr, bool* duplicateOut = nullptr);

    void removeTorrent(const std::wstring& infoHash, bool deleteFiles);
    void pauseTorrent(const std::wstring& infoHash);
    void resumeTorrent(const std::wstring& infoHash);
    void pauseAll();

    const std::vector<TorrentInfo>& torrents() const { return m_torrents; }

    void setDownloadLimitKBs(int kbs);
    void setUploadLimitKBs(int kbs);

    // Aggregate stats for the status bar.
    int totalDownloadRateBps() const;
    int totalUploadRateBps() const;

    // True once every torrent that was still downloading has finished
    // (ignores pure-seeding torrents unless waitForSeeding is set).
    bool allDownloadsFinished(bool waitForSeeding) const;

    // Call once a second (driven by the main window's timer) to pump
    // libtorrent's alert queue and refresh m_torrents.
    void tick();

    // Re-adds every torrent saved by a previous run. Call once at startup.
    void restoreSavedTorrents();

    // Writes the state of every torrent to disk and waits (a few seconds
    // at most) until that is done. Call before the process exits or the
    // PC shuts down; the destructor calls it too.
    void saveAllAndWait();

    // Fired whenever the torrent list or any torrent's stats changed.
    std::function<void()> onTorrentsChanged;
    // Fired once per torrent when its download completes.
    std::function<void(const std::wstring& name)> onTorrentFinished;
    // Fired when a torrent that addTorrent() accepted is rejected later by
    // the engine. `duplicate` is true when it is already in the list.
    std::function<void(bool duplicate, const std::wstring& message)> onAddFailed;

private:
    void applySessionDefaults();
    void pollAlerts();
    void requestResumeData(bool onlyIfModified);

    // Number of save requests the engine has not answered yet.
    int m_pendingSaves = 0;
    int m_ticksSinceSave = 0;
    bool m_savedOnExit = false;

    std::unique_ptr<lt::session> m_session;
    std::vector<TorrentInfo> m_torrents;
};
