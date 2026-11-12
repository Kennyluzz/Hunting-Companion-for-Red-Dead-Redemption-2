// Hunting Companion
// Read-only RDR2 Story Mode companion for Pearson, Trapper, challenges, and gear.
//
// The companion reads game catalog, inventory, vendor-storage, and challenge-rank
// state, then renders its menu and focused-animal HUD indicators. Challenge
// progress, inventory, transactions, vendor stock, and save data are never changed.

#include <windows.h>
#include "localization.hpp"
#include "cjk_overlay.hpp"
#include "embedded_challenge_objective_hashes.hpp"
#include "embedded_hunting_recommendations.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <iterator>
#include <cstdio>
#include <type_traits>
#include <string>

namespace {
using ScriptMainFn = void (*)();
using ScriptRegisterFn = void (*)(HMODULE, ScriptMainFn);
using ScriptUnregisterFn = void (*)(HMODULE);
using ScriptWaitFn = void (*)(DWORD);
using NativeInitFn = void (*)(std::uint64_t);
using NativePush64Fn = void (*)(std::uint64_t);
using NativeCallFn = std::uint64_t* (*)();

struct ScriptHookApi {
    ScriptRegisterFn registerScript{};
    ScriptRegisterFn registerAdditionalThread{};
    ScriptUnregisterFn unregisterScript{};
    ScriptWaitFn wait{};
    NativeInitFn nativeInit{};
    NativePush64Fn nativePush64{};
    NativeCallFn nativeCall{};
};
ScriptHookApi g_api{};
HMODULE g_module{};

template <typename FunctionPointer>
FunctionPointer resolve_export(HMODULE module, const char* decoratedName) noexcept {
    static_assert(std::is_pointer_v<FunctionPointer>, "Expected function pointer");
    static_assert(sizeof(FunctionPointer) == sizeof(FARPROC), "Unexpected x64 pointer size");
    const FARPROC address = GetProcAddress(module, decoratedName);
    FunctionPointer function{};
    if (address != nullptr) std::memcpy(&function, &address, sizeof(function));
    return function;
}

bool resolve_script_hook() noexcept {
    const HMODULE hook = GetModuleHandleW(L"ScriptHookRDR2.dll");
    if (hook == nullptr) return false;
    g_api.registerScript = resolve_export<ScriptRegisterFn>(hook, "?scriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z");
    g_api.registerAdditionalThread = resolve_export<ScriptRegisterFn>(hook, "?scriptRegisterAdditionalThread@@YAXPEAUHINSTANCE__@@P6AXXZ@Z");
    g_api.unregisterScript = resolve_export<ScriptUnregisterFn>(hook, "?scriptUnregister@@YAXPEAUHINSTANCE__@@@Z");
    g_api.wait = resolve_export<ScriptWaitFn>(hook, "?scriptWait@@YAXK@Z");
    g_api.nativeInit = resolve_export<NativeInitFn>(hook, "?nativeInit@@YAX_K@Z");
    g_api.nativePush64 = resolve_export<NativePush64Fn>(hook, "?nativePush64@@YAX_K@Z");
    g_api.nativeCall = resolve_export<NativeCallFn>(hook, "?nativeCall@@YAPEA_KXZ");
    return g_api.registerScript && g_api.unregisterScript && g_api.wait &&
           g_api.nativeInit && g_api.nativePush64 && g_api.nativeCall;
}

template <typename T>
void push_native_argument(T value) noexcept {
    static_assert(sizeof(T) <= sizeof(std::uint64_t), "Native argument exceeds 64 bits");
    std::uint64_t packed = 0;
    if constexpr (std::is_pointer_v<T>) {
        packed = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(value));
    } else {
        std::memcpy(&packed, &value, sizeof(T));
    }
    g_api.nativePush64(packed);
}

template <typename Return, typename... Args>
Return invoke_native(std::uint64_t hash, Args... args) noexcept {
    g_api.nativeInit(hash);
    (push_native_argument(args), ...);
    std::uint64_t* result = g_api.nativeCall();
    if constexpr (std::is_void_v<Return>) {
        (void)result;
        return;
    } else {
        if (result == nullptr) return Return{};
        return *reinterpret_cast<Return*>(result);
    }
}

// ScriptHook / RDR2 natives.
constexpr std::uint64_t kPlayerPedId = 0x096275889B8E0EE0ull;
constexpr std::uint64_t kPlayerId = 0x217E9DC48139933Dull;
constexpr std::uint64_t kGetSaddleHorseForPlayer = 0xB48050D326E9A2F3ull;
constexpr std::uint64_t kGetCarriedAttachedInfoForSlot = 0x608BC6A6AACD5036ull;
constexpr std::uint64_t kFindAllAttachedCarriableEntities = 0xB5ACE8B23A438EC0ull;
constexpr std::uint64_t kCreateItemset = 0xA1AF16083320065Aull;
constexpr std::uint64_t kDestroyItemset = 0x712BC69F10549B92ull;
constexpr std::uint64_t kGetItemsetSize = 0x55F2E375AC6018A9ull;
constexpr std::uint64_t kGetIndexedItemInItemset = 0x275A2E2C0FAB7612ull;
constexpr std::uint64_t kGetEntityFromItem = 0xEE04C0AFD4EFAF0Eull;
constexpr std::uint64_t kDoesEntityExist = 0xD42BD6EB2E0F1677ull;
constexpr std::uint64_t kIsEntityAPed = 0xCF8176912DDA4EA5ull;
constexpr std::uint64_t kGetIsCarriablePelt = 0x255B6DB4E3AD3C3Eull;
constexpr std::uint64_t kGetCarcassFromPelt = 0x2A77EF9BEC8518F4ull;
constexpr std::uint64_t kGetPedQuality = 0x7BCC6087D130312Aull;
constexpr std::uint64_t kInventoryGetInventoryIdFromPed = 0x13D234A2A3F66E63ull;
constexpr std::uint64_t kCountInventoryItems = 0xE787F05DFC977BDEull;
constexpr std::uint64_t kInventoryGetGuidFromItemId = 0x886DFD3E185C8A89ull;
constexpr std::uint64_t kInventoryIsGuidValid = 0xB881CA836CC4B6D4ull;
constexpr std::uint64_t kPlayerInventoryGetItemCount = 0xC97E0D2302382211ull;
constexpr std::uint64_t kItemDatabaseGetShopItemsCount = 0xC568B1A0F17C7025ull;
constexpr std::uint64_t kItemDatabaseGetShopItemInfo = 0x4A79B41B4EB91F4Eull;
constexpr std::uint64_t kItemDatabaseGetAcquireCostsCountFromCostType = 0xDEE7B3C76ED664BEull;
constexpr std::uint64_t kItemDatabaseFilloutItem = 0xAD73B614DF26CF8Aull;
constexpr std::uint64_t kItemDatabaseGetBundleAcquireCostsCountFromCost = 0x388088BFF3681189ull;
constexpr std::uint64_t kItemDatabaseFilloutBundle = 0xB542632693D53408ull;
constexpr std::uint64_t kGetPlayerInteractionTargetEntity = 0x3EE1F7A8C32F24E1ull;
constexpr std::uint64_t kGetEntityModel = 0xDA76A9F39210D365ull;
constexpr std::uint64_t kRequestStreamedTextureDict = 0xC1BA29DF5631B0F8ull;
constexpr std::uint64_t kHasStreamedTextureDictLoaded = 0x54D6900929CCF162ull;
constexpr std::uint64_t kDrawSprite = 0xC9884ECADE94CB34ull;
constexpr std::uint64_t kGetScreenResolution = 0x66773C92835D0909ull;
constexpr std::uint64_t kDrawRect = 0x405224591DF02025ull;
constexpr std::uint64_t kVarString = 0xFA925AC00EB830B9ull;
// Resolve a Rockstar label hash through the game's own active language table.
constexpr std::uint64_t kGetStringFromHashKey = 0xBD5DD5EAE2B6CE14ull;
constexpr std::uint64_t kBgDisplayText = 0x16794E044C9EFB58ull;
constexpr std::uint64_t kBgSetTextScale = 0xA1253A3C870B6843ull;
constexpr std::uint64_t kBgSetTextColor = 0x16FA5CE47F184F1Eull;
constexpr std::uint64_t kSetTextCentre = 0xBE5261939FBECB8Cull;
constexpr std::uint64_t kChalGetNumRanksCompleted = 0x58CB53DB63F84DE9ull;
constexpr std::uint64_t kChalGetMaxRanks = 0x58CB53DB63F84DEAull;
// Rockstar's current in-game interface language (0-12). This is used only
// for the companion's Auto language setting; an explicit companion locale wins.
constexpr std::uint64_t kGetCurrentLanguage = 0xDB917DA5C6835FCCull;
// Stable Rockstar localization label hashes for ItemDatabase entries.
// Resolve them through the game's native string table or the companion-selected
// official game-language table when that locale differs from the active game.
constexpr std::uint64_t kInventoryGetItemDescriptionHash = 0xA4550FE9C512E3DDull;
constexpr std::uint64_t kItemDbLocalizationGetNumLabelTypes = 0xCEC6A41E8910486Aull;
constexpr std::uint64_t kItemDbLocalizationGetNumValues = 0x49885D82A13EEAEAull;
constexpr std::uint64_t kItemDbLocalizationGetType = 0xCABF5D41D0073D4Aull;
constexpr std::uint64_t kItemDbLocalizationGetValue = 0x9AE5610FDCED6EA7ull;
constexpr int kExpectedChallengeRankCount = 10;
constexpr std::uint64_t kMoneyHash = 0x7C407083u;
constexpr std::uint32_t kDefaultMenuKey = VK_F8;
constexpr std::size_t kMaxRecipesPerShop = 256;
constexpr std::size_t kMaxRecipeCosts = 16;
constexpr std::size_t kMaxCategories = 24;
constexpr int kVisibleMenuRows = 10;

constexpr std::uint32_t kShopPearson = 0xC48F9BE5u;
constexpr std::uint32_t kShopTrapper = 0x55FBE337u;
constexpr std::uint32_t kCostCraftingPearson = 0x2D652C21u;
constexpr std::uint32_t kCostCraftingTrapper = 0x160953CAu;
constexpr std::uint32_t kCostCraftingTrapperPromo = 0x1565D351u;
constexpr std::uint32_t kPearsonStorageSlot = 0x550898DEu;
constexpr std::uint32_t kTrapperGiveStorageSlot = 0xAEEE1782u;
constexpr std::uint32_t kCharacterSlot = 0x4F31C2D3u;
constexpr std::uint32_t kSlotNone = 0xA1212100u;

// These two Pearson catalog records are internal, unlabelled placeholders,
// not player-facing recipes. Pearson itself remains fully supported.
constexpr std::uint32_t kPearsonUnlabelledItem1 = 0xC998B07Cu;
constexpr std::uint32_t kPearsonUnlabelledItem2 = 0xF9C120CDu;
constexpr bool is_pearson_unlabelled_placeholder(std::uint32_t itemHash) noexcept {
    return itemHash == kPearsonUnlabelledItem1 || itemHash == kPearsonUnlabelledItem2;
}

constexpr int kMaxShopItems = 2048;
constexpr int kMaxCostRows = 64;
constexpr std::size_t kMaxMaterials = 128;

constexpr std::uint32_t joaat(const char* text) noexcept {
    std::uint32_t hash = 0;
    while (*text != '\0') {
        std::uint8_t character = static_cast<std::uint8_t>(*text++);
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<std::uint8_t>(character + ('a' - 'A'));
        }
        hash += character;
        hash += hash << 10;
        hash ^= hash >> 6;
    }
    hash += hash << 3;
    hash ^= hash >> 11;
    hash += hash << 15;
    return hash;
}

struct ChallengeDefinition {
    const char* name;
    const char* rootHashName;
    const char* rankObjectives[kExpectedChallengeRankCount];
};

// These are the actual single-player challenge root identifiers used by
// RDR2's pause-menu challenge registry. Each root is hashed with JOAAT and
// passed to the CHAL_GET_* natives.
// The objective strings are concise paraphrases of the next rank's goal.
// They are reference text only: actual progress remains exclusively game-read.
constexpr auto kChallengeDefinitions = std::array{
    ChallengeDefinition{"Bandit", "sp_chal_band_root", {
        "Hold up 5 townsfolk.",
        "Rob 2 coaches or return 2 stolen coaches to a Fence.",
        "Rob the cash registers in 4 shops in one day.",
        "Rob or return 3 coaches in one day.",
        "Build a $250 bounty in one state.",
        "Steal 5 horses and sell them at the Clemens Cove Horse Fence.",
        "Rob $50 in cash and valuables from townsfolk and travellers.",
        "Steal 7 wagons and sell them to the Emerald Ranch Fence.",
        "Hogtie someone and leave them on railroad tracks 3 times.",
        "Rob 5 trains without dying or being caught."
    }},
    ChallengeDefinition{"Explorer", "sp_chal_expl_root", {
        "Find a treasure map.",
        "Find a treasure.",
        "Find a treasure.",
        "Find a treasure.",
        "Find a treasure.",
        "Find a treasure.",
        "Find a treasure.",
        "Find a treasure.",
        "Find a treasure.",
        "Find a treasure."
    }},
    ChallengeDefinition{"Gambler", "sp_chal_gamb_root", {
        "Win 5 hands of Poker.",
        "Double down and win 5 Blackjack hands.",
        "Win 3 games of Five Finger Fillet.",
        "Bust a Poker opponent at Flatneck, Saint Denis and Valentine.",
        "Win 3 Dominoes rounds without drawing, against 2 or fewer opponents.",
        "Beat Blackjack dealers in Rhodes and Van Horn.",
        "Beat the Five Finger Fillet player in Strawberry, Valentine and Van Horn.",
        "Win 3 Blackjack hands with at least 3 hits each.",
        "Win 3 games of Dominoes in a row.",
        "Win 3 hands of Poker in a row."
    }},
    ChallengeDefinition{"Herbalist", "sp_chal_herb_root", {
        "Pick 6 Yarrow.",
        "Pick and eat 4 different berry species.",
        "Craft 7 items using Sage.",
        "Pick 5 mushrooms and feed them to your horse.",
        "Craft 9 items using Indian Tobacco.",
        "Pick 15 different herb species.",
        "Craft and use 5 Special Miracle Tonics.",
        "Use Oleander Sage to craft 6 poison weapons.",
        "Pick one of every herb species.",
        "Season and cook all 11 types of meat."
    }},
    ChallengeDefinition{"Horseman", "sp_chal_horse_root", {
        "Kill 5 rabbits from horseback.",
        "Jump over 3 obstacles within 15 seconds.",
        "Ride from Valentine to Rhodes in under 5 minutes.",
        "Drag a victim 3,300 feet with your lasso while mounted.",
        "Trample 5 animals while on horseback.",
        "Ride Strawberry to Saint Denis in under 9 minutes without touching water.",
        "Kill 7 enemies from horseback without dismounting.",
        "Kill 9 predators from horseback.",
        "Ride Van Horn to Blackwater in under 17 minutes without touching water.",
        "Break every wild horse breed."
    }},
    ChallengeDefinition{"Master Hunter", "sp_chal_hunt_root", {
        "Skin 3 deer.",
        "Collect 3 perfect rabbit pelts.",
        "Track 10 animal species with your binoculars.",
        "Call an animal and get a clean kill 5 times.",
        "Skin 3 black bears or grizzlies.",
        "Kill 5 cougars with your bow, then skin them.",
        "Use bait to lure and kill one herbivore and one predator.",
        "Catch 3 small fish without using a fishing rod.",
        "Kill an opossum while it is playing possum.",
        "Find and kill the legendary panther Giaguaro."
    }},
    ChallengeDefinition{"Sharpshooter", "sp_chal_shot_root", {
        "Kill 3 flying birds.",
        "Kill 2 animal species in one Dead Eye use.",
        "Kill 5 flying birds while aboard a moving train.",
        "Kill an enemy from 80 feet away with a tomahawk.",
        "Kill 6 animals without switching or reloading your weapon.",
        "Hit a target from at least 660 feet with a long-scoped rifle.",
        "Get 7 headshots in a row.",
        "Disarm 3 enemies without reloading or switching weapons.",
        "Shoot 3 people's hats off in one Dead Eye use.",
        "Kill 3 flying birds with 3 consecutive long-scoped rifle shots."
    }},
    ChallengeDefinition{"Survivalist", "sp_chal_surv_root", {
        "Catch 3 Bluegill.",
        "Donate 5 animals to camp or the Trapper.",
        "Kill 5 animals with a Varmint Rifle.",
        "Craft dynamite, fire, improved, poison and small-game arrows.",
        "Catch a Bayou fish from a riverboat and another while on train tracks.",
        "Kill 5 scavengers while they feed on corpses.",
        "Kill 8 small-game animals with consecutive small-game arrow shots.",
        "Craft a homing and improved tomahawk, volatile dynamite and volatile fire bottle.",
        "Catch a fish weighing at least 19 pounds.",
        "Catch one of every type of fish."
    }},
    ChallengeDefinition{"Weapons Expert", "sp_chal_weap_root", {
        "Kill 3 enemies with a knife.",
        "Kill 3 enemies in 10 seconds using only throwing knives.",
        "Kill 3 birds of prey using only a tomahawk.",
        "Kill 10 enemies with a shotgun using crafted ammunition.",
        "Kill 5 mounted enemies with one throwing knife per kill.",
        "Kill 4 enemies at once with a single stick of dynamite.",
        "Kill 4 enemies in a row by throwing and retrieving the same tomahawk.",
        "Kill 15 enemies with a long-barrelled shotgun.",
        "Kill 9 unaware enemies from behind using a bow.",
        "Kill a grizzly without taking damage, using only throwing knives."
    }},
};

// Resolve the next-rank objective from Rockstar's official locale table whenever
// the game has a stable label ID for it. Only the 14 companion-specific composite
// objectives use the exact whole-sentence .lang dictionary fallback.
const char* localized_challenge_objective(const char* sourceObjective) noexcept {
    if (sourceObjective == nullptr || sourceObjective[0] == '\0') return "";
    const auto* first = std::begin(hunting_companion::embedded_challenge::kObjectiveHashes);
    const auto* last = std::end(hunting_companion::embedded_challenge::kObjectiveHashes);
    const auto* found = std::lower_bound(first, last, sourceObjective,
        [](const hunting_companion::embedded_challenge::ObjectiveHash& entry,
           const char* objective) {
            return std::strcmp(entry.objective, objective) < 0;
        });
    if (found != last && std::strcmp(found->objective, sourceObjective) == 0) {
        if (const char* official = hunting_companion::official_item_string(found->labelHash)) {
            return official;
        }
    }
    return hunting_companion::ui_text(sourceObjective);
}

constexpr bool all_challenge_objectives_defined() noexcept {
    for (const auto& challenge : kChallengeDefinitions) {
        for (const char* objective : challenge.rankObjectives) {
            if (objective == nullptr || objective[0] == '\0') return false;
        }
    }
    return true;
}
static_assert(all_challenge_objectives_defined(),
              "Every one of the 90 challenge ranks needs a description");

constexpr bool objective_fits_two_lines(const char* objective) noexcept {
    constexpr std::size_t kMaxCharactersPerLine = 48;
    std::size_t length = 0;
    while (objective[length] != '\0') ++length;
    if (length <= kMaxCharactersPerLine) return true;
    std::size_t firstLineLength = kMaxCharactersPerLine;
    while (firstLineLength > 0 && objective[firstLineLength] != ' ') --firstLineLength;
    if (firstLineLength == 0) firstLineLength = kMaxCharactersPerLine;
    std::size_t secondLineStart = firstLineLength;
    while (objective[secondLineStart] == ' ') ++secondLineStart;
    return length - secondLineStart <= kMaxCharactersPerLine;
}

constexpr bool all_challenge_objectives_fit_two_lines() noexcept {
    for (const auto& challenge : kChallengeDefinitions) {
        for (const char* objective : challenge.rankObjectives) {
            if (!objective_fits_two_lines(objective)) return false;
        }
    }
    return true;
}
static_assert(all_challenge_objectives_fit_two_lines(),
              "Challenge objective copy must fit within two bounded panel lines");

// Pin the expected JOAAT values at compile time so an accidental edit to a
// root string or the hash routine fails the build.
static_assert(joaat("sp_chal_band_root") == 0xD5D688C1u, "Bandit root hash mismatch");
static_assert(joaat("sp_chal_expl_root") == 0xB3CE15D7u, "Explorer root hash mismatch");
static_assert(joaat("sp_chal_gamb_root") == 0x7011A716u, "Gambler root hash mismatch");
static_assert(joaat("sp_chal_herb_root") == 0xB8678749u, "Herbalist root hash mismatch");
static_assert(joaat("sp_chal_horse_root") == 0x0908F687u, "Horseman root hash mismatch");
static_assert(joaat("sp_chal_hunt_root") == 0x4960B55Eu, "Master Hunter root hash mismatch");
static_assert(joaat("sp_chal_shot_root") == 0xD9B45839u, "Sharpshooter root hash mismatch");
static_assert(joaat("sp_chal_surv_root") == 0x63C5A991u, "Survivalist root hash mismatch");
static_assert(joaat("sp_chal_weap_root") == 0x004EE82Fu, "Weapons Expert root hash mismatch");
constexpr std::size_t kChallengeCount = kChallengeDefinitions.size();
static_assert(kChallengeCount == 9, "Story Mode has nine challenge categories");

struct ChallengeProgress {
    int ranksCompleted{}; // raw value returned by CHAL_GET_NUM_RANKS_COMPLETED
    int maxRanks{};        // raw value returned by CHAL_GET_MAX_RANKS
    bool available{};
};

struct Guid {
    alignas(8) std::int32_t data1{};
    alignas(8) std::int32_t data2{};
    alignas(8) std::int32_t data3{};
    alignas(8) std::int32_t data4{};
};
static_assert(sizeof(Guid) == 32, "RDR2 inventory GUID must be 32 bytes");

bool get_guid_from_item(int inventoryId, Guid* parent, std::uint32_t itemHash,
                        std::uint32_t slotHash, Guid* outGuid) noexcept {
    *outGuid = {};
    return invoke_native<int>(kInventoryGetGuidFromItemId, inventoryId, parent,
                              itemHash, slotHash, outGuid) != 0;
}

bool guid_is_valid(Guid* guid) noexcept {
    return invoke_native<int>(kInventoryIsGuidValid, guid) != 0;
}

int count_guid(int inventoryId, Guid* guid) noexcept {
    return invoke_native<int>(kPlayerInventoryGetItemCount, inventoryId, guid, false);
}

int vendor_material_count(int inventoryId, Guid* rootGuid,
                          std::uint32_t materialHash, std::uint32_t slotHash) noexcept {
    Guid itemGuid{};
    if (!get_guid_from_item(inventoryId, rootGuid, materialHash, slotHash, &itemGuid)) return 0;
    if (!guid_is_valid(&itemGuid)) return 0;
    const int count = count_guid(inventoryId, &itemGuid);
    return count > 0 ? count : 0;
}

struct RecipeIdentity { std::uint32_t itemHash; std::uint32_t categoryHash; const char* name; };
constexpr RecipeIdentity kRecipeIdentities[] = {
    RecipeIdentity{0x2A348502u, 0x1F5EEA44u, "Snake Skin Banjo Head"},
    RecipeIdentity{0x2DA8186Au, 0x1F5EEA44u, "Wolf Skull"},
    RecipeIdentity{0x3326D4FAu, 0x1F5EEA44u, "Campfire Seat Cover"},
    RecipeIdentity{0x36B2ED97u, 0xDCD17F1Fu, "Kit Satchel"},
    RecipeIdentity{0x4E14E705u, 0xDCD17F1Fu, "Materials Satchel"},
    RecipeIdentity{0x5A0673C0u, 0x1F5EEA44u, "Moose Antlers"},
    RecipeIdentity{0x5BE33D1Du, 0xDCD17F1Fu, "Valuables Satchel"},
    RecipeIdentity{0x7F624F7Du, 0x1F5EEA44u, "Lean-to Cow Hide Ground Cover"},
    RecipeIdentity{0x81A90B40u, 0x1F5EEA44u, "Boar Skin Table Cover"},
    RecipeIdentity{0x83786739u, 0x1F5EEA44u, "Alligator Skull"},
    RecipeIdentity{0xA020FAE6u, 0x1F5EEA44u, "Boar Skin Rug"},
    RecipeIdentity{0xA1CA0708u, 0xDCD17F1Fu, "Provisions Satchel"},
    RecipeIdentity{0xAB430AE8u, 0xDCD17F1Fu, "Tonics Satchel"},
    RecipeIdentity{0xB0C4AD75u, 0x1F5EEA44u, "Pronghorn Skull"},
    RecipeIdentity{0xB4B8749Bu, 0x1F5EEA44u, "Ox Hide Rug"},
    RecipeIdentity{0xBBBB14C1u, 0x1F5EEA44u, "Pronghorn Leather Table Tops"},
    RecipeIdentity{0xCD70FF85u, 0x1F5EEA44u, "Elk Antlers"},
    RecipeIdentity{0xCE127491u, 0x1F5EEA44u, "Big Horn Ram Skull"},
    RecipeIdentity{0xCE847162u, 0xDCD17F1Fu, "Ingredients Satchel"},
    RecipeIdentity{0xD006CD59u, 0x1F5EEA44u, "Scout Fire Ground Cover"},
    RecipeIdentity{0xDC99106Du, 0xDCD17F1Fu, "Legend of the East Satchel"},
    RecipeIdentity{0xF2992228u, 0x1F5EEA44u, "Campfire Log Seat Cover"},
    RecipeIdentity{0xF72FB942u, 0x1F5EEA44u, "Cougar Pelt Covered Chest"},
    RecipeIdentity{0xFF88D22Au, 0x1F5EEA44u, "Hanging Bones"},
    RecipeIdentity{0x036F1224u, 0x29A49A30u, "Wilderness Vest"},
    RecipeIdentity{0x04AFD7FEu, 0x851EBBD3u, "Beaver Mountain Hat"},
    RecipeIdentity{0x04E8FADBu, 0x29A49A30u, "Hinterland Vest"},
    RecipeIdentity{0x05FC4F77u, 0x851EBBD3u, "Goat Flop Hat"},
    RecipeIdentity{0x061E2F1Du, 0xCDFBBB30u, "Legendary Elk Moccasins"},
    RecipeIdentity{0x06E86861u, 0x851EBBD3u, "Legendary Beaver Flop Hat"},
    RecipeIdentity{0x076F5442u, 0xC46CC879u, "The Death Roll"},
    RecipeIdentity{0x0ABEC24Au, 0x851EBBD3u, "Legendary Ram Hat"},
    RecipeIdentity{0x0D9236AAu, 0x415F0998u, "Bandit Bandolier"},
    RecipeIdentity{0x0DE74ADAu, 0x9989398Bu, "Legendary Cougar Riding Gloves"},
    RecipeIdentity{0x1015ECC5u, 0xC46CC879u, "The Beast of Prey"},
    RecipeIdentity{0x110241BAu, 0x9989398Bu, "Legendary Buck & Fox Range Gloves"},
    RecipeIdentity{0x11FF5266u, 0x9989398Bu, "Winter Cavalry Gloves"},
    RecipeIdentity{0x13AE3B04u, 0x3D9F76CDu, "Sharpshooter Gun Belt"},
    RecipeIdentity{0x147DE3DAu, 0x09252916u, "Master Hunter Holster"},
    RecipeIdentity{0x14ED6C7Eu, 0x3D9F76CDu, "Gambler Gun Belt"},
    RecipeIdentity{0x1545A376u, 0x415F0998u, "Herbalist Bandolier"},
    RecipeIdentity{0x187C9718u, 0xC8F18EFCu, "Pronghorn Half Chaps"},
    RecipeIdentity{0x18E48396u, 0x6971A6CBu, "Boar Fringed Shotgun Chaps"},
    RecipeIdentity{0x1A667B0Au, 0xCDFBBB30u, "Bull Fowler Boots"},
    RecipeIdentity{0x1C50DE66u, 0x29A49A30u, "Legendary Cougar & Wolf Vest"},
    RecipeIdentity{0x1C5C7CC2u, 0xC8F18EFCu, "Muskrat Half Chaps"},
    RecipeIdentity{0x1CAC7B98u, 0x29A49A30u, "Legendary Buck Vest"},
    RecipeIdentity{0x1E55237Du, 0x851EBBD3u, "Legendary Cougar Flop Hat"},
    RecipeIdentity{0x1EC3956Eu, 0xCD31B12Cu, "Horseman Off-Hand Holster"},
    RecipeIdentity{0x21A51495u, 0x9989398Bu, "Iguana Range Gloves"},
    RecipeIdentity{0x21D78CD7u, 0x851EBBD3u, "Gila Monster Gambler's Hat"},
    RecipeIdentity{0x24B41AA7u, 0x6971A6CBu, "Sheep Batwing Chaps"},
    RecipeIdentity{0x25376605u, 0x09252916u, "Bandit Holster"},
    RecipeIdentity{0x261C06C9u, 0xC46CC879u, "The Rattler"},
    RecipeIdentity{0x26911A76u, 0xC8F18EFCu, "Javelina Half Chaps"},
    RecipeIdentity{0x26D2671Bu, 0x09252916u, "Herbalist Holster"},
    RecipeIdentity{0x279DD42Eu, 0x6971A6CBu, "Batwing Chaps"},
    RecipeIdentity{0x28A86103u, 0x851EBBD3u, "Pilgrim Accessory"},
    RecipeIdentity{0x2AA8CB35u, 0xC46CC879u, "The Ghost Bison"},
    RecipeIdentity{0x2B56DA12u, 0x6F58734Fu, "Cougar Cutaway Coat"},
    RecipeIdentity{0x2BEA8ED4u, 0xD582F9FAu, "Beaver Roping Saddle"},
    RecipeIdentity{0x2F0BB426u, 0x6F58734Fu, "Beaver Hunting Jacket"},
    RecipeIdentity{0x2F1FEB04u, 0x6971A6CBu, "Elk Fringed Shotgun Chaps"},
    RecipeIdentity{0x2F85F580u, 0xCD31B12Cu, "Gambler Off-Hand Holster"},
    RecipeIdentity{0x31A7A75Cu, 0x851EBBD3u, "Pursuer Accessory"},
    RecipeIdentity{0x31AC666Du, 0xC46CC879u, "The Wrangler"},
    RecipeIdentity{0x353FC03Cu, 0xD582F9FAu, "Cougar McClelland Saddle"},
    RecipeIdentity{0x35FDBD3Au, 0x6971A6CBu, "Bear Batwing Chaps"},
    RecipeIdentity{0x3716C79Eu, 0xC46CC879u, "The Huntsman"},
    RecipeIdentity{0x3D631CA8u, 0x29A49A30u, "Outdoorsmen Vest"},
    RecipeIdentity{0x3F8B767Cu, 0xCD31B12Cu, "Bandit Off-Hand Holster"},
    RecipeIdentity{0x41C8FF69u, 0x6F58734Fu, "Trapper's Cloak"},
    RecipeIdentity{0x42D097FCu, 0xC8F18EFCu, "Legendary Coyote Half Chaps"},
    RecipeIdentity{0x444F45DDu, 0x9989398Bu, "Legendary Elk Range Gloves"},
    RecipeIdentity{0x4668BD60u, 0x9989398Bu, "Badger Rifleman Gloves"},
    RecipeIdentity{0x49A985D7u, 0xA9BF2F76u, "Tomahawk"},
    RecipeIdentity{0x4B13E766u, 0x6971A6CBu, "Legendary Bison Batwing Chaps"},
    RecipeIdentity{0x4CCB2BB7u, 0xCDFBBB30u, "Boar & Bull Fowler Boots"},
    RecipeIdentity{0x4CDE0BEAu, 0x3D9F76CDu, "Explorer Gun Belt"},
    RecipeIdentity{0x4D1E264Eu, 0xC46CC879u, "The Marauder"},
    RecipeIdentity{0x4D229596u, 0x29A49A30u, "No-Man's Vest"},
    RecipeIdentity{0x5358D14Au, 0x09252916u, "Explorer Holster"},
    RecipeIdentity{0x5812069Du, 0x6971A6CBu, "Ox Fringed Shotgun Chaps"},
    RecipeIdentity{0x585116FBu, 0x851EBBD3u, "Raccoon Mountain Hat"},
    RecipeIdentity{0x5B5541C0u, 0x415F0998u, "Survivalist Bandolier"},
    RecipeIdentity{0x5B81CB0Fu, 0x851EBBD3u, "Legendary Coyote Mountain Hat"},
    RecipeIdentity{0x6032870Au, 0xCDFBBB30u, "Two Toned Moccasins"},
    RecipeIdentity{0x6048B751u, 0x9989398Bu, "Buck Riding Gloves"},
    RecipeIdentity{0x626B555Cu, 0x851EBBD3u, "Bison Gambler's Hat"},
    RecipeIdentity{0x648DB0C0u, 0xC46CC879u, "The Trophy Buck"},
    RecipeIdentity{0x64DCF194u, 0x29A49A30u, "Rustic Vest"},
    RecipeIdentity{0x65842E31u, 0x9989398Bu, "Moose Range Gloves"},
    RecipeIdentity{0x66DA1DF4u, 0x851EBBD3u, "Native Accessory"},
    RecipeIdentity{0x6890DD33u, 0xC46CC879u, "The Desperado"},
    RecipeIdentity{0x68B00B9Fu, 0x9989398Bu, "Boar Riding Gloves"},
    RecipeIdentity{0x694A52D4u, 0x9989398Bu, "Legendary Panther Range Gloves"},
    RecipeIdentity{0x6AF284B7u, 0x9989398Bu, "Snake Skin Cavalry Gloves"},
    RecipeIdentity{0x6C857105u, 0x851EBBD3u, "Ram Sombrero"},
    RecipeIdentity{0x6D597A84u, 0x851EBBD3u, "Grenadier Hat"},
    RecipeIdentity{0x6F463C85u, 0x9989398Bu, "Elk Riding Gloves"},
    RecipeIdentity{0x6FE8F76Bu, 0x415F0998u, "Explorer Bandolier"},
    RecipeIdentity{0x70F4720Au, 0x9B317DD1u, "Cover Scent Lotion"},
    RecipeIdentity{0x710B8C63u, 0x6971A6CBu, "Legendary Ram Batwing Chaps"},
    RecipeIdentity{0x727D970Bu, 0x851EBBD3u, "Legendary Alligator Gambler's Hat"},
    RecipeIdentity{0x73000D4Bu, 0x9B317DD1u, "Herbivore Bait"},
    RecipeIdentity{0x739D76A1u, 0x6F58734Fu, "Ram Shotgun Coat"},
    RecipeIdentity{0x73ADFC62u, 0x851EBBD3u, "Skunk Mountain Hat"},
    RecipeIdentity{0x741C2CF4u, 0xCDFBBB30u, "Javelina Moccasins"},
    RecipeIdentity{0x79069109u, 0xCD31B12Cu, "Survivalist Off-Hand Holster"},
    RecipeIdentity{0x795E4034u, 0x851EBBD3u, "Huntsman Accessory"},
    RecipeIdentity{0x7B3487C0u, 0x851EBBD3u, "Pioneer Accessory"},
    RecipeIdentity{0x7B8DA3D9u, 0x09252916u, "Horseman Holster"},
    RecipeIdentity{0x7C3D43CAu, 0xCD31B12Cu, "Weapons Expert Off-Hand Holster"},
    RecipeIdentity{0x7CDFD715u, 0x9989398Bu, "Pigskin Rifleman Gloves"},
    RecipeIdentity{0x7D795D72u, 0xD582F9FAu, "Rattlesnake Vaquero Saddle"},
    RecipeIdentity{0x7DBBF737u, 0x851EBBD3u, "Rat Flat Cap"},
    RecipeIdentity{0x7FEA9DBAu, 0x3D9F76CDu, "Horseman Gun Belt"},
    RecipeIdentity{0x80770CFFu, 0xC46CC879u, "The Mountain Man"},
    RecipeIdentity{0x8197ED62u, 0x09252916u, "Sharpshooter Holster"},
    RecipeIdentity{0x82835B7Fu, 0x6971A6CBu, "Bull Fringed Shotgun Chaps"},
    RecipeIdentity{0x86F2C5DEu, 0x851EBBD3u, "Muskrat Cavalry Hat"},
    RecipeIdentity{0x883181CAu, 0x415F0998u, "Horseman Bandolier"},
    RecipeIdentity{0x89137A7Eu, 0x851EBBD3u, "Legendary White Bison Hat"},
    RecipeIdentity{0x8AF24615u, 0xCDFBBB30u, "Skunk Trapper Boots"},
    RecipeIdentity{0x8B0CD608u, 0x6F58734Fu, "Legendary White Bison Coat"},
    RecipeIdentity{0x8BE9DC6Fu, 0x9989398Bu, "Legendary Pronghorn Range Gloves"},
    RecipeIdentity{0x9055FBECu, 0x29A49A30u, "Principal Vest"},
    RecipeIdentity{0x9200EEFAu, 0x6F58734Fu, "Coyote Scout Jacket"},
    RecipeIdentity{0x923775A8u, 0x6971A6CBu, "Legendary Wolf Batwing Chaps"},
    RecipeIdentity{0x92B3B4BCu, 0xCDFBBB30u, "Legendary Fox Moccasins"},
    RecipeIdentity{0x96C9B9A4u, 0xC46CC879u, "The Bounty Hunter"},
    RecipeIdentity{0x971306DEu, 0x851EBBD3u, "Scavenger Accessory"},
    RecipeIdentity{0x97BF4614u, 0x09252916u, "Survivalist Holster"},
    RecipeIdentity{0x986E0F1Bu, 0x851EBBD3u, "Judicious Accessory"},
    RecipeIdentity{0x9C588EEBu, 0x851EBBD3u, "Legendary Bear Head Hat"},
    RecipeIdentity{0x9C77E68Du, 0x851EBBD3u, "Majestic Accessory"},
    RecipeIdentity{0x9C7D5A0Fu, 0xCD31B12Cu, "Herbalist Off-Hand Holster"},
    RecipeIdentity{0x9E4AD291u, 0xA9BF2F76u, "Throwing Knife"},
    RecipeIdentity{0x9E8501E3u, 0x9B317DD1u, "Predator Bait"},
    RecipeIdentity{0xA267420Cu, 0xCDFBBB30u, "Legendary Boar & Bison Fowlers"},
    RecipeIdentity{0xA2D5BB36u, 0x415F0998u, "Sharpshooter Bandolier"},
    RecipeIdentity{0xA5E972D7u, 0xD905CE1Cu, "Tomahawk"},
    RecipeIdentity{0xA7E79553u, 0x851EBBD3u, "Stalker Accessory"},
    RecipeIdentity{0xAA95CF24u, 0x29A49A30u, "Country Vest"},
    RecipeIdentity{0xAB941800u, 0xC46CC879u, "The Stalker"},
    RecipeIdentity{0xAB9C65F4u, 0x29A49A30u, "Sheepskin Vest"},
    RecipeIdentity{0xAD678539u, 0x851EBBD3u, "Beaver Drifter Hat"},
    RecipeIdentity{0xADCE8C80u, 0x415F0998u, "Weapons Expert Bandolier"},
    RecipeIdentity{0xAED5E9BCu, 0xC46CC879u, "The Night Wrangler"},
    RecipeIdentity{0xB39B919Eu, 0x851EBBD3u, "Snake Bulldogger Hat"},
    RecipeIdentity{0xB5802A5Fu, 0xD582F9FAu, "Alligator Ranch Cutter Saddle"},
    RecipeIdentity{0xB7F16340u, 0x3D9F76CDu, "Survivalist Gun Belt"},
    RecipeIdentity{0xBA0D6076u, 0x29A49A30u, "Billy Vest"},
    RecipeIdentity{0xBA4EF0B9u, 0xC8F18EFCu, "Legendary Elk Half Chaps"},
    RecipeIdentity{0xBBA13AFCu, 0x6971A6CBu, "Bear Fringed Shotgun Chaps"},
    RecipeIdentity{0xBBF0D5E8u, 0x6F58734Fu, "Legendary Bear Coat"},
    RecipeIdentity{0xBDCFAF4Fu, 0xC8F18EFCu, "Deer Pelt Half Chaps"},
    RecipeIdentity{0xBDD4D09Du, 0x851EBBD3u, "Glorious Accessory"},
    RecipeIdentity{0xC014C1C2u, 0x29A49A30u, "Legendary Bison Vest"},
    RecipeIdentity{0xC3A3D2B5u, 0x851EBBD3u, "Homestead Accessory"},
    RecipeIdentity{0xC3B3D66Bu, 0xCD31B12Cu, "Master Hunter Off-Hand Holster"},
    RecipeIdentity{0xC473970Eu, 0x29A49A30u, "Rugged Wrangler Vest"},
    RecipeIdentity{0xC4EA99AAu, 0x3D9F76CDu, "Herbalist Gun Belt"},
    RecipeIdentity{0xC5459A31u, 0x9989398Bu, "Legendary Beaver Cavalry Gloves"},
    RecipeIdentity{0xC5E2D11Eu, 0xCDFBBB30u, "Legendary Bear Roper"},
    RecipeIdentity{0xC66C79E1u, 0x851EBBD3u, "Squirrel Flat Cap"},
    RecipeIdentity{0xC76C46D9u, 0xD582F9FAu, "Panther Trail Saddle"},
    RecipeIdentity{0xC7A9A76Fu, 0x9989398Bu, "Legendary Ram Rifleman Gloves"},
    RecipeIdentity{0xC7E0B325u, 0x851EBBD3u, "Fox Mountain Hat"},
    RecipeIdentity{0xC7F0E174u, 0xCD31B12Cu, "Sharpshooter Off-Hand Holster"},
    RecipeIdentity{0xC9887117u, 0x6F58734Fu, "Wolf Coat"},
    RecipeIdentity{0xC9B12985u, 0x851EBBD3u, "Deer Cavalry Hat"},
    RecipeIdentity{0xCA4316DDu, 0x6971A6CBu, "Moose Batwing Chaps"},
    RecipeIdentity{0xCA73B790u, 0x9B317DD1u, "Gun Oil"},
    RecipeIdentity{0xCBE5DE6Fu, 0xCDFBBB30u, "Legendary Moose Moccasins"},
    RecipeIdentity{0xCC03C967u, 0x6F58734Fu, "Legendary Moose Hunting Jacket"},
    RecipeIdentity{0xD225CCA0u, 0xD582F9FAu, "Boar Mother Hubbard Saddle"},
    RecipeIdentity{0xD377E160u, 0x415F0998u, "Gambler Bandolier"},
    RecipeIdentity{0xD443568Du, 0x415F0998u, "Master Hunter Bandolier"},
    RecipeIdentity{0xD44722FDu, 0x851EBBD3u, "Plantation Slouch Hat"},
    RecipeIdentity{0xD5905CEEu, 0x851EBBD3u, "Pronghorn Flop Hat"},
    RecipeIdentity{0xDA1C7ADCu, 0xCDFBBB30u, "Moccasins"},
    RecipeIdentity{0xDD3438B1u, 0x09252916u, "Weapons Expert Holster"},
    RecipeIdentity{0xDD60D28Fu, 0xC8F18EFCu, "Moose Half Chaps"},
    RecipeIdentity{0xDE333016u, 0xCD31B12Cu, "Explorer Off-Hand Holster"},
    RecipeIdentity{0xDE5A2905u, 0xD582F9FAu, "Bear Dakota Saddle"},
    RecipeIdentity{0xDEE7942Fu, 0x851EBBD3u, "Iguana Big Valley Hat"},
    RecipeIdentity{0xDFA86A70u, 0x6F58734Fu, "Legendary Pronghorn Coat"},
    RecipeIdentity{0xE46B39EDu, 0x29A49A30u, "Huntsman Vest"},
    RecipeIdentity{0xE4CA44D0u, 0x851EBBD3u, "Coyote Gambler's Hat"},
    RecipeIdentity{0xE56257BCu, 0x3D9F76CDu, "Master Hunter Gun Belt"},
    RecipeIdentity{0xEB4AEB74u, 0xCDFBBB30u, "Legendary Alligator Fowlers"},
    RecipeIdentity{0xECC59D6Du, 0xC46CC879u, "The Bear Hunter"},
    RecipeIdentity{0xED0395BEu, 0x851EBBD3u, "Rococo Accessory"},
    RecipeIdentity{0xF1C1EBD2u, 0x851EBBD3u, "Bear Grenadier Hat"},
    RecipeIdentity{0xF4F2C17Du, 0x851EBBD3u, "Elk Flop Hat"},
    RecipeIdentity{0xF5861CC8u, 0x851EBBD3u, "Rat Infantry Hardee Hat"},
    RecipeIdentity{0xF5A60D53u, 0x29A49A30u, "Legendary Alligator Vest"},
    RecipeIdentity{0xF6FFA49Fu, 0xC46CC879u, "The Dreamcatcher"},
    RecipeIdentity{0xF805D3D9u, 0x851EBBD3u, "Badger Mountain Hat"},
    RecipeIdentity{0xFB5FF58Fu, 0x3D9F76CDu, "Bandit Gun Belt"},
    RecipeIdentity{0xFBB3435Eu, 0x6F58734Fu, "Legendary Panther Cloak"},
    RecipeIdentity{0xFDDEB0C0u, 0x09252916u, "Gambler Holster"},
    RecipeIdentity{0xFE2DFD30u, 0xCDFBBB30u, "Workman's Pride Boots"},
    RecipeIdentity{0xFF3B18DDu, 0x3D9F76CDu, "Weapons Expert Gun Belt"},
    RecipeIdentity{0xFFB98288u, 0xC46CC879u, "The Bronco Buster"}
};
struct MaterialName { std::uint32_t hash; const char* name; };
constexpr MaterialName kMaterialNames[] = {
    MaterialName{0x07215CDAu, "Perfect Skunk Pelt"},
    MaterialName{0x0DAC0E73u, "Legendary Elk Pelt"},
    MaterialName{0x13A7442Eu, "Cardinal Feather"},
    MaterialName{0x158AFE6Eu, "Perfect Armadillo Skin"},
    MaterialName{0x166C9ECCu, "Crow Feather"},
    MaterialName{0x1AFEA7E3u, "Cedar Waxwing Feather"},
    MaterialName{0x1BA02C14u, "Perfect Gila Monster Skin"},
    MaterialName{0x1DD86958u, "Perfect Fox Pelt"},
    MaterialName{0x1E8254F3u, "Pigeon Feather"},
    MaterialName{0x1F8C9103u, "Buck Antlers"},
    MaterialName{0x20EA9D25u, "Legendary Wolf Pelt"},
    MaterialName{0x25357F3Bu, "Perfect Snake Skin"},
    MaterialName{0x26F21B6Bu, "Perfect Wolf Pelt"},
    MaterialName{0x2736D9DEu, "Legendary Panther Pelt"},
    MaterialName{0x2750B772u, "Perfect Ox Hide"},
    MaterialName{0x278A515Au, "Perfect Black Bear Pelt"},
    MaterialName{0x2FBDCD2Cu, "Sparrow Feather"},
    MaterialName{0x31A8BAA1u, "Duck Feather"},
    MaterialName{0x32F01C0Au, "Perfect Beaver Pelt"},
    MaterialName{0x35591FAEu, "Moose Antler"},
    MaterialName{0x385631CBu, "Quail Feather"},
    MaterialName{0x3C9FDAE1u, "Seagull Feather"},
    MaterialName{0x49AA50A7u, "Chicken Feather"},
    MaterialName{0x4D0CA201u, "Perfect Bear Pelt"},
    MaterialName{0x511DE77Du, "Woodpecker Feather"},
    MaterialName{0x51F09978u, "Songbird Feather"},
    MaterialName{0x57266169u, "Rooster Feather"},
    MaterialName{0x5763AD17u, "Perfect Sheep Hide"},
    MaterialName{0x5B21B527u, "Legendary Cougar Pelt"},
    MaterialName{0x61DC64F7u, "Perfect Wolf Carcass"},
    MaterialName{0x61E654EFu, "Legendary Fox Pelt"},
    MaterialName{0x63F90F27u, "Perfect Iguana Skin"},
    MaterialName{0x6B0C8C15u, "Perfect Ram Hide"},
    MaterialName{0x6DEBEC73u, "Goose Feather"},
    MaterialName{0x6E15B1D9u, "Perfect Pronghorn Carcass"},
    MaterialName{0x7508CA92u, "Perfect Collared Peccary Pig Pelt"},
    MaterialName{0x755F3AFEu, "Perfect Panther Pelt"},
    MaterialName{0x78935679u, "Perfect Muskrat Pelt"},
    MaterialName{0x78945008u, "Legendary White Bison Pelt"},
    MaterialName{0x7C407083u, "Money"},
    MaterialName{0x7DB8E633u, "Legendary Boar Pelt"},
    MaterialName{0x85E47B6Cu, "Eagle Feather"},
    MaterialName{0x8BEB5141u, "Blue Jay Feather"},
    MaterialName{0x91395440u, "Perfect Boar Pelt"},
    MaterialName{0x95389BDEu, "Perfect Cougar Pelt"},
    MaterialName{0x9DBFA8ACu, "Perfect Goat Hide"},
    MaterialName{0x9F2344FDu, "Perfect Alligator Skin"},
    MaterialName{0xA008F495u, "Perfect Squirrel Pelt"},
    MaterialName{0xA3F67E93u, "Perfect Pronghorn Hide"},
    MaterialName{0xA5869EBCu, "Legendary Coyote Pelt"},
    MaterialName{0xACB037A3u, "Vulture Feather"},
    MaterialName{0xB098CDF9u, "Perfect Elk Pelt"},
    MaterialName{0xB60B6DD8u, "Loon Feather"},
    MaterialName{0xB92EB910u, "Perfect Ram Carcass"},
    MaterialName{0xB993216Du, "Raven Feather"},
    MaterialName{0xBE4CA786u, "Perfect Pig Hide"},
    MaterialName{0xC05F8E61u, "Owl Feather"},
    MaterialName{0xC24749A2u, "Perfect Deer Pelt"},
    MaterialName{0xC5206B67u, "Legendary Tatanka Bison Pelt"},
    MaterialName{0xC7EFA080u, "Condor Feather"},
    MaterialName{0xC8F3B672u, "Perfect Rabbit Pelt"},
    MaterialName{0xCA338DCEu, "Robin Feather"},
    MaterialName{0xCB06E38Du, "Legendary Buck Pelt"},
    MaterialName{0xCDA1C15Au, "Perfect Cow Hide"},
    MaterialName{0xD0A84ABBu, "Perfect Coyote Pelt"},
    MaterialName{0xD4319786u, "Legendary Ram Hide"},
    MaterialName{0xD61C45AEu, "Perfect Buck Pelt"},
    MaterialName{0xD6CF1472u, "Pheasant Feather"},
    MaterialName{0xD705C729u, "Legendary Moose Pelt"},
    MaterialName{0xD8950135u, "Turkey Feather"},
    MaterialName{0xE15570B1u, "Legendary Bear Pelt"},
    MaterialName{0xE3A38EF1u, "Perfect Opossum Pelt"},
    MaterialName{0xE4636436u, "Perfect Badger Pelt"},
    MaterialName{0xE57B776Fu, "Legendary Alligator Skin"},
    MaterialName{0xE7B39B3Du, "Elk Antler"},
    MaterialName{0xEDC3437Du, "Perfect Raccoon Pelt"},
    MaterialName{0xEFEB63ACu, "Perfect Rat Pelt"},
    MaterialName{0xF1D41DECu, "Perfect Bison Pelt"},
    MaterialName{0xF305AD79u, "Perfect Moose Pelt"},
    MaterialName{0xF70C1DE0u, "Legendary Beaver Pelt"},
    MaterialName{0xF9DB919Du, "Legendary Pronghorn Hide"},
    MaterialName{0xFCD328D3u, "Perfect Bull Hide"},
    MaterialName{0xFE17FB41u, "Hawk Feather"},
    MaterialName{0xFFE50F1Fu, "Oriole Feather"}
};
struct CategoryDefinition { std::uint32_t hash; const char* label; };
constexpr CategoryDefinition kPearsonCategories[]{
    {0xDCD17F1Fu, "Satchels"},
    {0x1F5EEA44u, "Camp Decorations"},
    {0xDC708275u, "Other"}
};
constexpr CategoryDefinition kTrapperCategories[]{
    {0x851EBBD3u, "Hats & Accessories"},
    {0x29A49A30u, "Vests"},
    {0xCDFBBB30u, "Boots & Moccasins"},
    {0xC46CC879u, "Garment Sets"},
    {0x415F0998u, "Bandoliers"},
    {0x9989398Bu, "Gloves"},
    {0x3D9F76CDu, "Gun Belts"},
    {0x09252916u, "Holsters"},
    {0xC8F18EFCu, "Half Chaps"},
    {0x6971A6CBu, "Chaps"},
    {0xCD31B12Cu, "Off-hand Holsters"},
    {0x6F58734Fu, "Coats & Jackets"},
    {0xD582F9FAu, "Saddles"},
    {0xA9BF2F76u, "Throwing Weapons"},
    {0x9B317DD1u, "Provisions & Tools"},
    {0xD905CE1Cu, "Weapons & Tools"}
};

const RecipeIdentity* find_recipe_identity(std::uint32_t itemHash) noexcept {
    for (const auto& entry : kRecipeIdentities) {
        if (entry.itemHash == itemHash) return &entry;
    }
    return nullptr;
}

const char* find_material_name(std::uint32_t materialHash) noexcept {
    for (const auto& entry : kMaterialNames) {
        if (entry.hash == materialHash) return entry.name;
    }
    return nullptr;
}

// Craftable Now deliberately measures animal-part readiness only. The curated
// material-name table contains pelts, hides, feathers, antlers and carcasses;
// money and any other non-animal acquisition costs are outside this filter.
bool is_animal_part_material(std::uint32_t materialHash) noexcept {
    return materialHash != 0 && materialHash != kMoneyHash &&
           find_material_name(materialHash) != nullptr;
}

struct MaterialNeed {
    std::uint32_t hash{};
    int totalRequired{};
    int vendorStock{};
    int stillNeeded{};
};

struct VendorDemand {
    std::array<MaterialNeed, kMaxMaterials> materials{};
    std::size_t count{};
    bool ready{};
    int scannedItems{};
    int unownedRecipes{};
};

struct RecipeCost { std::uint32_t hash{}; int amount{}; };

struct RecipeEntry {
    std::uint32_t itemHash{};
    std::uint32_t bundleHash{};
    std::uint32_t categoryHash{};
    char name[96]{};
    std::array<RecipeCost, kMaxRecipeCosts> costs{};
    std::size_t costCount{};
    bool owned{};
    bool hasAnyCosts{};
};

struct VendorState {
    VendorDemand demand{};
    std::array<RecipeEntry, kMaxRecipesPerShop> recipes{};
    std::size_t recipeCount{};
};

constexpr std::size_t kMaxLocalizedItemNames = 1024;
struct LocalizedItemName {
    std::uint32_t itemHash{};
    std::uint32_t labelHash{};
    // Copy the native result immediately: Rockstar's native string buffer is
    // transient and must not be retained as a pointer across native calls.
    char text[256]{};
};
struct LocalizedItemNameCache {
    std::array<LocalizedItemName, kMaxLocalizedItemNames> entries{};
    std::size_t count{};
};

struct TrackerState {
    VendorState pearson{};
    VendorState trapper{};
    std::array<ChallengeProgress, kChallengeCount> challenges{};
    LocalizedItemNameCache localizedNames{};
    bool ready{};
};
TrackerState g_tracker{};
SRWLOCK g_trackerLock = SRWLOCK_INIT;

// Visible scan telemetry: expose the exact initialization stage and bounded
// progress so slow vendor scans do not look like a permanently frozen menu.
enum class ScanStage : LONG {
    Idle = 0, ResolvePlayer, PearsonCatalog, TrapperCatalog,
    PearsonStock, TrapperStock, ChallengeRanks, Complete, Failed
};
volatile LONG g_scanStage = static_cast<LONG>(ScanStage::Idle);
volatile LONG g_scanLastWorkStage = static_cast<LONG>(ScanStage::Idle);
volatile LONG g_scanCurrent = 0;
volatile LONG g_scanTotal = 0;
volatile LONG g_scanStartedTick = 0;
volatile LONG g_scanLastProgressTick = 0;
volatile LONG g_scanError = 0;
int g_detectedGameLanguage = -1;

void scan_heartbeat() noexcept {
    InterlockedExchange(&g_scanLastProgressTick, static_cast<LONG>(GetTickCount()));
}

void set_scan_stage(ScanStage stage, int total = 0) noexcept {
    InterlockedExchange(&g_scanCurrent, 0);
    InterlockedExchange(&g_scanTotal, std::max(0, total));
    scan_heartbeat();
    if (stage != ScanStage::Idle && stage != ScanStage::Complete && stage != ScanStage::Failed) {
        InterlockedExchange(&g_scanLastWorkStage, static_cast<LONG>(stage));
    }
    InterlockedExchange(&g_scanStage, static_cast<LONG>(stage));
}

void set_scan_progress(int current) noexcept {
    const int total = InterlockedCompareExchange(&g_scanTotal, 0, 0);
    InterlockedExchange(&g_scanCurrent, std::clamp(current, 0, std::max(0, total)));
    scan_heartbeat();
}

const char* scan_stage_label(ScanStage stage) noexcept {
    switch (stage) {
        case ScanStage::ResolvePlayer: return "CHECKING STORY MODE INVENTORY";
        case ScanStage::PearsonCatalog: return "READING PEARSON RECIPE CATALOG";
        case ScanStage::TrapperCatalog: return "READING TRAPPER RECIPE CATALOG";
        case ScanStage::PearsonStock: return "COUNTING PEARSON DONATION STOCK";
        case ScanStage::TrapperStock: return "COUNTING TRAPPER DONATION STOCK";
        case ScanStage::ChallengeRanks: return "READING CHALLENGE RANKS";
        case ScanStage::Complete: return "CATALOG READY";
        case ScanStage::Failed: return "CATALOG READ COULD NOT COMPLETE";
        case ScanStage::Idle: default: return "WAITING FOR STORY MODE";
    }
}

const char* game_language_name(int language) noexcept {
    // IDs documented by RDR2 GET_CURRENT_LANGUAGE.
    static constexpr const char* kNames[] = {
        "English", "French", "German", "Italian", "Spanish (Spain)",
        "Portuguese (Brazil)", "Polish", "Russian", "Korean",
        "Chinese (Traditional)", "Japanese", "Spanish (Mexico)",
        "Chinese (Simplified)"
    };
    return (language >= 0 && language < static_cast<int>(std::size(kNames)))
        ? hunting_companion::ui_text(kNames[language]) : "Unknown / unsupported";
}

const char* game_language_locale(int language) noexcept {
    static constexpr const char* kLocales[] = {
        "en-US", "fr-FR", "de-DE", "it-IT", "es-ES", "pt-BR", "pl-PL",
        "ru-RU", "ko-KR", "zh-TW", "ja-JP", "es-MX", "zh-CN"
    };
    return (language >= 0 && language < static_cast<int>(std::size(kLocales)))
        ? kLocales[language] : "unknown";
}

class TrackerReadLock {
public:
    TrackerReadLock() noexcept { AcquireSRWLockShared(&g_trackerLock); }
    ~TrackerReadLock() { ReleaseSRWLockShared(&g_trackerLock); }
    TrackerReadLock(const TrackerReadLock&) = delete;
    TrackerReadLock& operator=(const TrackerReadLock&) = delete;
};

// Discover the stable Rockstar label hash for an ItemDatabase item and ask
// Rockstar for the exact localized phrase. GET_STRING_FROM_HASH_KEY reads the
// game's currently active official language table; this is the primary path
// and needs no external translation mod or language pack. If the companion is
// pinned to a different language, use the matching compact Rockstar table
// embedded in this ASI instead of leaking the game's currently active language.
void remember_official_item_label(LocalizedItemNameCache& cache,
                                  std::uint32_t itemHash) noexcept {
    if (itemHash == 0 || itemHash == kMoneyHash) return;
    for (std::size_t i = 0; i < cache.count; ++i) {
        if (cache.entries[i].itemHash == itemHash) return;
    }
    if (cache.count >= cache.entries.size()) return;

    std::uint32_t selectedLabelHash = 0;
    char resolvedText[sizeof(LocalizedItemName::text)]{};
    const bool companionUsesGameLanguage =
        hunting_companion::active_ui_language() == g_detectedGameLanguage;

    auto acceptLabel = [&](std::uint32_t labelHash) noexcept -> bool {
        if (labelHash == 0) return false;
        const char* candidate = nullptr;
        if (companionUsesGameLanguage) {
            candidate = invoke_native<const char*>(kGetStringFromHashKey, labelHash);
        } else {
            candidate = hunting_companion::official_item_string(labelHash);
        }
        if (candidate == nullptr || candidate[0] == '\0') return false;
        const std::size_t length = std::strlen(candidate);
        // Only compact display labels are valid here. Reject descriptions,
        // prompt markup, and multi-line text so a secondary label cannot
        // flood a recipe/material row.
        if (length == 0 || length >= sizeof(resolvedText) ||
            std::strchr(candidate, '\n') != nullptr ||
            std::strchr(candidate, '\r') != nullptr ||
            std::strchr(candidate, '~') != nullptr) return false;
        for (std::size_t i = 0; i < length; ++i) {
            if (static_cast<unsigned char>(candidate[i]) < 0x20) return false;
        }
        if (std::strchr(candidate, '.') != nullptr ||
            std::strchr(candidate, '!') != nullptr ||
            std::strchr(candidate, '?') != nullptr) return false;
        std::memcpy(resolvedText, candidate, length);
        resolvedText[length] = '\0';
        selectedLabelHash = labelHash;
        return true;
    };

    const std::uint32_t descriptionHash = invoke_native<std::uint32_t>(
        kInventoryGetItemDescriptionHash, itemHash);
    bool resolved = acceptLabel(descriptionHash);
    if (!resolved) resolved = acceptLabel(itemHash);
    if (!resolved) {
        const int rawTypeCount = invoke_native<int>(
            kItemDbLocalizationGetNumLabelTypes, itemHash);
        const int typeCount = std::clamp(rawTypeCount, 0, 32);
        for (int typeIndex = 0; typeIndex < typeCount && !resolved; ++typeIndex) {
            const std::uint32_t labelType = invoke_native<std::uint32_t>(
                kItemDbLocalizationGetType, itemHash, typeIndex);
            if (labelType == 0) continue;
            const int rawValueCount = invoke_native<int>(
                kItemDbLocalizationGetNumValues, itemHash, labelType);
            const int valueCount = std::clamp(rawValueCount, 0, 32);
            for (int valueIndex = 0; valueIndex < valueCount && !resolved; ++valueIndex) {
                const std::uint32_t valueHash = invoke_native<std::uint32_t>(
                    kItemDbLocalizationGetValue, itemHash, labelType, valueIndex);
                resolved = acceptLabel(valueHash);
            }
        }
    }

    LocalizedItemName& entry = cache.entries[cache.count++];
    entry.itemHash = itemHash;
    if (resolved) {
        entry.labelHash = selectedLabelHash;
        std::memcpy(entry.text, resolvedText, std::strlen(resolvedText) + 1);
    }
}

const char* localized_item_name(const TrackerState& state,
                                std::uint32_t itemHash) noexcept {
    if (itemHash == 0) return nullptr;

    // RecipeIdentity.itemHash and MaterialName.hash are the exact Rockstar
    // localization IDs used to build the 290-entry embedded catalogue.
    // Resolve that stable ID first. ItemDatabase can expose a secondary label
    // hash for the same inventory object; relying on that secondary value can
    // miss the embedded row and leave a stale English/native-language buffer.
    // This direct lookup is also independent of the background scan cache, so
    // a companion locale switch changes every known item on the very next draw.
    if (const char* selected =
            hunting_companion::official_item_string(itemHash)) {
        return selected;
    }

    for (std::size_t i = 0; i < state.localizedNames.count; ++i) {
        const LocalizedItemName& entry = state.localizedNames.entries[i];
        if (entry.itemHash != itemHash) continue;

        // Keep the stable Rockstar label hash, not the localized text buffer.
        // Resolving it on every draw means a companion-language change takes
        // effect immediately, even while the optional background rescan runs.
        if (entry.labelHash != 0) {
            if (const char* selected =
                    hunting_companion::official_item_string(entry.labelHash)) {
                return selected;
            }
        }

        // Native text is a safe fallback only when the companion follows the
        // active game language. Never leak another game language into a pinned
        // companion locale.
        if (hunting_companion::active_ui_language() == g_detectedGameLanguage &&
            entry.text[0] != '\0') {
            return entry.text;
        }
        return nullptr;
    }
    return nullptr;
}

const char* recipe_display_name(const RecipeEntry& recipe) noexcept {
    const char* officialName = localized_item_name(g_tracker, recipe.itemHash);
    return officialName != nullptr ? officialName : recipe.name;
}

const char* display_material_name(std::uint32_t materialHash) noexcept {
    const char* officialName = localized_item_name(g_tracker, materialHash);
    return officialName != nullptr ? officialName : find_material_name(materialHash);
}

void add_requirement(VendorDemand& vendor, std::uint32_t materialHash, int amount) noexcept {
    if (materialHash == 0 || amount <= 0) return;
    for (std::size_t i = 0; i < vendor.count; ++i) {
        if (vendor.materials[i].hash == materialHash) {
            const long long sum = static_cast<long long>(vendor.materials[i].totalRequired) + amount;
            vendor.materials[i].totalRequired = static_cast<int>(std::min<long long>(sum, 1000000));
            return;
        }
    }
    if (vendor.count >= vendor.materials.size()) return;
    vendor.materials[vendor.count++] = MaterialNeed{materialHash, amount, 0, 0};
}

void add_recipe_cost(RecipeEntry& recipe, std::uint32_t materialHash, int amount) noexcept {
    if (materialHash == 0 || amount <= 0) return;
    for (std::size_t i = 0; i < recipe.costCount; ++i) {
        if (recipe.costs[i].hash == materialHash) {
            const long long sum = static_cast<long long>(recipe.costs[i].amount) + amount;
            recipe.costs[i].amount = static_cast<int>(std::min<long long>(sum, 1000000));
            return;
        }
    }
    if (recipe.costCount >= recipe.costs.size()) return;
    recipe.costs[recipe.costCount++] = RecipeCost{materialHash, amount};
}

int player_owned_count(int inventoryId, std::uint32_t itemHash) noexcept {
    if (itemHash == 0) return 0;
    const int count = invoke_native<int>(kCountInventoryItems, inventoryId, itemHash, false);
    return count > 0 ? count : 0;
}

void read_recipe_cost_rows(VendorDemand& vendor, RecipeEntry& recipe,
                           std::uint32_t recipeHash, std::uint32_t costTypeHash,
                           bool isBundle, bool includeInVendorDemand,
                           LocalizedItemNameCache& localizedNames) noexcept {
    if (recipeHash == 0 || costTypeHash == 0) return;
    const int rawCount = isBundle
        ? invoke_native<int>(kItemDatabaseGetBundleAcquireCostsCountFromCost, recipeHash, costTypeHash)
        : invoke_native<int>(kItemDatabaseGetAcquireCostsCountFromCostType, recipeHash, costTypeHash);
    const int rowCount = std::clamp(rawCount, 0, kMaxCostRows);
    for (int rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
        std::array<std::uint64_t, 2> row{};
        const int ok = isBundle
            ? invoke_native<int>(kItemDatabaseFilloutBundle, recipeHash, costTypeHash, rowIndex, row.data())
            : invoke_native<int>(kItemDatabaseFilloutItem, recipeHash, costTypeHash, rowIndex, row.data());
        if (ok) {
            const std::uint32_t materialHash = static_cast<std::uint32_t>(row[0] & 0xffffffffu);
            const int amount = static_cast<std::int32_t>(static_cast<std::uint32_t>(row[1] & 0xffffffffu));
            if (materialHash != 0 && amount > 0) {
                remember_official_item_label(localizedNames, materialHash);
                add_recipe_cost(recipe, materialHash, amount);
                recipe.hasAnyCosts = true;
                // Only recognized animal parts contribute to Craftable Now.
                if (includeInVendorDemand && is_animal_part_material(materialHash)) {
                    add_requirement(vendor, materialHash, amount);
                }
            }
        }
        g_api.wait(0);
        scan_heartbeat();
    }
}

void scan_vendor_catalog(VendorState& state, std::uint32_t shopHash,
                         std::uint32_t costTypeHash, std::uint32_t promoCostTypeHash,
                         int inventoryId, ScanStage stage,
                         LocalizedItemNameCache& localizedNames) noexcept {
    state = {};
    VendorDemand& vendor = state.demand;
    const int rawCount = invoke_native<int>(kItemDatabaseGetShopItemsCount, shopHash);
    if (rawCount < 0 || rawCount > kMaxShopItems) {
        InterlockedExchange(&g_scanError, rawCount < 0 ? 2 : 3);
        set_scan_stage(ScanStage::Failed, rawCount);
        return;
    }
    set_scan_stage(stage, rawCount);
    const bool isTrapper = (shopHash == kShopTrapper);
    for (int index = 0; index < rawCount; ++index) {
        set_scan_progress(index);
        std::array<std::uint64_t, 3> itemInfo{};
        const int ok = invoke_native<int>(kItemDatabaseGetShopItemInfo, shopHash, index, itemInfo.data());
        if (!ok) {
            g_api.wait(0);
            scan_heartbeat();
            set_scan_progress(index + 1);
            continue;
        }
        const std::uint32_t itemHash = static_cast<std::uint32_t>(itemInfo[0] & 0xffffffffu);
        const std::uint32_t bundleHash = static_cast<std::uint32_t>(itemInfo[1] & 0xffffffffu);

        // Pearson remains available; only its two opaque internal placeholder
        // records are excluded from the recipe list and scan totals.
        if (shopHash == kShopPearson && is_pearson_unlabelled_placeholder(itemHash)) {
            set_scan_progress(index + 1);
            g_api.wait(0);
            scan_heartbeat();
            continue;
        }
        ++vendor.scannedItems;

        // Record the stable ItemDatabase label ID for the selected companion
        // locale, including catalogue entries outside the curated identity set.
        remember_official_item_label(localizedNames, itemHash);
        const RecipeIdentity* identity = find_recipe_identity(itemHash);

        // Do not expose uncurated rows with fabricated names or hexadecimal
        // hashes. A non-curated row is only useful if we can resolve its real
        // Rockstar name from the selected locale's embedded table, either by
        // the stable item hash or by the secondary ItemDatabase label hash.
        bool hasEmbeddedOfficialName =
            hunting_companion::official_item_string(itemHash) != nullptr;
        if (!hasEmbeddedOfficialName) {
            for (std::size_t labelIndex = 0; labelIndex < localizedNames.count; ++labelIndex) {
                const LocalizedItemName& cached = localizedNames.entries[labelIndex];
                if (cached.itemHash == itemHash && cached.labelHash != 0 &&
                    hunting_companion::official_item_string(cached.labelHash) != nullptr) {
                    hasEmbeddedOfficialName = true;
                    break;
                }
            }
        }
        if (identity == nullptr && !hasEmbeddedOfficialName) {
            set_scan_progress(index + 1);
            g_api.wait(0);
            scan_heartbeat();
            continue;
        }

        const int itemOwned = player_owned_count(inventoryId, itemHash);
        const int bundleOwned = player_owned_count(inventoryId, bundleHash);

        RecipeEntry recipe{};
        recipe.itemHash = itemHash;
        recipe.bundleHash = bundleHash;
        recipe.owned = itemOwned > 0 || bundleOwned > 0;
        if (identity != nullptr) {
            recipe.categoryHash = identity->categoryHash;
            std::snprintf(recipe.name, sizeof(recipe.name), "%s", identity->name);
        } else {
            // The row has already been validated against the embedded table;
            // its visible text is resolved by recipe_display_name(). Keep no
            // English placeholder or internal hash in the cached record.
            recipe.name[0] = '\0';
        }

        int directRows = itemHash != 0
            ? invoke_native<int>(kItemDatabaseGetAcquireCostsCountFromCostType, itemHash, costTypeHash)
            : 0;
        if (directRows > 0) {
            read_recipe_cost_rows(vendor, recipe, itemHash, costTypeHash, false, !recipe.owned, localizedNames);
        } else if (isTrapper && promoCostTypeHash != 0 && itemHash != 0) {
            const int promoRows = invoke_native<int>(
                kItemDatabaseGetAcquireCostsCountFromCostType, itemHash, promoCostTypeHash);
            if (promoRows > 0) {
                read_recipe_cost_rows(vendor, recipe, itemHash, promoCostTypeHash, false, !recipe.owned, localizedNames);
            }
        }
        if (bundleHash != 0) {
            const int bundleRows = invoke_native<int>(
                kItemDatabaseGetBundleAcquireCostsCountFromCost, bundleHash, costTypeHash);
            if (bundleRows > 0) {
                read_recipe_cost_rows(vendor, recipe, bundleHash, costTypeHash, true, !recipe.owned, localizedNames);
            }
        }
        if (!recipe.owned && recipe.hasAnyCosts) ++vendor.unownedRecipes;
        if (state.recipeCount < state.recipes.size()) {
            state.recipes[state.recipeCount++] = recipe;
        }
        set_scan_progress(index + 1);
        g_api.wait(0);
        scan_heartbeat();
    }
    vendor.ready = true;
    set_scan_progress(rawCount);
}

bool build_tracker_state() noexcept {
    InterlockedExchange(&g_scanStartedTick, static_cast<LONG>(GetTickCount()));
    InterlockedExchange(&g_scanError, 0);
    set_scan_stage(ScanStage::ResolvePlayer, 3);

    const int playerPed = invoke_native<int>(kPlayerPedId);
    set_scan_progress(1);
    if (playerPed == 0) {
        InterlockedExchange(&g_scanError, 1);
        set_scan_stage(ScanStage::Failed);
        return false;
    }
    const int inventoryId = invoke_native<int>(kInventoryGetInventoryIdFromPed, playerPed);
    set_scan_progress(2);
    if (inventoryId < 0) {
        InterlockedExchange(&g_scanError, 4);
        set_scan_stage(ScanStage::Failed);
        return false;
    }

    Guid emptyParent{};
    Guid rootGuid{};
    if (!get_guid_from_item(inventoryId, &emptyParent, kCharacterSlot, kSlotNone, &rootGuid) ||
        !guid_is_valid(&rootGuid)) {
        InterlockedExchange(&g_scanError, 5);
        set_scan_stage(ScanStage::Failed);
        return false;
    }
    set_scan_progress(3);

    VendorState pearson{};
    VendorState trapper{};
    LocalizedItemNameCache localizedNames{};
    scan_vendor_catalog(pearson, kShopPearson, kCostCraftingPearson, 0,
                        inventoryId, ScanStage::PearsonCatalog, localizedNames);
    if (static_cast<ScanStage>(InterlockedCompareExchange(&g_scanStage, 0, 0)) == ScanStage::Failed) {
        return false;
    }
    scan_vendor_catalog(trapper, kShopTrapper, kCostCraftingTrapper,
                        kCostCraftingTrapperPromo, inventoryId, ScanStage::TrapperCatalog,
                        localizedNames);
    if (static_cast<ScanStage>(InterlockedCompareExchange(&g_scanStage, 0, 0)) == ScanStage::Failed) {
        return false;
    }
    if (!pearson.demand.ready || !trapper.demand.ready) {
        InterlockedExchange(&g_scanError, 6);
        set_scan_stage(ScanStage::Failed);
        return false;
    }

    set_scan_stage(ScanStage::PearsonStock, static_cast<int>(pearson.demand.count));
    for (std::size_t i = 0; i < pearson.demand.count; ++i) {
        auto& material = pearson.demand.materials[i];
        material.vendorStock = vendor_material_count(inventoryId, &rootGuid,
                                                      material.hash, kPearsonStorageSlot);
        material.stillNeeded = std::max(0, material.totalRequired - material.vendorStock);
        set_scan_progress(static_cast<int>(i + 1));
        g_api.wait(0);
        scan_heartbeat();
    }
    set_scan_stage(ScanStage::TrapperStock, static_cast<int>(trapper.demand.count));
    for (std::size_t i = 0; i < trapper.demand.count; ++i) {
        auto& material = trapper.demand.materials[i];
        material.vendorStock = vendor_material_count(inventoryId, &rootGuid,
                                                      material.hash, kTrapperGiveStorageSlot);
        material.stillNeeded = std::max(0, material.totalRequired - material.vendorStock);
        set_scan_progress(static_cast<int>(i + 1));
        g_api.wait(0);
        scan_heartbeat();
    }

    // Read the game's own challenge rank totals. These calls do not advance,
    // reset, or otherwise modify challenge progress. Validate each category
    // against its expected rank count before publishing it to the UI.
    set_scan_stage(ScanStage::ChallengeRanks, static_cast<int>(kChallengeCount));
    std::array<ChallengeProgress, kChallengeCount> challenges{};
    for (std::size_t i = 0; i < kChallengeDefinitions.size(); ++i) {
        const std::uint32_t challengeHash = joaat(kChallengeDefinitions[i].rootHashName);
        const int maxRanks = invoke_native<int>(kChalGetMaxRanks, challengeHash);
        const int completed = invoke_native<int>(kChalGetNumRanksCompleted, challengeHash);
        challenges[i] = ChallengeProgress{completed, maxRanks, false};
        if (maxRanks == kExpectedChallengeRankCount &&
            completed >= 0 && completed <= maxRanks) {
            challenges[i].available = true;
        }
        set_scan_progress(static_cast<int>(i + 1));
        g_api.wait(0);
    }

    AcquireSRWLockExclusive(&g_trackerLock);
    g_tracker.pearson = pearson;
    g_tracker.trapper = trapper;
    g_tracker.challenges = challenges;
    g_tracker.localizedNames = localizedNames;
    g_tracker.ready = true;
    ReleaseSRWLockExclusive(&g_trackerLock);
    set_scan_stage(ScanStage::Complete);
    return true;
}

struct MaterialSource {
    std::uint32_t hash;
    const char* species;
};

// Sources for all 83 distinct animal materials present in the extracted
// Pearson/Trapper recipe catalogs. Pelt quality does not affect the species.
constexpr std::array<MaterialSource, 83> kMaterialSources{{
    {0x8BEB5141u, "Blue Jay"}, {0x1F8C9103u, "Buck"},
    {0x13A7442Eu, "Cardinal"}, {0x1AFEA7E3u, "Cedar Waxwing"},
    {0x49AA50A7u, "Chicken"}, {0xC7EFA080u, "California Condor"},
    {0x166C9ECCu, "Crow"}, {0x31A8BAA1u, "Duck"},
    {0x85E47B6Cu, "Eagle"}, {0xE7B39B3Du, "Elk"},
    {0x6DEBEC73u, "Canada Goose"}, {0xFE17FB41u, "Hawk"},
    {0xE57B776Fu, "Alligator"}, {0xE15570B1u, "Grizzly Bear"},
    {0xF70C1DE0u, "Beaver"}, {0x7DB8E633u, "Boar"},
    {0xCB06E38Du, "Buck"}, {0x5B21B527u, "Cougar"},
    {0xA5869EBCu, "Coyote"}, {0x0DAC0E73u, "Elk"},
    {0x61E654EFu, "Fox"}, {0xD705C729u, "Moose"},
    {0x2736D9DEu, "Panther"}, {0xF9DB919Du, "Pronghorn"},
    {0xD4319786u, "Ram"}, {0xC5206B67u, "Bison"},
    {0x78945008u, "Bison"}, {0x20EA9D25u, "Wolf"},
    {0xB60B6DD8u, "Loon"}, {0x35591FAEu, "Moose"},
    {0xFFE50F1Fu, "Oriole"}, {0xC05F8E61u, "Owl"},
    {0x9F2344FDu, "Alligator"}, {0x158AFE6Eu, "Armadillo"},
    {0xE4636436u, "Badger"}, {0x4D0CA201u, "Grizzly Bear"},
    {0x32F01C0Au, "Beaver"}, {0xF1D41DECu, "Bison"},
    {0x278A515Au, "Black Bear"}, {0x91395440u, "Boar"},
    {0xD61C45AEu, "Buck"}, {0xFCD328D3u, "Bull"},
    {0x7508CA92u, "Javelina"}, {0x95389BDEu, "Cougar"},
    {0xCDA1C15Au, "Cow"}, {0xD0A84ABBu, "Coyote"},
    {0xC24749A2u, "Deer"}, {0xB098CDF9u, "Elk"},
    {0x1DD86958u, "Fox"}, {0x1BA02C14u, "Gila Monster"},
    {0x9DBFA8ACu, "Goat"}, {0x63F90F27u, "Iguana"},
    {0xF305AD79u, "Moose"}, {0x78935679u, "Muskrat"},
    {0xE3A38EF1u, "Opossum"}, {0x2750B772u, "Ox"},
    {0x755F3AFEu, "Panther"}, {0xBE4CA786u, "Pig"},
    {0x6E15B1D9u, "Pronghorn"}, {0xA3F67E93u, "Pronghorn"},
    {0xC8F3B672u, "Rabbit"}, {0xEDC3437Du, "Raccoon"},
    {0xB92EB910u, "Ram"}, {0x6B0C8C15u, "Ram"},
    {0xEFEB63ACu, "Rat"}, {0x5763AD17u, "Sheep"},
    {0x07215CDAu, "Skunk"}, {0x25357F3Bu, "Rattlesnake"},
    {0xA008F495u, "Squirrel"}, {0x61DC64F7u, "Wolf"},
    {0x26F21B6Bu, "Wolf"}, {0xD6CF1472u, "Pheasant"},
    {0x1E8254F3u, "Pigeon"}, {0x385631CBu, "Quail"},
    {0xB993216Du, "Raven"}, {0xCA338DCEu, "Robin"},
    {0x57266169u, "Rooster"}, {0x3C9FDAE1u, "Seagull"},
    {0x51F09978u, "Songbird"}, {0x2FBDCD2Cu, "Sparrow"},
    {0xD8950135u, "Turkey"}, {0xACB037A3u, "Vulture"},
    {0x511DE77Du, "Woodpecker"}
}};

const char* source_species(std::uint32_t materialHash) noexcept {
    for (const auto& source : kMaterialSources) {
        if (source.hash == materialHash) return source.species;
    }
    return nullptr;
}

bool species_needed(const VendorDemand& vendor, const char* species) noexcept {
    if (!vendor.ready || species == nullptr) return false;
    for (std::size_t i = 0; i < vendor.count; ++i) {
        const auto& material = vendor.materials[i];
        if (material.stillNeeded <= 0) continue;
        const char* source = source_species(material.hash);
        if (source != nullptr && std::strcmp(source, species) == 0) return true;
    }
    return false;
}

struct AnimalModel { std::uint32_t hash; const char* species; };
constexpr auto kAnimalModels = std::array{
    AnimalModel{joaat("a_c_alligator_01"), "Alligator"}, AnimalModel{joaat("a_c_alligator_02"), "Alligator"},
    AnimalModel{joaat("a_c_alligator_03"), "Alligator"}, AnimalModel{joaat("a_c_armadillo_01"), "Armadillo"},
    AnimalModel{joaat("a_c_badger_01"), "Badger"}, AnimalModel{joaat("a_c_bear_01"), "Grizzly Bear"},
    AnimalModel{joaat("a_c_bearblack_01"), "Black Bear"}, AnimalModel{joaat("a_c_beaver_01"), "Beaver"},
    AnimalModel{joaat("a_c_bighornram_01"), "Ram"}, AnimalModel{joaat("a_c_bluejay_01"), "Blue Jay"},
    AnimalModel{joaat("a_c_boar_01"), "Boar"}, AnimalModel{joaat("a_c_buck_01"), "Buck"},
    AnimalModel{joaat("a_c_buffalo_01"), "Bison"}, AnimalModel{joaat("a_c_bull_01"), "Bull"},
    AnimalModel{joaat("a_c_californiacondor_01"), "California Condor"}, AnimalModel{joaat("a_c_cardinal_01"), "Cardinal"},
    AnimalModel{joaat("a_c_cedarwaxwing_01"), "Cedar Waxwing"}, AnimalModel{joaat("a_c_chipmunk_01"), "Squirrel"},
    AnimalModel{joaat("a_c_cougar_01"), "Cougar"}, AnimalModel{joaat("a_c_cow"), "Cow"},
    AnimalModel{joaat("a_c_coyote_01"), "Coyote"}, AnimalModel{joaat("a_c_crow_01"), "Crow"},
    AnimalModel{joaat("a_c_deer_01"), "Deer"}, AnimalModel{joaat("a_c_duck_01"), "Duck"},
    AnimalModel{joaat("a_c_eagle_01"), "Eagle"}, AnimalModel{joaat("a_c_elk_01"), "Elk"},
    AnimalModel{joaat("a_c_fox_01"), "Fox"}, AnimalModel{joaat("a_c_goat_01"), "Goat"},
    AnimalModel{joaat("a_c_gilamonster_01"), "Gila Monster"}, AnimalModel{joaat("a_c_goosecanada_01"), "Canada Goose"},
    AnimalModel{joaat("a_c_hawk_01"), "Hawk"}, AnimalModel{joaat("a_c_iguana_01"), "Iguana"},
    AnimalModel{joaat("a_c_iguanadesert_01"), "Iguana"}, AnimalModel{joaat("a_c_javelina_01"), "Javelina"},
    AnimalModel{joaat("a_c_loon_01"), "Loon"}, AnimalModel{joaat("a_c_moose_01"), "Moose"},
    AnimalModel{joaat("a_c_muskrat_01"), "Muskrat"}, AnimalModel{joaat("a_c_oriole_01"), "Oriole"},
    AnimalModel{joaat("a_c_owl_01"), "Owl"}, AnimalModel{joaat("a_c_ox_01"), "Ox"},
    AnimalModel{joaat("a_c_panther_01"), "Panther"}, AnimalModel{joaat("a_c_pigeon"), "Pigeon"},
    AnimalModel{joaat("a_c_pheasant_01"), "Pheasant"}, AnimalModel{joaat("a_c_pig_01"), "Pig"},
    AnimalModel{joaat("a_c_possum_01"), "Opossum"}, AnimalModel{joaat("a_c_pronghorn_01"), "Pronghorn"},
    AnimalModel{joaat("a_c_quail_01"), "Quail"}, AnimalModel{joaat("a_c_rabbit_01"), "Rabbit"},
    AnimalModel{joaat("a_c_raccoon_01"), "Raccoon"}, AnimalModel{joaat("a_c_rat_01"), "Rat"},
    AnimalModel{joaat("a_c_raven_01"), "Raven"}, AnimalModel{joaat("a_c_robin_01"), "Robin"},
    AnimalModel{joaat("a_c_rooster_01"), "Rooster"}, AnimalModel{joaat("a_c_seagull_01"), "Seagull"},
    AnimalModel{joaat("a_c_sheep_01"), "Sheep"}, AnimalModel{joaat("a_c_skunk_01"), "Skunk"},
    AnimalModel{joaat("a_c_snakeblacktailrattle_01"), "Rattlesnake"}, AnimalModel{joaat("a_c_songbird_01"), "Songbird"},
    AnimalModel{joaat("a_c_sparrow_01"), "Sparrow"}, AnimalModel{joaat("a_c_squirrel_01"), "Squirrel"},
    AnimalModel{joaat("a_c_turkey_01"), "Turkey"}, AnimalModel{joaat("a_c_turkey_02"), "Turkey"},
    AnimalModel{joaat("a_c_turkeywild_01"), "Turkey"}, AnimalModel{joaat("a_c_vulture_01"), "Vulture"},
    AnimalModel{joaat("a_c_wolf"), "Wolf"},
    AnimalModel{joaat("a_c_wolf_medium"), "Wolf"}, AnimalModel{joaat("a_c_wolf_small"), "Wolf"},
    AnimalModel{joaat("a_c_woodpecker_01"), "Woodpecker"}, AnimalModel{joaat("a_c_woodpecker_02"), "Woodpecker"},
    AnimalModel{joaat("a_c_chicken_01"), "Chicken"},
    AnimalModel{joaat("a_c_bat_01"), "Bat"},
    AnimalModel{joaat("a_c_frogbull_01"), "Bullfrog"},
    AnimalModel{joaat("a_c_prairiechicken_01"), "Prairie Chicken"},
    AnimalModel{joaat("a_c_cormorant_01"), "Cormorant"},
    AnimalModel{joaat("a_c_crab_01"), "Crab"},
    AnimalModel{joaat("a_c_crawfish_01"), "Crayfish"},
    AnimalModel{joaat("a_c_cranewhooping_01"), "Crane"},
    AnimalModel{joaat("a_c_egret_01"), "Egret"},
    AnimalModel{joaat("a_c_heron_01"), "Heron"},
    AnimalModel{joaat("a_c_carolinaparakeet_01"), "Parakeet"},
    AnimalModel{joaat("a_c_parrot_01"), "Macaw"},
    AnimalModel{joaat("a_c_pelican_01"), "Pelican"},
    AnimalModel{joaat("a_c_redfootedbooby_01"), "Booby"},
    AnimalModel{joaat("a_c_roseatespoonbill_01"), "Spoonbill"},
    AnimalModel{joaat("a_c_toad_01"), "Toad"},
    AnimalModel{joaat("a_c_turtlesnapping_01"), "Turtle"}
};

struct CurrentHaulMaterial {
    std::uint32_t hash{};
    int inventoryCount{};
    int carriedCount{};
    int horseCount{};
};
struct CurrentHaulState {
    std::array<CurrentHaulMaterial, kMaxMaterials> materials{};
    std::size_t count{};
    int inventoryId{-1};
    DWORD lastRefreshTick{};
    bool ready{};
};
CurrentHaulState g_currentHaul{};
volatile LONG g_haulDirty = 1;

const char* find_animal_species(std::uint32_t modelHash) noexcept {
    for (const auto& animal : kAnimalModels) {
        if (animal.hash == modelHash) return animal.species;
    }
    return nullptr;
}

int current_haul_index(std::uint32_t hash) noexcept {
    for (std::size_t i = 0; i < g_currentHaul.count; ++i) {
        if (g_currentHaul.materials[i].hash == hash) return static_cast<int>(i);
    }
    return -1;
}

void add_current_haul_count(std::uint32_t hash, int amount, int source) noexcept {
    if (hash == 0 || amount <= 0 || !is_animal_part_material(hash)) return;
    int index = current_haul_index(hash);
    if (index < 0) {
        if (g_currentHaul.count >= g_currentHaul.materials.size()) return;
        index = static_cast<int>(g_currentHaul.count++);
        g_currentHaul.materials[static_cast<std::size_t>(index)].hash = hash;
    }
    CurrentHaulMaterial& material = g_currentHaul.materials[static_cast<std::size_t>(index)];
    int* destination = source == 0 ? &material.inventoryCount :
                       source == 1 ? &material.carriedCount : &material.horseCount;
    *destination = std::min(1000000, *destination + amount);
}

int current_haul_count(std::uint32_t hash) noexcept {
    const int index = current_haul_index(hash);
    if (index < 0) return 0;
    const CurrentHaulMaterial& material = g_currentHaul.materials[static_cast<std::size_t>(index)];
    return material.inventoryCount + material.carriedCount + material.horseCount;
}

bool current_haul_has_any() noexcept {
    for (std::size_t i = 0; i < g_currentHaul.count; ++i) {
        const CurrentHaulMaterial& material = g_currentHaul.materials[i];
        if (material.inventoryCount + material.carriedCount + material.horseCount > 0) return true;
    }
    return false;
}

struct CarriedAttachedInfo {
    std::uint64_t model{};
    std::uint64_t carryConfig{};
    std::uint64_t carriableSlot{};
    std::uint64_t entity{};
};
static_assert(sizeof(CarriedAttachedInfo) == 32);

void add_entity_animal_materials(int entity, int source, bool knownPelt) noexcept {
    if (entity == 0 || invoke_native<int>(kDoesEntityExist, entity) == 0) return;
    const bool isPelt = knownPelt ||
        invoke_native<int>(kGetIsCarriablePelt, entity) != 0;
    int carcass = entity;
    if (isPelt) carcass = invoke_native<int>(kGetCarcassFromPelt, entity);
    const bool carcassFound = carcass != 0 && invoke_native<int>(kDoesEntityExist, carcass) != 0 &&
                              invoke_native<int>(kIsEntityAPed, carcass) != 0;
    const std::uint32_t model = carcassFound ? invoke_native<std::uint32_t>(kGetEntityModel, carcass) : 0;
    const char* species = carcassFound ? find_animal_species(model) : nullptr;
    // _GET_PED_QUALITY reports the ordinary skinning grade (0=poor, 1=good, 2=perfect).
    const int quality = carcassFound ? invoke_native<int>(kGetPedQuality, carcass) : -1;
    if (!carcassFound || species == nullptr) return;
    // Chipmunks share a HUD recommendation group with squirrels, but do not
    // yield squirrel pelts. Do not let that display grouping invent loot.
    if (model == joaat("a_c_chipmunk_01")) return;

    // The quality value does not identify a legendary animal, so it is never used to infer one.
    const bool qualityKnown = quality >= 0 && quality <= 2;
    for (const auto& sourceEntry : kMaterialSources) {
        if (std::strcmp(sourceEntry.species, species) != 0) continue;
        const char* name = find_material_name(sourceEntry.hash);
        if (name == nullptr) continue;
        const bool perfectOnly = std::strstr(name, "Perfect") != nullptr;
        const bool legendaryOnly = std::strstr(name, "Legendary") != nullptr;
        const bool carcassMaterial = std::strstr(name, "Carcass") != nullptr;
        if (perfectOnly && qualityKnown && quality != 2) continue;
        // A legendary pelt has a distinct inventory item identity. The public
        // carried-entity natives do not expose that identity reliably, so
        // only count it when the player's inventory native reports the item.
        if (legendaryOnly) continue;
        // A carried pelt yields its pelt/hide/skin material. An intact animal
        // yields only a catalogued carcass material until it has been skinned;
        // do not promise pelt, feather or antler recipes prematurely.
        if (isPelt ? (carcassMaterial || !perfectOnly) : !carcassMaterial) continue;
        add_current_haul_count(sourceEntry.hash, 1, source);
    }
}

void update_current_haul() noexcept {
    g_currentHaul = {};
    const int playerPed = invoke_native<int>(kPlayerPedId);
    if (playerPed == 0) return;
    const int inventoryId = invoke_native<int>(kInventoryGetInventoryIdFromPed, playerPed);
    if (inventoryId < 0) return;
    g_currentHaul.inventoryId = inventoryId;
    g_currentHaul.ready = true;

    // Player inventory covers satchel animal parts, feathers, antlers and pelts.
    for (const auto& material : kMaterialNames) {
        if (!is_animal_part_material(material.hash)) continue;
        const int count = player_owned_count(inventoryId, material.hash);
        if (count > 0) add_current_haul_count(material.hash, count, 0);
    }

    // Read the player's carriable slots (0-4). Slots 5-7 are the saddle
    // horse's left, right and rear attachment points.
    for (int slot = 0; slot <= 4; ++slot) {
        CarriedAttachedInfo info{};
        if (invoke_native<bool>(kGetCarriedAttachedInfoForSlot, &info, playerPed, slot, 0) == 0)
            continue;
        const int entity = static_cast<int>(info.entity);
        if (entity == 0 || invoke_native<int>(kDoesEntityExist, entity) == 0) continue;
        const bool isPelt = invoke_native<int>(kGetIsCarriablePelt, entity) != 0;
        const bool isPed = invoke_native<int>(kIsEntityAPed, entity) != 0;
        if (isPelt || isPed) add_entity_animal_materials(entity, 1, isPelt);
    }

    // Use the saddle horse even when the player is standing beside it.
    const int playerId = invoke_native<int>(kPlayerId);
    const int horse = invoke_native<int>(kGetSaddleHorseForPlayer, playerId);
    if (horse != 0 && invoke_native<int>(kDoesEntityExist, horse) != 0) {
        const int itemSet = invoke_native<int>(kCreateItemset, true);
        if (itemSet != 0) {
            invoke_native<void>(kFindAllAttachedCarriableEntities, horse, itemSet);
            const int rawSize = invoke_native<int>(kGetItemsetSize, itemSet);
            const int size = std::clamp(rawSize, 0, 128);
            for (int i = 0; i < size; ++i) {
                const int item = invoke_native<int>(kGetIndexedItemInItemset, i, itemSet);
                const int entity = invoke_native<int>(kGetEntityFromItem, item);
                if (entity == 0 || invoke_native<int>(kDoesEntityExist, entity) == 0) continue;
                const bool isPelt = invoke_native<int>(kGetIsCarriablePelt, entity) != 0;
                const bool isPed = invoke_native<int>(kIsEntityAPed, entity) != 0;
                if (isPelt || isPed) add_entity_animal_materials(entity, 2, isPelt);
            }
            invoke_native<void>(kDestroyItemset, itemSet);
        }
    }
}

void refresh_current_haul_if_due() noexcept {
    const DWORD now = GetTickCount();
    if (g_currentHaul.lastRefreshTick != 0 &&
        now - g_currentHaul.lastRefreshTick < 500u) return;
    update_current_haul();
    g_currentHaul.lastRefreshTick = now;
    InterlockedExchange(&g_haulDirty, 1);
}

bool blip_texture_dictionary_ready() noexcept {
    constexpr const char* kBlipTextureDictionary = "BLIPS";
    if (invoke_native<int>(kHasStreamedTextureDictLoaded, kBlipTextureDictionary) != 0) {
        return true;
    }
    invoke_native<void>(kRequestStreamedTextureDict, kBlipTextureDictionary, false);
    return false;
}

struct HudIconLayout {
    float width{};
    float height{};
    float pearsonCenterX{};
    float trapperCenterX{};
    float centerY{};
};

bool get_hud_icon_layout(HudIconLayout* layout) noexcept {
    if (layout == nullptr) return false;
    int screenWidth = 0;
    int screenHeight = 0;
    invoke_native<void>(kGetScreenResolution, &screenWidth, &screenHeight);
    if (screenWidth <= 0 || screenHeight <= 0) return false;

    // Keep the sprite square in physical pixels at every aspect ratio. The
    // height remains proportional to output resolution, matching the game's
    // normalized HUD drawing convention.
    constexpr float kIconHeight = 0.042f;
    constexpr float kGapHeight = 0.010f;
    constexpr float kTargetPromptRightEdge = 0.972f;
    constexpr float kTargetPromptTopEdge = 0.755f;
    constexpr float kPromptIconGap = 0.012f;
    const float heightOverWidth = static_cast<float>(screenHeight) /
                                  static_cast<float>(screenWidth);
    layout->height = kIconHeight;
    layout->width = kIconHeight * heightOverWidth;
    const float gap = kGapHeight * heightOverWidth;

    // Anchor the pair to the lower-right native interaction-prompt column.
    // They remain above the weapon/info/track prompts and animal-name line,
    // instead of competing with localized prompt text of varying lengths.
    const float rightCenter = kTargetPromptRightEdge - layout->width * 0.5f;
    layout->trapperCenterX = rightCenter;
    layout->pearsonCenterX = rightCenter - layout->width - gap;
    layout->centerY = kTargetPromptTopEdge - layout->height * 0.5f - kPromptIconGap;
    return true;
}

void draw_vendor_icon(const char* spriteName, float centerX, float centerY,
                      float width, float height,
                      int red, int green, int blue) noexcept {
    invoke_native<void>(kDrawSprite, "BLIPS", spriteName, centerX, centerY,
                        width, height, 0.0f, red, green, blue, 255, false);
}

bool inventory_texture_dictionary_ready() noexcept {
    constexpr const char* kInventoryTextureDictionary = "inventory_items";
    if (invoke_native<int>(kHasStreamedTextureDictLoaded, kInventoryTextureDictionary) != 0) {
        return true;
    }
    invoke_native<void>(kRequestStreamedTextureDict, kInventoryTextureDictionary, false);
    return false;
}

void draw_rect(float x, float y, float width, float height,
               int red, int green, int blue, int alpha) noexcept;

// ---- Text fitting -----------------------------------------------------------
// Text is positioned in normalised screen coordinates, and the game offers no
// way to measure it, so widths are estimated per character class. The estimate
// is deliberately a little generous: text that is estimated to fit always fits.
// While g_textRightLimit is set, left-aligned text is shrunk and, if that is
// not enough, shortened so it never crosses that x position.
float g_textRightLimit = 0.0f;

struct TextRightLimitScope {
    explicit TextRightLimitScope(float limit) noexcept { g_textRightLimit = limit; }
    ~TextRightLimitScope() { g_textRightLimit = 0.0f; }
    TextRightLimitScope(const TextRightLimitScope&) = delete;
    TextRightLimitScope& operator=(const TextRightLimitScope&) = delete;
};

std::uint32_t next_codepoint(const char*& cursor) noexcept {
    const unsigned char lead = static_cast<unsigned char>(*cursor);
    std::uint32_t value = lead;
    int extra = 0;
    if (lead >= 0xF0) { value = lead & 0x07u; extra = 3; }
    else if (lead >= 0xE0) { value = lead & 0x0Fu; extra = 2; }
    else if (lead >= 0xC0) { value = lead & 0x1Fu; extra = 1; }
    ++cursor;
    for (int i = 0; i < extra && (static_cast<unsigned char>(*cursor) & 0xC0u) == 0x80u; ++i, ++cursor) {
        value = (value << 6) | (static_cast<unsigned char>(*cursor) & 0x3Fu);
    }
    return value;
}

bool is_wide_codepoint(std::uint32_t c) noexcept {
    return (c >= 0x2E80 && c <= 0x9FFF) || (c >= 0xAC00 && c <= 0xD7AF) ||
           (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFF00 && c <= 0xFFEF);
}

// Width of one character at text size 1, for a 16:9 screen.
float codepoint_width(std::uint32_t c) noexcept {
    if (c == ' ') return 0.00017f;
    if (c == '.' || c == ',' || c == ':' || c == ';' || c == '\'' || c == '!' || c == '|' || c == 0x2019)
        return 0.00017f;
    if (c < 0x80) return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                             ? 0.00041f : 0.00030f;
    if (is_wide_codepoint(c)) return 0.00046f;
    if (c >= 0x400 && c <= 0x52F) return 0.00045f;   // Cyrillic
    return 0.00041f;
}

float screen_width_correction() noexcept {
    int width = 0;
    int height = 0;
    invoke_native<void>(kGetScreenResolution, &width, &height);
    if (width <= 0 || height <= 0) return 1.0f;
    return (16.0f / 9.0f) / (static_cast<float>(width) / static_cast<float>(height));
}

// 1.20 look: the menu font can be switched in Settings. Each face carries a
// rough width factor (relative to the catalogue face the estimates were tuned
// for) so the text-fitting keeps working.
struct MenuFont { const char* face; const char* name; float widthFactor; };
constexpr MenuFont kMenuFonts[] = {
    {"$catalog1", "CATALOGUE", 1.00f},
    {"$body", "BODY", 0.94f},
    {"$body1", "BODY 1", 0.94f},
    {"$title1", "TITLE", 0.74f},
    {"$ledger", "LEDGER", 0.92f},
};
constexpr int kMenuFontCount = static_cast<int>(std::size(kMenuFonts));
int g_menuFontIndex = 0;

// The LEDGER face has no Cyrillic letters, so Russian falls back to CATALOGUE.
int effective_menu_font() noexcept {
    if (g_menuFontIndex == 4 && hunting_companion::active_ui_language() == 7) return 0;
    return g_menuFontIndex;
}

float text_width_estimate(const char* text, float size, float correction) noexcept {
    float width = 0.0f;
    for (const char* cursor = text; *cursor != '\0';) width += codepoint_width(next_codepoint(cursor));
    const float fontFactor =
        hunting_companion::uses_cjk_font(hunting_companion::active_ui_language())
            ? 1.0f : kMenuFonts[effective_menu_font()].widthFactor;
    return width * size * correction * fontFactor;
}

// Copies as much of `text` as fits in maxWidth, adding "..." when it is cut.
void shorten_to_width(const char* text, float size, float correction, float maxWidth,
                      char* out, std::size_t outSize) noexcept {
    constexpr char kEllipsis[] = "...";
    const float ellipsisWidth = text_width_estimate(kEllipsis, size, correction);
    float width = 0.0f;
    std::size_t length = 0;
    const char* cursor = text;
    while (*cursor != '\0') {
        const char* start = cursor;
        const float glyph = codepoint_width(next_codepoint(cursor)) * size * correction;
        const std::size_t bytes = static_cast<std::size_t>(cursor - start);
        if (width + glyph > maxWidth - ellipsisWidth || length + bytes + sizeof(kEllipsis) > outSize) {
            std::memcpy(out + length, kEllipsis, sizeof(kEllipsis));
            return;
        }
        std::memcpy(out + length, start, bytes);
        length += bytes;
        width += glyph;
    }
    out[length] = '\0';   // everything fit: no ellipsis needed
}

constexpr std::size_t kFitLineCapacity = 192;
struct FittedLines {
    char lines[3][kFitLineCapacity]{};
    int count{};
    bool complete{};      // false when the last line had to be cut with "..."
};

// Greedy word wrap (any character is a break opportunity for CJK text). Returns
// true when all of the text fit; otherwise the last line ends in "...".
bool wrap_to_width(const char* text, float size, float correction, float maxWidth,
                   int maxLines, FittedLines* out) noexcept {
    *out = FittedLines{};
    maxLines = std::min(maxLines, 3);
    const char* cursor = text;
    while (*cursor == ' ') ++cursor;
    while (*cursor != '\0' && out->count < maxLines) {
        char* line = out->lines[out->count];
        std::size_t length = 0;
        float width = 0.0f;
        std::size_t lastSpaceLength = 0;
        const char* lastSpaceCursor = nullptr;
        bool lineFull = false;
        while (*cursor != '\0' && !lineFull) {
            const char* start = cursor;
            const std::uint32_t c = next_codepoint(cursor);
            const float glyph = codepoint_width(c) * size * correction;
            const std::size_t bytes = static_cast<std::size_t>(cursor - start);
            if (width + glyph > maxWidth || length + bytes + 1 >= kFitLineCapacity) {
                lineFull = true;
                if (c == ' ') {
                    // The line ends exactly at a word boundary.
                } else if (lastSpaceCursor != nullptr && !is_wide_codepoint(c)) {
                    length = lastSpaceLength;
                    cursor = lastSpaceCursor;
                } else if (length == 0) {   // a single glyph wider than the line
                    std::memcpy(line, start, bytes);
                    length = bytes;
                } else {
                    cursor = start;
                }
                continue;
            }
            if (c == ' ') { lastSpaceLength = length; lastSpaceCursor = cursor; }
            std::memcpy(line + length, start, bytes);
            length += bytes;
            width += glyph;
        }
        while (length > 0 && line[length - 1] == ' ') --length;
        line[length] = '\0';
        ++out->count;
        while (*cursor == ' ') ++cursor;
    }
    out->complete = (*cursor == '\0');
    if (!out->complete && out->count > 0) {
        // Re-attach the remaining text so the cut line ends in "...".
        char* last = out->lines[out->count - 1];
        char joined[kFitLineCapacity * 2];
        std::snprintf(joined, sizeof(joined), "%s %s", last, cursor);
        shorten_to_width(joined, size, correction, maxWidth, last, kFitLineCapacity);
    }
    return out->complete;
}

bool g_showWeaponIcons = true;       // toggled from Settings or the icons hotkey

void draw_text_impl(const char* text, float x, float y, float referencePixelHeight,
                    int red, int green, int blue, int alpha,
                    bool localize, bool centered) noexcept;

struct RecommendationIcon {
    const char* dictionary;
    const char* sprite;
    const char* label;   // untranslated key; translated when drawn
};

void draw_icon_with_shadow(const RecommendationIcon& icon, float centerX, float centerY,
                           float width, float height) noexcept {
    // Soft drop shadow: two dark copies offset down and to the right.
    invoke_native<void>(kDrawSprite, icon.dictionary, icon.sprite, centerX + width * 0.045f,
                        centerY + height * 0.055f, width, height, 0.0f, 0, 0, 0, 120, false);
    invoke_native<void>(kDrawSprite, icon.dictionary, icon.sprite, centerX + width * 0.020f,
                        centerY + height * 0.028f, width, height, 0.0f, 0, 0, 0, 150, false);
    invoke_native<void>(kDrawSprite, icon.dictionary, icon.sprite, centerX, centerY,
                        width, height, 0.0f, 255, 255, 255, 255, false);
}

// Draws a centred label of up to two lines, always inside maxWidth.
void draw_label_with_shadow(const char* text, float centerX, float y, float maxWidth) noexcept {
    const float correction = screen_width_correction();
    FittedLines fitted;
    float size = 9.0f;
    while (!wrap_to_width(text, size, correction, maxWidth, 2, &fitted) && size > 7.5f) size -= 0.5f;
    for (int line = 0; line < fitted.count; ++line) {
        const float lineY = y + static_cast<float>(line) * size * 0.00155f;
        draw_text_impl(fitted.lines[line], centerX + 0.0006f, lineY + 0.0011f, size, 0, 0, 0, 215, false, true);
        draw_text_impl(fitted.lines[line], centerX, lineY, size, 238, 232, 218, 255, false, true);
    }
}

void draw_hunting_recommendation_icons(
    const hunting_companion::hunting::Recommendation& recommendation,
    const HudIconLayout& layout) noexcept {
    // Icons are laid out two to a row. A weapon shares its first row with its
    // first ammunition type; further ammunition types continue on the next row.
    // The first recommendation sits lowest, nearest the vendor icons, and the
    // others stack upward from it.
    struct IconRow { RecommendationIcon icons[2]; int count; };
    constexpr std::size_t kMaxRows = 6;
    IconRow rows[kMaxRows]{};
    std::size_t rowCount = 0;
    for (std::size_t choiceIndex = recommendation.choiceCount; choiceIndex-- > 0;) {
        const auto& choice = recommendation.choices[choiceIndex];
        RecommendationIcon items[1 + 4]{};
        std::size_t itemCount = 0;
        items[itemCount++] = RecommendationIcon{"inventory_items", choice.texture, choice.label};
        for (std::size_t ammo = 0; ammo < choice.ammoCount && itemCount < std::size(items); ++ammo) {
            items[itemCount++] = RecommendationIcon{"inventory_items", choice.ammo[ammo].texture, choice.ammo[ammo].label};
        }
        for (std::size_t first = 0; first < itemCount && rowCount < kMaxRows; first += 2) {
            IconRow& row = rows[rowCount++];
            row.count = static_cast<int>(std::min<std::size_t>(2, itemCount - first));
            for (int i = 0; i < row.count; ++i) row.icons[i] = items[first + static_cast<std::size_t>(i)];
        }
    }
    if (rowCount == 0) return;

    constexpr float kIconHeight = 0.050f;
    constexpr float kLabelGap = 0.004f;
    constexpr float kLabelBlockHeight = 0.032f;      // two lines of label text
    constexpr float kRowHeight = kIconHeight + kLabelGap + kLabelBlockHeight + 0.006f;
    constexpr float kRightEdge = 0.972f;             // same right edge as the vendor icons
    constexpr float kGapAboveVendorIcons = 0.014f;
    constexpr float kCellWidth = 0.078f;
    constexpr float kLabelRightLimit = 0.988f;       // labels stay clear of the screen edge
    const float heightOverWidth = layout.width / layout.height;
    const float iconWidth = kIconHeight * heightOverWidth;
    // The right-hand column ends on the same pixel as the vendor icons below it.
    const float rightColumnCenterX = kRightEdge - iconWidth * 0.5f;
    const float vendorTop = layout.centerY - layout.height * 0.5f;
    const float stackBottom = vendorTop - kGapAboveVendorIcons;

    char label[96]{};
    for (std::size_t r = 0; r < rowCount; ++r) {
        // rows[] runs from the top of the stack to the bottom.
        const float rowTop = stackBottom - static_cast<float>(rowCount - r) * kRowHeight;
        const IconRow& row = rows[r];
        for (int i = 0; i < row.count; ++i) {
            // A lone icon goes in the right-hand column.
            const int column = row.count == 1 ? 1 : i;
            const float centerX = rightColumnCenterX - kCellWidth * (1.0f - static_cast<float>(column));
            draw_icon_with_shadow(row.icons[i], centerX, rowTop + kIconHeight * 0.5f, iconWidth, kIconHeight);
            std::snprintf(label, sizeof(label), "%s", hunting_companion::ui_text(row.icons[i].label));
            // A label is wider than its icon, so the right column's label is
            // narrowed rather than allowed to run off the screen.
            const float labelWidth = std::min(kCellWidth * 0.97f, (kLabelRightLimit - centerX) * 2.0f);
            draw_label_with_shadow(label, centerX, rowTop + kIconHeight + kLabelGap, labelWidth);
        }
    }
}

void update_hud() noexcept {
    if (!g_tracker.ready) return;
    const int player = invoke_native<int>(0x217E9DC48139933Dull);
    int focusedEntity = 0;
    const int found = invoke_native<int>(kGetPlayerInteractionTargetEntity,
                                         player, &focusedEntity, true, false);
    if (!found || focusedEntity == 0) return;
    const std::uint32_t model = invoke_native<std::uint32_t>(kGetEntityModel, focusedEntity);
    const char* species = find_animal_species(model);
    if (species == nullptr) return;

    const bool needsPearson = species_needed(g_tracker.pearson.demand, species);
    const bool needsTrapper = species_needed(g_tracker.trapper.demand, species);

    HudIconLayout layout{};
    if (!get_hud_icon_layout(&layout)) return;

    // Recommendations are useful for every recognized animal, not only species
    // currently needed by Pearson or the Trapper.
    const auto* recommendation = hunting_companion::hunting::find(species);
    if (recommendation != nullptr && g_showWeaponIcons && inventory_texture_dictionary_ready()) {
        draw_hunting_recommendation_icons(*recommendation, layout);
    }

    if ((!needsPearson && !needsTrapper) || !blip_texture_dictionary_ready()) return;
    if (needsPearson) {
        draw_vendor_icon("blip_shop_butcher", layout.pearsonCenterX, layout.centerY,
                         layout.width, layout.height, 245, 184, 72);
    }
    if (needsTrapper) {
        draw_vendor_icon("blip_shop_animal_trapper", layout.trapperCenterX, layout.centerY,
                         layout.width, layout.height, 100, 190, 245);
    }
}

// -----------------------------------------------------------------------------
// In-game compendium menu. The menu reads the same vendor storage snapshot as
// the hunting HUD. It never counts the player's carried animal materials.
// -----------------------------------------------------------------------------

enum class MenuPage {
    Closed, Root, Categories, Recipes, Detail, CraftingUses,
    CraftingUseDetail, Challenges, ChallengeDetail, Settings
};
struct MenuCategory { std::uint32_t hash{}; const char* label{}; int recipeCount{}; };
struct MenuState {
    MenuPage page{MenuPage::Closed};
    bool trapper{};
    bool craftableNow{};
    int selection{};
    int scroll{};
    int rootReturnSelection{};
    std::uint32_t categoryHash{};
    int recipeIndex{-1};
    int categoryReturnSelection{};
    int categoryReturnScroll{};
    int recipeReturnSelection{};
    int recipeReturnScroll{};
    MenuPage detailReturnPage{MenuPage::Recipes};
    int challengeIndex{};
    int craftingUseIndex{-1};
    int challengeReturnSelection{};
    int challengeReturnScroll{};
    bool captureHotkey{};
    bool captureFilterHotkey{};
    bool captureIconsHotkey{};
};
MenuState g_menu{};
std::array<MenuCategory, kMaxCategories> g_menuCategories{};
std::size_t g_menuCategoryCount{};
std::array<int, kMaxRecipesPerShop> g_menuRecipeIndices{};
std::size_t g_menuRecipeCount{};
std::array<bool, 256> g_keyDown{};
std::array<bool, 256> g_keyPressed{};
struct CraftingUseEntry {
    bool trapper{};
    int recipeIndex{-1};
    const char* setName{};
    int readiness{};
};
std::array<CraftingUseEntry, kMaxRecipesPerShop * 2> g_craftingUseEntries{};
std::size_t g_craftingUseCount{};
DWORD g_menuHotkey = kDefaultMenuKey;
// Defaults avoid RDR2's own keyboard bindings and the keys most trainers use
// (F-keys, Numpad, Insert/Delete/Home/End, Space, letters).
constexpr DWORD kDefaultFilterKey = VK_OEM_4;  // [
constexpr DWORD kDefaultIconsKey = VK_OEM_6;   // ]
DWORD g_filterHotkey = kDefaultFilterKey;
DWORD g_iconsHotkey = kDefaultIconsKey;
bool g_refreshWorkerRegistered{};
volatile LONG g_refreshRequested = 1;
char g_iniPath[MAX_PATH]{};
char g_moduleDirectory[MAX_PATH]{};
wchar_t g_moduleDirectoryW[MAX_PATH]{};
int g_uiLanguageOverride = -1; // -1 follows the RDR2 language; 0-12 pins a UI locale.

VendorState& active_vendor() noexcept {
    return g_menu.trapper ? g_tracker.trapper : g_tracker.pearson;
}

const char* active_vendor_name() noexcept {
    return g_menu.trapper ? "Trapper" : "Pearson";
}

const char* category_label(bool trapper, std::uint32_t hash) noexcept {
    if (trapper) {
        for (const auto& entry : kTrapperCategories) if (entry.hash == hash) return entry.label;
    } else {
        for (const auto& entry : kPearsonCategories) if (entry.hash == hash) return entry.label;
    }
    return "Other";
}

void initialise_ini_path() noexcept {
    wchar_t modulePathW[MAX_PATH]{};
    const DWORD wideLength = GetModuleFileNameW(g_module, modulePathW, MAX_PATH);
    if (wideLength > 0 && wideLength < MAX_PATH) {
        wchar_t* wideSlash = std::wcsrchr(modulePathW, L'\\');
        wchar_t* wideAltSlash = std::wcsrchr(modulePathW, L'/');
        if (wideAltSlash != nullptr && (wideSlash == nullptr || wideAltSlash > wideSlash))
            wideSlash = wideAltSlash;
        if (wideSlash != nullptr) {
            *(wideSlash + 1) = L'\0';
            std::wcsncpy(g_moduleDirectoryW, modulePathW, MAX_PATH - 1);
            g_moduleDirectoryW[MAX_PATH - 1] = L'\0';
        }
    }

    char modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(g_module, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return;
    char* slash = std::strrchr(modulePath, '\\');
    char* altSlash = std::strrchr(modulePath, '/');
    if (altSlash != nullptr && (slash == nullptr || altSlash > slash)) slash = altSlash;
    if (slash == nullptr) return;
    *(slash + 1) = '\0';
    std::snprintf(g_moduleDirectory, sizeof(g_moduleDirectory), "%s", modulePath);
    std::snprintf(g_iniPath, sizeof(g_iniPath), "%sHuntingCompanion.ini", modulePath);
}

void load_configuration() noexcept {
    initialise_ini_path();
    if (g_iniPath[0] == '\0') {
        g_menuHotkey = kDefaultMenuKey;
        return;
    }
    const UINT key = GetPrivateProfileIntA("General", "MenuKey", kDefaultMenuKey, g_iniPath);
    g_menuHotkey = (key > 0 && key < 256) ? key : kDefaultMenuKey;
    const UINT filterKey = GetPrivateProfileIntA("General", "FilterKey", kDefaultFilterKey, g_iniPath);
    // Space is used by the game for jumping/sprinting, so it is not a valid default.
    g_filterHotkey = (filterKey > 0 && filterKey < 256 && filterKey != VK_SPACE)
        ? filterKey : kDefaultFilterKey;
    const UINT iconsKey = GetPrivateProfileIntA("General", "WeaponIconsKey", kDefaultIconsKey, g_iniPath);
    g_iconsHotkey = (iconsKey > 0 && iconsKey < 256) ? iconsKey : kDefaultIconsKey;
    g_showWeaponIcons = GetPrivateProfileIntA("General", "ShowWeaponIcons", 1, g_iniPath) != 0;
    const UINT menuFont = GetPrivateProfileIntA("General", "MenuFont", 0, g_iniPath);
    g_menuFontIndex = menuFont < static_cast<UINT>(kMenuFontCount) ? static_cast<int>(menuFont) : 0;
    const UINT configuredLanguage = GetPrivateProfileIntA(
        "General", "UILanguage", 0xFFFFFFFFu, g_iniPath);
    g_uiLanguageOverride = configuredLanguage == 0xFFFFFFFFu
        ? -1 : (configuredLanguage < 13u ? static_cast<int>(configuredLanguage) : -1);
    // Ensure usable configuration defaults exist even before Settings is opened.
    char value[16]{};
    std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(g_menuHotkey));
    WritePrivateProfileStringA("General", "MenuKey", value, g_iniPath);
    std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(g_filterHotkey));
    WritePrivateProfileStringA("General", "FilterKey", value, g_iniPath);
    std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(g_iconsHotkey));
    WritePrivateProfileStringA("General", "WeaponIconsKey", value, g_iniPath);
    WritePrivateProfileStringA("General", "ShowWeaponIcons", g_showWeaponIcons ? "1" : "0", g_iniPath);
    char languageValue[16]{};
    std::snprintf(languageValue, sizeof(languageValue), "%d", g_uiLanguageOverride);
    WritePrivateProfileStringA("General", "UILanguage", languageValue, g_iniPath);
}

void save_menu_hotkey() noexcept {
    if (g_iniPath[0] == '\0') return;
    char value[16]{};
    std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(g_menuHotkey));
    WritePrivateProfileStringA("General", "MenuKey", value, g_iniPath);
}

void save_filter_hotkey() noexcept {
    if (g_iniPath[0] == '\0') return;
    char value[16]{};
    std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(g_filterHotkey));
    WritePrivateProfileStringA("General", "FilterKey", value, g_iniPath);
}

void save_icons_settings() noexcept {
    if (g_iniPath[0] == '\0') return;
    char value[16]{};
    std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(g_iconsHotkey));
    WritePrivateProfileStringA("General", "WeaponIconsKey", value, g_iniPath);
    WritePrivateProfileStringA("General", "ShowWeaponIcons", g_showWeaponIcons ? "1" : "0", g_iniPath);
}

void change_menu_font(int direction) noexcept {
    g_menuFontIndex = (g_menuFontIndex + direction + kMenuFontCount) % kMenuFontCount;
    if (g_iniPath[0] == '\0') return;
    char value[16]{};
    std::snprintf(value, sizeof(value), "%d", g_menuFontIndex);
    WritePrivateProfileStringA("General", "MenuFont", value, g_iniPath);
}

void save_ui_language() noexcept {
    if (g_iniPath[0] == '\0') return;
    char value[16]{};
    std::snprintf(value, sizeof(value), "%d", g_uiLanguageOverride);
    WritePrivateProfileStringA("General", "UILanguage", value, g_iniPath);
}

// ---- CJK overlay integration ---------------------------------------------
// The language the user asked for: an explicit pick, or the game's own.
int requested_language() noexcept {
    return g_uiLanguageOverride >= 0 ? g_uiLanguageOverride : g_detectedGameLanguage;
}

// CJK text needs the overlay only when the game itself is NOT running in that
// language; otherwise the game's own fonts already contain the glyphs.
bool needs_overlay(int language) noexcept {
    // Overlay only when the companion language is CJK and the game is not running
    // in it. A Latin or Cyrillic companion language on a CJK game needs no overlay:
    // the game keeps a plain face with those letters loaded (see selected_font_face).
    if (language == g_detectedGameLanguage) return false;
    return hunting_companion::uses_cjk_font(language);
}

// True when a Latin/Cyrillic companion language is shown on a game running in a
// CJK language. The game's styled CJK faces lack accented Latin letters and only
// have wide Cyrillic, but its plain "standard"/"utility" face has both (checked
// in-game for Polish and Russian on a Japanese game).
bool plain_face_needed() noexcept {
    return hunting_companion::uses_cjk_font(g_detectedGameLanguage) &&
           !hunting_companion::uses_cjk_font(hunting_companion::active_ui_language());
}

// The language that can really be drawn right now. If the user picked a CJK
// language but the overlay is not available (not Vulkan, hook failed, still
// starting), fall back to English instead of showing empty squares.
int effective_language() noexcept {
    const int wanted = requested_language();
    if (needs_overlay(wanted) && !hunting_companion::overlay::ready()) return 0;
    return wanted;
}

bool overlay_fallback_active() noexcept {
    return effective_language() != requested_language();
}

void apply_effective_language() noexcept {
    const int effective = effective_language();
    if (effective != hunting_companion::active_ui_language()) {
        hunting_companion::load_ui_localization(g_detectedGameLanguage, effective);
    }
    hunting_companion::overlay::set_language(effective);
}

// Read once at attach time (the hooks must be installed before the game starts
// Vulkan, which is earlier than ScriptMain runs).
hunting_companion::overlay::Settings make_overlay_settings() noexcept {
    initialise_ini_path();
    hunting_companion::overlay::Settings settings;
    if (g_iniPath[0] != '\0') {
        const int scale = static_cast<int>(GetPrivateProfileIntA("Overlay", "CjkTextScalePercent", 100, g_iniPath));
        settings.textScale = (scale >= 40 && scale <= 300) ? static_cast<float>(scale) / 100.0f : 1.0f;
        const int offset = static_cast<int>(GetPrivateProfileIntA("Overlay", "CjkTextYOffsetPermille", 0, g_iniPath));
        settings.yOffset = (offset >= -100 && offset <= 100) ? static_cast<float>(offset) / 1000.0f : 0.0f;
        const int paperWhite = static_cast<int>(GetPrivateProfileIntA("Overlay", "CjkHdrPaperWhiteNits", 230, g_iniPath));
        settings.hdrPaperWhiteNits = (paperWhite >= 80 && paperWhite <= 1000) ? static_cast<float>(paperWhite) : 230.0f;
        const int backend = static_cast<int>(GetPrivateProfileIntA("Overlay", "OverlayBackend", 0, g_iniPath));
        settings.backend = (backend >= 0 && backend <= 2) ? backend : 0;
        settings.fileLog = GetPrivateProfileIntA("Overlay", "OverlayLog", 0, g_iniPath) != 0;
        settings.logDirectory = g_moduleDirectory;
    }
    return settings;
}

void change_ui_language(int direction) noexcept {
    if (direction == 0) return;
    if (direction > 0) {
        g_uiLanguageOverride = (g_uiLanguageOverride >= 12) ? -1 : g_uiLanguageOverride + 1;
    } else {
        g_uiLanguageOverride = (g_uiLanguageOverride <= -1) ? 12 : g_uiLanguageOverride - 1;
    }
    save_ui_language();
    // Reload immediately; no game restart is needed for the companion UI.
    apply_effective_language();
    // Every visible string is resolved against the newly selected immutable
    // locale table on the very next draw. The ItemDB snapshot stores stable
    // Rockstar hashes, not localized text, so a background rescan is neither
    // needed nor desirable here.
}

const char* key_name(DWORD key) noexcept {
    static char name[24]{};
    if (key >= VK_F1 && key <= VK_F24) {
        std::snprintf(name, sizeof(name), "F%u", static_cast<unsigned>(key - VK_F1 + 1));
        return name;
    }
    if (key >= 'A' && key <= 'Z') {
        name[0] = static_cast<char>(key);
        name[1] = '\0';
        return name;
    }
    if (key >= '0' && key <= '9') {
        name[0] = static_cast<char>(key);
        name[1] = '\0';
        return name;
    }
    switch (key) {
        case VK_SPACE: return "Space";
        case VK_OEM_1: return ";";
        case VK_OEM_PLUS: return "=";
        case VK_OEM_COMMA: return ",";
        case VK_OEM_MINUS: return "-";
        case VK_OEM_PERIOD: return ".";
        case VK_OEM_2: return "/";
        case VK_OEM_3: return "`";
        case VK_OEM_4: return "[";
        case VK_OEM_5: return "\\";
        case VK_OEM_6: return "]";
        case VK_OEM_7: return "'";
        case VK_TAB: return "Tab";
        case VK_HOME: return "Home";
        case VK_END: return "End";
        case VK_INSERT: return "Insert";
        case VK_DELETE: return "Delete";
        case VK_PRIOR: return "Page Up";
        case VK_NEXT: return "Page Down";
        case VK_NUMPAD0: return "Num 0";
        case VK_NUMPAD1: return "Num 1";
        case VK_NUMPAD2: return "Num 2";
        case VK_NUMPAD3: return "Num 3";
        case VK_NUMPAD4: return "Num 4";
        case VK_NUMPAD5: return "Num 5";
        case VK_NUMPAD6: return "Num 6";
        case VK_NUMPAD7: return "Num 7";
        case VK_NUMPAD8: return "Num 8";
        case VK_NUMPAD9: return "Num 9";
        default:
            std::snprintf(name, sizeof(name), "VK %u", static_cast<unsigned>(key));
            return name;
    }
}

void update_keyboard_edges() noexcept {
    for (unsigned key = 1; key < 256; ++key) {
        const bool down = (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
        g_keyPressed[key] = down && !g_keyDown[key];
        g_keyDown[key] = down;
    }
}

bool key_pressed(unsigned key) noexcept {
    return key < g_keyPressed.size() && g_keyPressed[key];
}

// Up/Down navigation starts immediately, then repeats after a short pause.
// Page Up/Down and all other keys remain edge-triggered.
bool key_repeated(unsigned key) noexcept {
    static DWORD nextUpRepeatAt = 0;
    static DWORD nextDownRepeatAt = 0;
    DWORD* nextRepeatAt = nullptr;
    if (key == VK_UP) nextRepeatAt = &nextUpRepeatAt;
    else if (key == VK_DOWN) nextRepeatAt = &nextDownRepeatAt;
    else return key_pressed(key);

    if (key >= g_keyDown.size() || !g_keyDown[key]) {
        *nextRepeatAt = 0;
        return false;
    }

    const DWORD now = GetTickCount();
    if (key_pressed(key)) {
        *nextRepeatAt = now + 340; // deliberate pause before scrolling accelerates
        return true;
    }
    if (*nextRepeatAt == 0) {
        *nextRepeatAt = now + 340;
        return false;
    }
    if (static_cast<LONG>(now - *nextRepeatAt) >= 0) {
        *nextRepeatAt = now + 90;
        return true;
    }
    return false;
}

bool recipe_craftable_now(const VendorState& vendor, const RecipeEntry& recipe) noexcept;

void build_menu_categories() noexcept {
    g_menuCategoryCount = 0;
    VendorState& vendor = active_vendor();
    auto append_if_used = [&](std::uint32_t hash, const char* label) {
        int count = 0;
        for (std::size_t i = 0; i < vendor.recipeCount; ++i) {
            const RecipeEntry& recipe = vendor.recipes[i];
            if (recipe.categoryHash == hash &&
                (!g_menu.craftableNow || recipe_craftable_now(vendor, recipe))) ++count;
        }
        if (count > 0 && g_menuCategoryCount < g_menuCategories.size()) {
            g_menuCategories[g_menuCategoryCount++] = MenuCategory{hash, label, count};
        }
    };
    if (g_menu.trapper) {
        for (const auto& category : kTrapperCategories) append_if_used(category.hash, category.label);
    } else {
        for (const auto& category : kPearsonCategories) append_if_used(category.hash, category.label);
    }
    // Keep new/unknown catalog groups visible instead of silently dropping them.
    for (std::size_t i = 0; i < vendor.recipeCount; ++i) {
        const std::uint32_t hash = vendor.recipes[i].categoryHash;
        bool known = false;
        for (std::size_t c = 0; c < g_menuCategoryCount; ++c) {
            if (g_menuCategories[c].hash == hash) { known = true; break; }
        }
        if (!known && g_menuCategoryCount < g_menuCategories.size()) {
            int count = 0;
            for (std::size_t j = 0; j < vendor.recipeCount; ++j) {
                const RecipeEntry& recipe = vendor.recipes[j];
                if (recipe.categoryHash == hash &&
                    (!g_menu.craftableNow || recipe_craftable_now(vendor, recipe))) ++count;
            }
            if (count > 0) {
                g_menuCategories[g_menuCategoryCount++] = MenuCategory{hash, "Other / new category", count};
            }
        }
    }
}

void build_menu_recipe_list() noexcept {
    g_menuRecipeCount = 0;
    VendorState& vendor = active_vendor();
    for (std::size_t i = 0; i < vendor.recipeCount; ++i) {
        if (vendor.recipes[i].categoryHash == g_menu.categoryHash &&
            (!g_menu.craftableNow || recipe_craftable_now(vendor, vendor.recipes[i])) &&
            g_menuRecipeCount < g_menuRecipeIndices.size()) {
            g_menuRecipeIndices[g_menuRecipeCount++] = static_cast<int>(i);
        }
    }
    // Alphabetical order makes the list predictable even if shop catalog order changes.
    for (std::size_t i = 1; i < g_menuRecipeCount; ++i) {
        const int value = g_menuRecipeIndices[i];
        std::size_t j = i;
        while (j > 0 && std::strcmp(vendor.recipes[static_cast<std::size_t>(g_menuRecipeIndices[j - 1])].name,
                                    vendor.recipes[static_cast<std::size_t>(value)].name) > 0) {
            g_menuRecipeIndices[j] = g_menuRecipeIndices[j - 1];
            --j;
        }
        g_menuRecipeIndices[j] = value;
    }
}

const RecipeEntry* selected_recipe() noexcept {
    if (g_menu.recipeIndex < 0) return nullptr;
    const VendorState& vendor = g_menu.trapper ? g_tracker.trapper : g_tracker.pearson;
    const std::size_t index = static_cast<std::size_t>(g_menu.recipeIndex);
    return index < vendor.recipeCount ? &vendor.recipes[index] : nullptr;
}

constexpr std::uint32_t kTrapperGarmentSetsCategoryHash = 0xC46CC879u;

struct GarmentSetDefinition {
    const char* setName;
    const char* components[5];
    std::size_t componentCount;
};

// The game exposes the 16 set entries separately from their garment recipes.
// This mapping supplies only the set-to-piece relationship; every material
// amount, ownership flag and donated count is resolved from the live catalog.
constexpr GarmentSetDefinition kGarmentSets[] = {
    {"The Death Roll", {"Legendary Alligator Gambler's Hat", "Legendary Panther Cloak", "Legendary Alligator Vest", "Legendary Alligator Fowlers", "Legendary Panther Range Gloves"}, 5},
    {"The Beast of Prey", {"Legendary Cougar Flop Hat", "Legendary Cougar & Wolf Vest", "Legendary Wolf Batwing Chaps", "Bull Fowler Boots", "Legendary Cougar Riding Gloves"}, 5},
    {"The Rattler", {"Ram Sombrero", "Outdoorsmen Vest", "Javelina Half Chaps", "Iguana Range Gloves", nullptr}, 4},
    {"The Ghost Bison", {"Legendary White Bison Hat", "Legendary White Bison Coat", "Legendary Elk Half Chaps", "Legendary Elk Moccasins", "Legendary Elk Range Gloves"}, 5},
    {"The Wrangler", {"Goat Flop Hat", "Trapper's Cloak", "No-Man's Vest", "Two Toned Moccasins", "Moose Range Gloves"}, 5},
    {"The Huntsman", {"Legendary Coyote Mountain Hat", "Legendary Pronghorn Coat", "Legendary Coyote Half Chaps", "Workman's Pride Boots", "Legendary Pronghorn Range Gloves"}, 5},
    {"The Marauder", {"Muskrat Cavalry Hat", "Principal Vest", "Bull Fringed Shotgun Chaps", "Snake Skin Cavalry Gloves", nullptr}, 4},
    {"The Trophy Buck", {"Raccoon Mountain Hat", "Legendary Buck Vest", "Legendary Fox Moccasins", "Legendary Buck & Fox Range Gloves", nullptr}, 4},
    {"The Desperado", {"Beaver Drifter Hat", "Wolf Coat", "Billy Vest", "Boar & Bull Fowler Boots", "Winter Cavalry Gloves"}, 5},
    {"The Mountain Man", {"Grenadier Hat", "Ram Shotgun Coat", "Country Vest", "Elk Riding Gloves", nullptr}, 4},
    {"The Bounty Hunter", {"Plantation Slouch Hat", "Beaver Hunting Jacket", "Sheepskin Vest", "Pronghorn Half Chaps", "Buck Riding Gloves"}, 5},
    {"The Stalker", {"Legendary Ram Hat", "Legendary Moose Hunting Jacket", "Legendary Ram Batwing Chaps", "Legendary Moose Moccasins", "Legendary Ram Rifleman Gloves"}, 5},
    {"The Night Wrangler", {"Coyote Gambler's Hat", "Cougar Cutaway Coat", "Wilderness Vest", "Moose Half Chaps", "Badger Rifleman Gloves"}, 5},
    {"The Bear Hunter", {"Legendary Bear Head Hat", "Legendary Bear Coat", "Legendary Bear Roper", "Boar Riding Gloves", nullptr}, 4},
    {"The Dreamcatcher", {"Legendary Beaver Flop Hat", "Legendary Bison Vest", "Legendary Bison Batwing Chaps", "Legendary Boar & Bison Fowlers", "Legendary Beaver Cavalry Gloves"}, 5},
    {"The Bronco Buster", {"Elk Flop Hat", "Coyote Scout Jacket", "Huntsman Vest", "Boar Fringed Shotgun Chaps", "Pigskin Rifleman Gloves"}, 5},
};
static_assert(std::size(kGarmentSets) == 16, "Expected 16 Trapper garment sets");

const GarmentSetDefinition* find_garment_set_definition(const char* name) noexcept {
    if (name == nullptr) return nullptr;
    for (const auto& set : kGarmentSets) {
        if (std::strcmp(name, set.setName) == 0) return &set;
    }
    return nullptr;
}

const GarmentSetDefinition* selected_garment_set() noexcept {
    const RecipeEntry* recipe = selected_recipe();
    if (recipe == nullptr || !g_menu.trapper ||
        recipe->categoryHash != kTrapperGarmentSetsCategoryHash) return nullptr;
    return find_garment_set_definition(recipe->name);
}

// Reinforced equipment is not made from donated pelts. Each piece unlocks
// at a challenge rank and is then purchased from the Trapper for cash.
// Prices and unlock ranks are reference metadata; current rank totals come
// separately from the game's read-only challenge natives above.
struct ReinforcedEquipmentInfo {
    const char* itemName;
    const char* challengeName;
    unsigned unlockRank;
    unsigned priceCents;
};

constexpr ReinforcedEquipmentInfo kReinforcedEquipment[] = {
    // Bandoliers
    {"Bandit Bandolier", "Bandit", 1, 2175},
    {"Weapons Expert Bandolier", "Weapons Expert", 10, 1700},
    {"Explorer Bandolier", "Explorer", 3, 3475},
    {"Herbalist Bandolier", "Herbalist", 10, 3025},
    {"Horseman Bandolier", "Horseman", 7, 1775},
    {"Master Hunter Bandolier", "Master Hunter", 3, 2450},
    {"Survivalist Bandolier", "Survivalist", 7, 2725},
    {"Gambler Bandolier", "Gambler", 3, 2100},
    {"Sharpshooter Bandolier", "Sharpshooter", 1, 2000},
    // Gun belts
    {"Bandit Gun Belt", "Bandit", 10, 1950},
    {"Weapons Expert Gun Belt", "Weapons Expert", 3, 1525},
    {"Explorer Gun Belt", "Explorer", 7, 3125},
    {"Herbalist Gun Belt", "Herbalist", 3, 2725},
    {"Horseman Gun Belt", "Horseman", 1, 1600},
    {"Master Hunter Gun Belt", "Master Hunter", 7, 2200},
    {"Survivalist Gun Belt", "Survivalist", 1, 2450},
    {"Gambler Gun Belt", "Gambler", 7, 1875},
    {"Sharpshooter Gun Belt", "Sharpshooter", 10, 1800},
    // Holsters
    {"Bandit Holster", "Bandit", 3, 1600},
    {"Weapons Expert Holster", "Weapons Expert", 1, 1225},
    {"Explorer Holster", "Explorer", 1, 2525},
    {"Herbalist Holster", "Herbalist", 7, 2200},
    {"Horseman Holster", "Horseman", 3, 1300},
    {"Master Hunter Holster", "Master Hunter", 10, 1800},
    {"Survivalist Holster", "Survivalist", 10, 1975},
    {"Gambler Holster", "Gambler", 1, 1525},
    {"Sharpshooter Holster", "Sharpshooter", 3, 1475},
    // Off-hand holsters
    {"Bandit Off-Hand Holster", "Bandit", 7, 1600},
    {"Weapons Expert Off-Hand Holster", "Weapons Expert", 7, 1225},
    {"Explorer Off-Hand Holster", "Explorer", 10, 2525},
    {"Herbalist Off-Hand Holster", "Herbalist", 1, 2200},
    {"Horseman Off-Hand Holster", "Horseman", 10, 1300},
    {"Master Hunter Off-Hand Holster", "Master Hunter", 1, 1800},
    {"Survivalist Off-Hand Holster", "Survivalist", 10, 1975},
    {"Gambler Off-Hand Holster", "Gambler", 10, 1525},
    {"Sharpshooter Off-Hand Holster", "Sharpshooter", 7, 1475},
};
static_assert(std::size(kReinforcedEquipment) == 36, "Expected 36 reinforced equipment rewards");

constexpr bool same_text(const char* left, const char* right) noexcept {
    while (*left != '\0' && *right != '\0') {
        if (*left != *right) return false;
        ++left;
        ++right;
    }
    return *left == *right;
}

constexpr std::size_t reinforced_reward_count(const char* challengeName) noexcept {
    std::size_t count = 0;
    for (const auto& item : kReinforcedEquipment) {
        if (same_text(item.challengeName, challengeName)) ++count;
    }
    return count;
}

static_assert(reinforced_reward_count("Bandit") == 4 &&
              reinforced_reward_count("Explorer") == 4 &&
              reinforced_reward_count("Gambler") == 4 &&
              reinforced_reward_count("Herbalist") == 4 &&
              reinforced_reward_count("Horseman") == 4 &&
              reinforced_reward_count("Master Hunter") == 4 &&
              reinforced_reward_count("Sharpshooter") == 4 &&
              reinforced_reward_count("Survivalist") == 4 &&
              reinforced_reward_count("Weapons Expert") == 4,
              "Each challenge category must own exactly four reinforced rewards");

const ReinforcedEquipmentInfo* find_reinforced_equipment(const char* name) noexcept {
    if (name == nullptr) return nullptr;
    for (const auto& item : kReinforcedEquipment) {
        if (std::strcmp(name, item.itemName) == 0) return &item;
    }
    return nullptr;
}

const RecipeEntry* find_trapper_recipe_by_name(const char* name) noexcept {
    if (name == nullptr) return nullptr;
    const VendorState& vendor = g_tracker.trapper;
    for (std::size_t i = 0; i < vendor.recipeCount; ++i) {
        if (std::strcmp(vendor.recipes[i].name, name) == 0) return &vendor.recipes[i];
    }
    return nullptr;
}

int find_trapper_recipe_index_by_name(const char* name) noexcept {
    if (name == nullptr) return -1;
    const VendorState& vendor = g_tracker.trapper;
    for (std::size_t i = 0; i < vendor.recipeCount; ++i) {
        if (std::strcmp(vendor.recipes[i].name, name) == 0) return static_cast<int>(i);
    }
    return -1;
}

const ChallengeDefinition* find_challenge_definition(const char* challengeName) noexcept {
    if (challengeName == nullptr) return nullptr;
    for (const auto& definition : kChallengeDefinitions) {
        if (std::strcmp(challengeName, definition.name) == 0) return &definition;
    }
    return nullptr;
}

const ChallengeProgress* find_challenge_progress(const char* challengeName) noexcept {
    if (challengeName == nullptr) return nullptr;
    for (std::size_t i = 0; i < kChallengeDefinitions.size(); ++i) {
        if (std::strcmp(challengeName, kChallengeDefinitions[i].name) == 0) {
            return &g_tracker.challenges[i];
        }
    }
    return nullptr;
}

const ReinforcedEquipmentInfo* reinforced_equipment_for_challenge(
    std::size_t challengeIndex, int row) noexcept {
    if (challengeIndex >= kChallengeDefinitions.size() || row < 0) return nullptr;
    const char* challengeName = kChallengeDefinitions[challengeIndex].name;
    int match = 0;
    for (const auto& item : kReinforcedEquipment) {
        if (std::strcmp(item.challengeName, challengeName) != 0) continue;
        if (match == row) return &item;
        ++match;
    }
    return nullptr;
}

int reinforced_equipment_count_for_challenge(std::size_t challengeIndex) noexcept {
    if (challengeIndex >= kChallengeDefinitions.size()) return 0;
    int count = 0;
    const char* challengeName = kChallengeDefinitions[challengeIndex].name;
    for (const auto& item : kReinforcedEquipment) {
        if (std::strcmp(item.challengeName, challengeName) == 0) ++count;
    }
    return count;
}

void garment_set_crafted_count(const GarmentSetDefinition& set,
                               int& crafted, int& total) noexcept {
    crafted = 0;
    total = static_cast<int>(set.componentCount);
    for (std::size_t i = 0; i < set.componentCount; ++i) {
        const RecipeEntry* component = find_trapper_recipe_by_name(set.components[i]);
        if (component != nullptr && component->owned) ++crafted;
    }
}

struct DetailLine {
    const char* componentName{};
    const RecipeEntry* component{};
    const RecipeCost* cost{};
    bool heading{};
    bool unavailable{};
};

std::size_t build_detail_lines(const RecipeEntry* recipe, DetailLine* lines,
                               std::size_t capacity) noexcept {
    if (recipe == nullptr || lines == nullptr || capacity == 0) return 0;
    std::size_t count = 0;
    const GarmentSetDefinition* set = selected_garment_set();
    if (set == nullptr) {
        for (std::size_t i = 0; i < recipe->costCount && count < capacity; ++i) {
            const RecipeCost& cost = recipe->costs[i];
            if (cost.hash != kMoneyHash) lines[count++] = DetailLine{nullptr, recipe, &cost, false, false};
        }
        return count;
    }

    for (std::size_t piece = 0; piece < set->componentCount && count < capacity; ++piece) {
        const char* componentName = set->components[piece];
        const RecipeEntry* component = find_trapper_recipe_by_name(componentName);
        lines[count++] = DetailLine{componentName, component, nullptr, true, component == nullptr};
        if (component == nullptr) {
            if (count < capacity) lines[count++] = DetailLine{"Catalog recipe not found", nullptr, nullptr, false, true};
            continue;
        }
        int materialCount = 0;
        for (std::size_t c = 0; c < component->costCount; ++c) {
            const RecipeCost& cost = component->costs[c];
            if (cost.hash == kMoneyHash) continue;
            ++materialCount;
            if (count < capacity) lines[count++] = DetailLine{componentName, component, &cost, false, false};
        }
        if (materialCount == 0 && count < capacity) {
            lines[count++] = DetailLine{"No animal-material cost exposed for this piece", component, nullptr, false, true};
        }
    }
    return count;
}

const MaterialNeed* find_vendor_material(const VendorDemand& demand, std::uint32_t hash) noexcept {
    for (std::size_t i = 0; i < demand.count; ++i) {
        if (demand.materials[i].hash == hash) return &demand.materials[i];
    }
    return nullptr;
}

bool recipe_materials_ready(const VendorState& vendor, const RecipeEntry& recipe) noexcept {
    if (recipe.owned || !recipe.hasAnyCosts || !vendor.demand.ready) return false;
    bool hasAnimalMaterials = false;
    for (std::size_t i = 0; i < recipe.costCount; ++i) {
        const RecipeCost& cost = recipe.costs[i];
        // Cash and all other non-animal costs are intentionally ignored.
        if (!is_animal_part_material(cost.hash)) continue;
        hasAnimalMaterials = true;
        const MaterialNeed* stock = find_vendor_material(vendor.demand, cost.hash);
        if (stock == nullptr || stock->vendorStock < cost.amount) return false;
    }
    return hasAnimalMaterials;
}

bool garment_set_craftable_now(const VendorState& vendor,
                               const GarmentSetDefinition& set) noexcept {
    std::array<RecipeCost, kMaxMaterials> required{};
    std::size_t requiredCount = 0;
    bool hasUnownedPiece = false;

    for (std::size_t piece = 0; piece < set.componentCount; ++piece) {
        const RecipeEntry* component = find_trapper_recipe_by_name(set.components[piece]);
        if (component == nullptr) return false;
        if (component->owned) continue;
        hasUnownedPiece = true;
        if (!component->hasAnyCosts || component->costCount == 0) return false;

        for (std::size_t costIndex = 0; costIndex < component->costCount; ++costIndex) {
            const RecipeCost& cost = component->costs[costIndex];
            // Aggregate only animal parts; cash and other costs do not affect
            // this material-readiness indicator.
            if (!is_animal_part_material(cost.hash)) continue;

            std::size_t existing = 0;
            while (existing < requiredCount && required[existing].hash != cost.hash) ++existing;
            if (existing < requiredCount) {
                const long long sum = static_cast<long long>(required[existing].amount) + cost.amount;
                required[existing].amount = static_cast<int>(std::min<long long>(sum, 1000000));
            } else {
                if (requiredCount >= required.size()) return false;
                required[requiredCount++] = cost;
            }
        }
    }

    if (!hasUnownedPiece || requiredCount == 0) return false;
    // Garment components draw from shared Trapper stock. Aggregate repeated
    // materials across every remaining piece before comparing the pool.
    for (std::size_t i = 0; i < requiredCount; ++i) {
        const MaterialNeed* stock = find_vendor_material(vendor.demand, required[i].hash);
        if (stock == nullptr || stock->vendorStock < required[i].amount) return false;
    }
    return true;
}

bool recipe_craftable_now(const VendorState& vendor, const RecipeEntry& recipe) noexcept {
    if (recipe.owned || !vendor.demand.ready) return false;

    if (g_menu.trapper && recipe.categoryHash == kTrapperGarmentSetsCategoryHash) {
        const GarmentSetDefinition* set = find_garment_set_definition(recipe.name);
        return set != nullptr && garment_set_craftable_now(vendor, *set);
    }

    // Reinforced equipment is challenge-unlocked and purchased, not crafted
    // from donated pelts; it is intentionally absent from this filter.
    if (g_menu.trapper && find_reinforced_equipment(recipe.name) != nullptr) return false;
    return recipe_materials_ready(vendor, recipe);
}

bool recipe_uses_current_haul(const RecipeEntry& recipe) noexcept {
    if (recipe.owned || !recipe.hasAnyCosts) return false;
    for (std::size_t i = 0; i < recipe.costCount; ++i) {
        const RecipeCost& cost = recipe.costs[i];
        if (is_animal_part_material(cost.hash) && current_haul_count(cost.hash) > 0) return true;
    }
    return false;
}

enum class CraftingUseReadiness {
    UsesHaul, NeedsOtherMaterials, AnimalPartsReady, ReadyAfterDonation
};

CraftingUseReadiness crafting_use_readiness(const VendorState& vendor,
                                             const RecipeEntry& recipe) noexcept {
    bool hasAnimalCost = false;
    bool animalCostsReady = true;
    bool otherCostsReady = true;
    bool hasCashCost = false;
    for (std::size_t i = 0; i < recipe.costCount; ++i) {
        const RecipeCost& cost = recipe.costs[i];
        if (is_animal_part_material(cost.hash)) {
            hasAnimalCost = true;
            const MaterialNeed* stock = find_vendor_material(vendor.demand, cost.hash);
            const int donated = stock != nullptr ? stock->vendorStock : 0;
            if (current_haul_count(cost.hash) + donated < cost.amount) animalCostsReady = false;
        } else if (cost.hash == kMoneyHash) {
            // Wallet cash is not represented by the inventory count native.
            hasCashCost = true;
        } else if (g_currentHaul.inventoryId < 0 ||
                   player_owned_count(g_currentHaul.inventoryId, cost.hash) < cost.amount) {
            otherCostsReady = false;
        }
    }
    if (!hasAnimalCost || !recipe.hasAnyCosts) return CraftingUseReadiness::NeedsOtherMaterials;
    if (!animalCostsReady) return CraftingUseReadiness::UsesHaul;
    if (!otherCostsReady) return CraftingUseReadiness::NeedsOtherMaterials;
    return hasCashCost ? CraftingUseReadiness::AnimalPartsReady
                       : CraftingUseReadiness::ReadyAfterDonation;
}

const RecipeEntry* crafting_use_recipe(const CraftingUseEntry& entry) noexcept {
    const VendorState& vendor = entry.trapper ? g_tracker.trapper : g_tracker.pearson;
    if (entry.recipeIndex < 0 ||
        static_cast<std::size_t>(entry.recipeIndex) >= vendor.recipeCount) return nullptr;
    return &vendor.recipes[static_cast<std::size_t>(entry.recipeIndex)];
}

const VendorState& crafting_use_vendor(const CraftingUseEntry& entry) noexcept {
    return entry.trapper ? g_tracker.trapper : g_tracker.pearson;
}

void append_crafting_use_entry(bool trapper, int recipeIndex, const char* setName) noexcept {
    const VendorState& vendor = trapper ? g_tracker.trapper : g_tracker.pearson;
    if (!vendor.demand.ready || recipeIndex < 0 ||
        static_cast<std::size_t>(recipeIndex) >= vendor.recipeCount) return;
    const RecipeEntry& recipe = vendor.recipes[static_cast<std::size_t>(recipeIndex)];
    if (!recipe_uses_current_haul(recipe)) return;
    for (std::size_t i = 0; i < g_craftingUseCount; ++i) {
        if (g_craftingUseEntries[i].trapper == trapper &&
            g_craftingUseEntries[i].recipeIndex == recipeIndex) return;
    }
    if (g_craftingUseCount < g_craftingUseEntries.size()) {
        g_craftingUseEntries[g_craftingUseCount++] =
            CraftingUseEntry{trapper, recipeIndex, setName};
    }
}

const char* crafting_use_label(const CraftingUseEntry& entry,
                               char* buffer, std::size_t size) noexcept {
    const RecipeEntry* recipe = crafting_use_recipe(entry);
    if (recipe == nullptr) return "";
    if (entry.setName != nullptr) {
        std::snprintf(buffer, size, "%s - %s - %s",
                      hunting_companion::ui_text("Trapper"), entry.setName,
                      recipe_display_name(*recipe));
    } else {
        std::snprintf(buffer, size, "%s - %s",
                      hunting_companion::ui_text(entry.trapper ? "Trapper" : "Pearson"),
                      recipe_display_name(*recipe));
    }
    return buffer;
}

void build_crafting_uses_list() noexcept {
    std::uint32_t previousHash = 0;
    bool previousTrapper = false;
    const bool hadSelection = g_menu.page == MenuPage::CraftingUses ||
                              g_menu.page == MenuPage::CraftingUseDetail;
    if (hadSelection) {
        const int selectedIndex = g_menu.page == MenuPage::CraftingUseDetail
            ? g_menu.craftingUseIndex : g_menu.selection;
        if (selectedIndex >= 0 && static_cast<std::size_t>(selectedIndex) < g_craftingUseCount) {
            const CraftingUseEntry& previous = g_craftingUseEntries[static_cast<std::size_t>(selectedIndex)];
            const RecipeEntry* recipe = crafting_use_recipe(previous);
            if (recipe != nullptr) {
                previousHash = recipe->itemHash;
                previousTrapper = previous.trapper;
            }
        }
    }

    g_craftingUseCount = 0;
    if (!g_currentHaul.ready || !current_haul_has_any()) {
        if (g_menu.page == MenuPage::CraftingUseDetail) {
            g_menu.page = MenuPage::CraftingUses;
            g_menu.craftingUseIndex = -1;
            g_menu.selection = g_menu.scroll = 0;
        } else if (g_menu.page == MenuPage::CraftingUses) {
            g_menu.selection = g_menu.scroll = 0;
        }
        return;
    }

    for (std::size_t i = 0; i < g_tracker.pearson.recipeCount; ++i) {
        append_crafting_use_entry(false, static_cast<int>(i), nullptr);
    }

    // Trapper garment-set catalog rows are bundles. Show the individual
    // uncrafted garments instead, so a hat already made is never suggested.
    for (std::size_t i = 0; i < g_tracker.trapper.recipeCount; ++i) {
        const RecipeEntry& recipe = g_tracker.trapper.recipes[i];
        if (recipe.categoryHash == kTrapperGarmentSetsCategoryHash ||
            find_reinforced_equipment(recipe.name) != nullptr) continue;
        append_crafting_use_entry(true, static_cast<int>(i), nullptr);
    }
    for (const auto& set : kGarmentSets) {
        for (std::size_t piece = 0; piece < set.componentCount; ++piece) {
            const int recipeIndex = find_trapper_recipe_index_by_name(set.components[piece]);
            if (recipeIndex >= 0) append_crafting_use_entry(true, recipeIndex, set.setName);
        }
    }

    // Stable alphabetical ordering keeps the same recipe grouping predictable.
    for (std::size_t i = 1; i < g_craftingUseCount; ++i) {
        const CraftingUseEntry value = g_craftingUseEntries[i];
        char valueLabel[160]{};
        crafting_use_label(value, valueLabel, sizeof(valueLabel));
        std::size_t j = i;
        while (j > 0) {
            char previousLabel[160]{};
            crafting_use_label(g_craftingUseEntries[j - 1], previousLabel, sizeof(previousLabel));
            if (std::strcmp(previousLabel, valueLabel) <= 0) break;
            g_craftingUseEntries[j] = g_craftingUseEntries[j - 1];
            --j;
        }
        g_craftingUseEntries[j] = value;
    }

    int restored = -1;
    if (previousHash != 0) {
        for (std::size_t i = 0; i < g_craftingUseCount; ++i) {
            const CraftingUseEntry& entry = g_craftingUseEntries[i];
            const RecipeEntry* recipe = crafting_use_recipe(entry);
            if (entry.trapper == previousTrapper && recipe != nullptr &&
                recipe->itemHash == previousHash) {
                restored = static_cast<int>(i);
                break;
            }
        }
    }
    for (std::size_t i = 0; i < g_craftingUseCount; ++i) {
        const CraftingUseEntry& entry = g_craftingUseEntries[i];
        const RecipeEntry* recipe = crafting_use_recipe(entry);
        g_craftingUseEntries[i].readiness = recipe != nullptr
            ? static_cast<int>(crafting_use_readiness(crafting_use_vendor(entry), *recipe))
            : static_cast<int>(CraftingUseReadiness::UsesHaul);
    }

    if (g_menu.page == MenuPage::CraftingUseDetail) {
        if (restored >= 0) g_menu.craftingUseIndex = restored;
        else {
            g_menu.page = MenuPage::CraftingUses;
            g_menu.craftingUseIndex = -1;
            g_menu.selection = 0;
            g_menu.scroll = 0;
        }
    } else if (g_menu.page == MenuPage::CraftingUses) {
        g_menu.selection = restored >= 0 ? restored :
            std::clamp(g_menu.selection, 0, std::max(0, static_cast<int>(g_craftingUseCount) - 1));
        if (g_craftingUseCount == 0) g_menu.selection = g_menu.scroll = 0;
        else {
            if (g_menu.selection < g_menu.scroll) g_menu.scroll = g_menu.selection;
            if (g_menu.selection >= g_menu.scroll + kVisibleMenuRows)
                g_menu.scroll = g_menu.selection - kVisibleMenuRows + 1;
        }
    }
}

CraftingUseReadiness selected_crafting_use_readiness() noexcept {
    if (g_menu.craftingUseIndex < 0 ||
        static_cast<std::size_t>(g_menu.craftingUseIndex) >= g_craftingUseCount)
        return CraftingUseReadiness::UsesHaul;
    const CraftingUseEntry& entry = g_craftingUseEntries[static_cast<std::size_t>(g_menu.craftingUseIndex)];
    const RecipeEntry* recipe = crafting_use_recipe(entry);
    return recipe != nullptr
        ? crafting_use_readiness(crafting_use_vendor(entry), *recipe)
        : CraftingUseReadiness::UsesHaul;
}

const char* crafting_use_readiness_label(CraftingUseReadiness readiness) noexcept {
    switch (readiness) {
        case CraftingUseReadiness::ReadyAfterDonation: return "READY AFTER DONATION";
        case CraftingUseReadiness::AnimalPartsReady: return "ANIMAL PARTS READY";
        case CraftingUseReadiness::NeedsOtherMaterials: return "OTHER COSTS NEEDED";
        case CraftingUseReadiness::UsesHaul: return "USES YOUR HAUL";
    }
    return "USES YOUR HAUL";
}

void toggle_craftable_filter() noexcept {
    const MenuPage previousPage = g_menu.page;
    const int previousSelection = g_menu.selection;
    std::uint32_t previousCategoryHash = g_menu.categoryHash;
    std::uint32_t previousRecipeHash = 0;

    if (previousPage == MenuPage::Categories &&
        previousSelection >= 0 &&
        static_cast<std::size_t>(previousSelection) < g_menuCategoryCount) {
        previousCategoryHash = g_menuCategories[static_cast<std::size_t>(previousSelection)].hash;
    } else if (previousPage == MenuPage::Recipes &&
               previousSelection >= 0 &&
               static_cast<std::size_t>(previousSelection) < g_menuRecipeCount) {
        const VendorState& vendor = active_vendor();
        const int recipeIndex = g_menuRecipeIndices[static_cast<std::size_t>(previousSelection)];
        previousRecipeHash = vendor.recipes[static_cast<std::size_t>(recipeIndex)].itemHash;
    }

    g_menu.craftableNow = !g_menu.craftableNow;
    build_menu_categories();

    auto find_category = [&]() noexcept -> int {
        for (std::size_t i = 0; i < g_menuCategoryCount; ++i) {
            if (g_menuCategories[i].hash == previousCategoryHash) return static_cast<int>(i);
        }
        return -1;
    };
    auto restore_scroll = [](int& selection, int& scroll, int count) noexcept {
        if (count <= 0) { selection = 0; scroll = 0; return; }
        selection = std::clamp(selection, 0, count - 1);
        if (selection < scroll) scroll = selection;
        if (selection >= scroll + kVisibleMenuRows) scroll = selection - kVisibleMenuRows + 1;
        scroll = std::clamp(scroll, 0, std::max(0, count - kVisibleMenuRows));
    };

    if (previousPage == MenuPage::Categories) {
        const int restored = find_category();
        g_menu.selection = restored >= 0 ? restored : previousSelection;
        restore_scroll(g_menu.selection, g_menu.scroll, static_cast<int>(g_menuCategoryCount));
        return;
    }

    build_menu_recipe_list();
    if (g_menuRecipeCount == 0) {
        g_menu.page = MenuPage::Categories;
        const int restored = find_category();
        g_menu.selection = restored >= 0 ? restored : 0;
        restore_scroll(g_menu.selection, g_menu.scroll, static_cast<int>(g_menuCategoryCount));
        return;
    }

    int restoredRecipe = -1;
    if (previousRecipeHash != 0) {
        const VendorState& vendor = active_vendor();
        for (std::size_t i = 0; i < g_menuRecipeCount; ++i) {
            const int recipeIndex = g_menuRecipeIndices[i];
            if (vendor.recipes[static_cast<std::size_t>(recipeIndex)].itemHash == previousRecipeHash) {
                restoredRecipe = static_cast<int>(i);
                break;
            }
        }
    }
    g_menu.selection = restoredRecipe >= 0 ? restoredRecipe : previousSelection;
    restore_scroll(g_menu.selection, g_menu.scroll, static_cast<int>(g_menuRecipeCount));

    const int categoryIndex = find_category();
    if (categoryIndex >= 0) {
        g_menu.categoryReturnSelection = categoryIndex;
        restore_scroll(g_menu.categoryReturnSelection, g_menu.categoryReturnScroll,
                       static_cast<int>(g_menuCategoryCount));
    }
}

bool root_item_disabled(int index) noexcept;

void move_selection(int delta, int count) noexcept {
    if (count <= 0) { g_menu.selection = 0; g_menu.scroll = 0; return; }
    g_menu.selection = std::clamp(g_menu.selection + delta, 0, count - 1);
    if (g_menu.selection < g_menu.scroll) g_menu.scroll = g_menu.selection;
    if (g_menu.selection >= g_menu.scroll + kVisibleMenuRows) {
        g_menu.scroll = g_menu.selection - kVisibleMenuRows + 1;
    }
}
void move_root_selection(int delta) noexcept {
    constexpr int kRootCount = 6;
    int candidate = g_menu.selection;
    for (int attempt = 0; attempt < kRootCount; ++attempt) {
        candidate = std::clamp(candidate + delta, 0, kRootCount - 1);
        if (!root_item_disabled(candidate)) { g_menu.selection = candidate; break; }
        if (candidate == 0 || candidate == kRootCount - 1) break;
    }
}

void open_menu() noexcept {
    g_menu = {};
    g_menu.page = MenuPage::Root;
    g_currentHaul.lastRefreshTick = 0;
    InterlockedExchange(&g_haulDirty, 1);
    // Vendor data refreshes on open; the separate haul snapshot polls while the menu stays open.
    InterlockedExchange(&g_refreshRequested, 1);
}

void go_back() noexcept {
    switch (g_menu.page) {
        case MenuPage::Root:
            g_menu.page = MenuPage::Closed;
            g_menu.selection = g_menu.scroll = 0;
            break;
        case MenuPage::Categories:
            g_menu.page = MenuPage::Root;
            g_menu.selection = g_menu.rootReturnSelection;
            g_menu.scroll = 0;
            break;
        case MenuPage::Recipes:
            g_menu.page = MenuPage::Categories;
            g_menu.selection = g_menu.categoryReturnSelection;
            g_menu.scroll = g_menu.categoryReturnScroll;
            break;
        case MenuPage::Detail:
            g_menu.page = g_menu.detailReturnPage;
            g_menu.selection = g_menu.recipeReturnSelection;
            g_menu.scroll = g_menu.recipeReturnScroll;
            break;
        case MenuPage::CraftingUses:
            g_menu.page = MenuPage::Root;
            g_menu.selection = 2;
            g_menu.scroll = 0;
            break;
        case MenuPage::CraftingUseDetail:
            g_menu.page = MenuPage::CraftingUses;
            g_menu.selection = g_menu.craftingUseIndex;
            g_menu.scroll = std::max(0, g_menu.selection - kVisibleMenuRows + 1);
            break;
        case MenuPage::Challenges:
            g_menu.page = MenuPage::Root;
            g_menu.selection = 3;
            g_menu.scroll = 0;
            break;
        case MenuPage::ChallengeDetail:
            g_menu.page = MenuPage::Challenges;
            g_menu.selection = g_menu.challengeReturnSelection;
            g_menu.scroll = g_menu.challengeReturnScroll;
            break;
        case MenuPage::Settings:
            g_menu.page = MenuPage::Root;
            g_menu.selection = 4;
            g_menu.scroll = 0;
            break;
        case MenuPage::Closed:
            break;
    }
}

int current_page_count() noexcept {
    switch (g_menu.page) {
        case MenuPage::Root: return 6;
        case MenuPage::Categories: return static_cast<int>(g_menuCategoryCount);
        case MenuPage::Recipes: return static_cast<int>(g_menuRecipeCount);
        case MenuPage::Detail: {
            const RecipeEntry* recipe = selected_recipe();
            DetailLine lines[64]{};
            return static_cast<int>(build_detail_lines(recipe, lines, std::size(lines)));
        }
        case MenuPage::CraftingUses: return static_cast<int>(g_craftingUseCount);
        case MenuPage::CraftingUseDetail: {
            if (g_menu.craftingUseIndex < 0 ||
                static_cast<std::size_t>(g_menu.craftingUseIndex) >= g_craftingUseCount) return 0;
            const RecipeEntry* recipe = crafting_use_recipe(g_craftingUseEntries[static_cast<std::size_t>(g_menu.craftingUseIndex)]);
            return recipe != nullptr ? static_cast<int>(recipe->costCount) : 0;
        }
        case MenuPage::Challenges: return static_cast<int>(kChallengeCount);
        case MenuPage::ChallengeDetail:
            return reinforced_equipment_count_for_challenge(
                static_cast<std::size_t>(g_menu.challengeIndex));
        case MenuPage::Settings: return 7;
        case MenuPage::Closed: return 0;
    }
    return 0;
}

void update_menu_input() noexcept {
    update_keyboard_edges();
    if (!g_menu.captureHotkey && !g_menu.captureFilterHotkey && !g_menu.captureIconsHotkey &&
        key_pressed(g_iconsHotkey) && g_iconsHotkey != g_menuHotkey) {
        g_showWeaponIcons = !g_showWeaponIcons;
        save_icons_settings();
    }
    const bool upRepeated = key_repeated(VK_UP);
    const bool downRepeated = key_repeated(VK_DOWN);
    if (g_menu.page != MenuPage::Closed && !g_tracker.ready) {
        // Keep the first-open loading state stable. Users can still close the
        // menu, but cannot enter empty categories before the first snapshot.
        if (key_pressed(g_menuHotkey) || key_pressed(VK_ESCAPE) || key_pressed(VK_BACK)) {
            g_menu.page = MenuPage::Closed;
        }
        return;
    }

    if (g_menu.captureIconsHotkey) {
        if (key_pressed(VK_ESCAPE)) {
            g_menu.captureIconsHotkey = false;
            return;
        }
        for (unsigned key = 1; key < 256; ++key) {
            if (!key_pressed(key)) continue;
            if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU ||
                key == VK_LSHIFT || key == VK_RSHIFT || key == VK_LCONTROL ||
                key == VK_RCONTROL || key == VK_LMENU || key == VK_RMENU ||
                key == g_menuHotkey) continue;
            g_iconsHotkey = key;
            save_icons_settings();
            g_menu.captureIconsHotkey = false;
            break;
        }
        return;
    }

    if (g_menu.captureFilterHotkey) {
        if (key_pressed(VK_ESCAPE)) {
            g_menu.captureFilterHotkey = false;
            return;
        }
        for (unsigned key = 1; key < 256; ++key) {
            if (!key_pressed(key)) continue;
            if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU ||
                key == VK_LSHIFT || key == VK_RSHIFT || key == VK_LCONTROL ||
                key == VK_RCONTROL || key == VK_LMENU || key == VK_RMENU ||
                key == g_menuHotkey) continue;
            g_filterHotkey = key;
            save_filter_hotkey();
            g_menu.captureFilterHotkey = false;
            break;
        }
        return;
    }

    if (g_menu.captureHotkey) {
        if (key_pressed(VK_ESCAPE)) {
            g_menu.captureHotkey = false;
            return;
        }
        for (unsigned key = 1; key < 256; ++key) {
            if (!key_pressed(key)) continue;
            if (key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU ||
                key == VK_LSHIFT || key == VK_RSHIFT || key == VK_LCONTROL ||
                key == VK_RCONTROL || key == VK_LMENU || key == VK_RMENU) continue;
            g_menuHotkey = key;
            save_menu_hotkey();
            g_menu.captureHotkey = false;
            break;
        }
        return;
    }

    if (g_menu.page == MenuPage::Closed) {
        if (key_pressed(g_menuHotkey)) open_menu();
        return;
    }
    if (g_menu.page == MenuPage::Root && root_item_disabled(2) && g_menu.selection == 2)
        g_menu.selection = 3;
    if (key_pressed(g_menuHotkey)) {
        g_menu.page = MenuPage::Closed;
        return;
    }
    if (key_pressed(VK_ESCAPE) || key_pressed(VK_BACK)) {
        go_back();
        return;
    }
    if ((g_menu.page == MenuPage::Categories || g_menu.page == MenuPage::Recipes) &&
        key_pressed(g_filterHotkey)) {
        toggle_craftable_filter();
        return;
    }
    if (g_menu.page == MenuPage::Detail || g_menu.page == MenuPage::CraftingUseDetail) {
        const int count = current_page_count();
        const int delta = upRepeated ? -1 : (downRepeated ? 1 :
                          (key_pressed(VK_PRIOR) ? -8 : (key_pressed(VK_NEXT) ? 8 : 0)));
        if (delta != 0 && count > 0) {
            g_menu.selection = std::clamp(g_menu.selection + delta, 0, count - 1);
            constexpr int kDetailRows = 8;
            if (g_menu.selection < g_menu.scroll) g_menu.scroll = g_menu.selection;
            if (g_menu.selection >= g_menu.scroll + kDetailRows) {
                g_menu.scroll = g_menu.selection - kDetailRows + 1;
            }
        }
    } else if (g_menu.page == MenuPage::Root) {
        if (upRepeated) move_root_selection(-1);
        if (downRepeated) move_root_selection(1);
        if (key_pressed(VK_PRIOR)) move_root_selection(-kVisibleMenuRows);
        if (key_pressed(VK_NEXT)) move_root_selection(kVisibleMenuRows);
    } else {
        if (upRepeated) move_selection(-1, current_page_count());
        if (downRepeated) move_selection(1, current_page_count());
        if (key_pressed(VK_PRIOR)) move_selection(-kVisibleMenuRows, current_page_count());
        if (key_pressed(VK_NEXT)) move_selection(kVisibleMenuRows, current_page_count());
    }

    if (g_menu.page == MenuPage::Settings && g_menu.selection == 3) {
        if (key_pressed(VK_LEFT) || key_pressed(VK_RIGHT) || key_pressed(VK_RETURN)) {
            g_showWeaponIcons = !g_showWeaponIcons;
            save_icons_settings();
            return;
        }
    }
    if (g_menu.page == MenuPage::Settings && g_menu.selection == 5) {
        if (key_pressed(VK_LEFT)) { change_menu_font(-1); return; }
        if (key_pressed(VK_RIGHT) || key_pressed(VK_RETURN)) { change_menu_font(1); return; }
    }
    if (g_menu.page == MenuPage::Settings && g_menu.selection == 1) {
        if (key_pressed(VK_LEFT)) { change_ui_language(-1); return; }
        if (key_pressed(VK_RIGHT) || key_pressed(VK_RETURN)) { change_ui_language(1); return; }
    }

    if (!key_pressed(VK_RETURN)) return;
    switch (g_menu.page) {
        case MenuPage::Root:
            if (g_menu.selection == 0 || g_menu.selection == 1) {
                g_menu.trapper = (g_menu.selection == 1);
                g_menu.rootReturnSelection = g_menu.selection;
                build_menu_categories();
                g_menu.page = MenuPage::Categories;
                g_menu.selection = g_menu.scroll = 0;
            } else if (g_menu.selection == 2 && !root_item_disabled(2)) {
                build_crafting_uses_list();
                g_menu.page = MenuPage::CraftingUses;
                g_menu.selection = g_menu.scroll = 0;
            } else if (g_menu.selection == 3) {
                g_menu.page = MenuPage::Challenges;
                g_menu.selection = g_menu.scroll = 0;
            } else if (g_menu.selection == 4) {
                g_menu.page = MenuPage::Settings;
                g_menu.selection = g_menu.scroll = 0;
            } else {
                g_menu.page = MenuPage::Closed;
            }
            break;
        case MenuPage::Categories:
            if (g_menu.selection >= 0 && static_cast<std::size_t>(g_menu.selection) < g_menuCategoryCount) {
                g_menu.categoryReturnSelection = g_menu.selection;
                g_menu.categoryReturnScroll = g_menu.scroll;
                g_menu.categoryHash = g_menuCategories[static_cast<std::size_t>(g_menu.selection)].hash;
                build_menu_recipe_list();
                g_menu.page = MenuPage::Recipes;
                g_menu.selection = g_menu.scroll = 0;
            }
            break;
        case MenuPage::Recipes:
            if (g_menu.selection >= 0 && static_cast<std::size_t>(g_menu.selection) < g_menuRecipeCount) {
                g_menu.recipeReturnSelection = g_menu.selection;
                g_menu.recipeReturnScroll = g_menu.scroll;
                g_menu.detailReturnPage = MenuPage::Recipes;
                g_menu.recipeIndex = g_menuRecipeIndices[static_cast<std::size_t>(g_menu.selection)];
                g_menu.page = MenuPage::Detail;
                g_menu.selection = g_menu.scroll = 0;
            }
            break;
        case MenuPage::Detail: break;
        case MenuPage::CraftingUses:
            if (g_menu.selection >= 0 &&
                static_cast<std::size_t>(g_menu.selection) < g_craftingUseCount) {
                g_menu.craftingUseIndex = g_menu.selection;
                g_menu.page = MenuPage::CraftingUseDetail;
                g_menu.selection = g_menu.scroll = 0;
            }
            break;
        case MenuPage::CraftingUseDetail: break;
        case MenuPage::Challenges:
            if (g_menu.selection >= 0 &&
                static_cast<std::size_t>(g_menu.selection) < kChallengeCount) {
                g_menu.challengeReturnSelection = g_menu.selection;
                g_menu.challengeReturnScroll = g_menu.scroll;
                g_menu.challengeIndex = g_menu.selection;
                g_menu.page = MenuPage::ChallengeDetail;
                g_menu.selection = g_menu.scroll = 0;
            }
            break;
        case MenuPage::ChallengeDetail: {
            const ReinforcedEquipmentInfo* item = reinforced_equipment_for_challenge(
                static_cast<std::size_t>(g_menu.challengeIndex), g_menu.selection);
            const int recipeIndex = item != nullptr
                ? find_trapper_recipe_index_by_name(item->itemName) : -1;
            if (recipeIndex >= 0) {
                g_menu.recipeReturnSelection = g_menu.selection;
                g_menu.recipeReturnScroll = g_menu.scroll;
                g_menu.detailReturnPage = MenuPage::ChallengeDetail;
                g_menu.recipeIndex = recipeIndex;
                g_menu.trapper = true;
                g_menu.page = MenuPage::Detail;
                g_menu.selection = g_menu.scroll = 0;
            }
            break;
        }
        case MenuPage::Settings:
            if (g_menu.selection == 0) {
                g_menu.captureHotkey = true;
            } else if (g_menu.selection == 2) {
                g_menu.captureFilterHotkey = true;
            } else if (g_menu.selection == 4) {
                g_menu.captureIconsHotkey = true;
            } else if (g_menu.selection == 6) {
                g_menu.page = MenuPage::Root;
                g_menu.selection = 4;
                g_menu.scroll = 0;
            }
            break;
        case MenuPage::Closed: break;
    }
}

void draw_rect(float x, float y, float width, float height,
               int red, int green, int blue, int alpha) noexcept {
    invoke_native<void>(kDrawRect, x, y, width, height,
                        red, green, blue, alpha, false, true);
}

float native_text_scale(float referencePixelHeight) noexcept {
    // _BG_SET_TEXT_SCALE expects the game's normalized text scale directly.
    // The scale is normalized to the game view and is independent of output
    // resolution, so it must not be multiplied by pixel dimensions.
    constexpr float kNativeScalePerReferencePixel = 0.01875f;
    return std::clamp(referencePixelHeight * kNativeScalePerReferencePixel,
                      0.14f, 0.465f);
}

// ---- 1.20 look -------------------------------------------------------------
// The menu is dressed with the game's own menu artwork. Every textured element
// has a plain-rectangle fallback, so the menu stays complete while the texture
// sets are still streaming in (or if one is ever missing).
namespace theme {
constexpr int kAccentRed = 168, kAccentGreen = 22, kAccentBlue = 22;   // selection red
constexpr int kGoldRed = 226, kGoldGreen = 188, kGoldBlue = 118;       // headings
constexpr int kTextRed = 236, kTextGreen = 230, kTextBlue = 218;       // body text
constexpr int kMutedRed = 168, kMutedGreen = 161, kMutedBlue = 146;    // hints
// Game texture sets and textures, and the title font face.
constexpr const char* kGeneric = "generic_textures";
constexpr const char* kMenu = "menu_textures";
constexpr const char* kSelectionBox = "selection_box_bg_1d";
constexpr const char* kChipBox = "selection_box_bg_1c";
constexpr const char* kInkPanel = "inkroller_1a";
constexpr const char* kHeaderPlate = "menu_header_1a";
constexpr const char* kDivider = "divider_line";
constexpr const char* kArrowUp = "scroller_arrow_top";
constexpr const char* kArrowDown = "scroller_arrow_bottom";
constexpr const char* kTitleFace = "$title1";
}

const char* g_fontFaceOverride = nullptr;
bool g_fontFaceForced = false;   // the override also applies on Asian-language games
struct FontFaceScope {
    explicit FontFaceScope(const char* face, bool forced = false) noexcept
        : previous(g_fontFaceOverride), previousForced(g_fontFaceForced) {
        g_fontFaceOverride = face;
        g_fontFaceForced = forced;
    }
    ~FontFaceScope() { g_fontFaceOverride = previous; g_fontFaceForced = previousForced; }
    FontFaceScope(const FontFaceScope&) = delete;
    FontFaceScope& operator=(const FontFaceScope&) = delete;
    const char* previous;
    bool previousForced;
};

// Plain Latin text can always use the game's own fonts, even while the rest of
// the interface is drawn by the CJK overlay.
bool g_forceNativeText = false;
struct NativeTextScope {
    explicit NativeTextScope(bool enable) noexcept : previous(g_forceNativeText) {
        g_forceNativeText = enable || previous;
    }
    ~NativeTextScope() { g_forceNativeText = previous; }
    NativeTextScope(const NativeTextScope&) = delete;
    NativeTextScope& operator=(const NativeTextScope&) = delete;
    bool previous;
};

bool menu_textures_ready() noexcept {
    bool ready = true;
    for (const char* dictionary : {theme::kGeneric, theme::kMenu}) {
        if (invoke_native<int>(kHasStreamedTextureDictLoaded, dictionary) == 0) {
            invoke_native<void>(kRequestStreamedTextureDict, dictionary, false);
            ready = false;
        }
    }
    return ready;
}

void draw_sprite(const char* dictionary, const char* name, float x, float y,
                 float width, float height,
                 int red, int green, int blue, int alpha) noexcept {
    invoke_native<void>(kDrawSprite, dictionary, name, x, y, width, height, 0.0f,
                        red, green, blue, alpha, false);
}

bool ascii_only(const char* text) noexcept {
    for (; *text != '\0'; ++text) {
        if (static_cast<unsigned char>(*text) >= 0x80) return false;
    }
    return true;
}

const char* selected_font_face() noexcept {
    // On a game running in an Asian language only the plain face has Latin
    // accents and normal Cyrillic, so it replaces every styled face there.
    // Plain ASCII (the brand line) is safe in any face, so it may insist.
    if (g_fontFaceOverride != nullptr && (g_fontFaceForced || !plain_face_needed()))
        return g_fontFaceOverride;
    if (effective_menu_font() != 0 && !plain_face_needed()) return kMenuFonts[effective_menu_font()].face;
    // The game's own catalogue font. Text in a CJK language that the game is not
    // itself running in cannot use native text (the game has not loaded those
    // glyphs); draw_text_impl() sends it to the CJK overlay instead. When the
    // companion language equals the game language the native font is correct.
    if (plain_face_needed()) {
        // Simplified Chinese maps the "standard" face to a Chinese font; its
        // "utility" face is the plain one there.
        return g_detectedGameLanguage == 12 ? "$util" : "$Font2";
    }
    return "$catalog1";
}

void draw_text_impl(const char* text, float x, float y, float referencePixelHeight,
                    int red, int green, int blue, int alpha,
                    bool localize, bool centered = false) noexcept {
    if (text == nullptr || text[0] == '\0') return;
    if (localize) text = hunting_companion::ui_text(text);

    // 1.20 look: text colours are drawn richer than specified. The game shows
    // tinted text rather flat, so the tint is pushed away from grey (white and
    // grey text are unaffected). Applies to native and overlay text alike.
    {
        constexpr float kVividness = 1.7f;
        const float luma = 0.299f * static_cast<float>(red) + 0.587f * static_cast<float>(green) +
                           0.114f * static_cast<float>(blue);
        const auto vivid = [luma](int channel) noexcept {
            const float value = luma + (static_cast<float>(channel) - luma) * kVividness;
            return static_cast<int>(std::clamp(value, 0.0f, 255.0f) + 0.5f);
        };
        red = vivid(red);
        green = vivid(green);
        blue = vivid(blue);
    }

    // Keep left-aligned menu text inside its panel: shrink it a little first,
    // then shorten it if it still would not fit.
    char fittedText[kFitLineCapacity * 2];
    if (g_textRightLimit > 0.0f && !centered) {
        const float correction = screen_width_correction();
        const float available = g_textRightLimit - x;
        const float width = text_width_estimate(text, referencePixelHeight, correction);
        if (available > 0.01f && width > available) {
            constexpr float kSmallestScale = 0.72f;
            const float scale = std::max(available / width, kSmallestScale);
            referencePixelHeight *= scale;
            if (width * scale > available) {
                shorten_to_width(text, referencePixelHeight, correction, available,
                                 fittedText, sizeof(fittedText));
                text = fittedText;
            }
        }
    }

    // CJK companion language that differs from the game language: the game has no
    // glyphs for it, so queue the text for the Vulkan/ImGui overlay (own embedded
    // fonts). Position, size and colour are passed through unchanged.
    if (!g_forceNativeText && needs_overlay(hunting_companion::active_ui_language())) {
        hunting_companion::overlay::submit(text, x, y, referencePixelHeight,
                                           red, green, blue, alpha, centered);
        return;
    }

    // Native path. Escape HTML metacharacters so a translated label cannot inject
    // Scaleform markup.
    char fontMarkup[8192]{};
    const char* fontFace = selected_font_face();
    char fontPrefix[96]{};
    std::snprintf(fontPrefix, sizeof(fontPrefix), "<FONT FACE='%s'>", fontFace);
    constexpr char kFontSuffix[] = "</FONT>";
    std::size_t used = std::strlen(fontPrefix);
    std::memcpy(fontMarkup, fontPrefix, used);
    bool markupFits = true;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        const char* replacement = nullptr;
        if (*cursor == '&') replacement = "&amp;";
        else if (*cursor == '<') replacement = "&lt;";
        else if (*cursor == '>') replacement = "&gt;";
        const std::size_t amount = replacement != nullptr ? std::strlen(replacement) : 1;
        if (used + amount + sizeof(kFontSuffix) > sizeof(fontMarkup)) {
            markupFits = false;
            break;
        }
        if (replacement != nullptr) {
            std::memcpy(fontMarkup + used, replacement, amount);
            used += amount;
        } else {
            fontMarkup[used++] = *cursor;
        }
    }
    if (markupFits) {
        std::memcpy(fontMarkup + used, kFontSuffix, sizeof(kFontSuffix));
    } else {
        // A very long body string is safer unwrapped than truncated.
        fontMarkup[0] = '\0';
    }
    const char* drawPayload = markupFits ? fontMarkup : text;
    const char* literal = invoke_native<const char*>(kVarString, 10,
                                                      "LITERAL_STRING", drawPayload);
    if (literal == nullptr) return;
    const char* gameString = invoke_native<const char*>(kVarString, 42,
                                                         "COLOR_STRING", 0, literal);
    if (gameString == nullptr) return;

    const float scale = native_text_scale(referencePixelHeight);
    invoke_native<void>(kBgSetTextScale, scale, scale);

    // Draw each label once; the translucent panel provides contrast.
    invoke_native<void>(kBgSetTextColor, red, green, blue, alpha);
    invoke_native<void>(kSetTextCentre, centered);
    invoke_native<void>(kBgDisplayText, gameString, x, y);
    invoke_native<void>(kSetTextCentre, false);
}

void draw_text(const char* text, float x, float y, float referencePixelHeight,
               int red = 232, int green = 226, int blue = 213, int alpha = 255) noexcept {
    draw_text_impl(text, x, y, referencePixelHeight, red, green, blue, alpha, true);
}

// Item/material labels are already localized by Rockstar's native string table
// or the selected official game-language table. Bypass phrase dictionaries.
void draw_text_raw(const char* text, float x, float y, float referencePixelHeight,
                   int red = 232, int green = 226, int blue = 213, int alpha = 255) noexcept {
    draw_text_impl(text, x, y, referencePixelHeight, red, green, blue, alpha, false);
}

// Word-wrap short reference copy to a fixed character budget and line count.
// This keeps challenge objectives inside the same right edge as the menu rows.
void draw_wrapped_text_impl(const char* text, float x, float y,
                            std::size_t maxCharacters, int maxLines,
                            float lineAdvance, float referencePixelHeight,
                            int red, int green, int blue, int alpha,
                            bool localize, bool centered = false) noexcept {
    if (text == nullptr || maxCharacters == 0 || maxLines <= 0) return;
    if (localize) text = hunting_companion::ui_text(text);
    const bool cjk = hunting_companion::uses_cjk_font(hunting_companion::active_ui_language());
    const std::size_t lineCharacters = cjk ? std::max<std::size_t>(8, maxCharacters / 2)
                                           : maxCharacters;
    const char* cursor = text;
    for (int lineIndex = 0; lineIndex < maxLines && *cursor != '\0'; ++lineIndex) {
        while (*cursor == ' ') ++cursor;
        if (*cursor == '\0') break;

        const std::size_t remaining = std::strlen(cursor);
        // Count UTF-8 code points, not bytes, so Japanese/Korean/Chinese text
        // is never split in the middle of a multi-byte character.
        std::size_t count = 0;
        std::size_t characters = 0;
        while (count < remaining && characters < lineCharacters) {
            const unsigned char lead = static_cast<unsigned char>(cursor[count]);
            std::size_t bytes = lead < 0x80 ? 1 :
                (lead & 0xE0) == 0xC0 ? 2 :
                (lead & 0xF0) == 0xE0 ? 3 :
                (lead & 0xF8) == 0xF0 ? 4 : 1;
            if (count + bytes > remaining) bytes = 1;
            count += bytes;
            ++characters;
        }
        if (count < remaining) {
            std::size_t breakAt = count;
            while (breakAt > 0 && cursor[breakAt] != ' ') {
                --breakAt;
            }
            if (breakAt > 0) count = breakAt;
            else if (!cjk) {
                // For Latin scripts, keep long unbroken tokens intact.
            }
            // CJK scripts do not use spaces between words; retaining the
            // codepoint boundary above is the correct wrap point.
        }

        char line[320]{};
        count = std::min(count, sizeof(line) - 1);
        std::memcpy(line, cursor, count);
        line[count] = '\0';
        const char* next = cursor + count;
        while (*next == ' ') ++next;
        const bool hasMore = (*next != '\0');
        if (hasMore && lineIndex == maxLines - 1) {
            constexpr char kEllipsis[] = "...";
            const std::size_t suffixLength = sizeof(kEllipsis) - 1;
            const std::size_t safeLength = std::min(count, sizeof(line) - suffixLength - 1);
            if (safeLength >= suffixLength) {
                std::memcpy(line + safeLength - suffixLength, kEllipsis, suffixLength);
                line[safeLength] = '\0';
            }
        }
        draw_text_impl(line, x, y + static_cast<float>(lineIndex) * lineAdvance,
                       referencePixelHeight, red, green, blue, alpha, localize, centered);
        cursor = next;
    }
}

void draw_wrapped_text(const char* text, float x, float y,
                       std::size_t maxCharacters, int maxLines,
                       float lineAdvance, float referencePixelHeight,
                       int red, int green, int blue, int alpha = 255) noexcept {
    draw_wrapped_text_impl(text, x, y, maxCharacters, maxLines, lineAdvance,
                           referencePixelHeight, red, green, blue, alpha, true);
}

void draw_wrapped_text_raw(const char* text, float x, float y,
                           std::size_t maxCharacters, int maxLines,
                           float lineAdvance, float referencePixelHeight,
                           int red, int green, int blue, int alpha = 255) noexcept {
    draw_wrapped_text_impl(text, x, y, maxCharacters, maxLines, lineAdvance,
                           referencePixelHeight, red, green, blue, alpha, false);
}

void draw_wrapped_centered_text(const char* text, float centerX, float y,
                                std::size_t maxCharacters, int maxLines,
                                float lineAdvance, float referencePixelHeight,
                                int red, int green, int blue, int alpha = 255) noexcept {
    draw_wrapped_text_impl(text, centerX, y, maxCharacters, maxLines, lineAdvance,
                           referencePixelHeight, red, green, blue, alpha, true, true);
}

// Shared horizontal bounds for rank bars on category and reinforced-item
// detail pages. In the 1.20 look rows have no plates, so the bar ends where the
// divider lines end, well inside the panel's rough painted edge.
constexpr float kMenuContentLeft = 0.071f;
constexpr float kMenuRewardRowRight = 0.212f + 0.345f * 0.5f;
constexpr float kMenuProgressBarWidth = kMenuRewardRowRight - kMenuContentLeft;

void draw_challenge_progress_bar(const ChallengeProgress& progress,
                                 float centerY) noexcept {
    if (!progress.available || progress.maxRanks <= 0) return;
    draw_rect(kMenuContentLeft + kMenuProgressBarWidth * 0.5f, centerY,
              kMenuProgressBarWidth, 0.014f, 54, 51, 47, 235);
    const float ratio = std::clamp(
        static_cast<float>(progress.ranksCompleted) /
            static_cast<float>(progress.maxRanks),
        0.0f, 1.0f);
    if (ratio > 0.0f) {
        const float fillWidth = kMenuProgressBarWidth * ratio;
        draw_rect(kMenuContentLeft + fillWidth * 0.5f, centerY,
                  fillWidth, 0.014f, 65, 166, 119, 250);
    }
}

void draw_menu_row(const char* label, const char* status, int row,
                   int selected, bool accent = false,
                   bool localizeLabel = true, bool disabled = false) noexcept {
    // Match the header title plate's exact horizontal bounds. Keep row height
    // compact so the list rows remain separate and do not overlap vertically.
    constexpr float kPanelCenter = 0.212f;
    constexpr float kPanelWidth = 0.365f;
    constexpr float kPanelLeft = kPanelCenter - kPanelWidth * 0.5f;
    constexpr float kRowStartY = 0.205f;
    constexpr float kRowHeight = 0.062f;
    const float y = kRowStartY + static_cast<float>(row) * kRowHeight;

    // Only the selected row carries a plate: the game's red selection box.
    // Other rows are plain text on the panel, separated by a hairline.
    (void)kPanelLeft;
    if (selected) {
        if (menu_textures_ready()) {
            draw_sprite(theme::kGeneric, theme::kSelectionBox, kPanelCenter, y + 0.020f,
                        kPanelWidth, 0.054f,
                        theme::kAccentRed, theme::kAccentGreen, theme::kAccentBlue, 240);
        } else {
            draw_rect(kPanelCenter, y + 0.020f, kPanelWidth, 0.052f,
                      theme::kAccentRed, theme::kAccentGreen, theme::kAccentBlue, 225);
        }
    } else {
        draw_rect(kPanelCenter, y + 0.0485f, kPanelWidth - 0.012f, 0.0012f, 255, 255, 255, 16);
    }
    const int labelRed = selected ? 255 : (disabled ? 104 : (accent ? theme::kGoldRed : theme::kTextRed));
    const int labelGreen = selected ? 255 : (disabled ? 104 : (accent ? theme::kGoldGreen : theme::kTextGreen));
    const int labelBlue = selected ? 255 : (disabled ? 104 : (accent ? theme::kGoldBlue : theme::kTextBlue));

    constexpr float kLabelLeft = 0.063f;
    constexpr float kRowRight = kPanelCenter + kPanelWidth * 0.5f - 0.007f;
    constexpr float kStatusGap = 0.010f;
    constexpr float kStatusSize = 13.0f;
    const float correction = screen_width_correction();

    // The status sits at the right of the row. It normally starts at a fixed
    // column, but moves left (and finally shrinks) when its text is longer.
    float labelRight = kRowRight;
    char statusText[160]{};
    if (status != nullptr && status[0] != '\0') {
        std::snprintf(statusText, sizeof(statusText), "%s", hunting_companion::ui_text(status));
        float statusSize = kStatusSize;
        float statusWidth = text_width_estimate(statusText, statusSize, correction);
        float statusX = std::min(0.315f, kRowRight - statusWidth);
        constexpr float kStatusLeftmost = 0.215f;
        if (statusX < kStatusLeftmost) {
            statusX = kStatusLeftmost;
            statusSize = std::max(kStatusSize * (kRowRight - kStatusLeftmost) / statusWidth, 9.5f);
            statusWidth = text_width_estimate(statusText, statusSize, correction);
            if (statusX + statusWidth > kRowRight) {
                char shortened[sizeof(statusText)];
                shorten_to_width(statusText, statusSize, correction, kRowRight - statusX,
                                 shortened, sizeof(shortened));
                std::snprintf(statusText, sizeof(statusText), "%s", shortened);
            }
        }
        draw_text_raw(statusText, statusX, y + 0.004f, statusSize,
                      selected ? 255 : (disabled ? 104 : theme::kGoldRed - 20),
                      selected ? 240 : (disabled ? 104 : theme::kGoldGreen - 20),
                      selected ? 225 : (disabled ? 104 : theme::kGoldBlue - 6));
        labelRight = statusX - kStatusGap;
    }

    // The label gets one large line when it fits, otherwise two smaller ones;
    // the size drops further before any text is cut.
    const char* labelText = localizeLabel ? hunting_companion::ui_text(label) : label;
    const float maxWidth = labelRight - kLabelLeft;
    const bool cjk = hunting_companion::uses_cjk_font(hunting_companion::active_ui_language());
    if (!cjk && text_width_estimate(labelText, 18.0f, correction) <= maxWidth) {
        draw_text_raw(labelText, kLabelLeft, y, 18.0f, labelRed, labelGreen, labelBlue);
        return;
    }
    if (cjk && text_width_estimate(labelText, 15.0f, correction) <= maxWidth) {
        draw_text_raw(labelText, kLabelLeft, y + 0.001f, 15.0f, labelRed, labelGreen, labelBlue);
        return;
    }
    FittedLines fitted;
    float size = 12.5f;
    while (!wrap_to_width(labelText, size, correction, maxWidth, 2, &fitted) && size > 9.5f) size -= 0.5f;
    for (int line = 0; line < fitted.count; ++line) {
        draw_text_raw(fitted.lines[line], kLabelLeft, y - 0.001f + static_cast<float>(line) * size * 0.00144f,
                      size, labelRed, labelGreen, labelBlue);
    }
}

void draw_menu_header(const char* title, const char* subtitle,
                      bool localizeTitle = true) noexcept {
    const bool textured = menu_textures_ready();
    if (textured) {
        // Ink-roller panel (rough painted edges) with the game's menu header plate.
        draw_rect(0.212f, 0.50f, 0.381f, 0.925f, 0, 0, 0, 150);
        draw_sprite(theme::kGeneric, theme::kInkPanel, 0.212f, 0.50f, 0.412f, 0.965f, 0, 0, 0, 238);
        draw_sprite(theme::kGeneric, theme::kHeaderPlate, 0.212f, 0.094f, 0.365f, 0.118f,
                    255, 255, 255, 255);
        draw_sprite(theme::kMenu, theme::kDivider, 0.212f, 0.1575f, 0.345f, 0.0042f,
                    theme::kGoldRed, theme::kGoldGreen, theme::kGoldBlue, 215);
    } else {
        draw_rect(0.212f, 0.50f, 0.395f, 0.94f, 0, 0, 0, 232);
        draw_rect(0.212f, 0.095f, 0.365f, 0.125f, 14, 12, 11, 250);
        draw_rect(0.212f, 0.157f, 0.34f, 0.0018f,
                  theme::kGoldRed, theme::kGoldGreen, theme::kGoldBlue, 200);
    }
    const bool cjkTitle = hunting_companion::uses_cjk_font(hunting_companion::active_ui_language());
    {
        // The brand line sits centred in the header frame, in the game's title
        // lettering. Plain Latin text can use that face in every language.
        const char* brand = hunting_companion::ui_text("HUNTING COMPANION");
        const bool plain = ascii_only(brand);
        const FontFaceScope titleFace(plain ? theme::kTitleFace : nullptr, plain);
        const NativeTextScope nativeText(plain);
        draw_text_impl(brand, 0.212f, 0.046f, 24.0f, 255, 255, 255, 255, false, true);
    }
    const char* titleText = localizeTitle ? hunting_companion::ui_text(title) : title;
    const bool filterChip = g_menu.page == MenuPage::Categories || g_menu.page == MenuPage::Recipes;
    const float titleRoom = (filterChip ? 0.258f : 0.3885f) - 0.071f;
    const float correction = screen_width_correction();
    if (cjkTitle) {
        if (text_width_estimate(titleText, 17.0f, correction) <= titleRoom) {
            draw_text_raw(titleText, 0.071f, 0.108f, 17.0f,
                          theme::kGoldRed, theme::kGoldGreen, theme::kGoldBlue);
        } else {
            draw_wrapped_text_raw(titleText, 0.071f, 0.107f, 36, 2, 0.018f, 12.5f,
                                  theme::kGoldRed, theme::kGoldGreen, theme::kGoldBlue);
        }
    } else {
        // Page titles share the title lettering (it is a condensed face, so
        // estimate it narrower than the catalogue font).
        const FontFaceScope titleFace(theme::kTitleFace);
        constexpr float kTitleFaceWidth = 0.80f;
        float size = 22.0f;
        const float width = text_width_estimate(titleText, size, correction) * kTitleFaceWidth /
                            kMenuFonts[effective_menu_font()].widthFactor;
        if (width > titleRoom) size = std::max(size * titleRoom / width, 13.0f);
        draw_text_raw(titleText, 0.071f, 0.109f + (22.0f - size) * 0.0007f, size,
                      theme::kGoldRed, theme::kGoldGreen, theme::kGoldBlue);
    }
    if (subtitle != nullptr) draw_text(subtitle, 0.071f, 0.168f, 13.0f,
                                       theme::kMutedRed, theme::kMutedGreen, theme::kMutedBlue);
    if (g_menu.page == MenuPage::Categories || g_menu.page == MenuPage::Recipes) {
        if (textured) {
            draw_sprite(theme::kGeneric, theme::kChipBox, 0.324f, 0.111f, 0.125f, 0.040f,
                        g_menu.craftableNow ? theme::kAccentRed : 60,
                        g_menu.craftableNow ? theme::kAccentGreen : 56,
                        g_menu.craftableNow ? theme::kAccentBlue : 52, 235);
        } else {
            draw_rect(0.324f, 0.111f, 0.125f, 0.040f,
                      g_menu.craftableNow ? 120 : 44, g_menu.craftableNow ? 18 : 40,
                      g_menu.craftableNow ? 18 : 37, 235);
        }
        draw_text(hunting_companion::ui_text(g_menu.craftableNow ? "FILTER: CRAFTABLE" : "FILTER: ALL"),
                  0.269f, 0.104f, 11.5f, 255, 244, 228);
    }
}

void draw_menu_footer() noexcept {
    if (menu_textures_ready()) {
        draw_sprite(theme::kMenu, theme::kDivider, 0.212f, 0.895f, 0.345f, 0.0042f,
                    theme::kGoldRed, theme::kGoldGreen, theme::kGoldBlue, 170);
    } else {
        draw_rect(0.212f, 0.895f, 0.34f, 0.0018f,
                  theme::kGoldRed, theme::kGoldGreen, theme::kGoldBlue, 160);
    }
    draw_text("UP/DOWN Navigate   ENTER Select   BACKSPACE Back", 0.065f, 0.855f, 14.0f,
              theme::kTextRed, theme::kTextGreen, theme::kTextBlue);
    char footer[48]{};
    std::snprintf(footer, sizeof(footer), hunting_companion::ui_text("%s Close"), key_name(g_menuHotkey));
    draw_text(footer, 0.065f, 0.908f, 13.5f,
              theme::kMutedRed, theme::kMutedGreen, theme::kMutedBlue);
    if (g_menu.page == MenuPage::Categories || g_menu.page == MenuPage::Recipes) {
        char filterHint[64]{};
        std::snprintf(filterHint, sizeof(filterHint), "%s: %s",
                      key_name(g_filterHotkey), hunting_companion::ui_text("Toggle Filter"));
        draw_text(filterHint, 0.240f, 0.908f, 12.5f,
                  theme::kMutedRed, theme::kMutedGreen, theme::kMutedBlue);
    }
}

void draw_list_page(const char* title, const char* subtitle,
                    int total, const char* (*labelAt)(int, char*, std::size_t),
                    const char* (*statusAt)(int, char*, std::size_t),
                    bool officialItemLabels = false,
                    bool (*disabledAt)(int) = nullptr) noexcept {
    draw_menu_header(title, subtitle);
    const int start = g_menu.scroll;
    const int end = std::min(total, start + kVisibleMenuRows);
    for (int i = start; i < end; ++i) {
        char label[128]{};
        char status[40]{};
        const char* labelText = labelAt ? labelAt(i, label, sizeof(label)) : "";
        const char* statusText = statusAt ? statusAt(i, status, sizeof(status)) : "";
        const bool disabled = disabledAt != nullptr && disabledAt(i);
        draw_menu_row(labelText, statusText, i - start, i == g_menu.selection,
                      false, !officialItemLabels, disabled);
    }
    char count[48]{};
    std::snprintf(count, sizeof(count), hunting_companion::ui_text("%d of %d"), total > 0 ? g_menu.selection + 1 : 0, total);
    draw_text(count, 0.31f, 0.808f, 13.5f,
              theme::kMutedRed, theme::kMutedGreen, theme::kMutedBlue);
    // Scroll arrows appear only while rows are hidden above or below.
    if (menu_textures_ready()) {
        const float arrowWidth = 0.0105f * screen_width_correction();
        if (start > 0)
            draw_sprite(theme::kMenu, theme::kArrowUp, 0.283f, 0.822f, arrowWidth, 0.0187f,
                        theme::kGoldRed, theme::kGoldGreen, theme::kGoldBlue, 235);
        if (end < total)
            draw_sprite(theme::kMenu, theme::kArrowDown, 0.297f, 0.822f, arrowWidth, 0.0187f,
                        theme::kGoldRed, theme::kGoldGreen, theme::kGoldBlue, 235);
    }
    draw_menu_footer();
}

const char* root_label(int index, char*, std::size_t) noexcept {
    static const char* labels[] = {
        "Pearson", "Trapper", "Crafting Uses", "Challenges", "Settings", "Close"
    };
    return (index >= 0 && index < 6) ? labels[index] : "";
}
bool root_item_disabled(int index) noexcept {
    // Satchel materials alone (antlers, claws...) count as haul, so gate on
    // the recipe list itself: no matching uncrafted recipe means nothing to open.
    return index == 2 && (!g_currentHaul.ready || !current_haul_has_any() ||
                          g_craftingUseCount == 0);
}
const char* root_row_status(int index, char*, std::size_t) noexcept {
    if (index != 2) return "";
    return root_item_disabled(index) ? "NO HAUL" : "AVAILABLE";
}
const char* no_status(int, char*, std::size_t) noexcept { return ""; }

const char* crafting_use_row_label(int index, char* buffer, std::size_t size) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= g_craftingUseCount) return "";
    return crafting_use_label(g_craftingUseEntries[static_cast<std::size_t>(index)], buffer, size);
}
const char* crafting_use_row_status(int index, char*, std::size_t) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= g_craftingUseCount) return "";
    const CraftingUseEntry& entry = g_craftingUseEntries[static_cast<std::size_t>(index)];
    return crafting_use_readiness_label(static_cast<CraftingUseReadiness>(entry.readiness));
}

const char* challenge_row_label(int index, char*, std::size_t) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= kChallengeCount) return "";
    return kChallengeDefinitions[static_cast<std::size_t>(index)].name;
}

const char* challenge_row_status(int index, char* buffer, std::size_t size) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= kChallengeCount) return "";
    const ChallengeProgress& progress = g_tracker.challenges[static_cast<std::size_t>(index)];
    if (!progress.available) return "UNAVAILABLE";
    std::snprintf(buffer, size, "%d / %d", progress.ranksCompleted, progress.maxRanks);
    return buffer;
}

const char* challenge_reward_row_label(int index, char*, std::size_t) noexcept {
    const ReinforcedEquipmentInfo* item = reinforced_equipment_for_challenge(
        static_cast<std::size_t>(g_menu.challengeIndex), index);
    if (item == nullptr) return "";
    const RecipeEntry* recipe = find_trapper_recipe_by_name(item->itemName);
    return recipe != nullptr ? recipe_display_name(*recipe) : item->itemName;
}

const char* challenge_reward_row_status(int index, char* buffer, std::size_t size) noexcept {
    const ReinforcedEquipmentInfo* item = reinforced_equipment_for_challenge(
        static_cast<std::size_t>(g_menu.challengeIndex), index);
    if (item == nullptr) return "";
    const RecipeEntry* recipe = find_trapper_recipe_by_name(item->itemName);
    if (recipe != nullptr && recipe->owned) return "OWNED";
    const ChallengeProgress* progress = find_challenge_progress(item->challengeName);
    if (progress == nullptr || !progress->available) return "RANK ?";
    if (progress->ranksCompleted >= static_cast<int>(item->unlockRank)) return "UNLOCKED";
    std::snprintf(buffer, size, hunting_companion::ui_text("RANK %u"), item->unlockRank);
    return buffer;
}

const char* category_row_label(int index, char*, std::size_t) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= g_menuCategoryCount) return "";
    return g_menuCategories[static_cast<std::size_t>(index)].label;
}
const char* category_row_status(int index, char* buffer, std::size_t size) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= g_menuCategoryCount) return "";
    std::snprintf(buffer, size, "%d", g_menuCategories[static_cast<std::size_t>(index)].recipeCount);
    return buffer;
}

const char* recipe_row_label(int index, char*, std::size_t) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= g_menuRecipeCount) return "";
    const VendorState& vendor = g_menu.trapper ? g_tracker.trapper : g_tracker.pearson;
    return recipe_display_name(vendor.recipes[static_cast<std::size_t>(g_menuRecipeIndices[static_cast<std::size_t>(index)])]);
}
const char* recipe_row_status(int index, char* buffer, std::size_t size) noexcept {
    if (index < 0 || static_cast<std::size_t>(index) >= g_menuRecipeCount) return "";
    const VendorState& vendor = g_menu.trapper ? g_tracker.trapper : g_tracker.pearson;
    const RecipeEntry& recipe = vendor.recipes[static_cast<std::size_t>(g_menuRecipeIndices[static_cast<std::size_t>(index)])];
    if (g_menu.trapper && recipe.categoryHash == kTrapperGarmentSetsCategoryHash) {
        const GarmentSetDefinition* set = find_garment_set_definition(recipe.name);
        if (set != nullptr) {
            int crafted = 0;
            int total = 0;
            garment_set_crafted_count(*set, crafted, total);
            std::snprintf(buffer, size, "%d / %d", crafted, total);
            return buffer;
        }
    }
    if (g_menu.trapper && find_reinforced_equipment(recipe.name) != nullptr) {
        if (recipe.owned) return "OWNED";
        const ReinforcedEquipmentInfo* item = find_reinforced_equipment(recipe.name);
        const ChallengeProgress* progress = item != nullptr
            ? find_challenge_progress(item->challengeName) : nullptr;
        if (progress == nullptr || !progress->available) return "RANK ?";
        if (progress->ranksCompleted >= static_cast<int>(item->unlockRank)) return "UNLOCKED";
        std::snprintf(buffer, size, hunting_companion::ui_text("RANK %u"), item->unlockRank);
        return buffer;
    }
    if (recipe.owned) return "CRAFTED";
    bool hasAnimal = false;
    for (std::size_t i = 0; i < recipe.costCount; ++i) {
        if (recipe.costs[i].hash != kMoneyHash) { hasAnimal = true; break; }
    }
    std::snprintf(buffer, size, "%s", hunting_companion::ui_text(hasAnimal ? "MATERIALS" : "OTHER COSTS"));
    return buffer;
}

void draw_settings_page() noexcept {
    draw_menu_header("SETTINGS", "Configuration is saved to HuntingCompanion.ini");
    char hotkeyLine[128]{};
    std::snprintf(hotkeyLine, sizeof(hotkeyLine), hunting_companion::ui_text("Menu hotkey: %s"), key_name(g_menuHotkey));
    draw_menu_row(hotkeyLine, "CHANGE", 0, g_menu.selection == 0);

    static constexpr const char* kLanguageNames[] = {
        "English", "French", "German", "Italian", "Spanish (Spain)",
        "Portuguese (Brazil)", "Polish", "Russian", "Korean",
        "Chinese (Traditional)", "Japanese", "Spanish (Mexico)",
        "Chinese (Simplified)"
    };
    const int selectedLanguage = g_uiLanguageOverride >= 0
        ? g_uiLanguageOverride : g_detectedGameLanguage;
    const char* interfaceName = g_uiLanguageOverride < 0
        ? hunting_companion::ui_text("Auto (RDR2)")
        : hunting_companion::ui_text(kLanguageNames[g_uiLanguageOverride]);
    draw_menu_row("Interface language", interfaceName, 1, g_menu.selection == 1);
    char filterKeyLine[128]{};
    std::snprintf(filterKeyLine, sizeof(filterKeyLine),
                  hunting_companion::ui_text("Craftable filter key: %s"), key_name(g_filterHotkey));
    draw_menu_row(filterKeyLine, "CHANGE", 2, g_menu.selection == 2);
    draw_menu_row("Weapon/ammo icons", g_showWeaponIcons ? "SHOWN" : "HIDDEN", 3,
                  g_menu.selection == 3);
    char iconsKeyLine[128]{};
    std::snprintf(iconsKeyLine, sizeof(iconsKeyLine),
                  hunting_companion::ui_text("Weapon/ammo icons key: %s"), key_name(g_iconsHotkey));
    draw_menu_row(iconsKeyLine, "CHANGE", 4, g_menu.selection == 4);
    draw_menu_row("Menu font", kMenuFonts[g_menuFontIndex].name, 5, g_menu.selection == 5);
    draw_menu_row("Back", "", 6, g_menu.selection == 6);

    // The seven rows end near y = 0.63, so the information block starts below them.
    char languageLine[256]{};
    std::snprintf(languageLine, sizeof(languageLine), hunting_companion::ui_text("RDR2 language: %s (%s)"),
                  game_language_name(g_detectedGameLanguage),
                  game_language_locale(g_detectedGameLanguage));
    draw_text(languageLine, 0.071f, 0.642f, 13.5f, 206, 200, 186);
    char interfaceLine[256]{};
    std::snprintf(interfaceLine, sizeof(interfaceLine),
                  hunting_companion::ui_text("Companion interface: %s (%s)"),
                  game_language_name(selectedLanguage),
                  hunting_companion::ui_locale());
    draw_text(interfaceLine, 0.071f, 0.676f, 12.5f, 184, 178, 164);
    if (overlay_fallback_active()) {
        // Only shown while the UI is in English (the fallback), so it is not translated.
        static constexpr char kOverlayFallbackNotice[] =
            "CJK overlay not ready (Vulkan only): showing English.";
        draw_text_raw(kOverlayFallbackNotice, 0.071f, 0.772f, 12.0f, 224, 160, 120);
    }
    const std::size_t officialCount = hunting_companion::loaded_official_string_count();
    char officialStatus[160]{};
    if (officialCount > 0) {
        std::snprintf(officialStatus, sizeof(officialStatus),
                      hunting_companion::ui_text("Official Rockstar language entries loaded: %llu"),
                      static_cast<unsigned long long>(officialCount));
        draw_wrapped_text(officialStatus, 0.071f, 0.710f, 44, 2, 0.026f, 12.0f, 145, 194, 157);
    } else {
        std::snprintf(officialStatus, sizeof(officialStatus),
                      hunting_companion::ui_text("Official item-name data unavailable for %s; English names used."),
                      hunting_companion::ui_text(hunting_companion::locale_name(selectedLanguage)));
        draw_wrapped_text(officialStatus, 0.071f, 0.710f, 44, 2, 0.026f, 12.0f, 220, 174, 128);
    }
    if (g_menu.captureHotkey || g_menu.captureFilterHotkey || g_menu.captureIconsHotkey) {
        draw_rect(0.212f, 0.52f, 0.34f, 0.12f, 0, 0, 0, 245);
        const char* prompt = g_menu.captureFilterHotkey ? "PRESS A KEY FOR THE CRAFTABLE FILTER" :
                             (g_menu.captureIconsHotkey ? "PRESS A KEY FOR THE WEAPON/AMMO ICONS" :
                              "PRESS A KEY TO ASSIGN THE MENU HOTKEY");
        draw_wrapped_text(prompt, 0.060f, 0.485f, 38, 2, 0.032f, 14.0f, 230, 205, 150);
        draw_text("ESC cancels", 0.15f, 0.565f, 13.0f, 210, 205, 195);
    }
    draw_menu_footer();
}

void draw_challenge_detail() noexcept {
    if (g_menu.challengeIndex < 0 ||
        static_cast<std::size_t>(g_menu.challengeIndex) >= kChallengeCount) return;
    const std::size_t challengeIndex = static_cast<std::size_t>(g_menu.challengeIndex);
    const ChallengeDefinition& definition = kChallengeDefinitions[challengeIndex];
    const ChallengeProgress& progress = g_tracker.challenges[challengeIndex];
    draw_menu_header(definition.name, "LIVE STORY MODE CHALLENGE / READ ONLY");

    if (!progress.available) {
        draw_text("CHALLENGE RANK DATA UNAVAILABLE", 0.071f, 0.235f,
                  16.0f, 224, 177, 133);
        char nativeResult[96]{};
        std::snprintf(nativeResult, sizeof(nativeResult),
                      hunting_companion::ui_text("NATIVE RETURNED: %d COMPLETED / %d MAX (EXPECTED MAX 10)"),
                      progress.ranksCompleted, progress.maxRanks);
        draw_text(nativeResult, 0.071f, 0.292f, 12.0f, 224, 211, 188);
        char rootLine[96]{};
        std::snprintf(rootLine, sizeof(rootLine),
                      hunting_companion::ui_text("CATEGORY ROOT: %s"),
                      definition.rootHashName);
        draw_text(rootLine, 0.071f, 0.337f, 11.0f, 190, 184, 170);
        draw_text("Check Story Mode state and ScriptHook compatibility.",
                  0.071f, 0.379f, 11.0f, 190, 184, 170);
    } else {
        char rankLine[64]{};
        std::snprintf(rankLine, sizeof(rankLine), hunting_companion::ui_text("RANKS COMPLETED: %d / %d"),
                      progress.ranksCompleted, progress.maxRanks);
        draw_text(rankLine, 0.071f, 0.226f, 17.0f, 236, 219, 185);

        const bool allComplete = progress.ranksCompleted >= progress.maxRanks;
        if (allComplete) {
            draw_text("ALL TEN RANKS COMPLETE", 0.071f, 0.281f,
                      13.0f, 130, 215, 151);
            draw_text("CHALLENGE STATUS", 0.071f, 0.326f,
                      11.0f, 220, 184, 112);
            draw_wrapped_text("Every rank in this challenge category is complete.",
                              0.071f, 0.350f, 48, 2, 0.028f, 13.0f,
                              225, 218, 202);
        } else {
            char activeRank[64]{};
            std::snprintf(activeRank, sizeof(activeRank), hunting_companion::ui_text("CURRENT RANK: %d / %d"),
                          progress.ranksCompleted + 1, progress.maxRanks);
            draw_text(activeRank, 0.071f, 0.281f, 13.0f, 215, 202, 177);
            draw_text("NEXT RANK OBJECTIVE", 0.071f, 0.326f,
                      11.0f, 220, 184, 112);
            const char* objective = localized_challenge_objective(
                definition.rankObjectives[progress.ranksCompleted]);
            // Translated objectives can be longer than the concise English source.
            // Three bounded lines prevent French/German/Portuguese copy from being
            // truncated while leaving clear space above the progress bar.
            draw_wrapped_text(objective, 0.071f, 0.350f, 48, 3, 0.022f, 12.5f,
                              225, 218, 202);
        }

        // Shared helper keeps this bar and every reinforced-item bar aligned
        // to the exact right edge of the reward-row plates.
        draw_challenge_progress_bar(progress, 0.434f);
        draw_text("Rank totals come from the game's challenge system; progress is never changed.",
                  0.071f, 0.462f, 10.5f, 174, 168, 154);
    }

    draw_text("REINFORCED REWARDS", 0.071f, 0.520f, 14.0f, 220, 184, 112);
    const int rewardCount = reinforced_equipment_count_for_challenge(challengeIndex);
    for (int i = 0; i < rewardCount; ++i) {
        char label[128]{};
        char status[40]{};
        const char* labelText = challenge_reward_row_label(i, label, sizeof(label));
        const char* statusText = challenge_reward_row_status(i, status, sizeof(status));
        draw_menu_row(labelText, statusText, i + 6, i == g_menu.selection,
                      false, false);
    }
    draw_menu_footer();
}

void draw_recipe_detail() noexcept {
    const RecipeEntry* recipe = selected_recipe();
    const VendorState& vendor = g_menu.trapper ? g_tracker.trapper : g_tracker.pearson;
    if (recipe == nullptr) return;
    const GarmentSetDefinition* garmentSet = selected_garment_set();
    const ReinforcedEquipmentInfo* reinforced = g_menu.trapper ? find_reinforced_equipment(recipe->name) : nullptr;
    if (reinforced != nullptr) {
        draw_menu_header(recipe_display_name(*recipe), "REINFORCED EQUIPMENT / CHALLENGE REWARD", false);
        const ChallengeProgress* progress = find_challenge_progress(reinforced->challengeName);
        const bool unlocked = progress != nullptr && progress->available &&
            progress->ranksCompleted >= static_cast<int>(reinforced->unlockRank);
        const char* itemStatus = recipe->owned ? "OWNED" :
            (unlocked ? "UNLOCKED - READY TO BUY" :
             (progress != nullptr && progress->available ? "LOCKED" : "RANK DATA UNAVAILABLE"));
        draw_text(itemStatus, 0.071f, 0.215f, 17.0f,
                  recipe->owned || unlocked ? 130 : 220,
                  recipe->owned || unlocked ? 205 : 184,
                  recipe->owned || unlocked ? 145 : 112);
        draw_text("PURCHASE PRICE", 0.071f, 0.282f, 14.0f, 220, 184, 112);
        char price[24]{};
        std::snprintf(price, sizeof(price), "$%u.%02u",
                      reinforced->priceCents / 100u, reinforced->priceCents % 100u);
        draw_text(price, 0.315f, 0.282f, 15.0f, 235, 224, 200);
        draw_text("UNLOCK REQUIREMENT", 0.071f, 0.350f, 14.0f, 220, 184, 112);
        char requirement[128]{};
        std::snprintf(requirement, sizeof(requirement), hunting_companion::ui_text("Complete the %s Challenge Rank %u."),
                      hunting_companion::ui_text(reinforced->challengeName), reinforced->unlockRank);
        draw_text(requirement, 0.071f, 0.402f, 14.0f, 235, 224, 200);
        if (progress != nullptr && progress->available) {
            char ranks[64]{};
            std::snprintf(ranks, sizeof(ranks), hunting_companion::ui_text("RANKS COMPLETED: %d / %d"),
                          progress->ranksCompleted, progress->maxRanks);
            draw_text(ranks, 0.071f, 0.474f, 14.0f, 214, 207, 190);

            const ChallengeDefinition* challenge =
                find_challenge_definition(reinforced->challengeName);
            const bool allComplete = progress->ranksCompleted >= progress->maxRanks;
            if (allComplete) {
                draw_text("ALL TEN RANKS COMPLETE", 0.071f, 0.510f,
                          12.0f, 130, 215, 151);
            } else {
                char currentRank[64]{};
                std::snprintf(currentRank, sizeof(currentRank),
                              hunting_companion::ui_text("CURRENT RANK: %d / %d"),
                              progress->ranksCompleted + 1, progress->maxRanks);
                draw_text(currentRank, 0.071f, 0.510f, 12.0f, 215, 202, 177);
                draw_text("NEXT RANK OBJECTIVE", 0.071f, 0.546f,
                          11.0f, 220, 184, 112);
                if (challenge != nullptr) {
                    const char* objective = localized_challenge_objective(
                        challenge->rankObjectives[progress->ranksCompleted]);
                    draw_wrapped_text(objective, 0.071f, 0.568f,
                                      48, 2, 0.024f, 12.0f,
                                      225, 218, 202);
                } else {
                    draw_text("Challenge objective reference unavailable.",
                              0.071f, 0.568f, 11.0f, 190, 184, 170);
                }
            }
            // Use exactly the same content bounds as the challenge category page.
            draw_challenge_progress_bar(*progress, 0.632f);
        }
        draw_text("No animal pelts or feathers are required.", 0.071f, 0.680f,
                  13.0f, 205, 200, 185);
        draw_text("Once unlocked, purchase the reward from the Trapper.", 0.071f, 0.727f,
                  13.0f, 205, 200, 185);
        draw_text("Live rank totals are read-only; challenge progress is never modified.",
                  0.071f, 0.774f, 11.0f, 174, 168, 154);
        draw_menu_footer();
        return;
    }
    draw_menu_header(recipe_display_name(*recipe), garmentSet != nullptr ? "GARMENT SET / INDIVIDUAL PIECES" :
                     (g_menu.trapper ? "TRAPPER RECIPE" : "PEARSON RECIPE"), false);
    if (garmentSet != nullptr) {
        draw_text("COMPONENT GARMENTS AND THEIR CATALOG MATERIAL COSTS", 0.071f, 0.215f,
                  14.0f, 220, 184, 112);
        draw_wrapped_centered_text(
            "Donated counts use shared Trapper stock; they are not reserved per piece.",
            0.212f, 0.250f, 49, 2, 0.019f, 11.0f, 188, 184, 174);
        DetailLine lines[64]{};
        const std::size_t lineCount = build_detail_lines(recipe, lines, std::size(lines));
        constexpr int kDetailRows = 8;
        const int start = g_menu.scroll;
        const int end = std::min(static_cast<int>(lineCount), start + kDetailRows);
        for (int i = start; i < end; ++i) {
            const DetailLine& line = lines[static_cast<std::size_t>(i)];
            const float y = 0.330f + static_cast<float>(i - start) * 0.056f;
            if (i == g_menu.selection) {
                if (menu_textures_ready()) {
                    draw_sprite(theme::kGeneric, theme::kSelectionBox, 0.212f, y + 0.018f,
                                0.365f, 0.050f,
                                theme::kAccentRed, theme::kAccentGreen, theme::kAccentBlue, 215);
                } else {
                    draw_rect(0.212f, y + 0.018f, 0.365f, 0.048f,
                              theme::kAccentRed, theme::kAccentGreen, theme::kAccentBlue, 200);
                }
            }
            if (line.heading) {
                const char* componentName = line.component != nullptr
                    ? recipe_display_name(*line.component)
                    : (line.componentName ? line.componentName : "Unknown garment");
                draw_text_raw(componentName, 0.071f, y, 14.0f, 236, 198, 139);
                if (line.component != nullptr) {
                    draw_text(line.component->owned ? "CRAFTED" : "TO CRAFT", 0.315f, y + 0.002f,
                              11.0f, line.component->owned ? 130 : 210,
                              line.component->owned ? 205 : 184,
                              line.component->owned ? 145 : 120);
                } else {
                    draw_text("NOT IN CATALOG", 0.290f, y + 0.002f, 10.0f, 205, 145, 120);
                }
            } else if (line.cost != nullptr && line.component != nullptr) {
                const char* material = display_material_name(line.cost->hash);
                char label[112]{};
                if (material != nullptr) std::snprintf(label, sizeof(label), "    %s", material);
                else std::snprintf(label, sizeof(label), "    Material %08X", line.cost->hash);
                draw_wrapped_text_raw(label, 0.078f, y - 0.001f, 32, 2, 0.017f, 11.0f,
                                      224, 218, 205);
                char right[32]{};
                if (line.component->owned) {
                    std::snprintf(right, sizeof(right), hunting_companion::ui_text("Recipe cost: %d"), line.cost->amount);
                } else {
                    const MaterialNeed* need = find_vendor_material(vendor.demand, line.cost->hash);
                    const int donated = need ? std::min(need->vendorStock, line.cost->amount) : 0;
                    std::snprintf(right, sizeof(right), "%d/%d", donated, line.cost->amount);
                }
                draw_text(right, 0.315f, y, 13.0f,
                          line.component->owned ? 160 : 215,
                          line.component->owned ? 194 : 184,
                          line.component->owned ? 168 : 125);
            } else {
                draw_text(line.componentName ? line.componentName : "Catalog details unavailable",
                          0.078f, y, 12.0f, 185, 177, 161);
            }
        }
        draw_text("UP/DOWN scrolls through the garment pieces and costs.",
                  0.071f, 0.805f, 11.0f, 174, 168, 154);
        draw_menu_footer();
        return;
    }
    if (recipe->owned) {
        draw_text("CRAFTED", 0.071f, 0.215f, 18.0f, 128, 218, 153);
        draw_text("This item is already in your owned catalog.", 0.071f, 0.270f, 13.0f);
    } else {
        draw_text("DONATED / NEEDED FOR THIS RECIPE", 0.071f, 0.215f, 16.0f, 220, 184, 112);
        draw_text("Counts use vendor donation stock, not your inventory.", 0.071f, 0.258f, 12.0f, 188, 184, 174);
    }

    int visible = 0;
    const int start = g_menu.scroll;
    const int maxRows = 8;
    for (std::size_t i = 0; i < recipe->costCount; ++i) {
        const RecipeCost& cost = recipe->costs[i];
        if (cost.hash == kMoneyHash) continue;
        if (visible < start) { ++visible; continue; }
        if (visible >= start + maxRows) { ++visible; continue; }
        const char* name = display_material_name(cost.hash);
        char label[128]{};
        if (name == nullptr) std::snprintf(label, sizeof(label), "Material %08X", cost.hash);
        else std::snprintf(label, sizeof(label), "%s", name);
        const float y = 0.330f + static_cast<float>(visible - start) * 0.060f;
        if (recipe->owned) {
            char right[32]{};
            std::snprintf(right, sizeof(right), hunting_companion::ui_text("Recipe cost: %d"), cost.amount);
            draw_wrapped_text_raw(label, 0.071f, y - 0.001f, 32, 2, 0.018f, 11.5f,
                                  222, 216, 203);
            draw_text(right, 0.305f, y, 15.0f, 187, 180, 164);
        } else {
            const MaterialNeed* need = find_vendor_material(vendor.demand, cost.hash);
            const int total = cost.amount;
            const int donated = need ? std::min(need->vendorStock, total) : 0;
            char right[32]{};
            std::snprintf(right, sizeof(right), "%d/%d", donated, total);
            draw_wrapped_text_raw(label, 0.071f, y - 0.001f, 32, 2, 0.018f, 11.5f,
                                  240, 234, 222);
            draw_text(right, 0.305f, y, 18.0f,
                      total > 0 && donated >= total ? 130 : 218,
                      total > 0 && donated >= total ? 205 : 185,
                      total > 0 && donated >= total ? 145 : 120);
        }
        ++visible;
    }

    int animalCostCount = 0;
    for (std::size_t i = 0; i < recipe->costCount; ++i) {
        if (recipe->costs[i].hash != kMoneyHash) ++animalCostCount;
    }
    if (animalCostCount == 0) {
        if (recipe->hasAnyCosts) {
            draw_text("No animal-material donation costs for this item.", 0.071f, 0.34f, 15.0f, 210, 205, 190);
            draw_text("Cash or other catalog requirements are not shown here.", 0.071f, 0.395f, 12.0f, 180, 175, 163);
        } else {
            draw_text("No animal-material costs were exposed for this entry.", 0.071f, 0.34f, 15.0f, 210, 205, 190);
            draw_text("It may rely on an unlock or another requirement.", 0.071f, 0.395f, 12.0f, 180, 175, 163);
        }
    }
    if (!recipe->owned) {
        draw_wrapped_centered_text(
            "Vendor stock is pooled; the same materials can appear in other recipes.",
            0.212f, 0.744f, 31, 3, 0.018f, 11.0f, 174, 168, 154);
    }
    draw_menu_footer();
}

void draw_crafting_uses_page() noexcept {
    draw_list_page("CRAFTING USES", "Uncrafted recipes that use your current haul",
                   static_cast<int>(g_craftingUseCount), crafting_use_row_label,
                   crafting_use_row_status, false);
    if (g_craftingUseCount == 0) {
        draw_wrapped_text("NO UNCRAFTED RECIPES MATCH YOUR CURRENT HAUL",
                          0.071f, 0.300f, 48, 2, 0.030f, 14.0f, 185, 177, 161);
        draw_text("Try another pelt or animal part.",
                  0.071f, 0.385f, 11.5f, 170, 164, 150);
    }
    draw_text("Counts include what you carry, your satchel and saddle horse.",
              0.071f, 0.790f, 10.5f, 160, 154, 142);
}

void draw_crafting_use_detail() noexcept {
    if (g_menu.craftingUseIndex < 0 ||
        static_cast<std::size_t>(g_menu.craftingUseIndex) >= g_craftingUseCount) return;
    const CraftingUseEntry& entry = g_craftingUseEntries[static_cast<std::size_t>(g_menu.craftingUseIndex)];
    const RecipeEntry* recipe = crafting_use_recipe(entry);
    if (recipe == nullptr) return;
    const VendorState& vendor = crafting_use_vendor(entry);
    const CraftingUseReadiness readiness = crafting_use_readiness(vendor, *recipe);
    char subtitle[128]{};
    if (entry.setName != nullptr) {
        std::snprintf(subtitle, sizeof(subtitle), "%s - %s",
                      hunting_companion::ui_text("TRAPPER RECIPE"), entry.setName);
    } else {
        std::snprintf(subtitle, sizeof(subtitle), "%s",
                      hunting_companion::ui_text(entry.trapper ? "TRAPPER RECIPE" : "PEARSON RECIPE"));
    }
    draw_menu_header(recipe_display_name(*recipe), subtitle, false);

    const char* readinessText = crafting_use_readiness_label(readiness);
    const bool ready = readiness == CraftingUseReadiness::ReadyAfterDonation;
    const bool partsReady = ready || readiness == CraftingUseReadiness::AnimalPartsReady;
    draw_wrapped_text(readinessText, 0.071f, 0.208f, 30, 2, 0.030f, 14.0f,
                      partsReady ? 130 : 218, partsReady ? 205 : 184, partsReady ? 145 : 120);
    draw_wrapped_text("HELD / NEEDED (YOUR HAUL + VENDOR STOCK)", 0.071f, 0.275f, 44, 2, 0.022f,
                      10.5f, 220, 184, 112);

    // Cash is read from the catalog in units that cannot be confirmed here, so
    // only material costs are listed.
    constexpr int kVisibleCosts = 8;
    int shown = 0;
    int skipped = 0;
    for (std::size_t costIndex = 0; costIndex < recipe->costCount; ++costIndex) {
        const RecipeCost& cost = recipe->costs[costIndex];
        if (cost.hash == kMoneyHash) continue;
        if (skipped < g_menu.scroll) { ++skipped; continue; }
        if (shown >= kVisibleCosts) break;
        const char* materialName = display_material_name(cost.hash);
        char label[128]{};
        if (materialName != nullptr) std::snprintf(label, sizeof(label), "%s", materialName);
        else std::snprintf(label, sizeof(label), "Material %08X", cost.hash);
        const float y = 0.350f + static_cast<float>(shown) * 0.052f;
        ++shown;
        draw_wrapped_text_raw(label, 0.071f, y, 28, 2, 0.016f, 11.5f,
                              224, 218, 205);
        char amount[48]{};
        bool met = false;
        if (is_animal_part_material(cost.hash)) {
            const int haul = current_haul_count(cost.hash);
            const MaterialNeed* stock = find_vendor_material(vendor.demand, cost.hash);
            const int donated = stock != nullptr ? stock->vendorStock : 0;
            std::snprintf(amount, sizeof(amount), "%d / %d", haul + donated, cost.amount);
            met = haul + donated >= cost.amount;
        } else {
            const int owned = g_currentHaul.inventoryId >= 0
                ? player_owned_count(g_currentHaul.inventoryId, cost.hash) : 0;
            std::snprintf(amount, sizeof(amount), "%d / %d", owned, cost.amount);
            met = owned >= cost.amount;
        }
        draw_text(amount, 0.315f, y + 0.001f, 12.0f,
                  met ? 130 : 218, met ? 205 : 184, met ? 145 : 120);
    }
    if (shown == 0 && g_menu.scroll == 0) {
        draw_text("No material costs were exposed for this catalog entry.",
                  0.071f, 0.340f, 12.0f, 190, 182, 168);
    }
    draw_menu_footer();
}

void draw_scan_progress() noexcept {
    const ScanStage stage = static_cast<ScanStage>(
        InterlockedCompareExchange(&g_scanStage, 0, 0));
    const int current = InterlockedCompareExchange(&g_scanCurrent, 0, 0);
    const int total = InterlockedCompareExchange(&g_scanTotal, 0, 0);
    const int error = InterlockedCompareExchange(&g_scanError, 0, 0);
    const ScanStage lastWorkStage = static_cast<ScanStage>(
        InterlockedCompareExchange(&g_scanLastWorkStage, 0, 0));
    const DWORD started = static_cast<DWORD>(InterlockedCompareExchange(&g_scanStartedTick, 0, 0));
    const DWORD lastProgress = static_cast<DWORD>(InterlockedCompareExchange(&g_scanLastProgressTick, 0, 0));
    const DWORD now = GetTickCount();
    const DWORD elapsedSeconds = started != 0 ? (now - started) / 1000u : 0u;
    const DWORD stalledSeconds = lastProgress != 0 ? (now - lastProgress) / 1000u : 0u;

    draw_menu_header("HUNTING COMPANION", "INITIALIZING READ-ONLY CATALOG");
    draw_text(scan_stage_label(stage), 0.071f, 0.235f, 15.0f, 225, 193, 145);
    if (stage == ScanStage::Failed) {
        const char* reason = "The game did not expose the required Story Mode inventory.";
        if (error == 2) reason = "The shop catalog returned an invalid item count.";
        else if (error == 3) reason = "The shop catalog exceeds the safe scan limit.";
        else if (error == 4 || error == 5) reason = "The player inventory is not ready yet.";
        else if (error == 6) reason = "The vendor catalogs could not be read completely.";
        draw_wrapped_text(reason, 0.071f, 0.282f, 48, 2, 0.030f, 12.0f,
                          197, 188, 174);
        char stoppedLine[96]{};
        std::snprintf(stoppedLine, sizeof(stoppedLine), hunting_companion::ui_text("Last stage: %s"),
                      scan_stage_label(lastWorkStage));
        draw_text(stoppedLine, 0.071f, 0.350f, 10.5f, 184, 176, 160);
        char elapsedLine[64]{};
        std::snprintf(elapsedLine, sizeof(elapsedLine), hunting_companion::ui_text("Elapsed: %lu seconds"),
                      static_cast<unsigned long>(elapsedSeconds));
        draw_text(elapsedLine, 0.071f, 0.385f, 10.5f, 184, 176, 160);
        draw_text("Close and reopen the compendium to retry after Story Mode loads.",
                  0.071f, 0.425f, 10.5f, 174, 167, 153);
    } else {
        char progressLine[64]{};
        if (total > 0) std::snprintf(progressLine, sizeof(progressLine), "%d / %d", current, total);
        else std::snprintf(progressLine, sizeof(progressLine), "Preparing...");
        draw_text(progressLine, 0.071f, 0.285f, 13.0f, 222, 214, 198);
        const float width = kMenuProgressBarWidth;
        const float left = kMenuContentLeft;
        draw_rect(left + width * 0.5f, 0.355f, width, 0.014f, 54, 51, 47, 235);
        if (total > 0 && current > 0) {
            const float ratio = std::clamp(static_cast<float>(current) /
                                           static_cast<float>(total), 0.0f, 1.0f);
            const float fill = width * ratio;
            draw_rect(left + fill * 0.5f, 0.355f, fill, 0.014f,
                      65, 166, 119, 250);
        }
        char elapsedLine[64]{};
        std::snprintf(elapsedLine, sizeof(elapsedLine), hunting_companion::ui_text("Elapsed: %lu seconds"),
                      static_cast<unsigned long>(elapsedSeconds));
        draw_text(elapsedLine, 0.071f, 0.397f, 10.5f, 169, 163, 150);
        draw_text("The scan yields between game queries; please allow it to finish.",
                  0.071f, 0.438f, 10.5f, 169, 163, 150);
        if (stalledSeconds >= 20u) {
            char stalledLine[80]{};
            std::snprintf(stalledLine, sizeof(stalledLine),
                          hunting_companion::ui_text("No scan progress for %lu seconds"), static_cast<unsigned long>(stalledSeconds));
            draw_text(stalledLine, 0.071f, 0.482f, 11.0f, 225, 174, 128);
            draw_wrapped_text("If it persists, report this stage and test once with other HUD or inventory mods disabled.",
                              0.071f, 0.520f, 54, 2, 0.032f, 10.5f, 185, 176, 160);
        }
    }
    draw_menu_footer();
}

void draw_menu() noexcept {
    if (g_menu.page == MenuPage::Closed) return;
    // Right edge of the menu rows; all menu text is kept to its left.
    const TextRightLimitScope keepTextInsidePanel(0.3885f);
    if (!g_tracker.ready) {
        draw_scan_progress();
        return;
    }
    switch (g_menu.page) {
        case MenuPage::Root:
            draw_list_page("HUNTING COMPANION", "Crafting, challenges and garment sets",
                           6, root_label, root_row_status, false, root_item_disabled);
            break;
        case MenuPage::Categories: {
            char subtitle[80]{};
            std::snprintf(subtitle, sizeof(subtitle), hunting_companion::ui_text("%s - Select a category"),
                          hunting_companion::ui_text(active_vendor_name()));
            draw_list_page("CATEGORIES", subtitle, static_cast<int>(g_menuCategoryCount),
                           category_row_label, category_row_status);
            break;
        }
        case MenuPage::Recipes: {
            char subtitle[96]{};
            std::snprintf(subtitle, sizeof(subtitle), "%s / %s",
                          hunting_companion::ui_text(active_vendor_name()),
                          hunting_companion::ui_text(category_label(g_menu.trapper, g_menu.categoryHash)));
            draw_list_page("RECIPES", subtitle, static_cast<int>(g_menuRecipeCount),
                           recipe_row_label, recipe_row_status, true);
            break;
        }
        case MenuPage::Detail: draw_recipe_detail(); break;
        case MenuPage::CraftingUses: draw_crafting_uses_page(); break;
        case MenuPage::CraftingUseDetail: draw_crafting_use_detail(); break;
        case MenuPage::Challenges:
            draw_list_page("CHALLENGES", "Live Story Mode rank totals (read-only)",
                           static_cast<int>(kChallengeCount), challenge_row_label,
                           challenge_row_status);
            break;
        case MenuPage::ChallengeDetail: draw_challenge_detail(); break;
        case MenuPage::Settings: draw_settings_page(); break;
        case MenuPage::Closed: break;
    }
}

void TrackerRefreshMain() {
    // One initial scan, then a scan each time the compendium is opened.
    bool initial = true;
    for (;;) {
        if (initial || InterlockedExchange(&g_refreshRequested, 0) != 0) {
            const bool ok = build_tracker_state();
            if (!ok) {
                // Preserve the last good snapshot; the next menu open retries.
                InterlockedExchange(&g_refreshRequested, 0);
            }
            initial = false;
        }
        g_api.wait(250);
    }
}

void ScriptMain() {
    load_configuration();
    g_detectedGameLanguage = invoke_native<int>(kGetCurrentLanguage);
    // First load is unconditional: the tables start empty even when the effective
    // language is already the default (English).
    hunting_companion::load_ui_localization(g_detectedGameLanguage, effective_language());
    hunting_companion::overlay::set_language(effective_language());

    // Fallback for ScriptHook builds without the additional-thread export:
    // vendor catalog data refreshes on compendium open; the haul scan remains a
    // lightweight, read-only poll while the menu is visible.
    if (!g_refreshWorkerRegistered) {
        build_tracker_state();
        InterlockedExchange(&g_refreshRequested, 0);
    }

    for (;;) {
        if (g_menu.page != MenuPage::Closed) refresh_current_haul_if_due();
        {
            TrackerReadLock trackerReadLock;
            if (g_tracker.ready &&
                InterlockedCompareExchange(&g_haulDirty, 0, 0) != 0) {
                build_crafting_uses_list();
                InterlockedExchange(&g_haulDirty, 0);
            }
            update_menu_input();
        }
        if (!g_refreshWorkerRegistered && InterlockedExchange(&g_refreshRequested, 0) != 0) {
            build_tracker_state();
        }
        // The overlay may become ready (or unavailable) at any time; keep the
        // companion language consistent with what can actually be drawn.
        apply_effective_language();
        hunting_companion::overlay::begin_frame();
        {
            TrackerReadLock trackerReadLock;
            update_hud();
            draw_menu();
        }
        hunting_companion::overlay::end_frame();
        g_api.wait(0);
    }
}
} // namespace

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        g_module = module;
        // Hook Vulkan as early as possible: the overlay can only follow a VkInstance
        // that is created after the hooks are in place. Does nothing visible unless
        // a CJK companion language is chosen while the game runs in another language.
        hunting_companion::overlay::install(module, make_overlay_settings());
        if (!resolve_script_hook()) return FALSE;
        if (g_api.registerAdditionalThread != nullptr) {
            // Set the flag before registration so ScriptMain always takes the
            // asynchronous path, even if ScriptHook starts the worker eagerly.
            g_refreshWorkerRegistered = true;
            g_api.registerAdditionalThread(module, TrackerRefreshMain);
        }
        g_api.registerScript(module, ScriptMain);
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_api.unregisterScript && g_module) g_api.unregisterScript(g_module);
    }
    return TRUE;
}
