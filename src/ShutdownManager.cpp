#include "ShutdownManager.h"
#include "Win32Util.h"

void ShutdownManager::arm()
{
    if (m_active)
        return; // already counting down

    m_active = true;
    m_secondsLeft = kCountdownSeconds;
    if (onCountdownStarted)
        onCountdownStarted(m_secondsLeft);
}

void ShutdownManager::cancel()
{
    if (!m_active)
        return;

    m_active = false;
    if (onCountdownCancelled)
        onCountdownCancelled();
}

void ShutdownManager::tick()
{
    if (!m_active)
        return;

    --m_secondsLeft;
    if (m_secondsLeft <= 0) {
        m_active = false;
        if (onShutdownInitiated)
            onShutdownInitiated();
        performShutdown();
        return;
    }
    if (onCountdownTick)
        onCountdownTick(m_secondsLeft);
}

void ShutdownManager::performShutdown()
{
    HANDLE token;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        TOKEN_PRIVILEGES tp;
        LUID luid;
        if (LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &luid)) {
            tp.PrivilegeCount = 1;
            tp.Privileges[0].Luid = luid;
            tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
            AdjustTokenPrivileges(token, FALSE, &tp, 0, nullptr, nullptr);
        }
        CloseHandle(token);
    }
    ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCEIFHUNG, SHTDN_REASON_FLAG_PLANNED);
}
