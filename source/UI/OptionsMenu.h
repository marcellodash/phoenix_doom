#pragma once

struct player_t;

void O_Init() noexcept;
void O_Control(player_t* const pPlayer) noexcept;   // N.B: Player is OPTIONAL!
void O_Drawer(const bool bPresent, const bool bSaveFrameBuffer) noexcept;
