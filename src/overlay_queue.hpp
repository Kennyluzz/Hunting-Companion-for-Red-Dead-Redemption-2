#pragma once
// Platform-independent part of the CJK overlay: the hand-off of text commands
// from the ScriptHook script thread to the render thread, plus layout maths.
// Kept header-only and free of Vulkan/ImGui so it can be unit-tested anywhere
// (tests/test_overlay_queue.cpp).

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <mutex>
#endif

namespace hunting_companion::overlay {

// One piece of text exactly as the mod would have handed it to the game's native
// text natives: normalised 0..1 screen coordinates and a "reference pixel" size.
// Fixed-size text buffer instead of std::string: no heap allocation per label and no
// dependence on std::string's move constructor (which some static MinGW libstdc++
// builds fail to link). Longer text is cut on a UTF-8 character boundary.
constexpr std::size_t kMaxTextBytes = 768;

struct TextCmd {
    char text[kMaxTextBytes] = {0};
    float x = 0.f;
    float y = 0.f;
    float referencePixelHeight = 12.f;
    std::uint8_t r = 255, g = 255, b = 255, a = 255;
    bool centered = false;

    bool empty() const { return text[0] == '\0'; }

    void set_text(const char* utf8) {
        std::size_t n = 0;
        if (utf8 != nullptr) while (utf8[n] != '\0' && n < kMaxTextBytes - 1) ++n;
        // If we stopped mid-string, back up so a multi-byte character is not split.
        if (utf8 != nullptr && utf8[n] != '\0') {
            while (n > 0 && (static_cast<unsigned char>(utf8[n]) & 0xC0) == 0x80) --n;
        }
        for (std::size_t i = 0; i < n; ++i) text[i] = utf8[i];
        text[n] = '\0';
    }
};

// Tiny lock so this header works with MinGW builds that lack std::mutex.
class Lock {
public:
#if defined(_WIN32)
    Lock() { InitializeSRWLock(&l_); }
    void lock() { AcquireSRWLockExclusive(&l_); }
    void unlock() { ReleaseSRWLockExclusive(&l_); }
private:
    SRWLOCK l_;
#else
    void lock() { m_.lock(); }
    void unlock() { m_.unlock(); }
private:
    std::mutex m_;
#endif
};

class Guard {
public:
    explicit Guard(Lock& l) : l_(l) { l_.lock(); }
    ~Guard() { l_.unlock(); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
private:
    Lock& l_;
};

// Script thread:  begin() -> add()... -> publish()   (once per game tick)
// Render thread:  snapshot()                         (once per present)
class FrameQueue {
public:
    void begin() { pending_.clear(); }
    void add(const TextCmd& cmd) { pending_.push_back(cmd); }

    // Makes everything added since begin() visible to the render thread.
    void publish(std::uint64_t nowMs) {
        Guard g(lock_);
        published_.swap(pending_);
        pending_.clear();
        stampMs_ = nowMs;
        everPublished_ = true;
    }

    // Copies the latest published frame. Returns false if nothing is published
    // or the script thread has not published for maxAgeMs (stalled/paused), so
    // stale text never lingers after the native panels have vanished.
    bool snapshot(std::vector<TextCmd>& out, std::uint64_t nowMs, std::uint64_t maxAgeMs) {
        Guard g(lock_);
        out.clear();
        if (!everPublished_ || nowMs - stampMs_ > maxAgeMs) return false;
        out = published_;
        return true;
    }

private:
    std::vector<TextCmd> pending_;    // script thread only
    std::vector<TextCmd> published_;  // guarded by lock_
    Lock lock_;
    std::uint64_t stampMs_ = 0;
    bool everPublished_ = false;
};

// The mod's "reference pixel" sizes were tuned against the game's native text.
// Measured on a 1080p-equivalent frame, one reference pixel is ~1.19 px of em
// height, i.e. about 0.0011 of the screen height. `userScale` is the ini knob.
// Re-measured on a 1440p screenshot: for the same "HUNTING COMPANION" title the overlay's
// capital letters were 18 px against 22 px for the native text, so the first estimate
// (0.0011) was 22% too small.
constexpr float kScreenHeightFractionPerReferencePixel = 0.00134f;

// CJK ideographs need more size than Latin letters to read as comfortably as Latin text
// drawn at the same reference size; the menu layout also draws CJK labels smaller (12.5 vs 18).
constexpr float kCjkSizeBoost = 1.25f;

inline float font_pixels(float referencePixelHeight, float screenHeightPx, float userScale) {
    float px = referencePixelHeight * kScreenHeightFractionPerReferencePixel * screenHeightPx * userScale;
    if (px < 6.f) px = 6.f;
    return px;
}

// Left edge of a text run. Native text with SET_TEXT_CENTRE treats x as the centre.
inline float left_edge(float xPx, float textWidthPx, bool centered) {
    return centered ? xPx - textWidthPx * 0.5f : xPx;
}

// ImGui feeds vertex colours to the GPU as-is. On an *_SRGB swapchain the hardware
// then treats them as linear and the text looks washed out, so convert first.
inline float srgb_to_linear(float c) {
    if (c <= 0.04045f) return c / 12.92f;
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

inline std::uint8_t color_channel(std::uint8_t c, bool srgbTarget) {
    if (!srgbTarget) return c;
    float v = srgb_to_linear(static_cast<float>(c) / 255.f) * 255.f + 0.5f;
    if (v < 0.f) v = 0.f;
    if (v > 255.f) v = 255.f;
    return static_cast<std::uint8_t>(v);
}

// Companion language index -> which embedded font to use.
// 8 ko-KR, 9 zh-TW, 10 ja-JP, 12 zh-CN (same numbering as the rest of the mod).
constexpr int kLatinFontSlot = 4;
inline int cjk_font_slot(int language) {
    switch (language) {
        case 10: return 0;  // Japanese
        case 8:  return 1;  // Korean
        case 12: return 2;  // Simplified Chinese
        case 9:  return 3;  // Traditional Chinese
        default: return -1;
    }
}
// Every language has an overlay font: the CJK ones above, and Latin/Cyrillic for the rest
// (used when the game itself runs in a CJK language and cannot draw accents/Cyrillic).
inline int overlay_font_slot(int language) {
    const int cjk = cjk_font_slot(language);
    return cjk >= 0 ? cjk : kLatinFontSlot;
}
inline float slot_size_boost(int slot) { return slot == kLatinFontSlot ? 1.0f : kCjkSizeBoost; }

} // namespace hunting_companion::overlay
