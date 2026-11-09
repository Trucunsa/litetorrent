#include "SessionManager.h"
#include "Win32Util.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <chrono>

#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/error_code.hpp>
#include <libtorrent/read_resume_data.hpp>
#include <libtorrent/write_resume_data.hpp>
#include <libtorrent/torrent_flags.hpp>

namespace {

std::wstring stateToString(lt::torrent_status::state_t s, bool paused, bool isFinished)
{
    // Internal tags only; the UI maps them to localized text.
    if (paused)
        return L"Paused";
    switch (s) {
    case lt::torrent_status::checking_files:
    case lt::torrent_status::checking_resume_data:
        return L"Checking";
    case lt::torrent_status::downloading_metadata:
        return L"Metadata";
    case lt::torrent_status::downloading:
        return L"Downloading";
    case lt::torrent_status::finished:
        return L"Finished";
    case lt::torrent_status::seeding:
        return L"Seeding";
    default:
        return isFinished ? L"Finished" : L"Idle";
    }
}

// Hex form of the info hash, used as the row key. (The raw 20 bytes are
// not valid UTF-8, so pushing them through a text conversion would be
// lossy and could make two different torrents look identical.)
std::wstring hashKey(const lt::sha1_hash& h)
{
    static const wchar_t digits[] = L"0123456789abcdef";
    std::wstring out;
    out.reserve(40);
    for (auto it = h.begin(); it != h.end(); ++it) {
        const unsigned char b = static_cast<unsigned char>(*it);
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 15]);
    }
    return out;
}

bool startsWithMagnet(const std::wstring& s)
{
    const std::wstring prefix = L"magnet:";
    if (s.size() < prefix.size())
        return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (towlower(s[i]) != prefix[i])
            return false;
    }
    return true;
}

} // namespace

SessionManager::SessionManager()
{
    m_stateDir = win::dataDir() + L"\\torrents";
    std::error_code ec;
    std::filesystem::create_directories(m_stateDir, ec);
    applySessionDefaults();
}

SessionManager::~SessionManager() = default;

void SessionManager::applySessionDefaults()
{
    lt::settings_pack pack;

    // Reasonable, invisible-to-the-user defaults. Nothing here is
    // exposed in the UI -- the whole point of "simple" is that the
    // user never has to look at this.
    pack.set_str(lt::settings_pack::user_agent, "LiteTorrent/1.1.0");
    pack.set_bool(lt::settings_pack::enable_dht, true);
    pack.set_bool(lt::settings_pack::enable_lsd, true);
    pack.set_bool(lt::settings_pack::enable_upnp, true);
    pack.set_bool(lt::settings_pack::enable_natpmp, true);
    pack.set_int(lt::settings_pack::alert_mask,
                 lt::alert_category::status |
                 lt::alert_category::error |
                 lt::alert_category::storage);
    // When TCP and uTP connections compete for the upload limit, let TCP
    // keep its share rather than throttling it proportionally.
    pack.set_int(lt::settings_pack::mixed_mode_algorithm, lt::settings_pack::prefer_tcp);

    lt::session_params params(pack);
    m_session = std::make_unique<lt::session>(params);
}

bool SessionManager::addTorrent(const std::wstring& source, const std::wstring& saveDir, std::wstring* errorOut)
{
    std::error_code fsEc;
    std::filesystem::create_directories(saveDir, fsEc);

    lt::add_torrent_params atp;
    atp.save_path = win::wideToUtf8(saveDir);

    lt::error_code ec;

    if (startsWithMagnet(source)) {
        atp = lt::parse_magnet_uri(win::wideToUtf8(source), ec);
        if (ec) {
            if (errorOut) *errorOut = win::utf8ToWide(ec.message());
            return false;
        }
        atp.save_path = win::wideToUtf8(saveDir);
    } else {
        // Local .torrent file.
        auto ti = std::make_shared<lt::torrent_info>(win::wideToUtf8(source), ec);
        if (ec) {
            if (errorOut) *errorOut = win::utf8ToWide(ec.message());
            return false;
        }
        atp.ti = ti;
    }

    m_session->async_add_torrent(std::move(atp));
    return true;
}

void SessionManager::removeTorrent(const std::wstring& infoHash, bool deleteFiles)
{
    for (auto& t : m_torrents) {
        if (t.infoHash == infoHash) {
            lt::remove_flags_t flags{};
            if (deleteFiles)
                flags |= lt::session::delete_files;
            m_session->remove_torrent(t.handle, flags);
            std::error_code ec;
            std::filesystem::remove(std::filesystem::path(m_stateDir) / (t.infoHash + L".resume"), ec);
            break;
        }
    }
}

void SessionManager::pauseTorrent(const std::wstring& infoHash)
{
    for (auto& t : m_torrents) {
        if (t.infoHash == infoHash) {
            t.handle.pause();
            break;
        }
    }
}

void SessionManager::resumeTorrent(const std::wstring& infoHash)
{
    for (auto& t : m_torrents) {
        if (t.infoHash == infoHash) {
            t.handle.resume();
            break;
        }
    }
}

void SessionManager::pauseAll()
{
    m_session->pause();
}

void SessionManager::setDownloadLimitKBs(int kbs)
{
    lt::settings_pack pack;
    pack.set_int(lt::settings_pack::download_rate_limit, kbs > 0 ? kbs * 1000 : 0);
    m_session->apply_settings(pack);
}

void SessionManager::setUploadLimitKBs(int kbs)
{
    lt::settings_pack pack;
    pack.set_int(lt::settings_pack::upload_rate_limit, kbs > 0 ? kbs * 1000 : 0);
    m_session->apply_settings(pack);
}

int SessionManager::totalDownloadRateBps() const
{
    int sum = 0;
    for (const auto& t : m_torrents) sum += t.downloadRateBps;
    return sum;
}

int SessionManager::totalUploadRateBps() const
{
    int sum = 0;
    for (const auto& t : m_torrents) sum += t.uploadRateBps;
    return sum;
}

bool SessionManager::allDownloadsFinished(bool waitForSeeding) const
{
    if (m_torrents.empty())
        return false; // nothing to call "finished" -- caller treats empty queue as idle

    for (const auto& t : m_torrents) {
        if (t.hasError)
            return false; // a broken torrent is not "finished": never shut down over it
        if (!t.isFinished)
            return false;
        if (waitForSeeding && t.state == L"Seeding")
            return false;
    }
    return true;
}

bool SessionManager::anyErrors() const
{
    for (const auto& t : m_torrents)
        if (t.hasError)
            return true;
    return false;
}

void SessionManager::tick()
{
    m_session->post_torrent_updates();
    pollAlerts();

    // Every minute, save resume data of torrents that changed.
    if (++m_ticksSinceSave >= 60) {
        m_ticksSinceSave = 0;
        for (const auto& t : m_torrents)
            requestSave(t.handle, true);
    }
}

void SessionManager::requestSave(const lt::torrent_handle& h, bool onlyIfModified)
{
    if (!h.is_valid())
        return;
    lt::resume_data_flags_t flags = lt::torrent_handle::save_info_dict;
    if (onlyIfModified)
        flags |= lt::torrent_handle::only_if_modified;
    h.save_resume_data(flags);
}

void SessionManager::handleSaveAlert(lt::alert* a)
{
    auto* sr = lt::alert_cast<lt::save_resume_data_alert>(a);
    if (!sr || !sr->handle.is_valid())
        return;
    const std::wstring name = hashKey(sr->handle.info_hashes().get_best()) + L".resume";
    const std::vector<char> buf = lt::write_resume_data_buf(sr->params);
    const auto finalPath = std::filesystem::path(m_stateDir) / name;
    const auto tmpPath = std::filesystem::path(m_stateDir) / (name + L".tmp");
    {
        std::ofstream f(tmpPath, std::ios::binary | std::ios::trunc);
        if (!f)
            return;
        f.write(buf.data(), static_cast<std::streamsize>(buf.size()));
        if (!f)
            return;
    }
    std::error_code ec;
    std::filesystem::rename(tmpPath, finalPath, ec); // atomic replace on the same volume
    if (ec) {
        std::filesystem::remove(finalPath, ec);
        std::filesystem::rename(tmpPath, finalPath, ec);
    }
}

void SessionManager::restoreSaved()
{
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(m_stateDir, ec)) {
        if (entry.path().extension() != L".resume")
            continue;
        std::ifstream f(entry.path(), std::ios::binary);
        if (!f)
            continue;
        std::vector<char> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        lt::error_code lec;
        lt::add_torrent_params atp = lt::read_resume_data(data, lec);
        if (lec || atp.save_path.empty())
            continue;
        m_session->async_add_torrent(std::move(atp));
    }
}

void SessionManager::saveAll(int timeoutMs)
{
    int pending = 0;
    for (const lt::torrent_handle& h : m_session->get_torrents()) {
        if (!h.is_valid())
            continue;
        requestSave(h, false);
        ++pending;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (pending > 0 && std::chrono::steady_clock::now() < deadline) {
        m_session->wait_for_alert(lt::milliseconds(100));
        std::vector<lt::alert*> alerts;
        m_session->pop_alerts(&alerts);
        for (lt::alert* a : alerts) {
            if (lt::alert_cast<lt::save_resume_data_alert>(a)) {
                handleSaveAlert(a);
                --pending;
            } else if (lt::alert_cast<lt::save_resume_data_failed_alert>(a)) {
                --pending;
            }
        }
    }
}

void SessionManager::pollAlerts()
{
    std::vector<lt::alert*> alerts;
    m_session->pop_alerts(&alerts);

    bool changed = false;

    for (lt::alert* a : alerts) {
        if (auto* su = lt::alert_cast<lt::state_update_alert>(a)) {
            for (const lt::torrent_status& st : su->status) {
                changed = true;
                std::wstring hash = hashKey(st.info_hashes.get_best());

                auto it = std::find_if(m_torrents.begin(), m_torrents.end(),
                                        [&](const TorrentInfo& t) { return t.infoHash == hash; });
                if (it == m_torrents.end()) {
                    m_torrents.push_back(TorrentInfo{});
                    it = m_torrents.end() - 1;
                    it->infoHash = hash;
                }

                TorrentInfo& t = *it;
                t.handle = st.handle;
                t.name = win::utf8ToWide(st.name);
                t.totalSize = st.total_wanted;
                t.downloaded = st.total_wanted_done;
                t.progress = st.progress;
                t.downloadRateBps = st.download_payload_rate;
                t.uploadRateBps = st.upload_payload_rate;
                t.numPeers = st.num_peers;
                t.numSeeds = st.num_seeds;
                t.isPaused = static_cast<bool>(st.flags & lt::torrent_flags::paused);
                t.isFinished = (st.state == lt::torrent_status::finished ||
                                st.state == lt::torrent_status::seeding);
                t.hasError = st.errc ? true : false;
                t.errorMessage = st.errc ? win::utf8ToWide(st.errc.message()) : std::wstring();
                t.state = stateToString(st.state, t.isPaused, t.isFinished);
                t.savePath = win::utf8ToWide(st.save_path);
                t.isChecking = (st.state == lt::torrent_status::checking_files ||
                                st.state == lt::torrent_status::checking_resume_data);
                t.needsMetadata = (st.state == lt::torrent_status::downloading_metadata);
            }
        } else if (auto* fin = lt::alert_cast<lt::torrent_finished_alert>(a)) {
            if (onTorrentFinished) {
                // torrent_alert::torrent_name() is deprecated; look the
                // name up from our own already-tracked snapshot instead.
                std::wstring name;
                for (const auto& t : m_torrents) {
                    if (t.handle == fin->handle) { name = t.name; break; }
                }
                onTorrentFinished(name);
            }
            changed = true;
        } else if (auto* rem = lt::alert_cast<lt::torrent_removed_alert>(a)) {
            // Make sure no late resume-data write leaves a stale file behind.
            std::error_code ec;
            std::filesystem::remove(std::filesystem::path(m_stateDir) /
                                    (hashKey(rem->info_hashes.get_best()) + L".resume"), ec);
            changed = true;
        } else if (auto* added = lt::alert_cast<lt::add_torrent_alert>(a)) {
            if (added->error) {
                if (onAddFailed) {
                    const bool dup = added->error == lt::errors::duplicate_torrent;
                    onAddFailed(dup, win::utf8ToWide(added->error.message()));
                }
            } else {
                requestSave(added->handle, false); // so a new torrent survives a crash
            }
        } else if (lt::alert_cast<lt::save_resume_data_alert>(a)) {
            handleSaveAlert(a);
        } else if (auto* err = lt::alert_cast<lt::torrent_error_alert>(a)) {
            std::wstring hash = hashKey(err->handle.info_hashes().get_best());
            for (auto& t : m_torrents) {
                if (t.infoHash == hash) {
                    t.hasError = true;
                    t.errorMessage = win::utf8ToWide(err->error.message());
                }
            }
            changed = true;
        }
    }

    // Drop entries for torrents no longer present in the session (removed).
    std::vector<lt::torrent_handle> live = m_session->get_torrents();
    m_torrents.erase(std::remove_if(m_torrents.begin(), m_torrents.end(),
                                     [&](const TorrentInfo& t) {
                                         return std::find(live.begin(), live.end(), t.handle) == live.end();
                                     }),
                      m_torrents.end());

    if (changed && onTorrentsChanged)
        onTorrentsChanged();
}
