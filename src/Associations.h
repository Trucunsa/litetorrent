#pragma once

#include <string>

// Registers LiteTorrent, for the current user only (HKCU, no admin rights),
// as a program that opens .torrent files and magnet: links, and lists it in
// Windows' "Default apps". Windows 10/11 do not let a program make itself
// the default silently: when another program already owns .torrent or
// magnet:, Windows asks the user once, or the user picks LiteTorrent in
// Settings -> Default apps.
namespace assoc {

// Writes (or refreshes, e.g. after the exe was moved) the registration.
void enable();
// Removes everything enable() wrote.
void disable();
// True when the user's own choice in Windows still points .torrent or
// magnet: to another program, so enable() alone will not take effect.
bool ownedByAnotherApp();
// Opens Settings -> Default apps (on LiteTorrent's page where supported).
void openDefaultAppsSettings();

} // namespace assoc
