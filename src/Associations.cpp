#include "Associations.h"
#include "Win32Util.h"

#include <shlobj.h>
#include <shellapi.h>

namespace {

constexpr wchar_t kFileProgId[] = L"LiteTorrent.torrent";
constexpr wchar_t kUrlProgId[] = L"LiteTorrent.magnet";
constexpr wchar_t kAppName[] = L"LiteTorrent";
constexpr wchar_t kCapabilities[] = L"Software\\LiteTorrent\\Capabilities";

std::wstring exePath()
{
    wchar_t buf[32768];
    const DWORD n = GetModuleFileNameW(nullptr, buf, 32768);
    return std::wstring(buf, n);
}

// Sets HKCU\<path>[name] = value (name == nullptr: the key's default value).
void setString(const std::wstring& path, const wchar_t* name, const std::wstring& value)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS)
        return;
    RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                   static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
}

std::wstring getString(const std::wstring& path, const wchar_t* name, HKEY root = HKEY_CURRENT_USER)
{
    wchar_t buf[2048];
    DWORD size = sizeof(buf);
    if (RegGetValueW(root, path.c_str(), name, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
        return {};
    return buf;
}

void deleteTree(const std::wstring& path)
{
    RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str());
    RegDeleteKeyW(HKEY_CURRENT_USER, path.c_str());
}

void deleteValue(const std::wstring& path, const wchar_t* name)
{
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;
    RegDeleteValueW(key, name);
    RegCloseKey(key);
}

// A ProgID / URL class whose "open" verb runs LiteTorrent with the item.
void writeClass(const std::wstring& path, const std::wstring& description, bool urlProtocol)
{
    const std::wstring exe = exePath();
    setString(path, nullptr, description);
    if (urlProtocol)
        setString(path, L"URL Protocol", L"");
    setString(path + L"\\DefaultIcon", nullptr, L"\"" + exe + L"\",0");
    setString(path + L"\\shell\\open\\command", nullptr, L"\"" + exe + L"\" \"%1\"");
}

bool commandIsOurs(const std::wstring& classPath)
{
    const std::wstring cmd = getString(classPath + L"\\shell\\open\\command", nullptr);
    const std::wstring exe = exePath();
    return !cmd.empty() && cmd.find(exe) != std::wstring::npos;
}

void notifyShell()
{
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

} // namespace

namespace assoc {

void enable()
{
    const std::wstring classes = L"Software\\Classes\\";

    writeClass(classes + kFileProgId, L"Torrent", false);
    writeClass(classes + kUrlProgId, L"URL:Magnet link", true);

    // .torrent: listed under "Open with", and the default when no other
    // program has been chosen by the user.
    setString(classes + L".torrent\\OpenWithProgids", kFileProgId, L"");
    const std::wstring current = getString(classes + L".torrent", nullptr);
    if (current.empty() || current == kFileProgId)
        setString(classes + L".torrent", nullptr, kFileProgId);

    // magnet: the protocol key itself, unless another program owns it.
    const std::wstring magnet = classes + L"magnet";
    if (getString(magnet, L"URL Protocol").empty() && getString(magnet, nullptr).empty())
        writeClass(magnet, L"URL:Magnet link", true);
    else if (commandIsOurs(magnet))
        writeClass(magnet, L"URL:Magnet link", true); // refresh our own path

    // "Default apps" listing.
    setString(kCapabilities, L"ApplicationName", kAppName);
    setString(kCapabilities, L"ApplicationDescription", L"Lightweight BitTorrent client");
    setString(std::wstring(kCapabilities) + L"\\FileAssociations", L".torrent", kFileProgId);
    setString(std::wstring(kCapabilities) + L"\\URLAssociations", L"magnet", kUrlProgId);
    setString(L"Software\\RegisteredApplications", kAppName, kCapabilities);

    notifyShell();
}

void disable()
{
    const std::wstring classes = L"Software\\Classes\\";

    if (getString(classes + L".torrent", nullptr) == kFileProgId)
        deleteValue(classes + L".torrent", nullptr);
    deleteValue(classes + L".torrent\\OpenWithProgids", kFileProgId);
    if (commandIsOurs(classes + L"magnet"))
        deleteTree(classes + L"magnet");
    deleteTree(classes + kFileProgId);
    deleteTree(classes + kUrlProgId);
    deleteValue(L"Software\\RegisteredApplications", kAppName);
    deleteTree(kCapabilities);

    notifyShell();
}

bool ownedByAnotherApp()
{
    const std::wstring fileChoice = getString(
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.torrent\\UserChoice", L"ProgId");
    const std::wstring urlChoice = getString(
        L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\magnet\\UserChoice", L"ProgId");
    // The user's explicit choice in Windows wins over everything else.
    if (!fileChoice.empty() && fileChoice != kFileProgId)
        return true;
    if (!urlChoice.empty() && urlChoice != kUrlProgId && !(urlChoice == L"magnet" && commandIsOurs(L"Software\\Classes\\magnet")))
        return true;
    // Without a user choice, the merged HKEY_CLASSES_ROOT view decides.
    if (fileChoice.empty()) {
        const std::wstring progId = getString(L".torrent", nullptr, HKEY_CLASSES_ROOT);
        if (!progId.empty() && progId != kFileProgId)
            return true;
    }
    if (urlChoice.empty()) {
        const std::wstring cmd = getString(L"magnet\\shell\\open\\command", nullptr, HKEY_CLASSES_ROOT);
        if (!cmd.empty() && cmd.find(exePath()) == std::wstring::npos)
            return true;
    }
    return false;
}

void openDefaultAppsSettings()
{
    ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps?registeredAppUser=LiteTorrent",
                  nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace assoc
