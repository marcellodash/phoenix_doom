#pragma once

#include "Base/Macros.h"
#include <cstdint>

struct mobj_t;
struct player_t;
struct state_t;

// Sprites to draw on the playscreen
enum psprnum_e {
    ps_weapon,      // Currently selected weapon
    ps_flash,       // Weapon muzzle flash
    NUMPSPRITES     // Number of shapes for array sizing
};

// Describe the state for a player gun sprite
struct pspdef_t {
    NON_ASSIGNABLE_STRUCT(pspdef_t)

    const state_t*  StatePtr;   // a NULL state means not active
    uint32_t        Time;       // Time to elapse before next state
    int32_t         WeaponX;    // X and Y in pixels
    int32_t         WeaponY;
};

void LowerPlayerWeapon(player_t& player) noexcept;
void A_WeaponReady(player_t& player, pspdef_t& psp) noexcept;
void A_ReFire(player_t& player, pspdef_t& psp) noexcept;
void A_Lower(player_t& player, pspdef_t& psp) noexcept;
void A_Raise(player_t& player, pspdef_t& psp) noexcept;
void A_GunFlash(player_t& player, pspdef_t& psp) noexcept;
void A_Punch(player_t& player, pspdef_t& psp) noexcept;
void A_Saw(player_t& player, pspdef_t& psp) noexcept;
void A_FireMissile(player_t& player, pspdef_t& psp) noexcept;
void A_FireBFG(player_t& player, pspdef_t& psp) noexcept;
void A_FirePlasma(player_t& player, pspdef_t& psp) noexcept;
void A_FirePistol(player_t& player, pspdef_t& psp) noexcept;
void A_FireShotgun(player_t& player, pspdef_t& psp) noexcept;
void A_CockSgun(player_t& player, pspdef_t& psp) noexcept;
void A_FireCGun(player_t& player, pspdef_t& psp) noexcept;
void A_Light0(player_t& player, pspdef_t& psp) noexcept;
void A_Light1(player_t& player, pspdef_t& psp) noexcept;
void A_Light2(player_t& player, pspdef_t& psp) noexcept;
void A_BFGSpray(mobj_t& mo) noexcept;
void A_BFGsound(player_t& player, pspdef_t& psp) noexcept;
void SetupPSprites(player_t& curplayer) noexcept;
void MovePSprites(player_t& curplayer) noexcept;
