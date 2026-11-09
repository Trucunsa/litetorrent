#pragma once

#include <functional>

// Implements the "shut down PC when downloads finish" feature end to end:
// arms itself when SessionManager reports all downloads done, counts down
// with a cancellable grace period, and invokes ExitWindowsEx. Owns no
// torrent state itself -- MainWindow feeds it the two signals it needs
// (armed condition met / a new torrent was added) and drives tick() from
// its own Win32 WM_TIMER handler once a countdown is active.
class ShutdownManager
{
public:
    ShutdownManager() = default;

    static constexpr int kCountdownSeconds = 15;

    bool isActive() const { return m_active; }

    // Call when SessionManager::allDownloadsFinished() becomes true.
    void arm();
    // Call whenever a new torrent is added or resumed -- cancels a
    // pending countdown so an in-progress add is never cut off.
    void cancel();

    // Call once a second, only while isActive() is true.
    void tick();

    std::function<void(int seconds)> onCountdownStarted;
    std::function<void(int secondsLeft)> onCountdownTick;
    std::function<void()> onCountdownCancelled;
    std::function<void()> onShutdownInitiated;

private:
    void performShutdown();

    int m_secondsLeft = 0;
    bool m_active = false;
};
