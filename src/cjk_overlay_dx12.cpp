// CJK text overlay: DirectX 12 renderer (EXPERIMENTAL).
//
// The Vulkan renderer in cjk_overlay.cpp is the proven one. This file does the
// same job when the game runs on DirectX 12: it watches the game create its
// DXGI swapchain, and just before each Present draws the queued text into the
// back buffer with Dear ImGui's DirectX 12 backend.
//
// STATUS: experimental. Confirmed working once under Proton (DirectX 12
// translated by vkd3d); not yet tested on native Windows. To keep the risk low it
//   * creates no Direct3D objects until there is CJK text to draw,
//   * turns itself off for the rest of the session after any failure,
//   * writes HuntingCompanion.overlay.log only when [Overlay] OverlayLog=1.
// [Overlay] OverlayBackend=1 in the ini switches this renderer off completely.

#include "cjk_overlay_internal.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_4.h>

#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "MinHook.h"

#include <vector>

// ---------------------------------------------------------------------------
// D3DCompile: ImGui's backend compiles two tiny shaders with it. Linking the
// import library would make the whole mod fail to load on a system without
// d3dcompiler_47.dll, so the call is forwarded at run time instead.
// ---------------------------------------------------------------------------
extern "C" HRESULT WINAPI D3DCompile(const void* data, SIZE_T data_size, const char* filename,
                                     const D3D_SHADER_MACRO* defines, ID3DInclude* include,
                                     const char* entrypoint, const char* target, UINT sflags,
                                     UINT eflags, ID3DBlob** shader, ID3DBlob** error_messages) {
    static pD3DCompile real = []() -> pD3DCompile {
        HMODULE module = LoadLibraryW(L"d3dcompiler_47.dll");
        if (!module) return nullptr;
        return reinterpret_cast<pD3DCompile>(
            reinterpret_cast<void*>(GetProcAddress(module, "D3DCompile")));
    }();
    if (!real) return E_NOTIMPL;
    return real(data, data_size, filename, defines, include, entrypoint, target, sflags, eflags,
                shader, error_messages);
}

namespace hunting_companion::overlay::detail {
namespace {

// vtable slots (checked against the mingw-w64 dxgi headers)
constexpr int kFactoryCreateSwapChain = 10;         // IDXGIFactory
constexpr int kFactoryCreateSwapChainForHwnd = 15;  // IDXGIFactory2
constexpr int kSwapPresent = 8;                     // IDXGISwapChain
constexpr int kSwapResizeBuffers = 13;              // IDXGISwapChain
constexpr int kSwapPresent1 = 22;                   // IDXGISwapChain1
constexpr int kSwapSetColorSpace1 = 38;             // IDXGISwapChain3
constexpr int kSwapResizeBuffers1 = 39;             // IDXGISwapChain3

using CreateFactoryFn = HRESULT(WINAPI*)(REFIID, void**);
using CreateFactory2Fn = HRESULT(WINAPI*)(UINT, REFIID, void**);
using CreateSwapChainFn = HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*, IUnknown*,
                                                      DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**);
using CreateSwapChainForHwndFn = HRESULT(STDMETHODCALLTYPE*)(
    IDXGIFactory2*, IUnknown*, HWND, const DXGI_SWAP_CHAIN_DESC1*,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*, IDXGIOutput*, IDXGISwapChain1**);
using PresentFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);
using Present1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*, UINT, UINT,
                                               const DXGI_PRESENT_PARAMETERS*);
using ResizeBuffersFn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT, UINT,
                                                    DXGI_FORMAT, UINT);
using ResizeBuffers1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, UINT, UINT, UINT,
                                                     DXGI_FORMAT, UINT, const UINT*,
                                                     IUnknown* const*);
using SetColorSpace1Fn = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*, DXGI_COLOR_SPACE_TYPE);

CreateFactoryFn g_realCreateFactory = nullptr;
CreateFactoryFn g_realCreateFactory1 = nullptr;
CreateFactory2Fn g_realCreateFactory2 = nullptr;
CreateSwapChainFn g_realCreateSwapChain = nullptr;
CreateSwapChainForHwndFn g_realCreateSwapChainForHwnd = nullptr;
PresentFn g_realPresent = nullptr;
Present1Fn g_realPresent1 = nullptr;
ResizeBuffersFn g_realResizeBuffers = nullptr;
ResizeBuffers1Fn g_realResizeBuffers1 = nullptr;
SetColorSpace1Fn g_realSetColorSpace1 = nullptr;

struct Frame {  // one per swapchain back buffer
    ID3D12Resource* buffer = nullptr;
    ID3D12CommandAllocator* allocator = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
    UINT64 fenceValue = 0;
};

// Everything below is guarded by the shared overlay lock.
struct State {
    IDXGISwapChain* swapchain = nullptr;   // identity only, never dereferenced or owned
    ID3D12CommandQueue* queue = nullptr;   // owned reference
    ID3D12Device* device = nullptr;        // owned reference
    ID3D12DescriptorHeap* rtvHeap = nullptr;
    ID3D12DescriptorHeap* srvHeap = nullptr;
    ID3D12GraphicsCommandList* list = nullptr;
    ID3D12Fence* fence = nullptr;
    HANDLE fenceEvent = nullptr;
    UINT64 fenceCounter = 0;
    std::vector<Frame> frames;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    DXGI_FORMAT imguiFormat = DXGI_FORMAT_UNKNOWN;
    UINT width = 0;
    UINT height = 0;
    int colorSpace = 0;                    // DXGI_COLOR_SPACE_TYPE last set by the game
    bool buffersReady = false;
    bool imguiReady = false;
    bool failed = false;                   // stays off for the rest of the session
    bool drewFirstFrame = false;
};
State g_state;

struct Depth {  // Present may call Present1 internally (or the reverse): draw once
    static thread_local int level;
    bool nested;
    Depth() : nested(level++ > 0) {}
    ~Depth() { --level; }
};
thread_local int Depth::level = 0;

template <typename T>
void release(T*& object) noexcept {
    if (object) {
        object->Release();
        object = nullptr;
    }
}

// Waits (bounded) until the GPU has finished every overlay command list.
void wait_for_gpu(State& s) noexcept {
    if (!s.fence || !s.fenceEvent || s.fenceCounter == 0) return;
    if (s.fence->GetCompletedValue() >= s.fenceCounter) return;
    if (SUCCEEDED(s.fence->SetEventOnCompletion(s.fenceCounter, s.fenceEvent))) {
        WaitForSingleObject(s.fenceEvent, 1000);
    }
}

// Back-buffer references must be gone before the game resizes or replaces its
// swapchain, otherwise those calls fail.
void release_buffers(State& s) noexcept {
    if (s.frames.empty() && !s.rtvHeap) return;
    wait_for_gpu(s);
    for (Frame& frame : s.frames) {
        release(frame.buffer);
        release(frame.allocator);
    }
    s.frames.clear();
    release(s.rtvHeap);
    release(s.list);  // it was created against one of the allocators
    s.buffersReady = false;
}

void release_everything(State& s) noexcept {
    release_buffers(s);
    if (s.imguiReady) {
        ImGui_ImplDX12_Shutdown();
        s.imguiReady = false;
    }
    release(s.srvHeap);
    release(s.fence);
    if (s.fenceEvent) {
        CloseHandle(s.fenceEvent);
        s.fenceEvent = nullptr;
    }
    s.fenceCounter = 0;
    release(s.device);
}

void fail(State& s, const char* step, HRESULT hr) noexcept {
    log("DirectX 12 overlay: %s failed (0x%08lX); overlay switched off for this session", step,
        static_cast<unsigned long>(hr));
    s.failed = true;
    release_everything(s);
    set_ready(false);
}

void srv_alloc(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
               D3D12_GPU_DESCRIPTOR_HANDLE* gpu) {
    // The overlay needs exactly one texture (the font atlas), so the heap has one slot.
    *cpu = g_state.srvHeap->GetCPUDescriptorHandleForHeapStart();
    *gpu = g_state.srvHeap->GetGPUDescriptorHandleForHeapStart();
}
void srv_free(ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {}

TargetEncoding encoding_for(const State& s) noexcept {
    if (s.colorSpace == static_cast<int>(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020))
        return TargetEncoding::Hdr10Pq;
    // scRGB (float back buffer) takes linear values; 8-bit vertex colours cap it at SDR white.
    if (s.format == DXGI_FORMAT_R16G16B16A16_FLOAT) return TargetEncoding::SrgbFormat;
    return TargetEncoding::Plain;
}

// Creates whatever is missing to draw into `swapchain`. Returns false (after
// calling fail) when something could not be created.
bool ensure_resources(State& s, IDXGISwapChain* swapchain) noexcept {
    HRESULT hr = S_OK;
    if (!s.device) {
        hr = s.queue->GetDevice(IID_PPV_ARGS(&s.device));
        if (FAILED(hr) || !s.device) { fail(s, "ID3D12CommandQueue::GetDevice", hr); return false; }
        log("DirectX 12 overlay: device obtained from the game's command queue");
    }

    DXGI_SWAP_CHAIN_DESC desc{};
    hr = swapchain->GetDesc(&desc);
    if (FAILED(hr)) { fail(s, "IDXGISwapChain::GetDesc", hr); return false; }
    if (desc.BufferCount == 0 || desc.BufferCount > 16) {
        fail(s, "unexpected back-buffer count", E_UNEXPECTED);
        return false;
    }

    if (s.buffersReady && (s.frames.size() != desc.BufferCount || s.format != desc.BufferDesc.Format ||
                           s.width != desc.BufferDesc.Width || s.height != desc.BufferDesc.Height)) {
        release_buffers(s);
    }
    if (s.imguiReady && s.imguiFormat != desc.BufferDesc.Format) {
        // ImGui's pipeline is tied to the render-target format (SDR <-> HDR switch).
        wait_for_gpu(s);
        ImGui_ImplDX12_Shutdown();
        s.imguiReady = false;
    }

    if (!s.fence) {
        hr = s.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&s.fence));
        if (FAILED(hr)) { fail(s, "CreateFence", hr); return false; }
        s.fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!s.fenceEvent) { fail(s, "CreateEvent", HRESULT_FROM_WIN32(GetLastError())); return false; }
        s.fenceCounter = 0;
    }

    if (!s.buffersReady) {
        s.format = desc.BufferDesc.Format;
        s.width = desc.BufferDesc.Width;
        s.height = desc.BufferDesc.Height;

        D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.NumDescriptors = desc.BufferCount;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        hr = s.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&s.rtvHeap));
        if (FAILED(hr)) { fail(s, "CreateDescriptorHeap(RTV)", hr); return false; }

        const UINT step = s.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE handle = s.rtvHeap->GetCPUDescriptorHandleForHeapStart();
        s.frames.assign(desc.BufferCount, Frame{});
        for (UINT i = 0; i < desc.BufferCount; ++i) {
            Frame& frame = s.frames[i];
            hr = swapchain->GetBuffer(i, IID_PPV_ARGS(&frame.buffer));
            if (FAILED(hr)) { fail(s, "IDXGISwapChain::GetBuffer", hr); return false; }
            D3D12_RENDER_TARGET_VIEW_DESC view{};
            view.Format = s.format;
            view.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            frame.rtv = handle;
            s.device->CreateRenderTargetView(frame.buffer, &view, frame.rtv);
            handle.ptr += step;
            hr = s.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                  IID_PPV_ARGS(&frame.allocator));
            if (FAILED(hr)) { fail(s, "CreateCommandAllocator", hr); return false; }
        }
        hr = s.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, s.frames[0].allocator,
                                         nullptr, IID_PPV_ARGS(&s.list));
        if (FAILED(hr)) { fail(s, "CreateCommandList", hr); return false; }
        s.list->Close();  // opened again with Reset() for every frame
        s.buffersReady = true;
        log("DirectX 12 overlay: %ux%u, %u back buffers, format %d, colour space %d", s.width,
            s.height, desc.BufferCount, static_cast<int>(s.format), s.colorSpace);
    }

    if (!s.imguiReady) {
        if (!claim_backend(Backend::Dx12)) {
            // The Vulkan renderer already owns ImGui (DirectX 12 translated to Vulkan).
            s.failed = true;
            release_everything(s);
            log("DirectX 12 overlay: the Vulkan renderer is active; standing down");
            return false;
        }
        if (!create_context()) { fail(s, "ImGui context", E_FAIL); return false; }
        if (!s.srvHeap) {
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.NumDescriptors = 1;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            hr = s.device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&s.srvHeap));
            if (FAILED(hr)) { fail(s, "CreateDescriptorHeap(SRV)", hr); return false; }
        }
        ImGui_ImplDX12_InitInfo info;
        info.Device = s.device;
        info.CommandQueue = s.queue;
        info.NumFramesInFlight = static_cast<int>(s.frames.size());
        info.RTVFormat = s.format;
        info.DSVFormat = DXGI_FORMAT_UNKNOWN;
        info.SrvDescriptorHeap = s.srvHeap;
        info.SrvDescriptorAllocFn = &srv_alloc;
        info.SrvDescriptorFreeFn = &srv_free;
        if (!ImGui_ImplDX12_Init(&info)) { fail(s, "ImGui_ImplDX12_Init", E_FAIL); return false; }
        s.imguiReady = true;
        s.imguiFormat = s.format;
        log("DirectX 12 overlay: ImGui renderer initialised");
    }
    return true;
}

// Draws the queued text into the current back buffer. Called from inside Present.
void render(IDXGISwapChain* swapchain) noexcept {
    ScopedLock lock;
    State& s = g_state;
    if (s.failed || disabled() || !s.queue || swapchain != s.swapchain) return;
    if (current_backend() == Backend::Vulkan) return;
    set_ready(true);  // the swapchain is known; text may now be queued for it

    static thread_local std::vector<TextCmd> cmds;
    if (!take_frame(cmds)) return;  // nothing to draw: touch nothing

    if (!ensure_resources(s, swapchain)) return;

    IDXGISwapChain3* swapchain3 = nullptr;
    HRESULT hr = swapchain->QueryInterface(IID_PPV_ARGS(&swapchain3));
    if (FAILED(hr) || !swapchain3) { fail(s, "QueryInterface(IDXGISwapChain3)", hr); return; }
    const UINT index = swapchain3->GetCurrentBackBufferIndex();
    swapchain3->Release();
    if (index >= s.frames.size()) { fail(s, "back-buffer index out of range", E_UNEXPECTED); return; }
    Frame& frame = s.frames[index];

    // The allocator of this back buffer may still be in use by the GPU.
    if (s.fence->GetCompletedValue() < frame.fenceValue) {
        if (FAILED(s.fence->SetEventOnCompletion(frame.fenceValue, s.fenceEvent)) ||
            WaitForSingleObject(s.fenceEvent, 100) != WAIT_OBJECT_0) {
            return;  // skip this frame rather than stall the game
        }
    }

    if (!s.drewFirstFrame) log("DirectX 12 overlay: drawing the first frame");

    hr = frame.allocator->Reset();
    if (FAILED(hr)) { fail(s, "ID3D12CommandAllocator::Reset", hr); return; }
    hr = s.list->Reset(frame.allocator, nullptr);
    if (FAILED(hr)) { fail(s, "ID3D12GraphicsCommandList::Reset", hr); return; }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = frame.buffer;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    s.list->ResourceBarrier(1, &barrier);
    s.list->OMSetRenderTargets(1, &frame.rtv, FALSE, nullptr);
    s.list->SetDescriptorHeaps(1, &s.srvHeap);

    const float width = static_cast<float>(s.width);
    const float height = static_cast<float>(s.height);
    begin_frame_io(width, height);
    ImGui_ImplDX12_NewFrame();  // creates the font texture on first use
    ImGui::NewFrame();
    add_text(cmds, width, height, encoding_for(s));
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), s.list);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    s.list->ResourceBarrier(1, &barrier);
    hr = s.list->Close();
    if (FAILED(hr)) { fail(s, "ID3D12GraphicsCommandList::Close", hr); return; }

    ID3D12CommandList* lists[] = {s.list};
    s.queue->ExecuteCommandLists(1, lists);
    frame.fenceValue = ++s.fenceCounter;
    hr = s.queue->Signal(s.fence, frame.fenceValue);
    if (FAILED(hr)) { fail(s, "ID3D12CommandQueue::Signal", hr); return; }

    if (!s.drewFirstFrame) {
        s.drewFirstFrame = true;
        log("DirectX 12 overlay: first frame submitted");
    }
}

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------
template <typename Fn>
bool hook_address(void* target, void* detour, Fn* original, const char* name) noexcept {
    if (*original) return true;  // this method is already hooked
    if (!target) return false;
    MH_STATUS status = MH_CreateHook(target, detour, reinterpret_cast<void**>(original));
    if (status != MH_OK) {
        log("DirectX 12 overlay: MH_CreateHook(%s) failed: %s", name, MH_StatusToString(status));
        *original = nullptr;
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK) {
        log("DirectX 12 overlay: MH_EnableHook(%s) failed: %s", name, MH_StatusToString(status));
        return false;
    }
    return true;
}

template <typename Fn>
bool hook_method(IUnknown* object, int slot, void* detour, Fn* original, const char* name) noexcept {
    void** vtable = *reinterpret_cast<void***>(object);
    return hook_address(vtable[slot], detour, original, name);
}

HRESULT STDMETHODCALLTYPE hk_Present(IDXGISwapChain* self, UINT syncInterval, UINT flags) {
    Depth depth;
    if (!depth.nested && !(flags & DXGI_PRESENT_TEST)) render(self);
    return g_realPresent(self, syncInterval, flags);
}

HRESULT STDMETHODCALLTYPE hk_Present1(IDXGISwapChain1* self, UINT syncInterval, UINT flags,
                                      const DXGI_PRESENT_PARAMETERS* parameters) {
    Depth depth;
    if (!depth.nested && !(flags & DXGI_PRESENT_TEST)) render(self);
    return g_realPresent1(self, syncInterval, flags, parameters);
}

void before_resize(IDXGISwapChain* self) noexcept {
    ScopedLock lock;
    if (self == g_state.swapchain) release_buffers(g_state);
}

HRESULT STDMETHODCALLTYPE hk_ResizeBuffers(IDXGISwapChain* self, UINT count, UINT width,
                                           UINT height, DXGI_FORMAT format, UINT flags) {
    before_resize(self);
    return g_realResizeBuffers(self, count, width, height, format, flags);
}

HRESULT STDMETHODCALLTYPE hk_ResizeBuffers1(IDXGISwapChain3* self, UINT count, UINT width,
                                            UINT height, DXGI_FORMAT format, UINT flags,
                                            const UINT* nodeMasks, IUnknown* const* queues) {
    before_resize(self);
    return g_realResizeBuffers1(self, count, width, height, format, flags, nodeMasks, queues);
}

HRESULT STDMETHODCALLTYPE hk_SetColorSpace1(IDXGISwapChain3* self, DXGI_COLOR_SPACE_TYPE space) {
    const HRESULT hr = g_realSetColorSpace1(self, space);
    if (SUCCEEDED(hr)) {
        ScopedLock lock;
        if (static_cast<IDXGISwapChain*>(self) == g_state.swapchain) {
            g_state.colorSpace = static_cast<int>(space);
            log("DirectX 12 overlay: game set colour space %d", static_cast<int>(space));
        }
    }
    return hr;
}

// The game is about to create a swapchain: let go of the old one's back buffers.
void before_swapchain_create() noexcept {
    ScopedLock lock;
    release_buffers(g_state);
}

// `device` is what the game passed as the swapchain's device. For DirectX 12
// that is the command queue the swapchain presents on, which is exactly the
// queue the overlay must submit its own command list to.
void after_swapchain_create(IUnknown* device, IDXGISwapChain* swapchain) noexcept {
    if (!device || !swapchain) return;
    ID3D12CommandQueue* queue = nullptr;
    if (FAILED(device->QueryInterface(IID_PPV_ARGS(&queue))) || !queue) return;  // not DirectX 12

    ScopedLock lock;
    State& s = g_state;
    if (s.queue != queue) {
        // New device/queue (first swapchain, or the game rebuilt its renderer).
        release_everything(s);
        release(s.queue);
        s.queue = queue;  // keeps the reference taken by QueryInterface
    } else {
        queue->Release();
    }
    s.swapchain = swapchain;
    s.colorSpace = 0;
    s.drewFirstFrame = false;
    log("DirectX 12 overlay: game created a DirectX 12 swapchain");

    hook_method(swapchain, kSwapPresent, reinterpret_cast<void*>(&hk_Present), &g_realPresent,
                "IDXGISwapChain::Present");
    hook_method(swapchain, kSwapResizeBuffers, reinterpret_cast<void*>(&hk_ResizeBuffers),
                &g_realResizeBuffers, "IDXGISwapChain::ResizeBuffers");
    IDXGISwapChain1* swapchain1 = nullptr;
    if (SUCCEEDED(swapchain->QueryInterface(IID_PPV_ARGS(&swapchain1))) && swapchain1) {
        hook_method(swapchain1, kSwapPresent1, reinterpret_cast<void*>(&hk_Present1),
                    &g_realPresent1, "IDXGISwapChain1::Present1");
        swapchain1->Release();
    }
    IDXGISwapChain3* swapchain3 = nullptr;
    if (SUCCEEDED(swapchain->QueryInterface(IID_PPV_ARGS(&swapchain3))) && swapchain3) {
        hook_method(swapchain3, kSwapSetColorSpace1, reinterpret_cast<void*>(&hk_SetColorSpace1),
                    &g_realSetColorSpace1, "IDXGISwapChain3::SetColorSpace1");
        hook_method(swapchain3, kSwapResizeBuffers1, reinterpret_cast<void*>(&hk_ResizeBuffers1),
                    &g_realResizeBuffers1, "IDXGISwapChain3::ResizeBuffers1");
        swapchain3->Release();
    } else {
        log("DirectX 12 overlay: IDXGISwapChain3 is not available; overlay cannot draw");
        s.failed = true;
    }
}

HRESULT STDMETHODCALLTYPE hk_CreateSwapChain(IDXGIFactory* self, IUnknown* device,
                                             DXGI_SWAP_CHAIN_DESC* desc, IDXGISwapChain** out) {
    before_swapchain_create();
    const HRESULT hr = g_realCreateSwapChain(self, device, desc, out);
    if (SUCCEEDED(hr) && out && *out) after_swapchain_create(device, *out);
    return hr;
}

HRESULT STDMETHODCALLTYPE hk_CreateSwapChainForHwnd(
    IDXGIFactory2* self, IUnknown* device, HWND window, const DXGI_SWAP_CHAIN_DESC1* desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreen, IDXGIOutput* output, IDXGISwapChain1** out) {
    before_swapchain_create();
    const HRESULT hr = g_realCreateSwapChainForHwnd(self, device, window, desc, fullscreen, output, out);
    if (SUCCEEDED(hr) && out && *out) after_swapchain_create(device, *out);
    return hr;
}

// Every DXGI factory the game creates gets its swapchain-creation methods hooked
// (the first time; all factories of one DXGI implementation share the code).
void hook_factory(void* created) noexcept {
    if (!created) return;
    IUnknown* unknown = static_cast<IUnknown*>(created);
    IDXGIFactory* factory = nullptr;
    if (SUCCEEDED(unknown->QueryInterface(IID_PPV_ARGS(&factory))) && factory) {
        hook_method(factory, kFactoryCreateSwapChain, reinterpret_cast<void*>(&hk_CreateSwapChain),
                    &g_realCreateSwapChain, "IDXGIFactory::CreateSwapChain");
        factory->Release();
    }
    IDXGIFactory2* factory2 = nullptr;
    if (SUCCEEDED(unknown->QueryInterface(IID_PPV_ARGS(&factory2))) && factory2) {
        hook_method(factory2, kFactoryCreateSwapChainForHwnd,
                    reinterpret_cast<void*>(&hk_CreateSwapChainForHwnd),
                    &g_realCreateSwapChainForHwnd, "IDXGIFactory2::CreateSwapChainForHwnd");
        factory2->Release();
    }
}

HRESULT WINAPI hk_CreateDXGIFactory(REFIID riid, void** factory) {
    const HRESULT hr = g_realCreateFactory(riid, factory);
    if (SUCCEEDED(hr) && factory) hook_factory(*factory);
    return hr;
}
HRESULT WINAPI hk_CreateDXGIFactory1(REFIID riid, void** factory) {
    const HRESULT hr = g_realCreateFactory1(riid, factory);
    if (SUCCEEDED(hr) && factory) hook_factory(*factory);
    return hr;
}
HRESULT WINAPI hk_CreateDXGIFactory2(UINT flags, REFIID riid, void** factory) {
    const HRESULT hr = g_realCreateFactory2(flags, riid, factory);
    if (SUCCEEDED(hr) && factory) hook_factory(*factory);
    return hr;
}

} // namespace

void install_dx12_hooks() noexcept {
    HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
    if (!dxgi) {
        log("DirectX 12 overlay: dxgi.dll could not be loaded; DirectX 12 overlay unavailable");
        return;
    }
    int hooked = 0;
    if (void* target = reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory")))
        hooked += hook_address(target, reinterpret_cast<void*>(&hk_CreateDXGIFactory),
                               &g_realCreateFactory, "CreateDXGIFactory") ? 1 : 0;
    if (void* target = reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory1")))
        hooked += hook_address(target, reinterpret_cast<void*>(&hk_CreateDXGIFactory1),
                               &g_realCreateFactory1, "CreateDXGIFactory1") ? 1 : 0;
    if (void* target = reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory2")))
        hooked += hook_address(target, reinterpret_cast<void*>(&hk_CreateDXGIFactory2),
                               &g_realCreateFactory2, "CreateDXGIFactory2") ? 1 : 0;
    // The game may already hold a factory created before these hooks existed. All
    // factories of one DXGI implementation share their code, so hooking the
    // methods through a short-lived factory of our own covers that one too.
    using RawCreateFactory = HRESULT(WINAPI*)(REFIID, void**);
    RawCreateFactory create = g_realCreateFactory1 ? g_realCreateFactory1 : g_realCreateFactory;
    if (!create) {
        create = reinterpret_cast<RawCreateFactory>(
            reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory1")));
    }
    IDXGIFactory* probe = nullptr;
    if (create && SUCCEEDED(create(IID_PPV_ARGS(&probe))) && probe) {
        hook_factory(probe);
        probe->Release();
    }
    log("DirectX 12 overlay: %d DXGI factory hooks installed, swapchain creation %s; idle until "
        "the game creates a DirectX 12 swapchain", hooked,
        (g_realCreateSwapChain || g_realCreateSwapChainForHwnd) ? "watched" : "NOT watched");
}

} // namespace hunting_companion::overlay::detail
