// Unit tests for the platform-independent overlay logic (no Vulkan/ImGui needed).
#include "overlay_queue.hpp"
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

using namespace hunting_companion::overlay;

static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

int main() {
    // Nothing published yet -> no snapshot.
    FrameQueue q;
    std::vector<TextCmd> out;
    assert(!q.snapshot(out, 1000, 80));

    // Publish two commands; snapshot sees them.
    q.begin();
    TextCmd a; a.set_text("\xE8\x8D\x92\xE9\x87\x8E"); a.x = 0.071f; a.y = 0.05f; a.referencePixelHeight = 22.f; a.centered = false;
    TextCmd b; b.set_text("centered"); b.centered = true;
    q.add(a); q.add(b);
    q.publish(1000);
    assert(q.snapshot(out, 1010, 80));
    assert(out.size() == 2 && std::strlen(out[0].text) == 6 && out[1].centered);

    // Pending (unpublished) work is invisible until publish().
    q.begin();
    TextCmd c; c.set_text("next");
    q.add(c);
    assert(q.snapshot(out, 1020, 80) && out.size() == 2);
    q.publish(1030);
    assert(q.snapshot(out, 1040, 80) && out.size() == 1 && std::strcmp(out[0].text, "next") == 0);

    // Stale frames expire: a stalled script thread must not leave text on screen.
    assert(!q.snapshot(out, 1030 + 81, 80));
    assert(q.snapshot(out, 1030 + 80, 80));

    // An empty published frame (menu closed) is a valid, empty snapshot.
    q.begin(); q.publish(2000);
    assert(q.snapshot(out, 2001, 80) && out.empty());

    // Layout maths. 22 reference px on 1080p ~ 26.6 px; scales with resolution and the ini knob.
    assert(near(font_pixels(22.f, 1080.f, 1.f), 31.834f, 0.05f));

    assert(near(font_pixels(22.f, 2160.f, 1.f), 2 * font_pixels(22.f, 1080.f, 1.f)));
    assert(near(font_pixels(22.f, 1080.f, 1.5f), 1.5f * font_pixels(22.f, 1080.f, 1.f)));
    assert(font_pixels(0.1f, 100.f, 1.f) >= 6.f);  // never degenerate
    assert(overlay_font_slot(10) == 0 && overlay_font_slot(0) == kLatinFontSlot && overlay_font_slot(6) == kLatinFontSlot);
    assert(slot_size_boost(kLatinFontSlot) == 1.0f && slot_size_boost(0) > 1.0f);
    assert(near(left_edge(500.f, 200.f, true), 400.f));
    assert(near(left_edge(500.f, 200.f, false), 500.f));

    // sRGB handling: identity on UNORM, darker mid-tones on SRGB targets, endpoints fixed.
    assert(color_channel(232, false) == 232);
    assert(color_channel(0, true) == 0 && color_channel(255, true) == 255);
    assert(color_channel(128, true) >= 54 && color_channel(128, true) <= 56);  // 0.2158 * 255 = 55
    assert(color_channel(232, true) < 232);

    // Language -> font slot (0 JP, 1 KR, 2 SC, 3 TC); everything else has none.
    assert(cjk_font_slot(10) == 0 && cjk_font_slot(8) == 1 && cjk_font_slot(12) == 2 && cjk_font_slot(9) == 3);
    for (int lang : {0, 1, 2, 3, 4, 5, 6, 7, 11, -1, 13}) assert(cjk_font_slot(lang) == -1);

    // Over-long text is cut without splitting a UTF-8 character (3-byte kanji at the boundary).
    {
        std::string longText;
        for (int i = 0; i < 400; ++i) longText += "\xE8\x8D\x92";  // 1200 bytes
        TextCmd t; t.set_text(longText.c_str());
        const std::size_t len = std::strlen(t.text);
        assert(len < kMaxTextBytes && len % 3 == 0 && len > 700);
        TextCmd e; e.set_text(nullptr); assert(e.empty());
        TextCmd s; s.set_text("short"); assert(std::strcmp(s.text, "short") == 0);
    }

    // Cross-thread hand-off: a producer publishing while a consumer snapshots must never crash or tear.
    FrameQueue shared;
    std::atomic<bool> stop{false};
    std::thread producer([&] {
        for (std::uint64_t i = 1; i <= 20000; ++i) {
            shared.begin();
            for (int k = 0; k < 5; ++k) { TextCmd t; t.set_text(("x" + std::to_string(k)).c_str()); shared.add(t); }
            shared.publish(i);
        }
        stop = true;
    });
    std::vector<TextCmd> seen;
    std::uint64_t snaps = 0;
    while (!stop) {
        if (shared.snapshot(seen, 1000000000ull, 2000000000ull)) { assert(seen.size() == 5); ++snaps; }
    }
    producer.join();
    assert(snaps > 0);  // the consumer really did observe published frames
    std::printf("overlay queue/layout tests passed (%llu concurrent snapshots, all consistent)\n",
                static_cast<unsigned long long>(snaps));
    return 0;
}
