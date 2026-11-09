#pragma once

#include "Win32Util.h"
#include <string>

struct AddTorrentResult
{
    std::wstring source;
    std::wstring saveDir;
};

// Shows a modal "Add torrent" window owned by `owner`. Returns true (and
// fills *result) if the user confirmed with a non-empty source; false if
// they cancelled. Pre-fills the save folder from AppSettings and writes
// back any folder the user changes to, immediately, the same way the old
// Qt popover did.
bool showAddTorrentDialog(HWND owner, HINSTANCE hInst, AddTorrentResult* result);
