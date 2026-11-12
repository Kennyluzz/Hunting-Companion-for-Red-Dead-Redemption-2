// CJK text overlay: Vulkan present hook + ImGui renderer.
// See cjk_overlay.hpp for the idea and third_party/README.md for the exact
// dependency versions this file was written against (ImGui 1.91.9).
//
// STATUS: the Vulkan path is tested in-game (Proton). For diagnostics set
// OverlayLog=1 in the ini and read the log (or WINEDEBUG=+debugstr).

#include "cjk_overlay.hpp"
#include "cjk_overlay_internal.hpp"
#include "embedded_cjk_fonts.hpp"
#include "overlay_queue.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "MinHook.h"

#include <atomic>
#include <cfloat>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

namespace hunting_companion::overlay {
namespace {

// ---------------------------------------------------------------------------
// Logging: debugger/Wine debug output always; a log file only if asked for.
// ---------------------------------------------------------------------------
Settings g_settings;
HANDLE g_logFile = INVALID_HANDLE_VALUE;
Lock g_logLock;

void hc_log(const char* fmt, ...) noexcept {
    char line[512];
    int n = std::snprintf(line, sizeof(line), "[HuntingCompanion/overlay] ");
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line + n, sizeof(line) - static_cast<size_t>(n) - 2, fmt, ap);
    va_end(ap);
    std::strcat(line, "\n");
    OutputDebugStringA(line);
    if (!g_settings.fileLog) return;
    Guard guard(g_logLock);
    if (g_logFile == INVALID_HANDLE_VALUE) {
        char path[MAX_PATH]{};
        std::snprintf(path, sizeof(path), "%s\\HuntingCompanion.overlay.log",
                      g_settings.logDirectory ? g_settings.logDirectory : ".");
        g_logFile = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
                                OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    if (g_logFile != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(g_logFile, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
    }
}

// ---------------------------------------------------------------------------
// Shared state between the script thread and the render thread.
// ---------------------------------------------------------------------------
FrameQueue g_queue;
std::atomic<int> g_language{0};
std::atomic<bool> g_ready{false};
std::atomic<bool> g_disabled{false};  // set after an unrecoverable error
std::atomic<int> g_backend{0};        // detail::Backend that owns ImGui

std::uint64_t now_ms() noexcept { return static_cast<std::uint64_t>(GetTickCount64()); }

// ---------------------------------------------------------------------------
// Vulkan plumbing
// ---------------------------------------------------------------------------
#define HC_DEVICE_FNS(X)                                                              \
    X(vkGetSwapchainImagesKHR) X(vkCreateImageView) X(vkDestroyImageView)             \
    X(vkCreateRenderPass) X(vkDestroyRenderPass) X(vkCreateFramebuffer)               \
    X(vkDestroyFramebuffer) X(vkCreateCommandPool) X(vkDestroyCommandPool)            \
    X(vkAllocateCommandBuffers) X(vkFreeCommandBuffers) X(vkBeginCommandBuffer)       \
    X(vkEndCommandBuffer) X(vkResetCommandBuffer) X(vkCmdBeginRenderPass)             \
    X(vkCmdEndRenderPass) X(vkCreateFence) X(vkDestroyFence) X(vkWaitForFences)       \
    X(vkResetFences) X(vkCreateSemaphore) X(vkDestroySemaphore)                       \
    X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) X(vkQueueSubmit)             \
    X(vkDeviceWaitIdle)

struct DeviceFns {
#define X(n) PFN_##n n = nullptr;
    HC_DEVICE_FNS(X)
#undef X
};

struct Slot {  // per swapchain image
    VkImageView view = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    VkSemaphore done = VK_NULL_HANDLE;
};

struct Swap {
    VkDevice device = VK_NULL_HANDLE;
    VkSwapchainKHR handle = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D extent{};
    bool canRender = false;       // swapchain images have COLOR_ATTACHMENT usage
    bool failed = false;
    bool resourcesReady = false;
    std::vector<VkImage> images;
    std::vector<Slot> slots;
};

struct Globals {
    SRWLOCK lock = SRWLOCK_INIT;

    HMODULE vulkan = nullptr;
    PFN_vkGetInstanceProcAddr realGIPA = nullptr;  // MinHook trampolines to the originals
    PFN_vkGetDeviceProcAddr realGDPA = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    std::unordered_map<VkDevice, VkPhysicalDevice> devicePhysical;
    std::unordered_map<VkQueue, std::uint32_t> queueFamily;
    std::uint32_t firstQueueFamily = 0;

    VkDevice fnDevice = VK_NULL_HANDLE;
    DeviceFns fn;

    std::unordered_map<VkSwapchainKHR, Swap> swaps;

    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkFormat renderPassFormat = VK_FORMAT_UNDEFINED;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::uint32_t commandPoolFamily = ~0u;
    VkDevice poolDevice = VK_NULL_HANDLE;

    bool imguiContext = false;
    bool imguiVulkan = false;
    ImFont* fonts[5]{};
    LARGE_INTEGER lastTime{};
    LARGE_INTEGER frequency{};
};
Globals* g = new Globals();  // intentionally leaked: avoids static-destruction order issues at exit

struct Locked {
    Locked() { AcquireSRWLockExclusive(&g->lock); }
    ~Locked() { ReleaseSRWLockExclusive(&g->lock); }
};

// What the game would have called, per route. The game may reach Vulkan through
// the loader's exported functions or through vkGet{Instance,Device}ProcAddr, so
// both are hooked, each with its own "real" pointers.
struct Real {
    PFN_vkCreateInstance createInstance = nullptr;
    PFN_vkCreateDevice createDevice = nullptr;
    PFN_vkDestroyDevice destroyDevice = nullptr;
    PFN_vkGetDeviceQueue getDeviceQueue = nullptr;
    PFN_vkCreateSwapchainKHR createSwapchain = nullptr;
    PFN_vkDestroySwapchainKHR destroySwapchain = nullptr;
    PFN_vkQueuePresentKHR queuePresent = nullptr;
};
Real g_realProc;    // pointers handed out by vkGet*ProcAddr
Real g_realExport;  // MinHook trampolines for the loader's exported functions

// Guards against the same call being intercepted twice (export hook + proc hook).
thread_local int t_depth = 0;
struct Depth {
    Depth() : nested(t_depth++ > 0) {}
    ~Depth() { --t_depth; }
    bool nested;
};

bool load_device_fns(VkDevice device) noexcept {
    if (g->fnDevice == device) return true;
    DeviceFns f;
    bool ok = g->realGDPA != nullptr;
    if (ok) {
#define X(n)                                                                              \
    f.n = reinterpret_cast<PFN_##n>(g->realGDPA(device, #n));                             \
    if (!f.n) { hc_log("missing device function %s", #n); ok = false; }
        HC_DEVICE_FNS(X)
#undef X
    }
    if (!ok) return false;
    g->fn = f;
    g->fnDevice = device;
    return true;
}

bool is_srgb(VkFormat f) noexcept {
    return f == VK_FORMAT_B8G8R8A8_SRGB || f == VK_FORMAT_R8G8B8A8_SRGB ||
           f == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
}

// ---------------------------------------------------------------------------
// Resource teardown (call with g->lock held)
// ---------------------------------------------------------------------------
void destroy_swap_resources(Swap& s) noexcept {
    if (!s.resourcesReady && s.slots.empty()) return;
    if (load_device_fns(s.device)) {
        const DeviceFns& f = g->fn;
        f.vkDeviceWaitIdle(s.device);
        for (Slot& slot : s.slots) {
            if (slot.fence) f.vkDestroyFence(s.device, slot.fence, nullptr);
            if (slot.done) f.vkDestroySemaphore(s.device, slot.done, nullptr);
            if (slot.cmd && g->commandPool) f.vkFreeCommandBuffers(s.device, g->commandPool, 1, &slot.cmd);
            if (slot.framebuffer) f.vkDestroyFramebuffer(s.device, slot.framebuffer, nullptr);
            if (slot.view) f.vkDestroyImageView(s.device, slot.view, nullptr);
        }
    }
    s.slots.clear();
    s.resourcesReady = false;
}

void destroy_imgui_vulkan() noexcept {
    if (g->imguiVulkan) {
        ImGui_ImplVulkan_Shutdown();
        g->imguiVulkan = false;
    }
}

void destroy_device_objects(VkDevice device) noexcept {
    for (auto& entry : g->swaps) {
        if (entry.second.device == device) destroy_swap_resources(entry.second);
    }
    if (!load_device_fns(device)) return;
    const DeviceFns& f = g->fn;
    f.vkDeviceWaitIdle(device);
    destroy_imgui_vulkan();
    if (g->renderPass) { f.vkDestroyRenderPass(device, g->renderPass, nullptr); g->renderPass = VK_NULL_HANDLE; }
    if (g->descriptorPool) { f.vkDestroyDescriptorPool(device, g->descriptorPool, nullptr); g->descriptorPool = VK_NULL_HANDLE; }
    if (g->commandPool) { f.vkDestroyCommandPool(device, g->commandPool, nullptr); g->commandPool = VK_NULL_HANDLE; }
    g->renderPassFormat = VK_FORMAT_UNDEFINED;
    g->commandPoolFamily = ~0u;
    g->poolDevice = VK_NULL_HANDLE;
}

// ---------------------------------------------------------------------------
// ImGui: context + fonts (once), Vulkan backend (per device / render-pass format)
// ---------------------------------------------------------------------------
bool create_imgui_context() noexcept {
    if (g->imguiContext) return true;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // never write imgui.ini
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse;

    static const ImWchar kRanges[] = {0x0020, 0xFFEF, 0};  // glyphs absent from the subset are skipped
    struct Src { const unsigned char* data; unsigned int size; const char* name; };
    const Src sources[5] = {
        {embedded_cjk_fonts::kJaJP, embedded_cjk_fonts::kJaJPSize, "NotoSansJP-subset"},
        {embedded_cjk_fonts::kKoKR, embedded_cjk_fonts::kKoKRSize, "NotoSansKR-subset"},
        {embedded_cjk_fonts::kZhCN, embedded_cjk_fonts::kZhCNSize, "NotoSansSC-subset"},
        {embedded_cjk_fonts::kZhTW, embedded_cjk_fonts::kZhTWSize, "NotoSansTC-subset"},
        {embedded_cjk_fonts::kLatin, embedded_cjk_fonts::kLatinSize, "NotoSans-latin-subset"},
    };
    for (int i = 0; i < 5; ++i) {
        ImFontConfig cfg;
        cfg.FontDataOwnedByAtlas = false;  // the bytes live in the .asi
        cfg.OversampleH = 1;
        cfg.OversampleV = 1;
        std::snprintf(cfg.Name, sizeof(cfg.Name), "%s", sources[i].name);
        g->fonts[i] = io.Fonts->AddFontFromMemoryTTF(
            const_cast<unsigned char*>(sources[i].data), static_cast<int>(sources[i].size),
            36.0f, &cfg, kRanges);
        if (!g->fonts[i]) hc_log("failed to load embedded font %s", sources[i].name);
    }
    if (!io.Fonts->Build()) {
        hc_log("ImGui font atlas build failed");
        return false;
    }
    g->imguiContext = true;
    QueryPerformanceFrequency(&g->frequency);
    QueryPerformanceCounter(&g->lastTime);
    hc_log("ImGui context ready, %d x %d font atlas", io.Fonts->TexWidth, io.Fonts->TexHeight);
    return true;
}

void VKAPI_CALL unused_stub() {}

PFN_vkVoidFunction imgui_loader(const char* name, void*) {
    PFN_vkVoidFunction fn = g->realGIPA ? g->realGIPA(g->instance, name) : nullptr;
    // ImGui's loader insists on the dynamic-rendering entry points even when unused. The
    // game rarely enables that extension; we use a render pass, so a stub is never called.
    if (!fn && name && (!std::strcmp(name, "vkCmdBeginRenderingKHR") ||
                        !std::strcmp(name, "vkCmdEndRenderingKHR"))) {
        fn = reinterpret_cast<PFN_vkVoidFunction>(&unused_stub);
    }
    if (!fn) hc_log("ImGui loader: %s is unavailable", name ? name : "?");
    return fn;
}

std::uint32_t family_for_queue(VkQueue queue) noexcept {
    auto it = g->queueFamily.find(queue);
    return it != g->queueFamily.end() ? it->second : g->firstQueueFamily;
}

bool create_render_pass(VkDevice device, VkFormat format) noexcept {
    const DeviceFns& f = g->fn;
    VkAttachmentDescription att{};
    att.format = format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;  // draw on top of the finished game frame
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1;
    sub.pColorAttachments = &ref;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rp{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rp.attachmentCount = 1;
    rp.pAttachments = &att;
    rp.subpassCount = 1;
    rp.pSubpasses = &sub;
    rp.dependencyCount = 1;
    rp.pDependencies = &dep;
    if (f.vkCreateRenderPass(device, &rp, nullptr, &g->renderPass) != VK_SUCCESS) {
        hc_log("vkCreateRenderPass failed");
        return false;
    }
    g->renderPassFormat = format;
    return true;
}

// Makes everything needed to draw into `s` (call with g->lock held, on the render
// thread, from inside a present so the queue is known). Idempotent.
bool ensure_ready(Swap& s, VkQueue queue) noexcept {
    if (s.failed || !s.canRender) return false;
    if (!detail::claim_backend(detail::Backend::Vulkan)) return false;
    if (s.resourcesReady && g->imguiVulkan && g->renderPassFormat == s.format) return true;
    if (!load_device_fns(s.device)) { s.failed = true; return false; }
    const DeviceFns& f = g->fn;
    const std::uint32_t family = family_for_queue(queue);

    if (!create_imgui_context()) { s.failed = true; return false; }

    // Render pass is tied to the swapchain format; ImGui's pipeline is tied to the render pass.
    if (g->renderPass && g->renderPassFormat != s.format) {
        for (auto& entry : g->swaps) destroy_swap_resources(entry.second);
        f.vkDeviceWaitIdle(s.device);
        destroy_imgui_vulkan();
        f.vkDestroyRenderPass(s.device, g->renderPass, nullptr);
        g->renderPass = VK_NULL_HANDLE;
    }
    if (!g->renderPass && !create_render_pass(s.device, s.format)) { s.failed = true; return false; }

    if (g->commandPool && g->commandPoolFamily != family) {
        for (auto& entry : g->swaps) destroy_swap_resources(entry.second);
        f.vkDestroyCommandPool(s.device, g->commandPool, nullptr);
        g->commandPool = VK_NULL_HANDLE;
    }
    if (!g->commandPool) {
        VkCommandPoolCreateInfo ci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        ci.queueFamilyIndex = family;
        if (f.vkCreateCommandPool(s.device, &ci, nullptr, &g->commandPool) != VK_SUCCESS) {
            hc_log("vkCreateCommandPool failed"); s.failed = true; return false;
        }
        g->commandPoolFamily = family;
        g->poolDevice = s.device;
    }
    if (!g->descriptorPool) {
        VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16};
        VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        ci.maxSets = 16;
        ci.poolSizeCount = 1;
        ci.pPoolSizes = &size;
        if (f.vkCreateDescriptorPool(s.device, &ci, nullptr, &g->descriptorPool) != VK_SUCCESS) {
            hc_log("vkCreateDescriptorPool failed"); s.failed = true; return false;
        }
    }

    if (!g->imguiVulkan) {
        auto phys = g->devicePhysical.find(s.device);
        if (phys == g->devicePhysical.end()) { hc_log("physical device unknown"); s.failed = true; return false; }
        if (!ImGui_ImplVulkan_LoadFunctions(VK_API_VERSION_1_0, imgui_loader, nullptr)) {
            hc_log("ImGui_ImplVulkan_LoadFunctions failed"); s.failed = true; return false;
        }
        ImGui_ImplVulkan_InitInfo ii{};
        ii.Instance = g->instance;
        ii.PhysicalDevice = phys->second;
        ii.Device = s.device;
        ii.QueueFamily = family;
        ii.Queue = queue;
        ii.DescriptorPool = g->descriptorPool;
        ii.RenderPass = g->renderPass;
        ii.MinImageCount = 2;
        ii.ImageCount = static_cast<uint32_t>(s.images.size() < 2 ? 2 : s.images.size());
        ii.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        ii.Subpass = 0;
        if (!ImGui_ImplVulkan_Init(&ii)) {
            hc_log("ImGui_ImplVulkan_Init failed"); s.failed = true; return false;
        }
        g->imguiVulkan = true;
        // Upload the font atlas now (a short one-off stall) instead of on the first
        // frame the menu opens. NewFrame of the Vulkan backend creates the texture.
        ImGui_ImplVulkan_NewFrame();
    }

    // Per-image resources.
    if (!s.resourcesReady) {
        s.slots.assign(s.images.size(), Slot{});
        std::vector<VkCommandBuffer> cmds(s.images.size());
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = g->commandPool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = static_cast<uint32_t>(cmds.size());
        if (f.vkAllocateCommandBuffers(s.device, &ai, cmds.data()) != VK_SUCCESS) {
            hc_log("vkAllocateCommandBuffers failed"); s.failed = true; return false;
        }
        for (size_t i = 0; i < s.images.size(); ++i) {
            Slot& slot = s.slots[i];
            slot.cmd = cmds[i];
            VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            vi.image = s.images[i];
            vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vi.format = s.format;
            vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            VkFramebufferCreateInfo fi{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            fi.renderPass = g->renderPass;
            fi.attachmentCount = 1;
            fi.width = s.extent.width;
            fi.height = s.extent.height;
            fi.layers = 1;
            VkFenceCreateInfo fci{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
            fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
            VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            bool ok = f.vkCreateImageView(s.device, &vi, nullptr, &slot.view) == VK_SUCCESS;
            if (ok) { fi.pAttachments = &slot.view; ok = f.vkCreateFramebuffer(s.device, &fi, nullptr, &slot.framebuffer) == VK_SUCCESS; }
            if (ok) ok = f.vkCreateFence(s.device, &fci, nullptr, &slot.fence) == VK_SUCCESS;
            if (ok) ok = f.vkCreateSemaphore(s.device, &sci, nullptr, &slot.done) == VK_SUCCESS;
            if (!ok) {
                hc_log("per-image resource creation failed (image %zu)", i);
                destroy_swap_resources(s);
                s.failed = true;
                return false;
            }
        }
        s.resourcesReady = true;
        hc_log("overlay ready: %ux%u, %zu images, format %d", s.extent.width, s.extent.height,
               s.images.size(), static_cast<int>(s.format));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
void imgui_begin_io(float width, float height) noexcept {
    ImGuiIO& io = ImGui::GetIO();
    LARGE_INTEGER nowTick;
    QueryPerformanceCounter(&nowTick);
    double dt = static_cast<double>(nowTick.QuadPart - g->lastTime.QuadPart) /
                static_cast<double>(g->frequency.QuadPart);
    g->lastTime = nowTick;
    io.DeltaTime = dt > 0.0001 ? static_cast<float>(dt) : 0.0001f;
    io.DisplaySize = ImVec2(width, height);
}

void imgui_add_text(const std::vector<TextCmd>& cmds, float W, float H,
                    TargetEncoding encoding) noexcept {
    const int slot = overlay_font_slot(g_language.load());
    ImFont* font = g->fonts[slot];
    const float boost = slot_size_boost(slot);
    if (!font) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    for (const TextCmd& c : cmds) {
        if (c.empty()) continue;
        const float px = font_pixels(c.referencePixelHeight, H, g_settings.textScale * boost);
        ImVec2 pos(c.x * W, (c.y + g_settings.yOffset) * H);
        if (c.centered) {
            const ImVec2 size = font->CalcTextSizeA(px, FLT_MAX, 0.0f, c.text);
            pos.x = left_edge(pos.x, size.x, true);
        }
        std::uint8_t rgb[3];
        encode_color(static_cast<std::uint8_t>(c.r), static_cast<std::uint8_t>(c.g),
                     static_cast<std::uint8_t>(c.b), encoding,
                     g_settings.hdrPaperWhiteNits, rgb);
        const ImU32 col = IM_COL32(rgb[0], rgb[1], rgb[2], c.a);
        dl->AddText(font, px, pos, col, c.text);
    }
}

void build_imgui_frame(const std::vector<TextCmd>& cmds, const Swap& s) noexcept {
    const float W = static_cast<float>(s.extent.width), H = static_cast<float>(s.extent.height);
    imgui_begin_io(W, H);
    ImGui_ImplVulkan_NewFrame();  // creates the font texture on first use
    ImGui::NewFrame();
    imgui_add_text(cmds, W, H, target_encoding(is_srgb(s.format), static_cast<int>(s.colorSpace)));
    ImGui::Render();
}

// Draws the overlay for this present. On success fills `patched`/`outSemaphore`
// so the real present waits on our work instead of the game's semaphores.
bool draw_overlay(VkQueue queue, const VkPresentInfoKHR& pi, const std::vector<TextCmd>& cmds,
                  VkSemaphore& outDone) noexcept {
    auto it = g->swaps.find(pi.pSwapchains[0]);
    if (it == g->swaps.end()) return false;
    Swap& s = it->second;
    if (!s.resourcesReady) return false;
    const std::uint32_t index = pi.pImageIndices[0];
    if (index >= s.slots.size()) return false;
    // Without a wait semaphore we cannot know the game has finished drawing.
    if (pi.waitSemaphoreCount == 0) return false;

    const DeviceFns& f = g->fn;
    Slot& slot = s.slots[index];
    if (f.vkWaitForFences(s.device, 1, &slot.fence, VK_TRUE, 100000000ull) != VK_SUCCESS) return false;

    build_imgui_frame(cmds, s);

    f.vkResetCommandBuffer(slot.cmd, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    f.vkBeginCommandBuffer(slot.cmd, &bi);
    VkRenderPassBeginInfo rb{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    rb.renderPass = g->renderPass;
    rb.framebuffer = slot.framebuffer;
    rb.renderArea.extent = s.extent;
    f.vkCmdBeginRenderPass(slot.cmd, &rb, VK_SUBPASS_CONTENTS_INLINE);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), slot.cmd);
    f.vkCmdEndRenderPass(slot.cmd);
    if (f.vkEndCommandBuffer(slot.cmd) != VK_SUCCESS) return false;

    std::vector<VkPipelineStageFlags> stages(pi.waitSemaphoreCount,
                                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
    VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    si.waitSemaphoreCount = pi.waitSemaphoreCount;
    si.pWaitSemaphores = pi.pWaitSemaphores;
    si.pWaitDstStageMask = stages.data();
    si.commandBufferCount = 1;
    si.pCommandBuffers = &slot.cmd;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = &slot.done;
    f.vkResetFences(s.device, 1, &slot.fence);
    if (f.vkQueueSubmit(queue, 1, &si, slot.fence) != VK_SUCCESS) {
        hc_log("overlay queue submit failed; disabling overlay");
        s.failed = true;  // the fence is now unsignalled; stop using this swapchain
        return false;
    }
    outDone = slot.done;  // the real present waits on this instead of the game's semaphores
    return true;
}

// ---------------------------------------------------------------------------
// Hook handlers (route-agnostic; `real` is whatever the game would have called)
// ---------------------------------------------------------------------------
VkResult on_create_instance(PFN_vkCreateInstance real, const VkInstanceCreateInfo* ci,
                            const VkAllocationCallbacks* a, VkInstance* out) {
    if (!real) return VK_ERROR_INITIALIZATION_FAILED;
    Depth depth;
    VkResult r = real(ci, a, out);
    if (!depth.nested && r == VK_SUCCESS && out && *out) {
        Locked lock;
        if (g->instance != VK_NULL_HANDLE && g->instance != *out)
            hc_log("a second VkInstance was created; the overlay follows the newest one");
        g->instance = *out;
        hc_log("vkCreateInstance observed");
    }
    return r;
}

VkResult on_create_device(PFN_vkCreateDevice real, VkPhysicalDevice phys,
                          const VkDeviceCreateInfo* ci, const VkAllocationCallbacks* a, VkDevice* out) {
    if (!real) return VK_ERROR_INITIALIZATION_FAILED;
    Depth depth;
    VkResult r = real(phys, ci, a, out);
    if (!depth.nested && r == VK_SUCCESS && out && *out) {
        Locked lock;
        g->devicePhysical[*out] = phys;
        if (ci && ci->queueCreateInfoCount > 0) g->firstQueueFamily = ci->pQueueCreateInfos[0].queueFamilyIndex;
        hc_log("vkCreateDevice observed");
    }
    return r;
}

void on_get_device_queue(PFN_vkGetDeviceQueue real, VkDevice device, std::uint32_t family,
                         std::uint32_t index, VkQueue* queue) {
    if (!real) return;
    Depth depth;
    real(device, family, index, queue);
    if (!depth.nested && queue && *queue) {
        Locked lock;
        g->queueFamily[*queue] = family;
    }
}

void on_destroy_device(PFN_vkDestroyDevice real, VkDevice device, const VkAllocationCallbacks* a) {
    if (!real) return;
    Depth depth;
    if (!depth.nested) {
        Locked lock;
        g_ready = false;
        destroy_device_objects(device);
        for (auto it = g->swaps.begin(); it != g->swaps.end();)
            it = (it->second.device == device) ? g->swaps.erase(it) : std::next(it);
        g->devicePhysical.erase(device);
        if (g->fnDevice == device) g->fnDevice = VK_NULL_HANDLE;
    }
    real(device, a);
}

VkResult on_create_swapchain(PFN_vkCreateSwapchainKHR real, VkDevice device,
                             const VkSwapchainCreateInfoKHR* ci, const VkAllocationCallbacks* a,
                             VkSwapchainKHR* out) {
    if (!real) return VK_ERROR_INITIALIZATION_FAILED;
    Depth depth;
    if (depth.nested || !ci) return real(device, ci, a, out);

    // Ask for COLOR_ATTACHMENT usage so we can draw into the images; fall back
    // to the game's own request if the surface refuses it.
    VkSwapchainCreateInfoKHR modified = *ci;
    modified.imageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    bool canRender = true;
    VkResult r = real(device, &modified, a, out);
    if (r != VK_SUCCESS) {
        canRender = (ci->imageUsage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0;
        r = real(device, ci, a, out);
    }
    if (r != VK_SUCCESS || !out || !*out) return r;

    Locked lock;
    // Retire the previous swapchain's resources (resize / mode change).
    for (auto& entry : g->swaps) {
        if (entry.second.device == device && entry.first != *out) destroy_swap_resources(entry.second);
    }
    Swap s;
    s.device = device;
    s.handle = *out;
    s.format = ci->imageFormat;
    s.colorSpace = ci->imageColorSpace;
    s.extent = ci->imageExtent;
    s.canRender = canRender;
    if (!load_device_fns(device)) {
        s.failed = true;
    } else {
        std::uint32_t count = 0;
        if (g->fn.vkGetSwapchainImagesKHR(device, *out, &count, nullptr) == VK_SUCCESS && count > 0) {
            s.images.resize(count);
            if (g->fn.vkGetSwapchainImagesKHR(device, *out, &count, s.images.data()) != VK_SUCCESS) s.failed = true;
        } else {
            s.failed = true;
        }
    }
    hc_log("vkCreateSwapchainKHR observed: %ux%u format %d colour space %d, %zu images, drawable=%d",
           s.extent.width, s.extent.height, static_cast<int>(s.format),
           static_cast<int>(s.colorSpace), s.images.size(),
           s.canRender && !s.failed);
    g->swaps[*out] = std::move(s);
    return r;
}

void on_destroy_swapchain(PFN_vkDestroySwapchainKHR real, VkDevice device, VkSwapchainKHR sc,
                          const VkAllocationCallbacks* a) {
    if (!real) return;
    Depth depth;
    if (!depth.nested) {
        Locked lock;
        auto it = g->swaps.find(sc);
        if (it != g->swaps.end()) {
            destroy_swap_resources(it->second);
            g->swaps.erase(it);
        }
        bool any = false;
        for (auto& entry : g->swaps) any = any || entry.second.resourcesReady;
        if (!any) g_ready = false;
    }
    real(device, sc, a);
}

VkResult on_present(PFN_vkQueuePresentKHR real, VkQueue queue, const VkPresentInfoKHR* pi) {
    if (!real) return VK_ERROR_INITIALIZATION_FAILED;
    Depth depth;
    if (depth.nested || g_disabled.load() || !pi || pi->swapchainCount != 1 ||
        !pi->pSwapchains || !pi->pImageIndices) {
        return real(queue, pi);
    }
    // The DirectX 12 renderer owns the overlay (or the ini asks for it): stay out of
    // the way entirely, including the ready flag.
    if (g_settings.backend == 2 ||
        g_backend.load() == static_cast<int>(detail::Backend::Dx12)) {
        return real(queue, pi);
    }

    VkSemaphore done = VK_NULL_HANDLE;
    bool use = false;
    {
        Locked lock;
        auto it = g->swaps.find(pi->pSwapchains[0]);
        if (it != g->swaps.end() && it->second.canRender && !it->second.failed) {
            if (ensure_ready(it->second, queue)) {
                if (!g_ready.exchange(true)) hc_log("overlay is ready");
                static thread_local std::vector<TextCmd> cmds;
                if (g_queue.snapshot(cmds, now_ms(), 80) && !cmds.empty()) {
                    try {
                        use = draw_overlay(queue, *pi, cmds, done);
                    } catch (...) {
                        hc_log("exception while drawing overlay; disabling it");
                        g_disabled = true;
                        g_ready = false;
                    }
                }
            } else {
                g_ready = false;
            }
        }
    }
    if (!use) return real(queue, pi);
    // Same present, but waiting on our overlay work (which itself waited on the game's semaphores).
    VkPresentInfoKHR patched = *pi;
    patched.waitSemaphoreCount = 1;
    patched.pWaitSemaphores = &done;
    return real(queue, &patched);
}

// Per-route entry points, generated from one template so the two routes cannot drift.
template <Real* R>
struct H {
    static VKAPI_ATTR VkResult VKAPI_CALL CreateInstance(const VkInstanceCreateInfo* ci, const VkAllocationCallbacks* a, VkInstance* o) { return on_create_instance(R->createInstance, ci, a, o); }
    static VKAPI_ATTR VkResult VKAPI_CALL CreateDevice(VkPhysicalDevice p, const VkDeviceCreateInfo* ci, const VkAllocationCallbacks* a, VkDevice* o) { return on_create_device(R->createDevice, p, ci, a, o); }
    static VKAPI_ATTR void VKAPI_CALL DestroyDevice(VkDevice d, const VkAllocationCallbacks* a) { on_destroy_device(R->destroyDevice, d, a); }
    static VKAPI_ATTR void VKAPI_CALL GetDeviceQueue(VkDevice d, uint32_t f, uint32_t i, VkQueue* q) { on_get_device_queue(R->getDeviceQueue, d, f, i, q); }
    static VKAPI_ATTR VkResult VKAPI_CALL CreateSwapchain(VkDevice d, const VkSwapchainCreateInfoKHR* ci, const VkAllocationCallbacks* a, VkSwapchainKHR* o) { return on_create_swapchain(R->createSwapchain, d, ci, a, o); }
    static VKAPI_ATTR void VKAPI_CALL DestroySwapchain(VkDevice d, VkSwapchainKHR s, const VkAllocationCallbacks* a) { on_destroy_swapchain(R->destroySwapchain, d, s, a); }
    static VKAPI_ATTR VkResult VKAPI_CALL QueuePresent(VkQueue q, const VkPresentInfoKHR* pi) { return on_present(R->queuePresent, q, pi); }
};

// vkGetInstanceProcAddr / vkGetDeviceProcAddr: swap in our wrappers for the functions we watch.
PFN_vkVoidFunction intercept(PFN_vkVoidFunction fn, const char* name) {
    if (!fn || !name) return fn;
    using P = H<&g_realProc>;
#define HC_ICPT(vkName, field, handler)                                                    \
    if (!std::strcmp(name, vkName)) {                                                      \
        if (!g_realProc.field) g_realProc.field = reinterpret_cast<decltype(g_realProc.field)>(fn); \
        return reinterpret_cast<PFN_vkVoidFunction>(&P::handler);                          \
    }
    HC_ICPT("vkCreateInstance", createInstance, CreateInstance)
    HC_ICPT("vkCreateDevice", createDevice, CreateDevice)
    HC_ICPT("vkDestroyDevice", destroyDevice, DestroyDevice)
    HC_ICPT("vkGetDeviceQueue", getDeviceQueue, GetDeviceQueue)
    HC_ICPT("vkCreateSwapchainKHR", createSwapchain, CreateSwapchain)
    HC_ICPT("vkDestroySwapchainKHR", destroySwapchain, DestroySwapchain)
    HC_ICPT("vkQueuePresentKHR", queuePresent, QueuePresent)
#undef HC_ICPT
    return fn;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL hk_GetInstanceProcAddr(VkInstance instance, const char* name) {
    PFN_vkVoidFunction fn = g->realGIPA(instance, name);
    return intercept(fn, name);
}
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL hk_GetDeviceProcAddr(VkDevice device, const char* name) {
    PFN_vkVoidFunction fn = g->realGDPA(device, name);
    return intercept(fn, name);
}

// ---------------------------------------------------------------------------
// Installation (worker thread, so DllMain does no real work under the loader lock)
// ---------------------------------------------------------------------------
template <typename Fn>
bool hook_export(HMODULE module, const char* name, void* detour, Fn* trampoline) {
    void* target = reinterpret_cast<void*>(GetProcAddress(module, name));
    if (!target) { hc_log("export %s not found", name); return false; }
    const MH_STATUS st = MH_CreateHook(target, detour, reinterpret_cast<void**>(trampoline));
    if (st != MH_OK) { hc_log("MH_CreateHook(%s) failed: %s", name, MH_StatusToString(st)); return false; }
    return true;
}

DWORD WINAPI install_thread(LPVOID) {
    // Load the loader ourselves (refcounted; the game will get the same module) so the
    // hooks are in place before the game creates its VkInstance.
    HMODULE vk = LoadLibraryW(L"vulkan-1.dll");
    const MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        hc_log("MH_Initialize failed: %s", MH_StatusToString(init));
        return 0;
    }
    if (!vk) {
        // No Vulkan on this machine: the DirectX 12 renderer can still work.
        hc_log("vulkan-1.dll could not be loaded; Vulkan overlay unavailable");
        if (g_settings.backend != 1) detail::install_dx12_hooks();
        return 0;
    }
    g->vulkan = vk;

    bool ok = true;
    ok &= hook_export(vk, "vkGetInstanceProcAddr", reinterpret_cast<void*>(&hk_GetInstanceProcAddr), &g->realGIPA);
    ok &= hook_export(vk, "vkGetDeviceProcAddr", reinterpret_cast<void*>(&hk_GetDeviceProcAddr), &g->realGDPA);
    if (!ok) { hc_log("could not hook the proc-address getters; overlay unavailable"); return 0; }

    // Direct (statically linked) callers: hook the same functions by export as well.
    using E = H<&g_realExport>;
    hook_export(vk, "vkCreateInstance", reinterpret_cast<void*>(&E::CreateInstance), &g_realExport.createInstance);
    hook_export(vk, "vkCreateDevice", reinterpret_cast<void*>(&E::CreateDevice), &g_realExport.createDevice);
    hook_export(vk, "vkDestroyDevice", reinterpret_cast<void*>(&E::DestroyDevice), &g_realExport.destroyDevice);
    hook_export(vk, "vkGetDeviceQueue", reinterpret_cast<void*>(&E::GetDeviceQueue), &g_realExport.getDeviceQueue);
    hook_export(vk, "vkCreateSwapchainKHR", reinterpret_cast<void*>(&E::CreateSwapchain), &g_realExport.createSwapchain);
    hook_export(vk, "vkDestroySwapchainKHR", reinterpret_cast<void*>(&E::DestroySwapchain), &g_realExport.destroySwapchain);
    hook_export(vk, "vkQueuePresentKHR", reinterpret_cast<void*>(&E::QueuePresent), &g_realExport.queuePresent);

    const MH_STATUS en = MH_EnableHook(MH_ALL_HOOKS);
    if (en != MH_OK) { hc_log("MH_EnableHook failed: %s", MH_StatusToString(en)); return 0; }
    hc_log("Vulkan hooks installed; waiting for the game to create a swapchain");
    if (g_settings.backend != 1) detail::install_dx12_hooks();
    return 0;
}

} // namespace

// ---------------------------------------------------------------------------
// Shared with the DirectX 12 renderer (cjk_overlay_internal.hpp)
// ---------------------------------------------------------------------------
namespace detail {

void log(const char* fmt, ...) noexcept {
    char line[400];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    hc_log("%s", line);
}

const Settings& settings() noexcept { return g_settings; }

void lock() noexcept { AcquireSRWLockExclusive(&g->lock); }
void unlock() noexcept { ReleaseSRWLockExclusive(&g->lock); }

bool claim_backend(Backend backend) noexcept {
    int expected = 0;
    const int wanted = static_cast<int>(backend);
    return g_backend.compare_exchange_strong(expected, wanted) || expected == wanted;
}
Backend current_backend() noexcept { return static_cast<Backend>(g_backend.load()); }

bool create_context() noexcept { return create_imgui_context(); }
void begin_frame_io(float width, float height) noexcept {
    imgui_begin_io(width, height);
}
void add_text(const std::vector<TextCmd>& cmds, float width, float height,
              TargetEncoding encoding) noexcept {
    imgui_add_text(cmds, width, height, encoding);
}

bool take_frame(std::vector<TextCmd>& out) noexcept {
    return g_queue.snapshot(out, now_ms(), 80) && !out.empty();
}

void set_ready(bool ready) noexcept { g_ready = ready; }
bool disabled() noexcept { return g_disabled.load(); }

} // namespace detail

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void install(void* moduleHandle, const Settings& settings) noexcept {
    g_settings = settings;
    // The hooks hand function pointers into this DLL to the game; never let it unload.
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(&install), &pinned);
    (void)moduleHandle;
    HANDLE thread = CreateThread(nullptr, 0, install_thread, nullptr, 0, nullptr);
    if (thread) CloseHandle(thread);
}

bool ready() noexcept { return g_ready.load() && !g_disabled.load(); }

void set_language(int language) noexcept { g_language.store(language); }

void begin_frame() noexcept { g_queue.begin(); }

void submit(const char* utf8, float x, float y, float referencePixelHeight,
            int r, int gr, int b, int a, bool centered) noexcept {
    if (utf8 == nullptr || utf8[0] == '\0') return;
    TextCmd cmd;
    cmd.set_text(utf8);
    cmd.x = x;
    cmd.y = y;
    cmd.referencePixelHeight = referencePixelHeight;
    cmd.r = static_cast<std::uint8_t>(r);
    cmd.g = static_cast<std::uint8_t>(gr);
    cmd.b = static_cast<std::uint8_t>(b);
    cmd.a = static_cast<std::uint8_t>(a);
    cmd.centered = centered;
    g_queue.add(cmd);
}

void end_frame() noexcept { g_queue.publish(now_ms()); }

} // namespace hunting_companion::overlay
