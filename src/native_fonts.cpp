// Text in any companion language through the game's own font libraries.
// See native_fonts.hpp. The approach follows the one RedM uses for its
// REGISTER_FONT_FILE native (see THIRD-PARTY-NOTICES.md).
//
//  1. At load, a worker thread finds two functions in RDR2.exe by byte pattern:
//     the game's "add a loaded .gfx to the GFx font library" function and the
//     streaming system's "request object" function, plus candidates for the
//     streaming info manager.
//  2. The first function is hooked. When the game adds the font library of its
//     own language during start-up, the hook records the Scaleform (gfx)
//     streaming store and the GFx font library it passes in, and that
//     library's slot in the store.
//  3. When the companion needs another library (font_lib_<language> in the
//     game's scaleform_frontend.rpf), its slot is found by reading the store's
//     name table, the streaming system loads it, and once it is resident the
//     game's own function adds it to the font library. From then on the game's
//     text renderer finds those fonts like any of its own.
//  4. Which font family to name in text comes from the game's fontmap.xml
//     (font_catalog.hpp), read through the game's own file system.
//
// Every step validates what it finds and gives up quietly; the companion then
// shows English for a CJK language, or the game's basic font for a Latin or
// Cyrillic one. Nothing touches the disk unless [Fonts] FontLog=1, which writes
// HuntingCompanion.fonts.log.

#include "native_fonts.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "MinHook.h"

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace hunting_companion::native_fonts {
namespace {

// ---------------------------------------------------------------------------
// Logging (only with FontLog=1)
// ---------------------------------------------------------------------------
Settings g_settings;
char g_logPath[MAX_PATH]{};
SRWLOCK g_logLock = SRWLOCK_INIT;

void log(const char* format, ...) noexcept {
    if (!g_settings.fileLog || g_logPath[0] == '\0') return;
    char line[512];
    const int prefix = std::snprintf(line, sizeof(line), "[%10lu] ", static_cast<unsigned long>(GetTickCount()));
    va_list args;
    va_start(args, format);
    std::vsnprintf(line + prefix, sizeof(line) - static_cast<std::size_t>(prefix), format, args);
    va_end(args);
    AcquireSRWLockExclusive(&g_logLock);
    HANDLE file = CreateFileA(g_logPath, FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(file, line, static_cast<DWORD>(std::strlen(line)), &written, nullptr);
        WriteFile(file, "\r\n", 2, &written, nullptr);
        CloseHandle(file);
    }
    ReleaseSRWLockExclusive(&g_logLock);
}

template <typename Fn>
Fn as_function(const void* address) noexcept {
    Fn fn{};
    std::memcpy(&fn, &address, sizeof(fn));
    return fn;
}

// ---------------------------------------------------------------------------
// Pattern scanning over RDR2.exe's executable sections
// ---------------------------------------------------------------------------
struct Pattern {
    std::uint8_t bytes[64];
    bool wild[64];
    std::size_t length;
};

Pattern parse_pattern(const char* text) noexcept {
    Pattern p{};
    for (const char* c = text; *c != '\0' && p.length < sizeof(p.bytes);) {
        if (*c == ' ') { ++c; continue; }
        if (*c == '?') {
            p.wild[p.length++] = true;
            while (*c == '?') ++c;
            continue;
        }
        unsigned value = 0;
        std::sscanf(c, "%2x", &value);
        p.bytes[p.length++] = static_cast<std::uint8_t>(value);
        c += 2;
    }
    return p;
}

struct Region { const std::uint8_t* begin; std::size_t size; };

int executable_regions(Region* out, int capacity) noexcept {
    const auto* base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    if (base == nullptr) return 0;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;
    const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    int count = 0;
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections && count < capacity; ++i, ++section) {
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
        out[count++] = Region{base + section->VirtualAddress, section->Misc.VirtualSize};
    }
    return count;
}

bool readable(const void* address, std::size_t size) noexcept {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) == 0) return false;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
    const auto* end = static_cast<const std::uint8_t*>(address) + size;
    return end <= static_cast<const std::uint8_t*>(info.BaseAddress) + info.RegionSize;
}

// Collects up to `capacity` matches and returns how many there are in total.
int find_all(const char* text, const std::uint8_t** out, int capacity) noexcept {
    const Pattern p = parse_pattern(text);
    Region regions[16];
    const int regionCount = executable_regions(regions, 16);
    int count = 0;
    for (int r = 0; r < regionCount; ++r) {
        const std::uint8_t* cursor = regions[r].begin;
        const std::uint8_t* end = regions[r].begin + regions[r].size;
        while (cursor + p.length <= end) {
            // Walk committed memory only.
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(cursor, &info, sizeof(info)) == 0) break;
            const std::uint8_t* chunkEnd = static_cast<const std::uint8_t*>(info.BaseAddress) + info.RegionSize;
            if (chunkEnd > end) chunkEnd = end;
            if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
                cursor = chunkEnd;
                continue;
            }
            for (; cursor + p.length <= chunkEnd; ++cursor) {
                if (cursor[0] != p.bytes[0]) continue;
                std::size_t i = 1;
                while (i < p.length && (p.wild[i] || cursor[i] == p.bytes[i])) ++i;
                if (i == p.length) {
                    if (count < capacity) out[count] = cursor;
                    ++count;
                }
            }
            // A pattern straddling two regions is ignored; game code is one region.
            if (cursor + p.length > chunkEnd) cursor = chunkEnd;
        }
    }
    return count;
}

// Returns the only match, or nullptr when there is none or more than one (an
// ambiguous pattern means this is not a game build the pattern was made for).
const std::uint8_t* find_unique(const char* text, int* matches) noexcept {
    const std::uint8_t* found = nullptr;
    const int count = find_all(text, &found, 1);
    if (matches != nullptr) *matches = count;
    return count == 1 ? found : nullptr;
}

const std::uint8_t* rip_target(const std::uint8_t* displacement) noexcept {
    std::int32_t offset = 0;
    std::memcpy(&offset, displacement, sizeof(offset));
    return displacement + 4 + offset;
}

const std::uint8_t* call_target(const std::uint8_t* callInstruction) noexcept {
    if (callInstruction == nullptr || callInstruction[0] != 0xE8) return nullptr;
    return rip_target(callInstruction + 1);
}

// Anywhere inside RDR2.exe's image (code or data).
bool inside_image(const void* address) noexcept {
    const auto* base = reinterpret_cast<const std::uint8_t*>(GetModuleHandleW(nullptr));
    if (base == nullptr) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    const auto* a = static_cast<const std::uint8_t*>(address);
    return a >= base && a < base + nt->OptionalHeader.SizeOfImage;
}

bool inside_main_module(const void* address) noexcept {
    Region regions[16];
    const int count = executable_regions(regions, 16);
    const auto* a = static_cast<const std::uint8_t*>(address);
    for (int i = 0; i < count; ++i) {
        if (a >= regions[i].begin && a < regions[i].begin + regions[i].size) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Game interfaces (layouts as used by RedM for RDR3)
// ---------------------------------------------------------------------------
// rage::strStreamingInfoManager: StreamingDataEntry* Entries at +0, int count at +24.
struct StreamingDataEntry { std::uint32_t handle; std::uint32_t flags; };
// rage::strStreamingModule: vtable at +0, base index at +8.
constexpr std::size_t kModuleBaseIndexOffset = 8;
constexpr std::uint32_t kInvalidSlot = 0xFFFFFFFFu;
// Request flags: keep the font library resident once loaded (RedM uses 7 for its own).
constexpr int kRequestFlags = 7;

using AssignFontLibFn = void (*)(void* store, void* fontLib, int localIndex);
// The game's own call passes a fourth integer (mov r9d, r13d before the call),
// passed explicitly here as 0.
using RequestObjectFn = void (*)(void* manager, std::uint32_t index, int flags, int extra);

AssignFontLibFn g_realAssign = nullptr;
RequestObjectFn g_requestObject = nullptr;
void* g_streamingManager = nullptr;          // chosen on first use, see choose_manager()
constexpr int kMaxManagerCandidates = 4;
void* g_managerCandidates[kMaxManagerCandidates + 1]{};
int g_managerCandidateCount = 0;
std::uint8_t* g_managerPointerSlots[kMaxManagerCandidates]{};  // globals holding a pointer to it
int g_managerPointerSlotCount = 0;

std::atomic<void*> g_store{nullptr};
std::atomic<void*> g_fontLib{nullptr};
std::atomic<int> g_gameAssigned[16];
std::atomic<int> g_gameAssignedCount{0};

enum class Setup : int { Scanning, Ready, Failed };
std::atomic<Setup> g_setup{Setup::Scanning};
char g_setupFailure[128]{};

std::uint32_t module_base_index(void* module) noexcept {
    std::uint32_t base = 0;
    std::memcpy(&base, static_cast<const std::uint8_t*>(module) + kModuleBaseIndexOffset, sizeof(base));
    return base;
}

void* vtable_entry(void* object, std::size_t slot) noexcept {
    void** vtable = *static_cast<void***>(object);
    return vtable[slot];
}

// ---------------------------------------------------------------------------
// Font libraries the companion may need
// ---------------------------------------------------------------------------
constexpr std::size_t kNameCapacity = 64;

enum class LibState : int { Idle, Requested, Ready, Failed };

struct Library {
    bool cjk;                   // false: Latin or Cyrillic
    const char* file;           // streaming name in scaleform_frontend.rpf
    const char* display;        // for the log and the Settings line
    const char* iniKey;         // [Fonts] <iniKey>Face / <iniKey>Bold
    std::atomic<LibState> state{LibState::Idle};
    std::uint32_t localIndex = kInvalidSlot;
    std::uint32_t globalIndex = kInvalidSlot;
    DWORD requestedTick = 0;
    DWORD firstAttemptTick = 0;
    char overrideFace[kNameCapacity]{};
    int overrideBold = -1;      // -1 = not set
    char reason[96]{};
    char status[160]{};
};

enum LibraryIndex { kEfigs, kRussian, kKorean, kChinese, kJapanese, kSimplifiedChinese };
Library g_libraries[] = {
    {false, "font_lib_efigs", "Latin", "Latin"},
    {false, "font_lib_russian", "Cyrillic", "Cyrillic"},
    {true, "font_lib_korean", "Korean", "Korean"},
    {true, "font_lib_chinese", "Traditional Chinese", "ChineseTraditional"},
    {true, "font_lib_japanese", "Japanese", "Japanese"},
    {true, "font_lib_simplified_chinese", "Simplified Chinese", "ChineseSimplified"},
};

// The font library RDR2 itself uses for each of its languages (GET_CURRENT_LANGUAGE).
Library* library_for(int language) noexcept {
    switch (language) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 11:
            return &g_libraries[kEfigs];   // English, French, German, Italian, Spanish, Portuguese, Polish
        case 7: return &g_libraries[kRussian];
        case 8: return &g_libraries[kKorean];
        case 9: return &g_libraries[kChinese];
        case 10: return &g_libraries[kJapanese];
        case 12: return &g_libraries[kSimplifiedChinese];
        default: return nullptr;
    }
}

void fail(Library& library, const char* reason) noexcept {
    std::snprintf(library.reason, sizeof(library.reason), "%s", reason);
    library.state.store(LibState::Failed);
    log("%s: unavailable: %s", library.file, reason);
}

// A font name ends up inside <FONT FACE='...'>, so it must not carry markup.
bool usable_face(const char* name) noexcept {
    if (name == nullptr || name[0] == '\0') return false;
    for (const char* c = name; *c != '\0'; ++c) {
        const unsigned char u = static_cast<unsigned char>(*c);
        if (u < 0x20 || *c == '\'' || *c == '"' || *c == '<' || *c == '>' || *c == '&') return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Hooks
// ---------------------------------------------------------------------------
void hk_assign(void* store, void* fontLib, int localIndex) {
    if (store != nullptr && fontLib != nullptr) {
        void* expected = nullptr;
        if (g_store.compare_exchange_strong(expected, store)) {
            g_fontLib.store(fontLib);
            log("game adds font library: store %p, font lib %p, slot %d", store, fontLib, localIndex);
        }
        const int count = g_gameAssignedCount.load();
        if (count < 16) {
            g_gameAssigned[count].store(localIndex);
            g_gameAssignedCount.store(count + 1);
        }
    }
    g_realAssign(store, fontLib, localIndex);
}

bool game_assigned(std::uint32_t localIndex) noexcept {
    const int count = g_gameAssignedCount.load();
    for (int i = 0; i < count; ++i) {
        if (static_cast<std::uint32_t>(g_gameAssigned[i].load()) == localIndex) return true;
    }
    return false;
}

DWORD WINAPI install_thread(LPVOID) {
    const MH_STATUS init = MH_Initialize();
    if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) {
        std::snprintf(g_setupFailure, sizeof(g_setupFailure), "hook library failed to start");
        g_setup.store(Setup::Failed);
        log("MH_Initialize failed: %s", MH_StatusToString(init));
        return 0;
    }
    // The game's code may still be unpacking when the ASI loader runs us; retry
    // for a while before deciding this build is not supported.
    const DWORD started = GetTickCount();
    int assignMatches = 0, managerMatches = 0, requestMatches = 0;
    const std::uint8_t* assignCall = nullptr;
    const std::uint8_t* managerSite = nullptr;
    const std::uint8_t* requestSite = nullptr;
    for (int attempt = 0;; ++attempt) {
        if (assignCall == nullptr) assignCall = find_unique("E8 ? ? ? ? 48 63 53 ? 48 8B 05", &assignMatches);
        if (managerSite == nullptr) {
            // On current builds this sequence appears more than once. Every match
            // loads a global's address; the candidates are told apart later, once
            // the game has filled them in (choose_manager).
            const std::uint8_t* sites[kMaxManagerCandidates]{};
            managerMatches = find_all("83 F8 FF 74 ? 8B 15 ? ? ? ? 48 8D 0D ? ? ? ? 03 D0", sites,
                                      kMaxManagerCandidates);
            if (managerMatches >= 1 && managerMatches <= kMaxManagerCandidates) {
                for (int i = 0; i < managerMatches; ++i) {
                    void* target = const_cast<std::uint8_t*>(rip_target(sites[i] + 14));
                    bool known = false;
                    for (int j = 0; j < g_managerCandidateCount; ++j) known |= g_managerCandidates[j] == target;
                    if (!known) g_managerCandidates[g_managerCandidateCount++] = target;
                }
                managerSite = sites[0];
            }
        }
        if (requestSite == nullptr)
            requestSite = find_unique("45 8B CD 41 B8 03 00 00 00 E8 ? ? ? ? 33 D2 48", &requestMatches);
        // The manager pattern is optional: the request call site gives candidates too.
        if (assignCall && requestSite && (managerSite != nullptr || attempt >= 40)) break;
        if (GetTickCount() - started > 60000u) {
            std::snprintf(g_setupFailure, sizeof(g_setupFailure),
                          "unsupported game build (patterns %d/%d/%d)", assignMatches, managerMatches,
                          requestMatches);
            g_setup.store(Setup::Failed);
            log("giving up: pattern matches assign=%d manager=%d request=%d", assignMatches,
                managerMatches, requestMatches);
            return 0;
        }
        Sleep(attempt < 20 ? 50 : 500);
    }
    const std::uint8_t* assign = call_target(assignCall);
    const std::uint8_t* request = call_target(requestSite + 9);
    if (!inside_main_module(assign) || !inside_main_module(request)) {
        std::snprintf(g_setupFailure, sizeof(g_setupFailure), "game functions not where expected");
        g_setup.store(Setup::Failed);
        log("call targets outside RDR2.exe: assign %p request %p", static_cast<const void*>(assign),
            static_cast<const void*>(request));
        return 0;
    }
    g_requestObject = as_function<RequestObjectFn>(request);
    // More manager candidates, read (never patched) from the code around the
    // game's own request call: the manager is its first argument, usually loaded
    // with lea rcx,[rip+x] (the object) or mov rcx,[rip+x] (a pointer to it)
    // shortly before the call.
    for (const std::uint8_t* c = requestSite + 9 - 1; c >= requestSite + 9 - 96; --c) {
        if (c[0] != 0x48 || c[2] != 0x0D || (c[1] != 0x8D && c[1] != 0x8B)) continue;
        const std::uint8_t* target = rip_target(c + 3);
        if (!inside_image(target)) continue;
        void* candidate = const_cast<std::uint8_t*>(target);
        if (c[1] == 0x8B) {
            // A global holding a pointer to the manager; read when it is needed,
            // as it may still be empty this early.
            if (g_managerPointerSlotCount < kMaxManagerCandidates)
                g_managerPointerSlots[g_managerPointerSlotCount++] = const_cast<std::uint8_t*>(target);
            log("  request call loads rcx from pointer %p", candidate);
            break;
        }
        bool known = false;
        for (int j = 0; j < g_managerCandidateCount; ++j) known |= g_managerCandidates[j] == candidate;
        if (!known && g_managerCandidateCount < kMaxManagerCandidates) {
            g_managerCandidates[g_managerCandidateCount++] = candidate;
            log("  request call loads rcx with %p", candidate);
        }
        break;  // the nearest load before the call is the argument
    }
    void* assignTarget = const_cast<std::uint8_t*>(assign);
    MH_STATUS status = MH_CreateHook(assignTarget, reinterpret_cast<void*>(&hk_assign),
                                     reinterpret_cast<void**>(&g_realAssign));
    if (status == MH_OK) status = MH_EnableHook(assignTarget);
    if (status != MH_OK) {
        std::snprintf(g_setupFailure, sizeof(g_setupFailure), "could not hook the font loader");
        g_setup.store(Setup::Failed);
        log("hooking the font-library function failed: %s", MH_StatusToString(status));
        return 0;
    }
    log("ready after %lu ms: add-font-library %p, request %p, %d streaming manager candidate(s) "
        "from %d pattern match(es)", static_cast<unsigned long>(GetTickCount() - started), assignTarget,
        static_cast<const void*>(request), g_managerCandidateCount, managerMatches);
    for (int i = 0; i < g_managerCandidateCount; ++i) log("  candidate %d: %p", i, g_managerCandidates[i]);
    if (g_store.load() == nullptr) log("waiting for the game to add its own font library");
    g_setup.store(Setup::Ready);
    return 0;
}

// rage::strStreamingInfoManager: the entry table at +0 and its entry count at
// +8 (current game build) or +24 (earlier layout). Its
// module manager starts with a vtable at +144, followed by two atArrays (+152
// and +168: pointer, then a 16-bit count); the gfx store is in one of them.
constexpr std::size_t kCountOffsets[] = {8, 24};
constexpr std::size_t kModuleArrayOffsets[] = {152, 168};
std::size_t g_countOffset = 8;

bool has_streaming_entries(void* candidate, int* countOut, std::size_t* offsetOut) noexcept {
    auto* base = static_cast<const std::uint8_t*>(candidate);
    if (candidate == nullptr || !readable(base, 192)) return false;
    const void* entries = nullptr;
    std::memcpy(&entries, base, sizeof(entries));
    if (entries == nullptr) return false;
    for (std::size_t offset : kCountOffsets) {
        std::uint32_t count = 0;
        std::memcpy(&count, base + offset, sizeof(count));
        if (countOut != nullptr) *countOut = static_cast<int>(count);
        if (count < 1000 || count > 8000000) continue;
        const auto* first = static_cast<const StreamingDataEntry*>(entries);
        if (readable(first, sizeof(StreamingDataEntry)) && readable(first + (count - 1), sizeof(StreamingDataEntry))) {
            if (offsetOut != nullptr) *offsetOut = offset;
            return true;
        }
    }
    return false;
}

bool lists_store(void* candidate, void* store) noexcept {
    auto* base = static_cast<const std::uint8_t*>(candidate);
    for (std::size_t offset : kModuleArrayOffsets) {
        void* const* modules = nullptr;
        std::uint16_t count = 0;
        std::memcpy(&modules, base + offset, sizeof(modules));
        std::memcpy(&count, base + offset + 8, sizeof(count));
        if (modules == nullptr || count == 0 || count > 256 || !readable(modules, sizeof(void*) * count)) continue;
        for (std::uint16_t i = 0; i < count; ++i) {
            if (modules[i] == store) return true;
        }
    }
    return false;
}

bool choose_manager(void* store) noexcept {
    if (g_streamingManager != nullptr) return true;
    void* candidates[2 * kMaxManagerCandidates + 1]{};
    int candidateCount = 0;
    for (int i = 0; i < g_managerCandidateCount; ++i) candidates[candidateCount++] = g_managerCandidates[i];
    for (int i = 0; i < g_managerPointerSlotCount; ++i) {
        void* pointed = nullptr;
        if (readable(g_managerPointerSlots[i], sizeof(pointed)))
            std::memcpy(&pointed, g_managerPointerSlots[i], sizeof(pointed));
        if (pointed != nullptr) candidates[candidateCount++] = pointed;
    }
    void* chosen = nullptr;
    int valid = 0;
    int count = 0;
    std::size_t chosenOffset = 8;
    for (int i = 0; i < candidateCount; ++i) {
        bool duplicate = false;
        for (int j = 0; j < i; ++j) duplicate |= candidates[j] == candidates[i];
        if (duplicate) continue;
        std::size_t offset = 8;
        const bool entries = has_streaming_entries(candidates[i], &count, &offset);
        const bool listed = entries && lists_store(candidates[i], store);
        log("manager candidate %p: %d entries%s, gfx store %s", candidates[i], count,
            entries ? "" : " (implausible)", listed ? "listed" : "not listed");
        if (listed) { chosen = candidates[i]; chosenOffset = offset; ++valid; }
    }
    if (valid != 1) {
        // No candidate fits: log their first bytes for a bug report.
        for (int i = 0; i < candidateCount; ++i) {
            if (!readable(candidates[i], 192)) continue;
            log("  bytes of %p:", candidates[i]);
            const auto* bytes = static_cast<const std::uint8_t*>(candidates[i]);
            for (int row = 0; row < 192; row += 32) {
                char hex[3 * 32 + 1]{};
                for (int k = 0; k < 32; ++k) std::snprintf(hex + 3 * k, 4, "%02X ", bytes[row + k]);
                log("    +%03d: %s", row, hex);
            }
        }
        return false;
    }
    g_streamingManager = chosen;
    g_countOffset = chosenOffset;
    log("using streaming manager %p (entry count at +%zu)", chosen, chosenOffset);
    return true;
}

StreamingDataEntry* entry_at(std::uint32_t index) noexcept {
    auto* manager = static_cast<std::uint8_t*>(g_streamingManager);
    StreamingDataEntry* entries = nullptr;
    int count = 0;
    std::memcpy(&entries, manager, sizeof(entries));
    std::memcpy(&count, manager + g_countOffset, sizeof(count));
    if (entries == nullptr || count <= 0 || index >= static_cast<std::uint32_t>(count)) return nullptr;
    return entries + index;
}

// ---------------------------------------------------------------------------
// Finding a font library's slot in the gfx store, by reading memory only
// ---------------------------------------------------------------------------
// The store keeps each file's name as a RAGE name hash (joaat). The game told us
// which slot its own library has (hk_assign), so the hash of that library's
// name is looked for in the memory the store points to. The record holding it
// shows where the slot number is kept (a name -> slot entry), or, in a table
// ordered by slot, how far apart neighbouring slots are. The same rule then
// gives every other library's slot. Nothing is called or written.
std::uint32_t joaat(const char* text) noexcept {
    std::uint32_t hash = 0;
    for (const char* c = text; *c != '\0'; ++c) {
        char ch = *c;
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        hash += static_cast<unsigned char>(ch);
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

std::size_t readable_span(const void* address, std::size_t wanted) noexcept {
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) == 0) return 0;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return 0;
    const auto* end = static_cast<const std::uint8_t*>(info.BaseAddress) + info.RegionSize;
    const std::size_t available = static_cast<std::size_t>(end - static_cast<const std::uint8_t*>(address));
    return available < wanted ? available : wanted;
}

struct ScanBlock { const std::uint8_t* begin; std::size_t size; };
constexpr int kMaxScanBlocks = 4096;
ScanBlock g_scanBlocks[kMaxScanBlocks];
int g_scanBlockCount = 0;

// Memory reachable from the store within three pointer hops.
void collect_store_memory(void* store) noexcept {
    g_scanBlockCount = 0;
    int depthStart[4] = {0, 0, 0, 0};
    const std::size_t blockSize[3] = {0x400, 0x1000, 0x400};
    const std::size_t first = readable_span(store, blockSize[0]);
    if (first == 0) return;
    g_scanBlocks[g_scanBlockCount++] = {static_cast<const std::uint8_t*>(store), first};
    for (int depth = 0; depth < 3; ++depth) {
        depthStart[depth + 1] = g_scanBlockCount;
        for (int b = depthStart[depth]; b < depthStart[depth + 1] && g_scanBlockCount < kMaxScanBlocks; ++b) {
            if (depth == 2) break;  // the last level is scanned, not followed
            const ScanBlock block = g_scanBlocks[b];
            for (std::size_t offset = 0; offset + 8 <= block.size && g_scanBlockCount < kMaxScanBlocks; offset += 8) {
                std::uint64_t value = 0;
                std::memcpy(&value, block.begin + offset, sizeof(value));
                if (value < 0x10000 || value > 0x7FFFFFFFFFFFull || (value & 7) != 0) continue;
                const auto* target = reinterpret_cast<const std::uint8_t*>(static_cast<std::uintptr_t>(value));
                bool seen = false;
                for (int i = 0; i < g_scanBlockCount && !seen; ++i) seen = g_scanBlocks[i].begin == target;
                if (seen || inside_main_module(target)) continue;  // skip code (vtables, functions)
                const std::size_t size = readable_span(target, blockSize[depth + 1]);
                if (size >= 16) g_scanBlocks[g_scanBlockCount++] = {target, size};
            }
        }
    }
}

struct HashHit { const std::uint8_t* at; int block; };

int find_hash(std::uint32_t hash, HashHit* hits, int capacity) noexcept {
    int count = 0;
    for (int b = 0; b < g_scanBlockCount; ++b) {
        const ScanBlock& block = g_scanBlocks[b];
        for (std::size_t offset = 0; offset + 4 <= block.size; offset += 4) {
            std::uint32_t value = 0;
            std::memcpy(&value, block.begin + offset, sizeof(value));
            if (value != hash) continue;
            if (count < capacity) hits[count] = {block.begin + offset, b};
            ++count;
        }
    }
    return count;
}

std::uint32_t read_u32(const std::uint8_t* at) noexcept {
    std::uint32_t value = 0;
    std::memcpy(&value, at, sizeof(value));
    return value;
}

bool plausible_slot(void* store, std::uint32_t slot) noexcept {
    return slot < 100000 && entry_at(slot + module_base_index(store)) != nullptr;
}

// Works out every library's slot once; kInvalidSlot where it could not be found.
std::atomic<int> g_gameLanguage{-1};

std::uint32_t slot_of(void* store, const Library& wanted) noexcept {
    static bool done = false;
    static std::uint32_t slots[sizeof(g_libraries) / sizeof(g_libraries[0])];
    const std::size_t libraryCount = sizeof(g_libraries) / sizeof(g_libraries[0]);
    if (!done) {
        done = true;
        for (std::size_t i = 0; i < libraryCount; ++i) slots[i] = kInvalidSlot;
        // The library the game added itself (the one for its language), and its slot.
        const std::uint32_t knownSlot = static_cast<std::uint32_t>(g_gameAssigned[0].load());
        const Library* gameLibrary = library_for(g_gameLanguage.load());
        int knownLibrary = -1;
        if (gameLibrary == nullptr || g_gameAssignedCount.load() == 0) {
            log("slot search: the game's own font library is not known");
            for (std::size_t i = 0; i < libraryCount; ++i) {
                if (&g_libraries[i] == &wanted) return kInvalidSlot;
            }
        }
        collect_store_memory(store);
        log("slot search: %d memory blocks reachable from the gfx store, base index %u", g_scanBlockCount,
            module_base_index(store));
        constexpr int kMaxHits = 16;
        HashHit hits[libraryCount][kMaxHits];
        int hitCount[libraryCount]{};
        for (std::size_t i = 0; i < libraryCount; ++i) {
            hitCount[i] = find_hash(joaat(g_libraries[i].file), hits[i], kMaxHits);
            log("  %s (hash %08X): %d hit(s)", g_libraries[i].file, joaat(g_libraries[i].file), hitCount[i]);
            for (int h = 0; h < hitCount[i] && h < kMaxHits; ++h) {
                char hex[3 * 24 + 1]{};
                const std::uint8_t* around = hits[i][h].at - 8;
                if (readable_span(around, 24) == 24)
                    for (int k = 0; k < 24; ++k) std::snprintf(hex + 3 * k, 4, "%02X ", around[k]);
                log("    at %p (block %d): -8: %s", static_cast<const void*>(hits[i][h].at), hits[i][h].block, hex);
            }
        }
        // Which library did the game add? The one whose hash sits next to its slot.
        static const int kSlotOffsets[] = {4, -4, 8, -8, 12, -12, 16, -16};
        int slotOffset = 0;
        int slotBlock = -1;
        // Nearest offsets first, across every place the name appears.
        for (int offset : kSlotOffsets) {
            for (std::size_t i = 0; i < libraryCount && knownLibrary < 0; ++i) {
                if (&g_libraries[i] != gameLibrary) continue;
                for (int h = 0; h < hitCount[i] && h < kMaxHits; ++h) {
                    if (readable_span(hits[i][h].at + offset, 4) == 4 && read_u32(hits[i][h].at + offset) == knownSlot) {
                        knownLibrary = static_cast<int>(i);
                        slotOffset = offset;
                        slotBlock = hits[i][h].block;
                        break;
                    }
                }
            }
            if (knownLibrary >= 0) break;
        }
        if (knownLibrary >= 0) {
            // Name -> slot records: read every library's slot at the same offset in
            // the same table. (The names also appear in another table, next to
            // unrelated values.)
            log("  slot found next to the name hash of %s (offset %+d, block %d)", g_libraries[knownLibrary].file,
                slotOffset, slotBlock);
            for (std::size_t i = 0; i < libraryCount; ++i) {
                for (int h = 0; h < hitCount[i] && h < kMaxHits; ++h) {
                    if (hits[i][h].block != slotBlock) continue;
                    if (readable_span(hits[i][h].at + slotOffset, 4) != 4) continue;
                    const std::uint32_t slot = read_u32(hits[i][h].at + slotOffset);
                    if (!plausible_slot(store, slot)) continue;
                    slots[i] = slot;
                    break;
                }
            }
        } else {
            // A table ordered by slot: neighbouring libraries are one record apart.
            // The game's library is the efigs one on Latin games; try each library
            // the game could have added and keep the reading that is consistent.
            for (std::size_t anchor = 0; anchor < libraryCount && knownLibrary < 0; ++anchor) {
                for (int h = 0; h < hitCount[anchor] && h < kMaxHits && knownLibrary < 0; ++h) {
                    const HashHit base = hits[anchor][h];
                    std::ptrdiff_t stride = 0;
                    for (std::size_t i = 0; i < libraryCount; ++i) {
                        if (i == anchor) continue;
                        for (int k = 0; k < hitCount[i] && k < kMaxHits; ++k) {
                            if (hits[i][k].block != base.block) continue;
                            std::ptrdiff_t delta = hits[i][k].at - base.at;
                            if (delta < 0) delta = -delta;
                            if (delta > 0 && (stride == 0 || delta < stride)) stride = delta;
                        }
                    }
                    if (stride == 0) continue;
                    std::uint32_t candidate[sizeof(g_libraries) / sizeof(g_libraries[0])];
                    bool consistent = true;
                    int found = 0;
                    for (std::size_t i = 0; i < libraryCount; ++i) {
                        candidate[i] = kInvalidSlot;
                        if (i == anchor) { candidate[i] = knownSlot; continue; }
                        for (int k = 0; k < hitCount[i] && k < kMaxHits; ++k) {
                            if (hits[i][k].block != base.block) continue;
                            const std::ptrdiff_t delta = hits[i][k].at - base.at;
                            if (delta % stride != 0) { consistent = false; break; }
                            const std::ptrdiff_t slot = static_cast<std::ptrdiff_t>(knownSlot) + delta / stride;
                            if (slot < 0 || !plausible_slot(store, static_cast<std::uint32_t>(slot))) {
                                consistent = false;
                                break;
                            }
                            candidate[i] = static_cast<std::uint32_t>(slot);
                            ++found;
                            break;
                        }
                    }
                    // Without a slot number beside the names, only the library the
                    // game added for its own language is a safe anchor.
                    if (!consistent || found < 2 || &g_libraries[anchor] != gameLibrary) continue;
                    knownLibrary = static_cast<int>(anchor);
                    for (std::size_t i = 0; i < libraryCount; ++i) slots[i] = candidate[i];
                    log("  slots from a table ordered by slot (record size %td, anchored on %s)", stride,
                        g_libraries[anchor].file);
                }
            }
        }
        // Two libraries cannot share a slot; drop any that do rather than load
        // the wrong file.
        for (std::size_t i = 0; i < libraryCount; ++i) {
            for (std::size_t j = i + 1; j < libraryCount; ++j) {
                if (slots[i] != kInvalidSlot && slots[i] == slots[j]) {
                    log("  %s and %s both read slot %u; neither is used", g_libraries[i].file, g_libraries[j].file,
                        slots[i]);
                    slots[i] = slots[j] = kInvalidSlot;
                }
            }
        }
        for (std::size_t i = 0; i < libraryCount; ++i) {
            if (slots[i] == kInvalidSlot) log("  %s: slot not found", g_libraries[i].file);
            else log("  %s: slot %u", g_libraries[i].file, slots[i]);
        }
    }
    for (std::size_t i = 0; i < libraryCount; ++i) {
        if (&g_libraries[i] == &wanted) return slots[i];
    }
    return kInvalidSlot;
}

void start_request(Library& library) noexcept {
    const Setup setup = g_setup.load();
    if (setup == Setup::Scanning) return;  // try again next frame
    if (setup == Setup::Failed) { fail(library, g_setupFailure); return; }
    void* store = g_store.load();
    if (store == nullptr) {
        // Normally captured long before Story Mode loads; allow some slack.
        if (library.firstAttemptTick == 0) library.firstAttemptTick = GetTickCount();
        if (GetTickCount() - library.firstAttemptTick < 30000u) return;
        fail(library, "the game's font library was not seen at start-up");
        return;
    }
    if (!choose_manager(store)) {
        fail(library, "the game's streaming manager could not be identified");
        return;
    }
    const std::uint32_t slot = slot_of(store, library);
    if (slot == kInvalidSlot) {
        fail(library, "its place in the game's font store was not found");
        return;
    }
    library.localIndex = slot;
    library.globalIndex = slot + module_base_index(store);
    StreamingDataEntry* entry = entry_at(library.globalIndex);
    if (entry == nullptr) {
        fail(library, "streaming index out of range");
        return;
    }
    log("%s: slot %u (streaming index %u), flags %08X, added by the game: %s", library.file, slot,
        library.globalIndex, entry->flags, game_assigned(slot) ? "yes" : "no");
    if (game_assigned(slot)) {
        // The game uses this library itself, so it is already in the font library.
        library.state.store(LibState::Ready);
        return;
    }
    library.requestedTick = GetTickCount();
    library.state.store(LibState::Requested);
    g_requestObject(g_streamingManager, library.globalIndex, kRequestFlags, 0);
}

void advance(Library& library) noexcept {
    StreamingDataEntry* entry = entry_at(library.globalIndex);
    if (entry == nullptr) { fail(library, "streaming entry vanished"); return; }
    if ((entry->flags & 3u) == 1u) {
        g_realAssign(g_store.load(), g_fontLib.load(), static_cast<int>(library.localIndex));
        library.state.store(LibState::Ready);
        log("%s: loaded and added to the font library", library.file);
        return;
    }
    if (GetTickCount() - library.requestedTick > 30000u) {
        log("%s: entry flags still %08X after 30 seconds", library.file, entry->flags);
        fail(library, "the game did not load it within 30 seconds");
    }
}

// ---------------------------------------------------------------------------
// Reading files through the game's own file system
// ---------------------------------------------------------------------------
// rage::fiDevice (RDR3 layout as used by RedM): virtual Open at slot 4, Read 10,
// Close 18, GetFileLength 21. GetDevice(path, allowRoot, a, b) finds the device
// that owns a path such as "common:/...". The game decrypts and applies update
// archives itself, so this reads exactly what the game reads.
using GetDeviceFn = void* (*)(const char* path, bool allowRoot, bool a, bool b);
using DeviceOpenFn = std::uint64_t (*)(void* device, const char* path, bool readOnly);
using DeviceReadFn = std::uint32_t (*)(void* device, std::uint64_t handle, void* buffer, std::uint32_t size);
using DeviceCloseFn = std::int32_t (*)(void* device, std::uint64_t handle);
using DeviceLengthFn = int (*)(void* device, std::uint64_t handle);
constexpr std::uint64_t kInvalidHandle = ~0ull;

GetDeviceFn game_file_device() noexcept {
    static GetDeviceFn getDevice = nullptr;
    static bool searched = false;
    if (!searched) {
        searched = true;
        int matches = 0;
        const std::uint8_t* site = find_unique("41 8A F0 40 8A EA 41 B8 07 00 00 00", &matches);
        if (site != nullptr && inside_main_module(site - 0x14)) getDevice = as_function<GetDeviceFn>(site - 0x14);
        else log("the game's file-device function was not found (%d matches)", matches);
    }
    return getDevice;
}

// Reads a whole file through the game's own file system into a heap buffer
// (NUL-terminated). Free it with HeapFree(GetProcessHeap(), 0, data).
char* read_file_via_game(const char* path, std::size_t* size) noexcept {
    *size = 0;
    const GetDeviceFn getDevice = game_file_device();
    if (getDevice == nullptr) return nullptr;
    void* device = getDevice(path, true, true, true);
    if (device == nullptr) {
        log("no game device for %s", path);
        return nullptr;
    }
    const auto open = as_function<DeviceOpenFn>(vtable_entry(device, 4));
    const auto read = as_function<DeviceReadFn>(vtable_entry(device, 10));
    const auto close = as_function<DeviceCloseFn>(vtable_entry(device, 18));
    const auto length = as_function<DeviceLengthFn>(vtable_entry(device, 21));
    const std::uint64_t handle = open(device, path, true);
    if (handle == kInvalidHandle) {
        log("%s could not be opened", path);
        return nullptr;
    }
    const int fileSize = length(device, handle);
    if (fileSize <= 0 || fileSize > 8 * 1024 * 1024) {
        log("%s has an unexpected size (%d)", path, fileSize);
        close(device, handle);
        return nullptr;
    }
    char* buffer = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(fileSize) + 1));
    std::uint32_t total = 0;
    if (buffer != nullptr) {
        while (total < static_cast<std::uint32_t>(fileSize)) {
            const std::uint32_t got =
                read(device, handle, buffer + total, static_cast<std::uint32_t>(fileSize) - total);
            if (got == 0 || got == 0xFFFFFFFFu) break;
            total += got;
        }
        buffer[total] = '\0';
    }
    close(device, handle);
    if (buffer != nullptr && total == 0) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return nullptr;
    }
    *size = total;
    return buffer;
}

void read_overrides() noexcept {
    if (g_settings.iniPath == nullptr || g_settings.iniPath[0] == '\0') return;
    for (Library& library : g_libraries) {
        char key[64];
        std::snprintf(key, sizeof(key), "%sFace", library.iniKey);
        GetPrivateProfileStringA("Fonts", key, "", library.overrideFace, sizeof(library.overrideFace),
                                 g_settings.iniPath);
        if (!usable_face(library.overrideFace)) library.overrideFace[0] = '\0';
        std::snprintf(key, sizeof(key), "%sBold", library.iniKey);
        const int bold = static_cast<int>(GetPrivateProfileIntA("Fonts", key, -1, g_settings.iniPath));
        library.overrideBold = (bold == 0 || bold == 1) ? bold : -1;
    }
}

}  // namespace

void install(const Settings& settings) noexcept {
    g_settings = settings;
    if (settings.fileLog && settings.logDirectory != nullptr) {
        std::snprintf(g_logPath, sizeof(g_logPath), "%sHuntingCompanion.fonts.log", settings.logDirectory);
    }
    read_overrides();
    // The hook hands a function in this DLL to the game; never let it unload.
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                       reinterpret_cast<LPCWSTR>(&install), &pinned);
    HANDLE thread = CreateThread(nullptr, 0, install_thread, nullptr, 0, nullptr);
    if (thread != nullptr) CloseHandle(thread);
}

void diagnostic(const char* format, ...) noexcept {
    if (!g_settings.fileLog) return;
    char line[400];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    log("%s", line);
}

void set_game_language(int language) noexcept { g_gameLanguage.store(language); }

bool needs_library(int language, int gameLanguage) noexcept {
    if (language == gameLanguage) return false;
    const Library* wanted = library_for(language);
    const Library* game = library_for(gameLanguage);
    if (wanted == nullptr || wanted == game) return false;
    // Japanese, Korean and both Chinese scripts: no other library has the glyphs,
    // including the other CJK libraries.
    if (wanted->cjk) return true;
    // Latin (with Polish/Spanish/... accents) or Cyrillic on a CJK game: the CJK
    // libraries lack most of those letters. Between Latin and Cyrillic games the
    // game's own fonts already cover both, so nothing is loaded there.
    return game != nullptr && game->cjk;
}

bool loading(int language) noexcept {
    const Library* library = library_for(language);
    if (library == nullptr) return false;
    const LibState state = library->state.load();
    return state == LibState::Idle || state == LibState::Requested;
}

bool falls_back_to_english(int language) noexcept {
    const Library* library = library_for(language);
    return library != nullptr && library->cjk;
}

void update() noexcept {
    for (Library& library : g_libraries) {
        if (library.state.load() == LibState::Requested) advance(library);
    }
}

char* read_game_file(const char* path, std::size_t* size) noexcept { return read_file_via_game(path, size); }

void free_game_file(char* data) noexcept {
    if (data != nullptr) HeapFree(GetProcessHeap(), 0, data);
}

bool library_ready(int language) noexcept {
    Library* library = library_for(language);
    if (library == nullptr) return false;
    switch (library->state.load()) {
        case LibState::Idle: start_request(*library); return false;
        case LibState::Requested: return false;
        case LibState::Failed: return false;
        case LibState::Ready: return true;
    }
    return false;
}

const char* fallback_face(int language, bool* bold) noexcept {
    const Library* library = library_for(language);
    if (bold != nullptr) *bold = false;
    if (library == nullptr || library->state.load() != LibState::Ready) return nullptr;
    return override_face(language, bold);
}

const char* override_face(int language, bool* bold) noexcept {
    const Library* library = library_for(language);
    if (library == nullptr || library->overrideFace[0] == '\0') return nullptr;
    if (bold != nullptr) *bold = library->overrideBold == 1;
    return library->overrideFace;
}

void mark_unusable(int language, const char* reason) noexcept {
    Library* library = library_for(language);
    if (library != nullptr && library->state.load() != LibState::Failed) fail(*library, reason);
}

const char* status_line(int language) noexcept {
    Library* library = library_for(language);
    if (library == nullptr) return nullptr;
    switch (library->state.load()) {
        case LibState::Idle:
        case LibState::Requested:
            std::snprintf(library->status, sizeof(library->status),
                          "Loading the game's %s font...", library->display);
            break;
        case LibState::Ready:
            std::snprintf(library->status, sizeof(library->status), "%s text uses the game's own %s fonts.",
                          library->display, library->display);
            break;
        case LibState::Failed:
            std::snprintf(library->status, sizeof(library->status), "%s font unavailable (%s): %s.",
                          library->display, library->reason,
                          library->cjk ? "showing English" : "using the game's basic font");
            break;
    }
    return library->status;
}

}  // namespace hunting_companion::native_fonts
