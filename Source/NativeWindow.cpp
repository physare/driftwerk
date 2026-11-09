// Small native helper for FULL SCREEN, kept apart from the JUCE code so <windows.h> stays out of it.
//
// With Windows display scaling (125%, 150%...), the borderless full-screen window can be scaled a
// second time after it opens (Windows sends it a DPI change and grows it by the scale), so it ends
// up 1.5x larger than the screen and the rack looks zoomed in and cut off. coverMonitor() puts the
// window back to exactly its monitor's rectangle, measured by Windows itself in the window's own
// DPI units; JUCE then reads the real size back and the rack is fitted into it.
#ifdef _WIN32
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace dwnative
{
bool coverMonitor (void* nativeHandle)
{
    auto hwnd = static_cast<HWND> (nativeHandle);
    if (hwnd == nullptr || ! IsWindow (hwnd) || IsIconic (hwnd)) return false;

    // Work in the window's own DPI awareness, so its rectangle and the monitor's are in the same units.
    using SetThreadCtx = HANDLE (WINAPI*) (HANDLE);
    using GetWindowCtx = HANDLE (WINAPI*) (HWND);
    static const auto user32 = GetModuleHandleW (L"user32.dll");
    static const auto setThreadCtx = user32 != nullptr ? reinterpret_cast<SetThreadCtx> (GetProcAddress (user32, "SetThreadDpiAwarenessContext")) : nullptr;
    static const auto getWindowCtx = user32 != nullptr ? reinterpret_cast<GetWindowCtx> (GetProcAddress (user32, "GetWindowDpiAwarenessContext")) : nullptr;

    HANDLE previous = nullptr;
    if (setThreadCtx != nullptr && getWindowCtx != nullptr)
        if (auto ctx = getWindowCtx (hwnd))
            previous = setThreadCtx (ctx);

    bool changed = false;
    MONITORINFO mi {};
    mi.cbSize = sizeof (mi);
    if (GetMonitorInfoW (MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST), &mi))
    {
        RECT wr {};
        GetWindowRect (hwnd, &wr);
        const RECT m = mi.rcMonitor;
        if (wr.left != m.left || wr.top != m.top || wr.right != m.right || wr.bottom != m.bottom)
        {
            SetWindowPos (hwnd, nullptr, m.left, m.top, m.right - m.left, m.bottom - m.top,
                          SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            changed = true;
        }
    }

    if (previous != nullptr) setThreadCtx (previous);
    return changed;
}
} // namespace dwnative
#else
namespace dwnative
{
bool coverMonitor (void*) { return false; }
} // namespace dwnative
#endif
