#pragma once
// Shared between the two overlay renderers (Vulkan in cjk_overlay.cpp, DirectX 12
// in cjk_overlay_dx12.cpp). Everything here is implemented in cjk_overlay.cpp.

#include "cjk_overlay.hpp"
#include "overlay_queue.hpp"

#include <vector>

namespace hunting_companion::overlay::detail {

// Same sink as the Vulkan renderer: debugger output, plus the log file when on.
void log(const char* fmt, ...) noexcept;
const Settings& settings() noexcept;

// The one overlay lock; every renderer entry point takes it.
void lock() noexcept;
void unlock() noexcept;
struct ScopedLock {
    ScopedLock() noexcept { lock(); }
    ~ScopedLock() { unlock(); }
    ScopedLock(const ScopedLock&) = delete;
    ScopedLock& operator=(const ScopedLock&) = delete;
};

// Only one renderer may own ImGui. Returns true if `backend` owns it (now or already).
enum class Backend : int { None = 0, Vulkan = 1, Dx12 = 2 };
bool claim_backend(Backend backend) noexcept;
Backend current_backend() noexcept;

// ImGui context and fonts (lock held). Safe to call repeatedly.
bool create_context() noexcept;
// Display size and frame time for the coming ImGui frame (before the backend's NewFrame).
void begin_frame_io(float width, float height) noexcept;
// Adds the queued text to the current ImGui frame (between NewFrame and Render).
void add_text(const std::vector<TextCmd>& cmds, float width, float height,
              TargetEncoding encoding) noexcept;

// Latest text published by the script thread; false when there is none to draw.
bool take_frame(std::vector<TextCmd>& out) noexcept;

void set_ready(bool ready) noexcept;
bool disabled() noexcept;

// cjk_overlay_dx12.cpp: installs the DXGI hooks (MinHook is already initialised).
void install_dx12_hooks() noexcept;

} // namespace hunting_companion::overlay::detail
