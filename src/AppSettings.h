#pragma once

#include <string>

// Thin wrapper around the registry (HKCU\Software\LiteTorrent) holding the
// handful of options the app exposes. Everything else (ports, encryption,
// DHT, trackers) is fixed at sane defaults inside SessionManager and is
// never surfaced to the user.
class AppSettings
{
public:
    AppSettings();

    std::wstring downloadDir() const;
    void setDownloadDir(const std::wstring& path);

    // 0 means unlimited.
    int downloadLimitKBs() const;
    void setDownloadLimitKBs(int kbs);

    int uploadLimitKBs() const;
    void setUploadLimitKBs(int kbs);

    bool shutdownWhenFinished() const;
    void setShutdownWhenFinished(bool enabled);

    bool waitForSeedingToFinish() const;
    void setWaitForSeedingToFinish(bool enabled);

    // L"ru" or L"en". Defaults to L"ru".
    std::wstring language() const;
    void setLanguage(const std::wstring& code);

private:
    static std::wstring defaultDownloadDir();
};
