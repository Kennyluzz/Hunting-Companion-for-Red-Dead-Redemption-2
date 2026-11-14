#include "text_input.hpp"

#include <windows.h>
#include <imm.h>

#include <cstdint>
#include <cstring>
#include <cwchar>

namespace hunting_companion::text_input {
namespace {

HWND g_window = nullptr;
WNDPROC g_previous = nullptr;
volatile LONG g_active = 0;
UINT g_imeMessage = 0;
HIMC g_ownContext = nullptr;
HIMC g_savedContext = nullptr;
bool g_contextSwapped = false;

// Typed UTF-16 units, written by the game's window thread and read by the
// script thread (single producer, single consumer).
constexpr LONG kRingSize = 256;
wchar_t g_ring[kRingSize]{};
volatile LONG g_head = 0;  // next write (window thread)
volatile LONG g_tail = 0;  // next read (script thread)

SRWLOCK g_compositionLock = SRWLOCK_INIT;
wchar_t g_composition[64]{};

void push_unit(wchar_t unit) noexcept {
    const LONG head = g_head;
    const LONG next = (head + 1) % kRingSize;
    if (next == InterlockedCompareExchange(&g_tail, 0, 0)) return;  // full: drop
    g_ring[head] = unit;
    InterlockedExchange(&g_head, next);
}

void push_typed(wchar_t unit) noexcept {
    // Control characters (Backspace, Enter, Tab, Esc, Ctrl+letter) and DEL are
    // keys, not text.
    if (unit < 0x20 || unit == 0x7F) return;
    push_unit(unit);
}

void set_composition(const wchar_t* text, std::size_t length) noexcept {
    AcquireSRWLockExclusive(&g_compositionLock);
    const std::size_t kept = length < 63 ? length : 63;
    if (text != nullptr && kept > 0) std::memcpy(g_composition, text, kept * sizeof(wchar_t));
    g_composition[kept] = L'\0';
    ReleaseSRWLockExclusive(&g_compositionLock);
}

void read_ime_string(HWND window, DWORD kind, bool committed) noexcept {
    HIMC context = ImmGetContext(window);
    if (context == nullptr) return;
    wchar_t text[64]{};
    const LONG bytes = ImmGetCompositionStringW(context, kind, text, sizeof(text) - sizeof(wchar_t));
    ImmReleaseContext(window, context);
    const std::size_t length = bytes > 0 ? static_cast<std::size_t>(bytes) / sizeof(wchar_t) : 0;
    if (committed) {
        for (std::size_t i = 0; i < length; ++i) push_typed(text[i]);
    } else {
        set_composition(text, length);
    }
}

LRESULT pass_on(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept {
    return g_previous != nullptr ? CallWindowProcW(g_previous, window, message, wParam, lParam)
                                 : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == g_imeMessage && g_imeMessage != 0) {
        // Runs on the window's own thread, as the IME functions require.
        if (wParam != 0 && !g_contextSwapped) {
            if (g_ownContext == nullptr) g_ownContext = ImmCreateContext();
            if (g_ownContext != nullptr) {
                g_savedContext = ImmAssociateContext(window, g_ownContext);
                g_contextSwapped = true;
            }
        } else if (wParam == 0 && g_contextSwapped) {
            HIMC context = ImmGetContext(window);
            if (context != nullptr) {
                ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
                ImmReleaseContext(window, context);
            }
            ImmAssociateContext(window, g_savedContext);
            g_contextSwapped = false;
            set_composition(nullptr, 0);
        }
        return 0;
    }
    if (InterlockedCompareExchange(&g_active, 0, 0) == 0) {
        return pass_on(window, message, wParam, lParam);
    }
    switch (message) {
        case WM_CHAR:
        case WM_IME_CHAR:
            push_typed(static_cast<wchar_t>(wParam));
            return 0;
        case WM_IME_SETCONTEXT:
            // The search box shows the text being composed itself; the IME
            // still shows its candidate list.
            return DefWindowProcW(window, message, wParam,
                                  lParam & ~static_cast<LPARAM>(ISC_SHOWUICOMPOSITIONWINDOW));
        case WM_IME_STARTCOMPOSITION:
            set_composition(nullptr, 0);
            return 0;
        case WM_IME_COMPOSITION:
            if (lParam & GCS_RESULTSTR) {
                read_ime_string(window, GCS_RESULTSTR, true);
                set_composition(nullptr, 0);
            }
            if (lParam & GCS_COMPSTR) read_ime_string(window, GCS_COMPSTR, false);
            return 0;
        case WM_IME_ENDCOMPOSITION:
            set_composition(nullptr, 0);
            return 0;
        case WM_IME_NOTIFY:
        case WM_IME_CONTROL:
        case WM_IME_SELECT:
        case WM_INPUTLANGCHANGE:
            return DefWindowProcW(window, message, wParam, lParam);
        default:
            return pass_on(window, message, wParam, lParam);
    }
}

BOOL CALLBACK find_game_window(HWND window, LPARAM result) {
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    if (process != GetCurrentProcessId() || !IsWindowVisible(window) ||
        GetWindow(window, GW_OWNER) != nullptr) return TRUE;
    wchar_t className[64]{};
    GetClassNameW(window, className, 64);
    HWND* found = reinterpret_cast<HWND*>(result);
    if (std::wcscmp(className, L"sgaWindow") == 0) {
        *found = window;
        return FALSE;
    }
    if (*found == nullptr) *found = window;
    return TRUE;
}

bool install() noexcept {
    if (g_window != nullptr) return true;
    HWND window = nullptr;
    EnumWindows(find_game_window, reinterpret_cast<LPARAM>(&window));
    if (window == nullptr) return false;
    g_imeMessage = RegisterWindowMessageW(L"HuntingCompanion.TextInput");
    g_previous = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC));
    const LONG_PTR previous = SetWindowLongPtrW(window, GWLP_WNDPROC,
                                                reinterpret_cast<LONG_PTR>(&window_proc));
    if (previous == 0) return false;
    g_previous = reinterpret_cast<WNDPROC>(previous);
    g_window = window;
    return true;
}

int encode_utf8(std::uint32_t codepoint, char out[5]) noexcept {
    int length = 0;
    if (codepoint < 0x80) {
        out[length++] = static_cast<char>(codepoint);
    } else if (codepoint < 0x800) {
        out[length++] = static_cast<char>(0xC0 | (codepoint >> 6));
        out[length++] = static_cast<char>(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        out[length++] = static_cast<char>(0xE0 | (codepoint >> 12));
        out[length++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[length++] = static_cast<char>(0x80 | (codepoint & 0x3F));
    } else {
        out[length++] = static_cast<char>(0xF0 | (codepoint >> 18));
        out[length++] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        out[length++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        out[length++] = static_cast<char>(0x80 | (codepoint & 0x3F));
    }
    out[length] = '\0';
    return length;
}

bool pop_unit(wchar_t* unit) noexcept {
    const LONG tail = g_tail;
    if (tail == InterlockedCompareExchange(&g_head, 0, 0)) return false;
    *unit = g_ring[tail];
    InterlockedExchange(&g_tail, (tail + 1) % kRingSize);
    return true;
}

}  // namespace

void set_active(bool active) noexcept {
    const bool wasActive = InterlockedCompareExchange(&g_active, 0, 0) != 0;
    if (active == wasActive) return;
    if (active && !install()) return;
    // Anything typed while the box was closed is dropped.
    InterlockedExchange(&g_tail, InterlockedCompareExchange(&g_head, 0, 0));
    InterlockedExchange(&g_active, active ? 1 : 0);
    if (g_window != nullptr && g_imeMessage != 0)
        PostMessageW(g_window, g_imeMessage, active ? 1 : 0, 0);
    if (!active) set_composition(nullptr, 0);
}

bool next_character(char out[5]) noexcept {
    static wchar_t pendingHigh = 0;
    wchar_t unit = 0;
    while (pop_unit(&unit)) {
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            pendingHigh = unit;
            continue;
        }
        std::uint32_t codepoint = unit;
        if (unit >= 0xDC00 && unit <= 0xDFFF) {
            if (pendingHigh == 0) continue;
            codepoint = 0x10000 + ((static_cast<std::uint32_t>(pendingHigh) - 0xD800) << 10) +
                        (static_cast<std::uint32_t>(unit) - 0xDC00);
        }
        pendingHigh = 0;
        encode_utf8(codepoint, out);
        return true;
    }
    return false;
}

void composition(char* out, std::size_t size) noexcept {
    if (out == nullptr || size == 0) return;
    out[0] = '\0';
    wchar_t copy[64]{};
    AcquireSRWLockShared(&g_compositionLock);
    std::memcpy(copy, g_composition, sizeof(copy));
    ReleaseSRWLockShared(&g_compositionLock);
    if (copy[0] == L'\0') return;
    const int written = WideCharToMultiByte(CP_UTF8, 0, copy, -1, out, static_cast<int>(size), nullptr, nullptr);
    if (written <= 0) out[0] = '\0';
}

}  // namespace hunting_companion::text_input
