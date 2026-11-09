#pragma once

#include "Win32Util.h"
#include "SessionManager.h"

#include <commctrl.h>
#include <vector>
#include <array>

// Thin wrapper around a native SysListView32 control (report view),
// replacing the old Qt TorrentListModel/QTableView pair. A refresh updates
// changed cells in place; the rows are only rebuilt when a torrent is
// added or removed.
class TorrentListView
{
public:
    enum Column {
        ColName = 0,
        ColSize,
        ColProgress,
        ColDown,
        ColUp,
        ColRemaining,
        ColPeers,
        ColState,
        ColCount
    };

    void create(HWND parent, HINSTANCE hInst, int id);

    HWND hwnd() const { return m_hwnd; }

    // Repositions/resizes the control and re-stretches the Name column to
    // fill whatever width is left over.
    void layout(int x, int y, int w, int h);

    // Rebuilds all rows from the current snapshot, re-selecting whichever
    // torrent (by info hash) was selected before, if it's still present.
    void refresh(const std::vector<TorrentInfo>& torrents);

    // Re-applies column header text and re-renders existing rows after a
    // language change.
    void retranslate();

    // After a DPI change: re-applies the font and rescales column widths.
    void applyDpi(int totalWidth);

    const TorrentInfo* selected() const;
    std::wstring selectedInfoHash() const;
    void selectByHash(const std::wstring& infoHash);

    // Called from MainWindow's WM_NOTIFY handler for NM_CUSTOMDRAW on this
    // control, to paint a small progress bar in the Progress column
    // instead of plain "42.3%" text.
    LRESULT handleCustomDraw(NMLVCUSTOMDRAW* cd);

    bool empty() const { return m_rows.empty(); }

private:
    void setRowText(int row, const TorrentInfo& t, bool force);

    HWND m_hwnd = nullptr;
    std::vector<TorrentInfo> m_rows;
    // Last text written to each cell, so unchanged cells are skipped.
    std::vector<std::array<std::wstring, ColCount>> m_cellText;
};
