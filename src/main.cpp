#include "Win32Util.h"
#include "MainWindow.h"

#include <shellapi.h>
#include <objbase.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // Needed for the folder-picker (IFileDialog) used by the Add-torrent
    // window; harmless to call even on paths that never open it.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    MainWindow window;
    if (!window.create(hInstance))
        return 1;
    window.show(nCmdShow);

    // Windows launches the app with the file's path as argv[1] when a
    // .torrent file is double-clicked or sent through "Open with ->
    // LiteTorrent" -- there's no separate "received a file" event to
    // react to the way a URL-scheme handler or share-sheet would give
    // you.
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc > 1)
            window.openFromCommandLine(argv[1]);
        LocalFree(argv);
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
