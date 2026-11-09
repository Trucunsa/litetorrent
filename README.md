<p align="center"><img src="docs/icon.png" width="96" alt="LiteTorrent icon"></p>

# LiteTorrent

**English** · [Русский](README.ru.md)

A deliberately small BitTorrent client for Windows: one 9.5 MB `.exe`, no installer, no ads, no settings screen, and a checkbox that shuts the PC down when the downloads are done.

![Main window](docs/screenshot-main.png)

> The screenshots were taken under Wine on Linux, so fonts and window frames look slightly different on real Windows.

## Download

Get `LiteTorrent-Windows-x64.zip` from the [Releases](../../releases) page, unpack it and run `litetorrent.exe`. Nothing else is needed next to it.

- Windows 10 or 11, 64-bit.
- The exe is **not code-signed**. SmartScreen will warn about an unknown publisher; choose "More info" → "Run anyway" if you trust this build, or build it yourself (see [Building](#building)).
- SHA-256 of the released exe is listed in the release notes.

## What it does

- Adds torrents from a magnet link, a `.torrent` file, drag-and-drop onto the window, or "Open with" / double-click in Explorer (the file path is taken from the command line).
- Lets you choose the save folder when adding, and remembers the last one.
- Shows name, size, progress, download and upload speed, time remaining, peers (seeds) and state.
- Pause / resume and delete (with or without the downloaded files) for the selected torrent; "Folder" opens the folder that torrent was saved to.
- **Remembers the task list.** Torrents, their progress and their paused state survive closing the app, a restart, or a crash (progress is saved once a minute and on exit).
- **One running copy.** Opening another `.torrent` while LiteTorrent is running adds it to the existing window instead of starting a second program. Adding a torrent that is already in the list says so.
- **Shut down the PC when all downloads finish.** A window in the middle of the screen, on top of everything, counts down 15 seconds with a "Cancel shutdown" button. It only fires for downloads that were still running after you ticked the box — ticking it over an already-complete list does nothing — and never while a torrent is stopped with an error.
- Russian and English interface, switched by the RU / EN buttons in the top-right corner.

## What it does not do — read this before relying on it

This is a small project and it is honest about its limits:

- **Lightly tested.** The author has downloaded torrents with it on Windows. Everything else was checked only under Wine: adding from the command line and the add dialog, saving and restoring the list across restarts, handing a torrent to the running copy, the duplicate notice, pause surviving a restart, the delete dialog, the shutdown countdown and its cancel button, and layout at 150 % scaling. There is no automated test suite. Please report what you find.
- No choice of individual files inside a torrent, no priorities, no queueing, no scheduler.
- No speed-limit controls in the interface (see [Hidden settings](#settings-and-where-they-are-stored)).
- No tray icon, no notifications, no auto-update, no portable-mode switch.
- At most three torrents download at the same time (libtorrent's default); the rest show "Queued" and start on their own.
- No proxy, VPN binding, or anonymous mode. **Your IP address is visible to peers and trackers**, as with any ordinary BitTorrent client.
- Left out of the engine to save space: I2P, WebTorrent, super-seeding, share mode, streaming (piece deadlines), BEP 38 mutable torrents, and libtorrent's logging.

## Network behaviour

Nothing is hidden here either. The app talks only BitTorrent; there is no telemetry, no analytics, no ads and no update check.

| Setting | Value |
|---|---|
| DHT, Local Service Discovery | on |
| UPnP and NAT-PMP | on — the app asks your router to forward its listening port |
| Listening port, protocol encryption, connection limits | libtorrent defaults |
| TCP / uTP mixed mode | `prefer_tcp` |
| Seeding | yes — finished torrents keep uploading until paused, deleted, or the app is closed |
| HTTPS trackers and web seeds | supported (OpenSSL is linked in) |
| User agent | `LiteTorrent/1.1.0` |

## Settings and where they are stored

Settings live in the registry under `HKEY_CURRENT_USER\Software\LiteTorrent`. The task list lives in `%APPDATA%\LiteTorrent\resume`, one small `.fastresume` file per torrent (its metadata, save folder, pause state and which pieces are done). Nothing is written anywhere else, apart from the files you download. To remove every trace of the app, delete the exe, that registry key and that folder.

| Value | Type | Meaning |
|---|---|---|
| `DownloadDir` | string | Last chosen save folder (default: your Downloads folder) |
| `Language` | string | `ru` or `en` |
| `ShutdownWhenFinished` | DWORD | Last state of the shutdown checkbox (the box itself always starts unticked) |
| `DownloadLimitKBs`, `UploadLimitKBs` | DWORD | Speed limits in kB/s, `0` = unlimited. No interface for these; edit by hand and restart |
| `WaitForSeedingToFinish` | DWORD | `1` makes the shutdown feature wait while anything is still seeding. No interface |

Shutting down uses the standard `ExitWindowsEx` call, which closes other programs the normal way; unsaved work in other apps is at risk exactly as with a normal shutdown.

## How it is built

- **Language:** C++17, about 2,700 lines.
- **Interface:** plain Win32 API with the standard common controls, per-monitor DPI aware (sizes and fonts follow the display scale). No Qt, no .NET, no Electron, no web view.
- **Engine:** [libtorrent](https://github.com/arvidn/libtorrent) 2.0.11, compiled from source with unused features switched off.
- **TLS / hashing:** OpenSSL 3.6.5, static.
- **Headers only:** Boost 1.92.
- Everything is linked statically into one exe with unused code removed (`-Os`, `--gc-sections`, stripped). It depends only on DLLs that ship with Windows.

Roughly where the 9.5 MB goes: OpenSSL 5.6 MB, libtorrent 3.6 MB, C++ runtime 1.0 MB, the app itself well under 0.5 MB.

Earlier iterations used Qt 6 and weighed 93 MB unpacked; dropping Qt brought that to 20 MB, and static linking with a trimmed engine to 9.5 MB.

### Source layout

| File | Role |
|---|---|
| `src/main.cpp` | Entry point, command-line handling, message loop |
| `src/MainWindow.*` | The window: toolbar, list, status bar, timers, drag-and-drop |
| `src/TorrentListView.*` | Wrapper around the native list control, in-place row updates, progress bar |
| `src/AddTorrentDialog.*` | "Add torrent" dialog |
| `src/SessionManager.*` | The only code that touches libtorrent |
| `src/ShutdownManager.*` | Countdown and shutdown |
| `src/AppSettings.*` | Registry access |
| `src/Localization.*` | RU / EN string table |
| `resources/` | Icon, version info, application manifest |

## Building

The released exe was cross-compiled on Linux (Debian's MinGW-w64, GCC 13, posix threads). `build.sh` repeats that build (the result is functionally the same but not byte-identical, because the exe embeds a build timestamp):

```sh
sudo apt install g++-mingw-w64-x86-64-posix cmake ninja-build git
SYSROOT=/path/to/mingw64 ./build.sh
```

`SYSROOT` must contain Boost headers and OpenSSL static libraries built for MinGW-w64 x86_64. The release used the MSYS2 packages `mingw-w64-x86_64-boost` 1.92.0-4 and `mingw-w64-x86_64-openssl` 3.6.5-1, unpacked. The script downloads libtorrent itself, builds it, then builds and links the app into `build/litetorrent.exe`.

Building natively on Windows with MSYS2 should work with the same flags but has not been tried.

## How this was made

The author of this repository is not a programmer. The code was written by Claude, an AI model made by Anthropic, working from the author's requirements and feedback over a series of iterations. Treat it accordingly: read the source before trusting it, and expect rough edges.

## Legal

LiteTorrent is a general-purpose file-transfer tool. You are responsible for what you download and share with it; use it only for content you have the right to distribute. The skull-and-crossbones icon is a joke, not an invitation.

## License

LiteTorrent's own code is released under the [MIT License](LICENSE).

The released exe also contains libtorrent (BSD 3-Clause), OpenSSL (Apache 2.0), Boost (Boost Software License 1.0), and the GCC / MinGW-w64 runtime libraries. See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
