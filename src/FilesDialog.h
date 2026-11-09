#pragma once

#include "Win32Util.h"
#include "SessionManager.h"

#include <vector>

// Modal "Files" window: one checkbox per file of a torrent. Returns true
// when the user pressed OK; `files[i].wanted` then holds the new choice.
bool showFilesDialog(HWND owner, HINSTANCE hInst, const std::wstring& torrentName,
                     std::vector<TorrentFileEntry>& files);
