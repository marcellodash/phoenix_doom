#pragma once

#include "Angle.h"
#include "Game/DoomDefines.h"   // FIXME: DC - Break the dependency on 'DoomDefines.h'

// Table related defines
static constexpr uint32_t FINEANGLES        = 8192;             // Size of the fineangle table
static constexpr uint32_t FINEMASK          = FINEANGLES - 1;   // Rounding mask for table
static constexpr uint32_t ANGLETOFINESHIFT  = 19;               // Convert angle_t to fineangle table
static constexpr uint32_t SLOPERANGE        = 2048;             // Number of entries in tantoangle table
static constexpr uint32_t SLOPEBITS         = 11;               // Power of 2 for SLOPERANGE (2<<11)

// The table data and other stuff
extern Fixed        gFineTangent[4096];
extern Fixed*       gFineCosine;
extern Fixed        gFineSine[10240];
extern angle_t      gTanToAngle[2049];
extern angle_t      gXToViewAngle[MAXSCREENWIDTH + 1];
extern int32_t      gViewAngleToX[FINEANGLES / 4];
extern uint32_t     gYSlope[MAXSCREENHEIGHT];           // 6.10 frac
extern uint32_t     gDistScale[MAXSCREENWIDTH];         // 1.15 frac
extern uint32_t     gIDivTable[8192];                   // 1.0 / 0-5500 for recipocal muls   
extern uint32_t     gCenterX;                           // Center X coord in fixed point
extern uint32_t     gCenterY;                           // Center Y coord in fixed point
extern uint32_t     gScreenWidth;                       // Width of the view screen
extern uint32_t     gScreenHeight;                      // Height of the view screen
extern Fixed        gStretch;                           // Stretch factor
extern Fixed        gStretchWidth;                      // Stretch factor * ScreenWidth
extern uint32_t     gScreenXOffset;                     // True X coord for projected screen
extern uint32_t     gScreenYOffset;                     // True Y coord for projected screen
extern uint32_t     gGunXScale;                         // Scale factor for player's weapon for X
extern uint32_t     gGunYScale;                         // Scale factor for player's weapon for Y
extern uint32_t     gLightMins[256];                    // Minimum light factors
extern uint32_t     gLightSubs[256];                    // Light subtraction
extern Fixed        gLightCoefs[256];                   // Light coeffecient
extern Fixed        gPlaneLightCoef[256];               // Plane light coeffecient
