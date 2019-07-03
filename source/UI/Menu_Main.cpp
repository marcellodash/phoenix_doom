#include "Menu_Main.h"

#include "Burger.h"
#include "Game/Data.h"
#include "Game/DoomDefines.h"
#include "Game/DoomRez.h"
#include "Game/Resources.h"
#include "Intermission_Main.h"
#include "Options_Main.h"
#include "ThreeDO.h"

#define CURSORX 50      // X coord of skull cursor 
#define AREAY 66
#define DIFFICULTYY 116
#define OPTIONSY 166

typedef enum {
    level,
    difficulty,
    options,
    NUMMENUITEMS
} menu_t;

enum {      // Enum to index to the shapes 
    DIFFSHAPE1,
    DIFFSHAPE2,
    DIFFSHAPE3,
    DIFFSHAPE4,
    DIFFSHAPE5,
    AREASHAPE,
    DIFFSHAPE
};

static uint32_t gCursorFrame;                   // Skull animation frame 
static uint32_t gCursorCount;                   // Time mark to animate the skull 
static uint32_t gCursorPos;                     // Y position of the skull 
static uint32_t gMoveCount;                     // Time mark to move the skull 
static uint32_t gPlayerMap;                     // Map requested 
static uint32_t gPlayerSkill;                   // Requested skill 
static uint32_t gSleepMark;                     // Time from last access
static uint32_t gCursorYs[NUMMENUITEMS] = {
    AREAY -2 , 
    DIFFICULTYY - 2,
    OPTIONSY - 2
};
static uint32_t gOptionActive;

/**********************************

    Init memory needed for the main game menu

**********************************/

void M_Start(void)
{
    gCursorCount = 0;               // Init the animation timer 
    gCursorFrame = 0;               // Init the animation frame 
    gCursorPos = 0;                 // Topmost y position 
    gPlayerSkill = gStartSkill;     // Init the skill level 
    gPlayerMap = gStartMap;         // Init the starting map 
    gSleepMark = ReadTick();
    gOptionActive = false;          // Option screen on 
}

/**********************************

    Release memory used by the main menu

**********************************/

void M_Stop(void)
{
    WritePrefsFile();       // Save the current prefs 
}

/**********************************

    Execute every tick

**********************************/

uint32_t M_Ticker(void)
{
    uint32_t buttons;

    buttons = gJoyPadButtons;    // Get the joypad buttons 

// Exit menu if button press 

    if (gTotalGameTicks > (TICKSPERSEC/2) &&     // Minimum time... 
        ((buttons & PadStart) ||        // Start always works! 
        ((buttons & (PadA|PadB|PadC|PadStart)) && (gCursorPos!=options)))) {
        gStartMap = gPlayerMap;   // set map number 
        gStartSkill = (skill_e)gPlayerSkill;  // Set skill level 
        return ga_completed;        // done with menu 
    }
    
    if (buttons) {          // If buttons are held down then reset the timer 
        gSleepMark = ReadTick();
    }
    
    if (gOptionActive) {
        O_Control(0);
        if (gNewJoyPadButtons&PadX) {
            gOptionActive = false;
        }
        return ga_nothing;
    }
    
    if ((gNewJoyPadButtons&PadX) ||      // Pressed abort? 
        ((ReadTick()-gSleepMark)>=(TICKSPERSEC*15))) {
        return ga_died;                 // Exit now 
    }

    // Animate skull 
    gCursorCount += gElapsedTime;     // Add time 
    if (gCursorCount>=(TICKSPERSEC/4)) { // Time to toggle the shape? 
        gCursorFrame ^= 1;
        gCursorCount = 0;        // Reset the count 
    }

// Check for movement 

    if (! (buttons & (PadUp|PadDown|PadLeft|PadRight|PadA|PadB|PadC|PadD) ) ) {
        gMoveCount = TICKSPERSEC;    // Move immediately on next press 
    } else {
        gMoveCount += gElapsedTime;   // Time unit 
        if ( (gMoveCount >= (TICKSPERSEC/3)) ||      // Allow slow 
            (gCursorPos == level && gMoveCount >= (TICKSPERSEC/5))) { // Fast? 
            gMoveCount = 0;      // Reset the timer 
            if (buttons & PadDown) {
                ++gCursorPos;
                if (gCursorPos >= NUMMENUITEMS) {      // Off the bottom? 
                    gCursorPos = 0;
                }
            }
            if (buttons & PadUp) {      // Going up? 
                if (!gCursorPos) {       // At the top already? 
                    gCursorPos = NUMMENUITEMS;
                }
                --gCursorPos;
            }

            switch (gCursorPos) {
            case level:             // Select level to start with 
                if (buttons & PadRight) {
                    if (gPlayerMap < gMaxLevel) {
                        ++gPlayerMap;
                    }
                }
                if (buttons & PadLeft) {
                    if (gPlayerMap!=1) {
                        --gPlayerMap;
                    }
                }
                break;
            case difficulty:        // Select game difficulty 
                if (buttons & PadRight) {
                    if (gPlayerSkill < sk_nightmare) {
                        ++gPlayerSkill;
                    }
                }
                if (buttons & PadLeft) {
                    if (gPlayerSkill) {
                        --gPlayerSkill;
                    }
                }
                break;
            case options:
                if (buttons & (PadA|PadB|PadC|PadD|PadRight|PadLeft)) {
                    gOptionActive = true;
                }
            }
        }
    }
    return ga_nothing;      // Don't quit! 
}

//--------------------------------------------------------------------------------------------------
// Draw the main menu
//--------------------------------------------------------------------------------------------------
void M_Drawer() {    
    DrawRezShape(0, 0, rMAINDOOM);
    
    if (gOptionActive) {
        O_Drawer();
    } 
    else {    
        const void* const pShapes = loadResourceData(rMAINMENU);   // Load shape group

        // Draw new skull
        DrawMShape(CURSORX, gCursorYs[gCursorPos], GetShapeIndexPtr(loadResourceData(rSKULLS), gCursorFrame));
        releaseResource(rSKULLS);

        // Draw start level information
        PrintBigFont(CURSORX + 24, AREAY, "Level");
        PrintNumber(CURSORX + 40, AREAY + 20, gPlayerMap, 0);

        // Draw difficulty information
        DrawMShape(CURSORX + 24, DIFFICULTYY, GetShapeIndexPtr(pShapes, DIFFSHAPE));
        DrawMShape(CURSORX + 40, DIFFICULTYY + 20, GetShapeIndexPtr(pShapes, gPlayerSkill));

        // Draw the options screen
        PrintBigFont(CURSORX + 24, OPTIONSY, "Options Menu");
        releaseResource(rMAINMENU);
        UpdateAndPageFlip(true);
    }
}