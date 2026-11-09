#include "Win32Util.h"
#include "MainWindow.h"

#include <shellapi.h>
#include <objbase.h>

namespace {

// If LiteTorrent is already running, gives it `argument` (a .torrent path
// or magnet link; may be empty), brings its window forward and returns
// true -- the caller then simply exits. Two copies would mean two separate
// lists fighting over the same network port and the same resume files.
bool handOverToRunningInstance(const std::wstring& argument)
{
    HWND existing = nullptr;
    // The other copy may itself still be starting up; give it a moment.
    for (int attempt = 0; attempt < 30 && !existing; ++attempt) {
        existing = FindWindowW(MainWindow::className(), nullptr);
        if (!existing)
            Sleep(100);
    }
    if (!existing)
        return false;

    COPYDATASTRUCT cds{};
    cds.dwData = 1;
    cds.cbData = static_cast<DWORD>((argument.size() + 1) * sizeof(wchar_t));
    cds.lpData = const_cast<wchar_t*>(argument.c_str());

    // Lets the running copy take the foreground when it handles the message.
    DWORD pid = 0;
    GetWindowThreadProcessId(existing, &pid);
    AllowSetForegroundWindow(pid);

    DWORD_PTR result = 0;
    SendMessageTimeoutW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds),
                        SMTO_ABORTIFHUNG, 5000, &result);
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // Windows launches the app with the file's path as argv[1] when a
    // .torrent file is double-clicked or sent through "Open with ->
    // LiteTorrent" -- there's no separate "received a file" event.
    std::wstring argument;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc > 1)
            argument = argv[1];
        LocalFree(argv);
    }
    // A relative path only means something in *this* process's working
    // directory, so make it absolute before it can travel to another copy.
    if (!argument.empty() && _wcsnicmp(argument.c_str(), L"magnet:", 7) != 0) {
        wchar_t full[32768];
        const DWORD n = GetFullPathNameW(argument.c_str(), 32768, full, nullptr);
        if (n > 0 && n < 32768)
            argument.assign(full, n);
    }

    // One copy per user session.
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\LiteTorrent.SingleInstance");
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (handOverToRunningInstance(argument)) {
            CloseHandle(mutex);
            return 0;
        }
        // The other copy never showed a window (it is probably on its way
        // out): carry on and start normally.
    }

    // Needed for the folder picker (IFileDialog) used by the Add-torrent
    // window; harmless on paths that never open it.
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    int exitCode = 0;
    {
        MainWindow window;
        if (!window.create(hInstance))
            return 1;
        window.show(nCmdShow);

        if (!argument.empty())
            window.openFromCommandLine(argument);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            // Keyboard handling (Tab, Enter, Esc) for the small secondary
            // windows, e.g. the shutdown countdown.
            HWND root = GetAncestor(msg.hwnd, GA_ROOT);
            if (root && root != window.hwnd() && IsDialogMessageW(root, &msg))
                continue;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        exitCode = static_cast<int>(msg.wParam);
    }

    CoUninitialize();
    if (mutex)
        CloseHandle(mutex);
    return exitCode;
}
