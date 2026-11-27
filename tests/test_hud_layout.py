#!/usr/bin/env python3
"""HUD layout, colour choices and the wrapping Settings list: source wiring."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

required = (
    # Every HUD icon goes through the group placement.
    "hud_place_icon(which, kHudWeapons, defaultX, centerY, 0.0f, 0.0f, layout)",
    "const int which = row.icons[i].ammo ? kHudAmmoIcon : kHudWeaponIcon;",
    "hud_place_icon(kHudWeaponIcon, kHudWeapons, centerX, rowTop + kIconHeight * 0.5f,",
    "hud_place_icon(kHudPearsonIcon, kHudVendors, layout.pearsonCenterX, layout.centerY,",
    "hud_place_icon(kHudTrapperIcon, kHudVendors, layout.trapperCenterX, layout.centerY,",
    "return g_hudPlacement[icon].scale * hud_scale(group);",
    "return g_hudPlacement[group].scale * g_hudPlacement[kHudAll].scale;",
    # Saved and loaded, bounded.
    'WritePrivateProfileStringA("HUD", key, value, g_iniPath);',
    "placement.scale = std::clamp(placement.scale, kHudMinScale, kHudMaxScale);",
    # Moving: Enter keeps, Backspace restores, the game ignores its controls.
    "placement = g_menu.adjustStart;",
    "invoke_native<void>(kDisableAllControlActions, 0);",
    "if (hud_preview_active()) {",
    # Colours.
    'WritePrivateProfileStringA("General", "TextColour", value, g_iniPath);',
    "apply_text_colour(&red, &green, &blue);",
    "hud_label_colour(&red, &green, &blue);",
    # Optional: icons on low-star animals only for what they can give.
    "const bool lowStar = !g_lowStarIcons && !legendary && quality >= 0 && quality < 2;",
    "if (lowStar && !needsPearson && !needsTrapper) return;",
    # Settings wraps only on a new key press.
    "move_settings_selection(1, key_pressed(VK_DOWN));",
    "move_settings_selection(-1, key_pressed(VK_UP));",
)
for token in required:
    if token not in SOURCE:
        raise AssertionError(f"Missing HUD layout/colour wiring: {token}")
print("PASS: HUD layout groups, colour choices and Settings wrap-around wiring.")
