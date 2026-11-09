#include "TorrentListView.h"
#include "Localization.h"

#include <algorithm>
#include <cstdio>

namespace {
constexpr int kColWidths[TorrentListView::ColCount] = {220, 85, 110, 85, 85, 75, 70, 110};

std::wstring stateText(const TorrentInfo& t)
{
    const Localization& loc = Localization::instance();
    if (t.hasError)
        return loc.t(Localization::StateErrorPrefix) + t.errorMessage;
    if (t.isPaused)
        return loc.t(Localization::StatePaused);
    if (t.isChecking)
        return loc.t(Localization::StateChecking);
    if (t.isQueued)
        return loc.t(Localization::StateQueued);
    if (t.needsMetadata)
        return loc.t(Localization::StateMetadata);
    if (t.isFinished)
        return loc.t(Localization::StateSeeding);
    return loc.t(Localization::StateDownloading);
}

std::wstring peersText(const TorrentInfo& t)
{
    wchar_t buf[32];
    swprintf(buf, 32, L"%d (%d)", t.numPeers, t.numSeeds);
    return buf;
}

std::wstring progressText(const TorrentInfo& t)
{
    wchar_t buf[16];
    swprintf(buf, 16, L"%.1f%%", t.progress * 100.0f);
    return buf;
}
}

void TorrentListView::create(HWND parent, HINSTANCE hInst, int id)
{
    m_hwnd = CreateWindowExW(
        WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
        0, 0, 0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), hInst, nullptr);

    ListView_SetExtendedListViewStyle(m_hwnd,
        LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);

    HFONT font = reinterpret_cast<HFONT>(win::uiFont());
    SendMessageW(m_hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);

    for (int c = 0; c < ColCount; ++c) {
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = const_cast<LPWSTR>(L"");
        col.cx = win::px(kColWidths[c]);
        col.iSubItem = c;
        ListView_InsertColumn(m_hwnd, c, &col);
    }

    retranslate();
}

void TorrentListView::layout(int x, int y, int w, int h)
{
    if (!m_hwnd)
        return;
    MoveWindow(m_hwnd, x, y, w, h, TRUE);

    // Stretch the Name column to absorb whatever width the other
    // (fixed-width) columns don't use, mirroring the old
    // QHeaderView::Stretch policy on column 0.
    int fixedTotal = 0;
    for (int c = 1; c < ColCount; ++c) {
        const int cw = win::px(kColWidths[c]);
        fixedTotal += cw;
        // Re-applied on every layout so a move to a monitor with another
        // scale factor resizes the columns too.
        ListView_SetColumnWidth(m_hwnd, c, cw);
    }
    const int scrollbarAllowance = win::px(24);
    int nameWidth = w - fixedTotal - scrollbarAllowance;
    if (nameWidth < win::px(140))
        nameWidth = win::px(140);
    ListView_SetColumnWidth(m_hwnd, ColName, nameWidth);
}

void TorrentListView::retranslate()
{
    const Localization& loc = Localization::instance();
    const Localization::Key headerKeys[ColCount] = {
        Localization::ColName, Localization::ColSize, Localization::ColProgress,
        Localization::ColDown, Localization::ColUp, Localization::ColRemaining,
        Localization::ColPeers, Localization::ColState,
    };
    for (int c = 0; c < ColCount; ++c) {
        std::wstring text = loc.t(headerKeys[c]);
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT;
        col.pszText = text.data();
        ListView_SetColumn(m_hwnd, c, &col);
    }

    // Re-render existing rows: state/peers/progress text is all re-derived
    // from Localization, so a plain re-set of visible text is enough --
    // no need to touch m_rows.
    for (size_t i = 0; i < m_rows.size(); ++i)
        setRowText(static_cast<int>(i), m_rows[i], true);
}

void TorrentListView::setRowText(int row, const TorrentInfo& t, bool force)
{
    const Localization& loc = Localization::instance();
    std::wstring name = t.name.empty() ? loc.t(Localization::FetchingMetadata) : t.name;
    std::wstring size = win::formatBytes(t.totalSize);
    std::wstring progress = progressText(t);
    std::wstring down = win::formatSpeed(t.downloadRateBps);
    std::wstring up = win::formatSpeed(t.uploadRateBps);
    std::wstring remaining = (t.isPaused || t.isQueued || t.isChecking || t.hasError || t.isFinished)
        ? L"-" : win::formatEta(t.totalSize - t.downloaded, t.downloadRateBps);
    std::wstring peers = peersText(t);
    std::wstring state = stateText(t);

    const std::wstring* cells[ColCount] = {&name, &size, &progress, &down, &up, &remaining, &peers, &state};

    // Only touch cells whose text actually changed: every SetItemText
    // makes the control repaint that cell, and on a typical tick most of
    // a row (name, size, state) is identical to the previous second.
    if (m_cellText.size() <= static_cast<size_t>(row))
        m_cellText.resize(static_cast<size_t>(row) + 1);
    auto& cache = m_cellText[static_cast<size_t>(row)];
    for (int c = 0; c < ColCount; ++c) {
        if (!force && cache[c] == *cells[c])
            continue;
        cache[c] = *cells[c];
        ListView_SetItemText(m_hwnd, row, c, const_cast<LPWSTR>(cache[c].c_str()));
    }
}

void TorrentListView::refresh(const std::vector<TorrentInfo>& torrents)
{
    // Fast path -- the overwhelmingly common one: same torrents in the
    // same order as a second ago, only their numbers moved. Update the
    // changed cells in place; no delete/re-insert, no flicker, and the
    // selection never has to be restored because it was never lost.
    bool sameSet = torrents.size() == m_rows.size();
    for (size_t i = 0; sameSet && i < torrents.size(); ++i)
        sameSet = torrents[i].infoHash == m_rows[i].infoHash;

    if (sameSet) {
        for (size_t i = 0; i < torrents.size(); ++i) {
            const bool progressMoved = torrents[i].progress != m_rows[i].progress;
            m_rows[i] = torrents[i];
            setRowText(static_cast<int>(i), m_rows[i], false);
            if (progressMoved) {
                RECT rc{};
                ListView_GetSubItemRect(m_hwnd, static_cast<int>(i), ColProgress, LVIR_BOUNDS, &rc);
                InvalidateRect(m_hwnd, &rc, FALSE);
            }
        }
        return;
    }

    // Slow path: a torrent was added or removed -- rebuild.
    const std::wstring selectedHash = selectedInfoHash();

    SendMessageW(m_hwnd, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(m_hwnd);
    m_rows = torrents;
    m_cellText.clear();

    for (size_t i = 0; i < m_rows.size(); ++i) {
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.iSubItem = 0;
        item.pszText = const_cast<LPWSTR>(L"");
        ListView_InsertItem(m_hwnd, &item);
        setRowText(static_cast<int>(i), m_rows[i], true);
    }

    if (!selectedHash.empty())
        selectByHash(selectedHash);

    SendMessageW(m_hwnd, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

const TorrentInfo* TorrentListView::selected() const
{
    const int sel = ListView_GetNextItem(m_hwnd, -1, LVNI_SELECTED);
    if (sel < 0 || sel >= static_cast<int>(m_rows.size()))
        return nullptr;
    return &m_rows[static_cast<size_t>(sel)];
}

std::wstring TorrentListView::selectedInfoHash() const
{
    const TorrentInfo* t = selected();
    return t ? t->infoHash : std::wstring();
}

void TorrentListView::selectByHash(const std::wstring& infoHash)
{
    for (size_t i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].infoHash == infoHash) {
            ListView_SetItemState(m_hwnd, static_cast<int>(i),
                                   LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
            break;
        }
    }
}

LRESULT TorrentListView::handleCustomDraw(NMLVCUSTOMDRAW* cd)
{
    switch (cd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT:
        return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        return CDRF_NOTIFYSUBITEMDRAW;
    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
        if (cd->iSubItem != ColProgress)
            return CDRF_DODEFAULT;

        const int row = static_cast<int>(cd->nmcd.dwItemSpec);
        if (row < 0 || row >= static_cast<int>(m_rows.size()))
            return CDRF_DODEFAULT;

        RECT rc{};
        ListView_GetSubItemRect(m_hwnd, row, ColProgress, LVIR_BOUNDS, &rc);
        rc.left += win::px(4); rc.right -= win::px(6); rc.top += win::px(6); rc.bottom -= win::px(6);
        if (rc.right <= rc.left || rc.bottom <= rc.top)
            return CDRF_DODEFAULT;

        HDC dc = cd->nmcd.hdc;
        const float progress = m_rows[static_cast<size_t>(row)].progress;

        // Track.
        HBRUSH track = CreateSolidBrush(RGB(0xE3, 0xE3, 0xE6));
        FillRect(dc, &rc, track);
        DeleteObject(track);

        // Fill, proportional to progress.
        RECT fill = rc;
        fill.right = rc.left + static_cast<LONG>((rc.right - rc.left) * std::clamp(progress, 0.0f, 1.0f));
        if (fill.right > fill.left) {
            HBRUSH accent = CreateSolidBrush(RGB(0x4F, 0x46, 0xE5));
            FillRect(dc, &fill, accent);
            DeleteObject(accent);
        }

        FrameRect(dc, &rc, reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

        return CDRF_SKIPDEFAULT;
    }
    default:
        return CDRF_DODEFAULT;
    }
}
