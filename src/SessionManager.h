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
    std::wstring state;              // internal tag: "Checking", "Metadata", "Downloading", "Seeding"...
    std::wstring savePath;           // folder the torrent's files are saved in
    bool isFinished = false;         // download complete (may still be seeding)
    bool isPaused = false;
    bool hasError = false;
    bool isChecking = false;         // verifying files on disk
    bool needsMetadata = false;      // magnet link still fetching its metadata
    std::wstring errorMessage;
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
    bool addTorrent(const std::wstring& source, const std::wstring& saveDir, std::wstring* errorOut = nullptr);

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
    // Torrents with an error never count as finished -- see anyErrors().
    bool allDownloadsFinished(bool waitForSeeding) const;
    bool anyErrors() const;

    // Re-adds the torrents saved by a previous run (call once at start-up).
    void restoreSaved();
    // Writes every torrent's resume data to disk, waiting up to timeoutMs
    // for libtorrent to produce it. Called on exit and before shutdown.
    void saveAll(int timeoutMs);

    // Call once a second (driven by the main window's timer) to pump
    // libtorrent's alert queue and refresh m_torrents.
    void tick();

    // Fired whenever the torrent list or any torrent's stats changed.
    std::function<void()> onTorrentsChanged;
    // Fired once per torrent when its download completes.
    std::function<void(const std::wstring& name)> onTorrentFinished;
    // Fired when an add request was rejected as a duplicate / failed.
    std::function<void(bool duplicate, const std::wstring& message)> onAddFailed;

private:
    void applySessionDefaults();
    void pollAlerts();
    void handleSaveAlert(lt::alert* a);
    void requestSave(const lt::torrent_handle& h, bool onlyIfModified);

    std::wstring m_stateDir;
    int m_ticksSinceSave = 0;

    std::unique_ptr<lt::session> m_session;
    std::vector<TorrentInfo> m_torrents;
};
