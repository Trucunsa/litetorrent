#include "SessionManager.h"
#include "Win32Util.h"

#include <algorithm>
#include <filesystem>

#include <libtorrent/alert_types.hpp>
#include <libtorrent/magnet_uri.hpp>
#include <libtorrent/add_torrent_params.hpp>
#include <libtorrent/torrent_info.hpp>
#include <libtorrent/session_params.hpp>
#include <libtorrent/settings_pack.hpp>
#include <libtorrent/error_code.hpp>

namespace {

std::wstring stateToString(lt::torrent_status::state_t s, bool paused, bool isFinished)
{
    if (paused)
        return L"Paused"; // placeholder -- state text is looked up fresh from Localization by the UI layer
    switch (s) {
    case lt::torrent_status::checking_files:
        return L"Checking";
    case lt::torrent_status::downloading_metadata:
        return L"Fetching metadata";
    case lt::torrent_status::downloading:
        return L"Downloading";
    case lt::torrent_status::finished:
        return L"Finished";
    case lt::torrent_status::seeding:
        return L"Seeding";
    case lt::torrent_status::checking_resume_data:
        return L"Checking";
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
    applySessionDefaults();
}

SessionManager::~SessionManager() = default;

void SessionManager::applySessionDefaults()
{
    lt::settings_pack pack;

    // Reasonable, invisible-to-the-user defaults. Nothing here is
    // exposed in the UI -- the whole point of "simple" is that the
    // user never has to look at this.
    pack.set_str(lt::settings_pack::user_agent, "LiteTorrent/0.1.0");
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
            continue; // don't let a broken torrent block shutdown forever
        if (!t.isFinished)
            return false;
        if (waitForSeeding && t.state == L"Seeding")
            return false;
    }
    return true;
}

void SessionManager::tick()
{
    m_session->post_torrent_updates();
    pollAlerts();
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
        } else if (lt::alert_cast<lt::torrent_removed_alert>(a)) {
            // Handled by letting the next state_update prune below.
            changed = true;
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
