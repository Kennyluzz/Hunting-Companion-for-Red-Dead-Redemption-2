// Hunting Companion 1.0.0
// Read-only RDR2 Story Mode companion for Pearson, Trapper, challenges, and gear.
//
// The companion reads game catalog, inventory, vendor-storage, and challenge-rank
// state, then renders its menu and focused-animal HUD indicators. Challenge
// progress, inventory, transactions, vendor stock, and save data are never changed.

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <cstdio>
#include <type_traits>

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
constexpr std::uint64_t kBgDisplayText = 0x16794E044C9EFB58ull;
constexpr std::uint64_t kBgSetTextScale = 0xA1253A3C870B6843ull;
constexpr std::uint64_t kBgSetTextColor = 0x16FA5CE47F184F1Eull;
constexpr std::uint64_t kChalGetNumRanksCompleted = 0x58CB53DB63F84DE9ull;
constexpr std::uint64_t kChalGetMaxRanks = 0x58CB53DB63F84DEAull;
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
// RDR2's pause-menu challenge registry (not generic `challenge_<category>`
// guesses). An independently in-game-tested ScriptHookRDR2 implementation
// resolves the same roots with JOAAT and passes them to CHAL_GET_*.
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

// Pin the expected JOAAT values at compile time. This catches accidental
// edits to the root strings or the hash routine before a release build.
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
    RecipeIdentity{0xC998B07Cu, 0x1F5EEA44u, "Unlabelled Pearson item (C998B07C)"},
    RecipeIdentity{0xCD70FF85u, 0x1F5EEA44u, "Elk Antlers"},
    RecipeIdentity{0xCE127491u, 0x1F5EEA44u, "Big Horn Ram Skull"},
    RecipeIdentity{0xCE847162u, 0xDCD17F1Fu, "Ingredients Satchel"},
    RecipeIdentity{0xD006CD59u, 0x1F5EEA44u, "Scout Fire Ground Cover"},
    RecipeIdentity{0xDC99106Du, 0xDCD17F1Fu, "Legend of the East Satchel"},
    RecipeIdentity{0xF2992228u, 0x1F5EEA44u, "Campfire Log Seat Cover"},
    RecipeIdentity{0xF72FB942u, 0x1F5EEA44u, "Cougar Pelt Covered Chest"},
    RecipeIdentity{0xF9C120CDu, 0xDC708275u, "Unlabelled Pearson item (F9C120CD)"},
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

struct TrackerState {
    VendorState pearson{};
    VendorState trapper{};
    std::array<ChallengeProgress, kChallengeCount> challenges{};
    bool ready{};
};
TrackerState g_tracker{};
SRWLOCK g_trackerLock = SRWLOCK_INIT;

class TrackerReadLock {
public:
    TrackerReadLock() noexcept { AcquireSRWLockShared(&g_trackerLock); }
    ~TrackerReadLock() { ReleaseSRWLockShared(&g_trackerLock); }
    TrackerReadLock(const TrackerReadLock&) = delete;
    TrackerReadLock& operator=(const TrackerReadLock&) = delete;
};


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
                           bool isBundle, bool includeInVendorDemand) noexcept {
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
                add_recipe_cost(recipe, materialHash, amount);
                recipe.hasAnyCosts = true;
                // Money is a purchase cost, not a donated animal material.
                if (includeInVendorDemand && materialHash != kMoneyHash) {
                    add_requirement(vendor, materialHash, amount);
                }
            }
        }
        g_api.wait(0);
    }
}

void scan_vendor_catalog(VendorState& state, std::uint32_t shopHash,
                         std::uint32_t costTypeHash, std::uint32_t promoCostTypeHash,
                         int inventoryId) noexcept {
    state = {};
    VendorDemand& vendor = state.demand;
    const int rawCount = invoke_native<int>(kItemDatabaseGetShopItemsCount, shopHash);
    if (rawCount < 0 || rawCount > kMaxShopItems) return;
    const bool isTrapper = (shopHash == kShopTrapper);
    for (int index = 0; index < rawCount; ++index) {
        std::array<std::uint64_t, 3> itemInfo{};
        const int ok = invoke_native<int>(kItemDatabaseGetShopItemInfo, shopHash, index, itemInfo.data());
        if (!ok) {
            g_api.wait(0);
            continue;
        }
        ++vendor.scannedItems;
        const std::uint32_t itemHash = static_cast<std::uint32_t>(itemInfo[0] & 0xffffffffu);
        const std::uint32_t bundleHash = static_cast<std::uint32_t>(itemInfo[1] & 0xffffffffu);
        const int itemOwned = player_owned_count(inventoryId, itemHash);
        const int bundleOwned = player_owned_count(inventoryId, bundleHash);

        RecipeEntry recipe{};
        recipe.itemHash = itemHash;
        recipe.bundleHash = bundleHash;
        recipe.owned = itemOwned > 0 || bundleOwned > 0;
        if (const RecipeIdentity* identity = find_recipe_identity(itemHash)) {
            recipe.categoryHash = identity->categoryHash;
            std::snprintf(recipe.name, sizeof(recipe.name), "%s", identity->name);
        } else {
            std::snprintf(recipe.name, sizeof(recipe.name), "Unknown recipe %08X", itemHash);
        }

        int directRows = itemHash != 0
            ? invoke_native<int>(kItemDatabaseGetAcquireCostsCountFromCostType, itemHash, costTypeHash)
            : 0;
        if (directRows > 0) {
            read_recipe_cost_rows(vendor, recipe, itemHash, costTypeHash, false, !recipe.owned);
        } else if (isTrapper && promoCostTypeHash != 0 && itemHash != 0) {
            const int promoRows = invoke_native<int>(
                kItemDatabaseGetAcquireCostsCountFromCostType, itemHash, promoCostTypeHash);
            if (promoRows > 0) {
                read_recipe_cost_rows(vendor, recipe, itemHash, promoCostTypeHash, false, !recipe.owned);
            }
        }
        if (bundleHash != 0) {
            const int bundleRows = invoke_native<int>(
                kItemDatabaseGetBundleAcquireCostsCountFromCost, bundleHash, costTypeHash);
            if (bundleRows > 0) {
                read_recipe_cost_rows(vendor, recipe, bundleHash, costTypeHash, true, !recipe.owned);
            }
        }
        if (!recipe.owned && recipe.hasAnyCosts) ++vendor.unownedRecipes;
        if (state.recipeCount < state.recipes.size()) {
            state.recipes[state.recipeCount++] = recipe;
        }
        g_api.wait(0);
    }
    vendor.ready = true;
}

bool build_tracker_state() noexcept {
    const int playerPed = invoke_native<int>(kPlayerPedId);
    if (playerPed == 0) return false;
    const int inventoryId = invoke_native<int>(kInventoryGetInventoryIdFromPed, playerPed);
    if (inventoryId < 0) return false;

    Guid emptyParent{};
    Guid rootGuid{};
    if (!get_guid_from_item(inventoryId, &emptyParent, kCharacterSlot, kSlotNone, &rootGuid) ||
        !guid_is_valid(&rootGuid)) return false;

    VendorState pearson{};
    VendorState trapper{};
    scan_vendor_catalog(pearson, kShopPearson, kCostCraftingPearson, 0, inventoryId);
    scan_vendor_catalog(trapper, kShopTrapper, kCostCraftingTrapper,
                        kCostCraftingTrapperPromo, inventoryId);
    if (!pearson.demand.ready || !trapper.demand.ready) return false;

    for (std::size_t i = 0; i < pearson.demand.count; ++i) {
        auto& material = pearson.demand.materials[i];
        material.vendorStock = vendor_material_count(inventoryId, &rootGuid,
                                                      material.hash, kPearsonStorageSlot);
        material.stillNeeded = std::max(0, material.totalRequired - material.vendorStock);
        g_api.wait(0);
    }
    for (std::size_t i = 0; i < trapper.demand.count; ++i) {
        auto& material = trapper.demand.materials[i];
        material.vendorStock = vendor_material_count(inventoryId, &rootGuid,
                                                      material.hash, kTrapperGiveStorageSlot);
        material.stillNeeded = std::max(0, material.totalRequired - material.vendorStock);
        g_api.wait(0);
    }

    // Read the game's own challenge rank totals. These calls do not advance,
    // reset, or otherwise modify challenge progress. Validate each category
    // against its expected rank count before publishing it to the UI.
    std::array<ChallengeProgress, kChallengeCount> challenges{};
    for (std::size_t i = 0; i < kChallengeDefinitions.size(); ++i) {
        const std::uint32_t challengeHash = joaat(kChallengeDefinitions[i].rootHashName);
        const int maxRanks = invoke_native<int>(kChalGetMaxRanks, challengeHash);
        const int completed = invoke_native<int>(kChalGetNumRanksCompleted, challengeHash);
        // Preserve the raw native results even when validation fails. The UI
        // can then show exactly what the game returned instead of hiding the
        // evidence behind a generic unavailable message.
        challenges[i] = ChallengeProgress{completed, maxRanks, false};
        if (maxRanks == kExpectedChallengeRankCount &&
            completed >= 0 && completed <= maxRanks) {
            challenges[i].available = true;
        }
        g_api.wait(0);
    }

    AcquireSRWLockExclusive(&g_trackerLock);
    g_tracker.pearson = pearson;
    g_tracker.trapper = trapper;
    g_tracker.challenges = challenges;
    g_tracker.ready = true;
    ReleaseSRWLockExclusive(&g_trackerLock);
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
    AnimalModel{joaat("a_c_cedarwaxwing_01"), "Cedar Waxwing"}, AnimalModel{joaat("a_c_chipmunk_01"), "Western Chipmunk"},
    AnimalModel{joaat("a_c_cougar_01"), "Cougar"}, AnimalModel{joaat("a_c_cow"), "Cow"},
    AnimalModel{joaat("a_c_coyote_01"), "Coyote"}, AnimalModel{joaat("a_c_crow_01"), "Crow"},
    AnimalModel{joaat("a_c_deer_01"), "Deer"}, AnimalModel{joaat("a_c_duck_01"), "Duck"},
    AnimalModel{joaat("a_c_eagle_01"), "Eagle"}, AnimalModel{joaat("a_c_elk_01"), "Elk"},
    AnimalModel{joaat("a_c_fox_01"), "Fox"}, AnimalModel{joaat("a_c_goat_01"), "Goat"},
    AnimalModel{joaat("a_c_gilamonster_01"), "Gila Monster"}, AnimalModel{joaat("a_c_goosecanada_01"), "Canada Goose"},
    AnimalModel{joaat("a_c_hawk_01"), "Hawk"}, AnimalModel{joaat("a_c_iguana_01"), "Iguana"},
    AnimalModel{joaat("a_c_iguanadesert_01"), "Desert Iguana"}, AnimalModel{joaat("a_c_javelina_01"), "Javelina"},
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
    AnimalModel{joaat("a_c_turkeywild_01"), "Turkey"}, AnimalModel{joaat("a_c_wolf"), "Wolf"},
    AnimalModel{joaat("a_c_wolf_medium"), "Wolf"}, AnimalModel{joaat("a_c_wolf_small"), "Wolf"},
    AnimalModel{joaat("a_c_woodpecker_01"), "Woodpecker"}, AnimalModel{joaat("a_c_woodpecker_02"), "Woodpecker"},
    AnimalModel{joaat("a_c_chicken_01"), "Chicken"}
};

const char* find_animal_species(std::uint32_t modelHash) noexcept {
    for (const auto& animal : kAnimalModels) {
        if (animal.hash == modelHash) return animal.species;
    }
    return nullptr;
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
    constexpr float kIconRowRightEdge = 0.876f;
    const float heightOverWidth = static_cast<float>(screenHeight) /
                                  static_cast<float>(screenWidth);
    layout->height = kIconHeight;
    layout->width = kIconHeight * heightOverWidth;
    const float gap = kGapHeight * heightOverWidth;

    // Fixed vendor slots: Pearson on the left, Trapper on the right. The row
    // is aligned with the lower action-prompt stack, just above the vanilla
    // animal-name separator.
    const float rightCenter = kIconRowRightEdge - layout->width * 0.5f;
    layout->trapperCenterX = rightCenter;
    layout->pearsonCenterX = rightCenter - layout->width - gap;
    layout->centerY = 0.891f;
    return true;
}

void draw_vendor_icon(const char* spriteName, float centerX, float centerY,
                      float width, float height,
                      int red, int green, int blue) noexcept {
    invoke_native<void>(kDrawSprite, "BLIPS", spriteName, centerX, centerY,
                        width, height, 0.0f, red, green, blue, 255, false);
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
    if (!needsPearson && !needsTrapper) return;
    if (!blip_texture_dictionary_ready()) return;

    HudIconLayout layout{};
    if (!get_hud_icon_layout(&layout)) return;

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
// the hunting HUD. It never counts Arthur's carried animal materials.
// -----------------------------------------------------------------------------

enum class MenuPage {
    Closed, Root, Categories, Recipes, Detail, Challenges,
    ChallengeDetail, Settings
};
struct MenuCategory { std::uint32_t hash{}; const char* label{}; int recipeCount{}; };
struct MenuState {
    MenuPage page{MenuPage::Closed};
    bool trapper{};
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
    int challengeReturnSelection{};
    int challengeReturnScroll{};
    bool captureHotkey{};
};
MenuState g_menu{};
std::array<MenuCategory, kMaxCategories> g_menuCategories{};
std::size_t g_menuCategoryCount{};
std::array<int, kMaxRecipesPerShop> g_menuRecipeIndices{};
std::size_t g_menuRecipeCount{};
std::array<bool, 256> g_keyDown{};
std::array<bool, 256> g_keyPressed{};
DWORD g_menuHotkey = kDefaultMenuKey;
bool g_refreshWorkerRegistered{};
volatile LONG g_refreshRequested = 1;
char g_iniPath[MAX_PATH]{};

VendorState& active_vendor() noexcept {
    return g_menu.trapper ? g_tracker.trapper : g_tracker.pearson;
}

const char* active_vendor_name() noexcept {
    return g_menu.trapper ? "TRAPPER" : "PEARSON";
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
    char modulePath[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(g_module, modulePath, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return;
    char* slash = std::strrchr(modulePath, '\\');
    char* altSlash = std::strrchr(modulePath, '/');
    if (altSlash != nullptr && (slash == nullptr || altSlash > slash)) slash = altSlash;
    if (slash == nullptr) return;
    *(slash + 1) = '\0';
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
    // Ensure a usable INI exists even when the user has not opened Settings yet.
    char value[16]{};
    std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(g_menuHotkey));
    WritePrivateProfileStringA("General", "MenuKey", value, g_iniPath);
}

void save_menu_hotkey() noexcept {
    if (g_iniPath[0] == '\0') return;
    char value[16]{};
    std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(g_menuHotkey));
    WritePrivateProfileStringA("General", "MenuKey", value, g_iniPath);
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

void build_menu_categories() noexcept {
    g_menuCategoryCount = 0;
    VendorState& vendor = active_vendor();
    auto append_if_used = [&](std::uint32_t hash, const char* label) {
        int count = 0;
        for (std::size_t i = 0; i < vendor.recipeCount; ++i) {
            if (vendor.recipes[i].categoryHash == hash) ++count;
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
                if (vendor.recipes[j].categoryHash == hash) ++count;
            }
            g_menuCategories[g_menuCategoryCount++] = MenuCategory{hash, "Other / new category", count};
        }
    }
}

void build_menu_recipe_list() noexcept {
    g_menuRecipeCount = 0;
    VendorState& vendor = active_vendor();
    for (std::size_t i = 0; i < vendor.recipeCount; ++i) {
        if (vendor.recipes[i].categoryHash == g_menu.categoryHash &&
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

void move_selection(int delta, int count) noexcept {
    if (count <= 0) { g_menu.selection = 0; g_menu.scroll = 0; return; }
    g_menu.selection = std::clamp(g_menu.selection + delta, 0, count - 1);
    if (g_menu.selection < g_menu.scroll) g_menu.scroll = g_menu.selection;
    if (g_menu.selection >= g_menu.scroll + kVisibleMenuRows) {
        g_menu.scroll = g_menu.selection - kVisibleMenuRows + 1;
    }
}

void open_menu() noexcept {
    g_menu = {};
    g_menu.page = MenuPage::Root;
    // Refresh on demand when the player opens the compendium; no periodic scan.
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
        case MenuPage::Challenges:
            g_menu.page = MenuPage::Root;
            g_menu.selection = 2;
            g_menu.scroll = 0;
            break;
        case MenuPage::ChallengeDetail:
            g_menu.page = MenuPage::Challenges;
            g_menu.selection = g_menu.challengeReturnSelection;
            g_menu.scroll = g_menu.challengeReturnScroll;
            break;
        case MenuPage::Settings:
            g_menu.page = MenuPage::Root;
            g_menu.selection = 3;
            g_menu.scroll = 0;
            break;
        case MenuPage::Closed:
            break;
    }
}

int current_page_count() noexcept {
    switch (g_menu.page) {
        case MenuPage::Root: return 5;
        case MenuPage::Categories: return static_cast<int>(g_menuCategoryCount);
        case MenuPage::Recipes: return static_cast<int>(g_menuRecipeCount);
        case MenuPage::Detail: {
            const RecipeEntry* recipe = selected_recipe();
            DetailLine lines[64]{};
            return static_cast<int>(build_detail_lines(recipe, lines, std::size(lines)));
        }
        case MenuPage::Challenges: return static_cast<int>(kChallengeCount);
        case MenuPage::ChallengeDetail:
            return reinforced_equipment_count_for_challenge(
                static_cast<std::size_t>(g_menu.challengeIndex));
        case MenuPage::Settings: return 2;
        case MenuPage::Closed: return 0;
    }
    return 0;
}

void update_menu_input() noexcept {
    update_keyboard_edges();
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
    if (key_pressed(g_menuHotkey)) {
        g_menu.page = MenuPage::Closed;
        return;
    }
    if (key_pressed(VK_ESCAPE) || key_pressed(VK_BACK)) {
        go_back();
        return;
    }
    if (g_menu.page == MenuPage::Detail) {
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
    } else {
        if (upRepeated) move_selection(-1, current_page_count());
        if (downRepeated) move_selection(1, current_page_count());
        if (key_pressed(VK_PRIOR)) move_selection(-kVisibleMenuRows, current_page_count());
        if (key_pressed(VK_NEXT)) move_selection(kVisibleMenuRows, current_page_count());
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
            } else if (g_menu.selection == 2) {
                g_menu.page = MenuPage::Challenges;
                g_menu.selection = g_menu.scroll = 0;
            } else if (g_menu.selection == 3) {
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
            } else {
                g_menu.page = MenuPage::Root;
                g_menu.selection = 3;
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

void draw_text(const char* text, float x, float y, float referencePixelHeight,
               int red = 232, int green = 226, int blue = 213, int alpha = 255) noexcept {
    if (text == nullptr || text[0] == '\0') return;

    // Restore the western RDR2 title face. The helper maps design-reference
    // sizes to practical native scales above. COLOR_STRING is required for
    // the embedded FONT tag to be interpreted by BG_DISPLAY_TEXT.
    char richTemplate[768]{};
    const int written = std::snprintf(richTemplate, sizeof(richTemplate),
                                      "<FONT FACE='$title'>%s</FONT>", text);
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(richTemplate)) return;

    const char* literal = invoke_native<const char*>(kVarString, 10,
                                                      "LITERAL_STRING", richTemplate);
    if (literal == nullptr) return;
    const char* gameString = invoke_native<const char*>(kVarString, 42,
                                                         "COLOR_STRING", 0, literal);
    if (gameString == nullptr) return;

    const float scale = native_text_scale(referencePixelHeight);
    invoke_native<void>(kBgSetTextScale, scale, scale);

    // Draw each label once; the translucent panel provides contrast.
    invoke_native<void>(kBgSetTextColor, red, green, blue, alpha);
    invoke_native<void>(kBgDisplayText, gameString, x, y);
}

// Word-wrap short reference copy to a fixed character budget and line count.
// This keeps challenge objectives inside the same right edge as the menu rows.
void draw_wrapped_text(const char* text, float x, float y,
                       std::size_t maxCharacters, int maxLines,
                       float lineAdvance, float referencePixelHeight,
                       int red, int green, int blue, int alpha = 255) noexcept {
    if (text == nullptr || maxCharacters == 0 || maxLines <= 0) return;
    const char* cursor = text;
    for (int lineIndex = 0; lineIndex < maxLines && *cursor != '\0'; ++lineIndex) {
        while (*cursor == ' ') ++cursor;
        if (*cursor == '\0') break;

        const std::size_t remaining = std::strlen(cursor);
        std::size_t count = std::min(remaining, maxCharacters);
        if (remaining > maxCharacters) {
            std::size_t breakAt = count;
            while (breakAt > 0 && cursor[breakAt] != ' ') --breakAt;
            if (breakAt > 0) count = breakAt;
        }

        char line[128]{};
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
        draw_text(line, x, y + static_cast<float>(lineIndex) * lineAdvance,
                  referencePixelHeight, red, green, blue, alpha);
        cursor = next;
    }
}

// Shared horizontal bounds for rank bars on category and reinforced-item
// detail pages. The right edge is the exact right edge of draw_menu_row plates.
constexpr float kMenuContentLeft = 0.071f;
constexpr float kMenuRewardRowRight = 0.212f + 0.365f * 0.5f;
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
                   int selected, bool accent = false) noexcept {
    // Match the header title plate's exact horizontal bounds. Keep row height
    // compact so the list rows remain separate and do not overlap vertically.
    constexpr float kPanelCenter = 0.212f;
    constexpr float kPanelWidth = 0.365f;
    constexpr float kPanelLeft = kPanelCenter - kPanelWidth * 0.5f;
    constexpr float kRowStartY = 0.205f;
    constexpr float kRowHeight = 0.062f;
    const float y = kRowStartY + static_cast<float>(row) * kRowHeight;

    // Layered rows borrow the compact, high-contrast menu rhythm used by
    // Rampage-inspired row rhythm and bronze selection accent.
    draw_rect(kPanelCenter, y + 0.020f,
              kPanelWidth, 0.052f,
              selected ? 108 : 18, selected ? 96 : 16, selected ? 78 : 14,
              selected ? 235 : 152);
    if (selected) {
        draw_rect(kPanelLeft + 0.004f, y + 0.020f, 0.0035f, 0.052f,
                  191, 150, 86, 245);
    }
    draw_text(label, 0.063f, y, 18.0f,
              selected ? 255 : (accent ? 235 : 222),
              selected ? 250 : (accent ? 206 : 216),
              selected ? 238 : (accent ? 150 : 204));
    if (status != nullptr && status[0] != '\0') {
        draw_text(status, 0.315f, y + 0.004f, 13.0f,
                  selected ? 245 : 194, selected ? 229 : 184, selected ? 205 : 163);
    }
}

void draw_menu_header(const char* title, const char* subtitle) noexcept {
    draw_rect(0.212f, 0.50f, 0.395f, 0.94f, 0, 0, 0, 226);
    draw_rect(0.212f, 0.095f, 0.365f, 0.125f, 6, 8, 10, 250);
    draw_rect(0.212f, 0.157f, 0.34f, 0.0024f, 180, 150, 96, 235);
    draw_text("HUNTING COMPANION", 0.071f, 0.050f, 22.0f, 240, 232, 215);
    draw_text(title, 0.071f, 0.112f, 20.0f, 228, 190, 119);
    if (subtitle != nullptr) draw_text(subtitle, 0.071f, 0.168f, 14.0f, 196, 190, 176);
}

void draw_menu_footer() noexcept {
    draw_rect(0.212f, 0.895f, 0.36f, 0.0024f, 145, 128, 98, 230);
    draw_text("UP/DOWN Navigate   ENTER Select   BACKSPACE Back", 0.065f, 0.858f, 13.0f, 218, 211, 197);
    char footer[48]{};
    std::snprintf(footer, sizeof(footer), "%s Close", key_name(g_menuHotkey));
    draw_text(footer, 0.065f, 0.910f, 12.0f, 190, 184, 170);
}

void draw_list_page(const char* title, const char* subtitle,
                    int total, const char* (*labelAt)(int, char*, std::size_t),
                    const char* (*statusAt)(int, char*, std::size_t)) noexcept {
    draw_menu_header(title, subtitle);
    const int start = g_menu.scroll;
    const int end = std::min(total, start + kVisibleMenuRows);
    for (int i = start; i < end; ++i) {
        char label[128]{};
        char status[40]{};
        const char* labelText = labelAt ? labelAt(i, label, sizeof(label)) : "";
        const char* statusText = statusAt ? statusAt(i, status, sizeof(status)) : "";
        draw_menu_row(labelText, statusText, i - start, i == g_menu.selection);
    }
    char count[48]{};
    std::snprintf(count, sizeof(count), "%d of %d", total > 0 ? g_menu.selection + 1 : 0, total);
    draw_text(count, 0.31f, 0.81f, 12.0f, 170, 164, 150);
    draw_menu_footer();
}

const char* root_label(int index, char*, std::size_t) noexcept {
    static const char* labels[] = {
        "Pearson", "Trapper", "Challenges", "Settings", "Close"
    };
    return (index >= 0 && index < 5) ? labels[index] : "";
}
const char* no_status(int, char*, std::size_t) noexcept { return ""; }

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
    return item != nullptr ? item->itemName : "";
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
    std::snprintf(buffer, size, "LOCKED R%u", item->unlockRank);
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
    return vendor.recipes[static_cast<std::size_t>(g_menuRecipeIndices[static_cast<std::size_t>(index)])].name;
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
        std::snprintf(buffer, size, "LOCKED R%u", item->unlockRank);
        return buffer;
    }
    if (recipe.owned) return "CRAFTED";
    bool hasAnimal = false;
    for (std::size_t i = 0; i < recipe.costCount; ++i) {
        if (recipe.costs[i].hash != kMoneyHash) { hasAnimal = true; break; }
    }
    std::snprintf(buffer, size, "%s", hasAnimal ? "MATERIALS" : "OTHER COSTS");
    return buffer;
}

void draw_settings_page() noexcept {
    draw_menu_header("SETTINGS", "Configuration is saved to HuntingCompanion.ini");
    char hotkeyLine[80]{};
    std::snprintf(hotkeyLine, sizeof(hotkeyLine), "Menu hotkey: %s", key_name(g_menuHotkey));
    draw_menu_row(hotkeyLine, "CHANGE", 0, g_menu.selection == 0);
    draw_menu_row("Back", "", 1, g_menu.selection == 1);
    if (g_menu.captureHotkey) {
        draw_rect(0.212f, 0.52f, 0.34f, 0.12f, 0, 0, 0, 245);
        draw_text("PRESS A KEY TO ASSIGN THE MENU HOTKEY", 0.083f, 0.49f, 16.0f, 230, 205, 150);
        draw_text("ESC cancels", 0.15f, 0.55f, 13.0f, 210, 205, 195);
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
                      "NATIVE RETURNED: %d COMPLETED / %d MAX (EXPECTED MAX 10)",
                      progress.ranksCompleted, progress.maxRanks);
        draw_text(nativeResult, 0.071f, 0.292f, 12.0f, 224, 211, 188);
        char rootLine[96]{};
        std::snprintf(rootLine, sizeof(rootLine), "CATEGORY ROOT: %s",
                      definition.rootHashName);
        draw_text(rootLine, 0.071f, 0.337f, 11.0f, 190, 184, 170);
        draw_text("Check Story Mode state and ScriptHook compatibility.",
                  0.071f, 0.379f, 11.0f, 190, 184, 170);
    } else {
        char rankLine[64]{};
        std::snprintf(rankLine, sizeof(rankLine), "RANKS COMPLETED: %d / %d",
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
            std::snprintf(activeRank, sizeof(activeRank), "CURRENT RANK: %d / %d",
                          progress.ranksCompleted + 1, progress.maxRanks);
            draw_text(activeRank, 0.071f, 0.281f, 13.0f, 215, 202, 177);
            draw_text("NEXT RANK OBJECTIVE", 0.071f, 0.326f,
                      11.0f, 220, 184, 112);
            const char* objective = definition.rankObjectives[progress.ranksCompleted];
            draw_wrapped_text(objective, 0.071f, 0.350f, 48, 2, 0.028f, 13.0f,
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
        draw_menu_row(labelText, statusText, i + 6, i == g_menu.selection);
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
        draw_menu_header(recipe->name, "REINFORCED EQUIPMENT / CHALLENGE REWARD");
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
        std::snprintf(requirement, sizeof(requirement), "Complete the %s Challenge Rank %u.",
                      reinforced->challengeName, reinforced->unlockRank);
        draw_text(requirement, 0.071f, 0.402f, 14.0f, 235, 224, 200);
        if (progress != nullptr && progress->available) {
            char ranks[64]{};
            std::snprintf(ranks, sizeof(ranks), "RANKS COMPLETED: %d / %d",
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
                              "CURRENT RANK: %d / %d",
                              progress->ranksCompleted + 1, progress->maxRanks);
                draw_text(currentRank, 0.071f, 0.510f, 12.0f, 215, 202, 177);
                draw_text("NEXT RANK OBJECTIVE", 0.071f, 0.546f,
                          11.0f, 220, 184, 112);
                if (challenge != nullptr) {
                    const char* objective =
                        challenge->rankObjectives[progress->ranksCompleted];
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
    draw_menu_header(recipe->name, garmentSet != nullptr ? "GARMENT SET / INDIVIDUAL PIECES" :
                     (g_menu.trapper ? "TRAPPER RECIPE" : "PEARSON RECIPE"));
    if (garmentSet != nullptr) {
        draw_text("COMPONENT GARMENTS AND THEIR CATALOG MATERIAL COSTS", 0.071f, 0.215f,
                  14.0f, 220, 184, 112);
        draw_text("Donated counts use shared Trapper stock; they are not reserved per piece.",
                  0.071f, 0.258f, 11.0f, 188, 184, 174);
        DetailLine lines[64]{};
        const std::size_t lineCount = build_detail_lines(recipe, lines, std::size(lines));
        constexpr int kDetailRows = 8;
        const int start = g_menu.scroll;
        const int end = std::min(static_cast<int>(lineCount), start + kDetailRows);
        for (int i = start; i < end; ++i) {
            const DetailLine& line = lines[static_cast<std::size_t>(i)];
            const float y = 0.330f + static_cast<float>(i - start) * 0.056f;
            if (i == g_menu.selection) {
                draw_rect(0.212f, y + 0.018f, 0.365f, 0.048f, 64, 53, 40, 205);
                draw_rect(0.0315f, y + 0.018f, 0.0035f, 0.048f, 191, 150, 86, 245);
            }
            if (line.heading) {
                draw_text(line.componentName ? line.componentName : "Unknown garment",
                          0.071f, y, 14.0f, 236, 198, 139);
                if (line.component != nullptr) {
                    draw_text(line.component->owned ? "CRAFTED" : "TO CRAFT", 0.315f, y + 0.002f,
                              11.0f, line.component->owned ? 130 : 210,
                              line.component->owned ? 205 : 184,
                              line.component->owned ? 145 : 120);
                } else {
                    draw_text("NOT IN CATALOG", 0.290f, y + 0.002f, 10.0f, 205, 145, 120);
                }
            } else if (line.cost != nullptr && line.component != nullptr) {
                const char* material = find_material_name(line.cost->hash);
                char label[112]{};
                if (material != nullptr) std::snprintf(label, sizeof(label), "    %s", material);
                else std::snprintf(label, sizeof(label), "    Material %08X", line.cost->hash);
                draw_text(label, 0.078f, y, 13.0f, 224, 218, 205);
                char right[32]{};
                if (line.component->owned) {
                    std::snprintf(right, sizeof(right), "Recipe cost: %d", line.cost->amount);
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
        draw_text("Counts use vendor donation stock, not Arthur's inventory.", 0.071f, 0.258f, 12.0f, 188, 184, 174);
    }

    int visible = 0;
    const int start = g_menu.scroll;
    const int maxRows = 8;
    for (std::size_t i = 0; i < recipe->costCount; ++i) {
        const RecipeCost& cost = recipe->costs[i];
        if (cost.hash == kMoneyHash) continue;
        if (visible < start) { ++visible; continue; }
        if (visible >= start + maxRows) { ++visible; continue; }
        const char* name = find_material_name(cost.hash);
        char label[128]{};
        if (name == nullptr) std::snprintf(label, sizeof(label), "Material %08X", cost.hash);
        else std::snprintf(label, sizeof(label), "%s", name);
        const float y = 0.330f + static_cast<float>(visible - start) * 0.060f;
        if (recipe->owned) {
            char right[32]{};
            std::snprintf(right, sizeof(right), "Recipe cost: %d", cost.amount);
            draw_text(label, 0.071f, y, 17.0f, 222, 216, 203);
            draw_text(right, 0.305f, y, 15.0f, 187, 180, 164);
        } else {
            const MaterialNeed* need = find_vendor_material(vendor.demand, cost.hash);
            const int total = cost.amount;
            const int donated = need ? std::min(need->vendorStock, total) : 0;
            char right[32]{};
            std::snprintf(right, sizeof(right), "%d/%d", donated, total);
            draw_text(label, 0.071f, y, 17.0f, 240, 234, 222);
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
        draw_text("Vendor stock is pooled; the same materials can appear in other recipes.",
                  0.071f, 0.805f, 12.0f, 174, 168, 154);
    }
    draw_menu_footer();
}

void draw_menu() noexcept {
    if (g_menu.page == MenuPage::Closed) return;
    if (!g_tracker.ready) {
        draw_menu_header("LOADING CATALOG", "Reading vendor recipes and donated materials...");
        draw_menu_footer();
        return;
    }
    switch (g_menu.page) {
        case MenuPage::Root:
            draw_list_page("HUNTING COMPANION", "Crafting, challenges and garment sets", 5, root_label, no_status);
            break;
        case MenuPage::Categories: {
            char subtitle[80]{};
            std::snprintf(subtitle, sizeof(subtitle), "%s - Select a category", active_vendor_name());
            draw_list_page("CATEGORIES", subtitle, static_cast<int>(g_menuCategoryCount),
                           category_row_label, category_row_status);
            break;
        }
        case MenuPage::Recipes: {
            char subtitle[96]{};
            std::snprintf(subtitle, sizeof(subtitle), "%s / %s",
                          active_vendor_name(), category_label(g_menu.trapper, g_menu.categoryHash));
            draw_list_page("RECIPES", subtitle, static_cast<int>(g_menuRecipeCount),
                           recipe_row_label, recipe_row_status);
            break;
        }
        case MenuPage::Detail: draw_recipe_detail(); break;
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
    // One initial scan, then event-driven scans requested when the compendium
    // is opened. No timer-based polling or 30-second refresh loop.
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

    // Fallback for ScriptHook builds without the additional-thread export:
    // refresh on compendium open, never on a timer.
    if (!g_refreshWorkerRegistered) {
        build_tracker_state();
        InterlockedExchange(&g_refreshRequested, 0);
    }

    for (;;) {
        {
            TrackerReadLock trackerReadLock;
            update_menu_input();
        }
        if (!g_refreshWorkerRegistered && InterlockedExchange(&g_refreshRequested, 0) != 0) {
            build_tracker_state();
        }
        {
            TrackerReadLock trackerReadLock;
            update_hud();
            draw_menu();
        }
        g_api.wait(0);
    }
}
} // namespace

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        g_module = module;
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
