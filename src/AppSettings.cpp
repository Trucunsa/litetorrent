#include "AppSettings.h"
#include "Win32Util.h"

#include <shlobj.h>

namespace {
constexpr wchar_t kKeyPath[] = L"Software\\LiteTorrent";

// Opens (creating if needed) HKCU\Software\LiteTorrent for the duration of
// one call -- these settings are read/written rarely enough that there's
// no reason to keep the key open between calls.
HKEY openKey(bool forWrite)
{
    HKEY key = nullptr;
    if (forWrite) {
        RegCreateKeyExW(HKEY_CURRENT_USER, kKeyPath, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    } else {
        RegOpenKeyExW(HKEY_CURRENT_USER, kKeyPath, 0, KEY_QUERY_VALUE, &key);
    }
    return key;
}

std::wstring readString(const wchar_t* name, const std::wstring& fallback)
{
    HKEY key = openKey(false);
    if (!key)
        return fallback;

    wchar_t buf[2048];
    DWORD size = sizeof(buf);
    DWORD type = 0;
    LONG rc = RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(buf), &size);
    RegCloseKey(key);

    if (rc != ERROR_SUCCESS || type != REG_SZ)
        return fallback;
    return std::wstring(buf, (size / sizeof(wchar_t)) - 1); // drop the trailing NUL
}

void writeString(const wchar_t* name, const std::wstring& value)
{
    HKEY key = openKey(true);
    if (!key)
        return;
    RegSetValueExW(key, name, 0, REG_SZ,
                    reinterpret_cast<const BYTE*>(value.c_str()),
                    static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

int readInt(const wchar_t* name, int fallback)
{
    HKEY key = openKey(false);
    if (!key)
        return fallback;

    DWORD value = 0;
    DWORD size = sizeof(value);
    DWORD type = 0;
    LONG rc = RegQueryValueExW(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);

    if (rc != ERROR_SUCCESS || type != REG_DWORD)
        return fallback;
    return static_cast<int>(value);
}

void writeInt(const wchar_t* name, int value)
{
    HKEY key = openKey(true);
    if (!key)
        return;
    const DWORD v = static_cast<DWORD>(value);
    RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&v), sizeof(v));
    RegCloseKey(key);
}

bool readBool(const wchar_t* name, bool fallback)
{
    return readInt(name, fallback ? 1 : 0) != 0;
}

void writeBool(const wchar_t* name, bool value)
{
    writeInt(name, value ? 1 : 0);
}
}

AppSettings::AppSettings() = default;

std::wstring AppSettings::defaultDownloadDir()
{
    PWSTR path = nullptr;
    std::wstring result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path))) {
        result = path;
        CoTaskMemFree(path);
    }
    if (result.empty()) {
        wchar_t profile[MAX_PATH];
        DWORD n = GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
        if (n > 0 && n < MAX_PATH)
            result = std::wstring(profile) + L"\\Downloads";
    }
    return result;
}

std::wstring AppSettings::downloadDir() const
{
    return readString(L"DownloadDir", defaultDownloadDir());
}

void AppSettings::setDownloadDir(const std::wstring& path)
{
    writeString(L"DownloadDir", path);
}

int AppSettings::downloadLimitKBs() const
{
    return readInt(L"DownloadLimitKBs", 0);
}

void AppSettings::setDownloadLimitKBs(int kbs)
{
    writeInt(L"DownloadLimitKBs", kbs);
}

int AppSettings::uploadLimitKBs() const
{
    return readInt(L"UploadLimitKBs", 0);
}

void AppSettings::setUploadLimitKBs(int kbs)
{
    writeInt(L"UploadLimitKBs", kbs);
}

bool AppSettings::shutdownWhenFinished() const
{
    return readBool(L"ShutdownWhenFinished", false);
}

void AppSettings::setShutdownWhenFinished(bool enabled)
{
    writeBool(L"ShutdownWhenFinished", enabled);
}

bool AppSettings::waitForSeedingToFinish() const
{
    return readBool(L"WaitForSeedingToFinish", false);
}

void AppSettings::setWaitForSeedingToFinish(bool enabled)
{
    writeBool(L"WaitForSeedingToFinish", enabled);
}

std::wstring AppSettings::language() const
{
    return readString(L"Language", L"ru");
}

void AppSettings::setLanguage(const std::wstring& code)
{
    writeString(L"Language", code);
}

int AppSettings::fileAssociation() const
{
    return readInt(L"FileAssociation", -1);
}

void AppSettings::setFileAssociation(int state)
{
    writeInt(L"FileAssociation", state);
}
