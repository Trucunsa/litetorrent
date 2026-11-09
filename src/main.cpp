#include "Win32Util.h"
#include "MainWindow.h"

#include <shellapi.h>
#include <objbase.h>

namespace {

// A second launch (typically a double-click on a .torrent while the app is
// already running) hands its argument to the running window and exits.
bool forwardToRunningInstance(const std::wstring& arg)
{
    for (int attempt = 0; attempt < 50; ++attempt) { // the first instance may still be starting up
        HWND target = FindWindowW(kMainWindowClass, nullptr);
        if (target) {
            DWORD pid = 0;
            GetWindowThreadProcessId(target, &pid);
            AllowSetForegroundWindow(pid);

            COPYDATASTRUCT cds{};
            cds.dwData = kCopyDataOpen;
            cds.cbData = static_cast<DWORD>((arg.size() + 1) * sizeof(wchar_t));
            cds.lpData = const_cast<wchar_t*>(arg.c_str());
            DWORD_PTR result = 0;
            return SendMessageTimeoutW(target, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds),
                                       SMTO_ABORTIFHUNG, 5000, &result) != 0;
        }
        Sleep(100);
    }
    return false;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // Windows launches the app with the file's path as argv[1] when a
    // .torrent file is double-clicked or sent through "Open with ->
    // LiteTorrent".
    std::wstring arg;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc > 1)
            arg = argv[1];
        LocalFree(argv);
    }

    // One window per user session: if LiteTorrent is already running, pass
    // the argument over to it and quit.
    HANDLE singleton = CreateMutexW(nullptr, FALSE, L"Local\\LiteTorrent.SingleInstance");
    if (singleton && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (forwardToRunningInstance(arg))
            return 0;
        // The other instance did not answer; fall through and run on our own.
    }

    // Needed for the folder-picker (IFileDialog) used by the Add-torrent
    // window; harmless to call even on paths that never open it.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    MainWindow window;
    if (!window.create(hInstance))
        return 1;
    window.show(nCmdShow);

    if (!arg.empty())
        window.openFromCommandLine(arg);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    CoUninitialize();
    if (singleton)
        CloseHandle(singleton);
    return static_cast<int>(msg.wParam);
}
