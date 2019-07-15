#include "Renderer_Internal.h"

#include "Base/Endian.h"
#include "Base/FMath.h"
#include "Base/Tables.h"
#include "Blit.h"
#include "Game/Data.h"
#include "Textures.h"
#include "Video.h"
#include <algorithm>

//----------------------------------------------------------------------------------------------------------------------
// Code for drawing walls and skies in the game.
//
// Notes:
//  (1) Clip values are the solid pixel bounding the range
//  (2) floorclip starts out ScreenHeight
//  (3) ceilingclip starts out -1
//  (4) clipbounds[] = (ceilingclip +1 ) << 8 + floorclip
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(Renderer)

static uint32_t gClipBoundTop[MAXSCREENWIDTH];          // Bounds top y for vertical clipping
static uint32_t gClipBoundBottom[MAXSCREENWIDTH];       // Bounds bottom y for vertical clipping

struct drawtex_t {
    const ImageData*    pData;          // Image data for the texture
    int32_t             topheight;      // Top texture height in global pixels
    int32_t             bottomheight;   // Bottom texture height in global pixels
    Fixed               texturemid;     // Anchor point for texture
};

//----------------------------------------------------------------------------------------------------------------------
// Draw a single column of a wall clipped to the 3D view
//----------------------------------------------------------------------------------------------------------------------
static void drawClippedWallColumn(
    const int32_t viewX,
    const int32_t viewY,
    const uint32_t columnHeight,
    const float invColumnScale,
    const uint32_t texX,
    const uint32_t texY,
    const ImageData& texData
) noexcept {
    // Clip to bottom of the screen
    if (viewY >= (int32_t) gScreenHeight)
        return;
    
    // Clip to top of the screen
    const uint32_t pixelsOffscreenAtTop = (viewY < 0) ? -viewY : 0;

    if (pixelsOffscreenAtTop >= columnHeight)
        return;
    
    // Compute clipped column height and texture coordinate
    const int32_t clippedViewY = viewY + pixelsOffscreenAtTop;
    const uint32_t maxColumnHeight = gScreenHeight - clippedViewY;
    const uint32_t clippedColumnHeight = std::min(columnHeight - pixelsOffscreenAtTop, maxColumnHeight);

    const float texYStep = invColumnScale;
    const float clippedTexY = (float) texY + texYStep * (float) pixelsOffscreenAtTop;

    // Compute light multiplier
    const Fixed lightMultiplier = getLightMultiplier(gTxTextureLight, MAX_WALL_LIGHT_VALUE);

    // Do the blit
    Blit::blitColumn<
        Blit::BCF_STEP_Y |
        Blit::BCF_V_WRAP_WRAP |
        Blit::BCF_COLOR_MULT_RGB
    >(
        texData,
        (float) texX,
        (float) clippedTexY,
        Video::gFrameBuffer,
        Video::SCREEN_WIDTH,
        Video::SCREEN_HEIGHT,
        viewX + gScreenXOffset,
        clippedViewY + gScreenYOffset,
        clippedColumnHeight,
        0,
        invColumnScale,
        lightMultiplier,
        lightMultiplier,
        lightMultiplier
    );
}

//----------------------------------------------------------------------------------------------------------------------
// Draws a single column of the sky
//----------------------------------------------------------------------------------------------------------------------
static void drawSkyColumn(const uint32_t viewX) noexcept {
    // Note: sky textures are 256 pixels wide so the mask wraps it around
    const uint32_t texX = (((gXToViewAngle[viewX] + gViewAngle) >> ANGLETOSKYSHIFT) & 0xFF);

    // Figure out the sky column height and texel step (y)
    const Texture* const pTexture = (const Texture*) getWallTexture(getCurrentSkyTexNum());     // FIXME: don't keep doing this for each column
    const uint32_t skyTexH = pTexture->data.height;

    const Fixed skyScale = fixedDiv(intToFixed(gScreenHeight), intToFixed(Renderer::REFERENCE_3D_VIEW_HEIGHT));
    const Fixed scaledColHeight = fixedMul(intToFixed(skyTexH), skyScale);
    const uint32_t roundColHeight = ((scaledColHeight & FRACMASK) != 0) ? 1 : 0;
    const uint32_t colHeight = (uint32_t) fixedToInt(scaledColHeight) + roundColHeight;
    BLIT_ASSERT(colHeight < gScreenHeight);

    const float texYStep = FMath::doomFixed16ToFloat<float>(Blit::calcTexelStep(skyTexH, colHeight));

    // Draw the sky column
    Blit::blitColumn<
        Blit::BCF_STEP_Y
    >(
        pTexture->data,
        (float) texX,
        0,
        Video::gFrameBuffer,
        Video::SCREEN_WIDTH,
        Video::SCREEN_HEIGHT,
        viewX + gScreenXOffset,
        gScreenYOffset,
        colHeight,
        0,
        texYStep
    );
}

//----------------------------------------------------------------------------------------------------------------------
// Compute the screen location and part of the texture to use for the given draw texture and then draw a single wall
// column based on that information.
//----------------------------------------------------------------------------------------------------------------------
static void drawWallColumn(
    const drawtex_t& tex, 
    const uint32_t viewX,
    const uint32_t texX,
    const float wallTopY,
    const float wallBottomY,
    const float invColumnScale
) noexcept {
    // Compute height of column from source image height and make sure not invalid
    const float columnHeightF = wallBottomY - wallTopY;

    if (columnHeightF < 0)
        return;
    
    const uint32_t columnHeight = (uint32_t) columnHeightF + 1;
    
    // View Y to draw at and y position in texture to use
    const ImageData& texData = *tex.pData;
    const uint32_t texWidth = texData.width;
    const uint32_t texHeight = texData.height;

    const uint32_t viewY = (uint32_t) wallTopY;
    const uint32_t texYNotOffset = fixedToInt(tex.texturemid - (tex.topheight << FIXEDTOHEIGHT));
    const uint32_t texY = ((int32_t) texYNotOffset < 0) ? texYNotOffset + texHeight : texYNotOffset;    // DC: This is required for correct vertical alignment in some cases

    // Draw the column
    drawClippedWallColumn(
        viewX,
        viewY,
        columnHeight,
        invColumnScale,
        (texX % texWidth),      // Wraparound texture x coord!
        (texY % texHeight),     // Wraparound texture y coord!
        texData
    );
}

//----------------------------------------------------------------------------------------------------------------------
// Draw a single wall texture.
// Also save states for pending ceiling, floor and future clipping
//----------------------------------------------------------------------------------------------------------------------
static void drawSeg(const viswall_t& seg) noexcept {
    // If there is nothing (no upper or lower part) to draw for this seg then just bail immediately...
    const uint32_t actionBits = seg.WallActions;
    
    if ((actionBits & (AC_TOPTEXTURE|AC_BOTTOMTEXTURE)) == 0)
        return;

    // Lighting stuff
    const uint32_t lightLevel = seg.seglightlevel;
    const float lightMin = (float) gLightMins[lightLevel];
    const float lightMax = (float) lightLevel;
    const float lightSub = (float) gLightSubs[lightLevel];
    const float lightCoef = FMath::doomFixed16ToFloat<float>(gLightCoefs[lightLevel] << 9);
    
    // Y center of the screen
    const float viewCenterY = (float) gCenterY;
    
    // How much to step scale for each x pixel and seg center angle
    const float segLeftScale = seg.LeftScale;
    const float segScaleStep = seg.ScaleStep;
    const float segCenterAngle = FMath::doomAngleToRadians<float>(seg.CenterAngle);

    // Store the current y coordinate for the top and bottom of the top and bottom walls here.
    // Also store the step per pixel in y for the top and bottom coords.
    // This helps make the result sub-pixel accurate.
    float topTexTYStep;
    float topTexBYStep;
    float topTexTY;
    float topTexBY;
    float bottomTexTYStep;
    float bottomTexBYStep;
    float bottomTexTY;
    float bottomTexBY;
    
    // Storing some texture related params here
    drawtex_t topTex;
    drawtex_t bottomTex;

    // Setup parameters for the top and bottom wall (if present).
    // Note that if the seg is solid the 'top' wall is actually the entire wall.        
    if (actionBits & AC_TOPTEXTURE) {   
        const Texture* const pTex = seg.t_texture;

        topTex.topheight = seg.t_topheight;
        topTex.bottomheight = seg.t_bottomheight;
        topTex.texturemid = seg.t_texturemid;         
        topTex.pData = &pTex->data;

        const float topWorldTY = FMath::doomFixed6ToFloat<float>(seg.t_topheight);
        const float topWorldBY = FMath::doomFixed6ToFloat<float>(seg.t_bottomheight);

        topTexTYStep = -segScaleStep * topWorldTY;
        topTexBYStep = -segScaleStep * topWorldBY;
        topTexTY = viewCenterY - topWorldTY * segLeftScale;
        topTexBY = viewCenterY - topWorldBY * segLeftScale;
    }
        
    if (actionBits & AC_BOTTOMTEXTURE) {
        const Texture* const pTex = seg.b_texture;

        bottomTex.topheight = seg.b_topheight;
        bottomTex.bottomheight = seg.b_bottomheight;
        bottomTex.texturemid = seg.b_texturemid;
        bottomTex.pData = &pTex->data;

        const float bottomWorldTY = FMath::doomFixed6ToFloat<float>(seg.b_topheight);
        const float bottomWorldBY = FMath::doomFixed6ToFloat<float>(seg.b_bottomheight);
        
        bottomTexTYStep = -segScaleStep * bottomWorldTY;
        bottomTexBYStep = -segScaleStep * bottomWorldBY;
        bottomTexTY = viewCenterY - bottomWorldTY * segLeftScale;
        bottomTexBY = viewCenterY - bottomWorldBY * segLeftScale;
    }
        
    // Init the scale fraction and step through all the columns in the seg    
    uint32_t numColumnsDone = 0;
    
    for (int32_t viewX = seg.leftX; viewX <= seg.rightX; ++viewX) {
        const float columnScale = segLeftScale + segScaleStep * (float) numColumnsDone;
        const float invColumnScale = 1.0f / columnScale;
        
        // Calculate texture offset into shape
        const uint32_t texX = (uint32_t)(
            seg.offset - (
                std::tan(segCenterAngle - getViewAngleForX(viewX)) *
                seg.distance
            )
        );

        // Save lighting params
        {
            float texturelight = columnScale * lightCoef - lightSub;
            if (texturelight < lightMin) {
                texturelight = lightMin;
            }                
            if (texturelight > lightMax) {
                texturelight = lightMax;
            }                
            gTxTextureLight = (uint32_t) texturelight;
        }
        
        // Daw the top and bottom textures (if present) and update increments for the next column
        if (actionBits & AC_TOPTEXTURE) {
            drawWallColumn(topTex, viewX, texX, topTexTY, topTexBY, invColumnScale);
            topTexTY += topTexTYStep;
            topTexBY += topTexBYStep;
        }

        if (actionBits & AC_BOTTOMTEXTURE) {
            drawWallColumn(bottomTex, viewX, texX, bottomTexTY, bottomTexBY, invColumnScale);
            bottomTexTY += bottomTexTYStep;
            bottomTexBY += bottomTexBYStep;
        }
        
        ++numColumnsDone;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Given a span of pixels, see if it is already defined in a record somewhere.
// If it is, then merge it otherwise make a new plane definition.
//----------------------------------------------------------------------------------------------------------------------
static visplane_t* findPlane(
    visplane_t& plane,
    const Fixed height,
    const uint32_t picHandle,
    const int32_t start,
    const int32_t stop,
    const uint32_t light
) noexcept {
    visplane_t* pPlane = &plane + 1;    // Automatically skip to the next plane

    while (pPlane < gpEndVisPlane) {
        if ((height == pPlane->height) &&           // Same plane as before?
            (picHandle == pPlane->picHandle) &&
            (light == pPlane->planeLight) &&
            (pPlane->cols[start].isUndefined())     // Not defined yet?
        ) {
            if (start < pPlane->minX) {     // In range of the plane?
                pPlane->minX = start;       // Mark the new edge
            }
            
            if (stop > pPlane->maxX) {
                pPlane->maxX = stop;        // Mark the new edge
            }

            return pPlane;                  // Use the same one as before
        }

        ++pPlane;
    }
    
    // Make a new plane
    ASSERT_LOG(gpEndVisPlane - gVisPlanes < MAXVISPLANES, "No more visplanes!");
    pPlane = gpEndVisPlane;
    ++gpEndVisPlane;

    pPlane->height = height;            // Init all the vars in the visplane
    pPlane->picHandle = picHandle;
    pPlane->minX = start;
    pPlane->maxX = stop;
    pPlane->planeLight = light;         // Set the light level

    // Quickly fill in the visplane table:
    // A brute force method to fill in the visplane record FAST!
    {
        ColumnYBounds* pSet = pPlane->cols;  

        for (uint32_t j = gScreenWidth / 8; j > 0; --j) {
            pSet[0] = ColumnYBounds::UNDEFINED();
            pSet[1] = ColumnYBounds::UNDEFINED();
            pSet[2] = ColumnYBounds::UNDEFINED();
            pSet[3] = ColumnYBounds::UNDEFINED();
            pSet[4] = ColumnYBounds::UNDEFINED();
            pSet[5] = ColumnYBounds::UNDEFINED();
            pSet[6] = ColumnYBounds::UNDEFINED();
            pSet[7] = ColumnYBounds::UNDEFINED();
            pSet += 8;
        }
    }

    return pPlane;
}

//----------------------------------------------------------------------------------------------------------------------
// Do a fake wall rendering so I can get all the visplane records.
// This is a fake-o routine so I can later draw the wall segments from back to front.
//----------------------------------------------------------------------------------------------------------------------
static void segLoop(const viswall_t& seg) noexcept {
    // Note: visplanes[0] is zero to force a FindPlane on the first pass    
    float _scale = seg.LeftScale;                   // Init the scale fraction
    visplane_t* pFloorPlane = gVisPlanes;           // Reset the visplane pointers
    visplane_t* pCeilPlane = gVisPlanes;            // Reset the visplane pointers
    const uint32_t actionBits = seg.WallActions;

    for (int32_t viewX = seg.leftX; viewX <= seg.rightX; ++viewX) {
        float scale = std::fmin(_scale, MAX_RENDER_SCALE);      // Current scaling factor        
        const int32_t ceilingClipY = gClipBoundTop[viewX];      // Get the top y clip
        const int32_t floorClipY = gClipBoundBottom[viewX];     // Get the bottom y clip

        // Shall I add the floor?
        if (actionBits & AC_ADDFLOOR) {
            int32_t top = (int32_t)(gCenterY - scale * FMath::doomFixed6ToFloat<float>(seg.floorheight));   // Y coord of top of floor
            if (top <= ceilingClipY) {
                top = ceilingClipY + 1;       // Clip the top of floor to the bottom of the visible area
            }
            int32_t bottom = floorClipY - 1;    // Draw to the bottom of the screen

            if (top <= bottom) {    // Valid span?
                if (pFloorPlane->cols[viewX].isDefined()) {     // Not already covered?
                    pFloorPlane = findPlane(
                        *pFloorPlane,
                        seg.floorheight,
                        seg.FloorPic,
                        viewX,
                        seg.rightX,
                        seg.seglightlevel
                    );
                }
                if (top) {
                    --top;
                }
                pFloorPlane->cols[viewX] = ColumnYBounds{ (uint16_t) top, (uint16_t) bottom };      // Set the new vertical span
            }
        }

        // Handle ceilings
        if (actionBits & AC_ADDCEILING) {
            int32_t top = ceilingClipY + 1;     // Start from the ceiling
            int32_t bottom = (int32_t)(gCenterY - 1.0f - scale * FMath::doomFixed6ToFloat<float>(seg.ceilingheight));   // Bottom of the height
            
            if (bottom >= floorClipY) {     // Clip the bottom?
                bottom = floorClipY - 1;
            }

            if (top <= bottom) {    // Valid span?
                if (pCeilPlane->cols[viewX].isDefined()) {      // Already in use?
                    pCeilPlane = findPlane(
                        *pCeilPlane,
                        seg.ceilingheight,
                        seg.CeilingPic,
                        viewX,
                        seg.rightX,
                        seg.seglightlevel
                    );
                }
                if (top) {
                    --top;
                }
                pCeilPlane->cols[viewX] = { (uint16_t) top, (uint16_t) bottom };    // Set the vertical span
            }
        }

        // Sprite clip sils
        if (actionBits & (AC_BOTTOMSIL|AC_NEWFLOOR)) {
            int32_t low = (int32_t)(gCenterY - scale * FMath::doomFixed6ToFloat<float>(seg.floornewheight));
            if (low > floorClipY) {
                low = floorClipY;
            }
            if (low < 0) {
                low = 0;
            }
            if (actionBits & AC_BOTTOMSIL) {
                seg.BottomSil[viewX] = low;
            } 
            if (actionBits & AC_NEWFLOOR) {
                gClipBoundBottom[viewX] = low;
            }
        }

        if (actionBits & (AC_TOPSIL|AC_NEWCEILING)) {
            int32_t high = (int32_t)(gCenterY - 1.0f - scale * FMath::doomFixed6ToFloat<float>(seg.ceilingnewheight));
            if (high < ceilingClipY) {
                high = ceilingClipY;
            }
            if (high > (int32_t) gScreenHeight - 1) {
                high = gScreenHeight - 1;
            }
            if (actionBits & AC_TOPSIL) {
                seg.TopSil[viewX] = high + 1;
            }
            if (actionBits & AC_NEWCEILING) {
                gClipBoundTop[viewX] = high;
            }
        }

        // I can draw the sky right now!!
        if (actionBits & AC_ADDSKY) {
            int32_t bottom = (int32_t)(gCenterY - scale * FMath::doomFixed6ToFloat<float>(seg.ceilingheight));
            if (bottom > floorClipY) {
                bottom = floorClipY;
            }

            if (ceilingClipY + 1 < bottom) {    // Valid?
                drawSkyColumn(viewX);           // Draw the sky
            }
        }

        // Step to the next scale
        _scale += seg.ScaleStep;
    }
}

//----------------------------------------------------------------------------------------------------------------------
// Follow the list of walls and draw each and every wall fragment.
// Note : I draw the walls closest to farthest and I maintain a ZBuffer
//----------------------------------------------------------------------------------------------------------------------
void drawAllLineSegs() noexcept {
    // Init the vertical clipping records
    for (uint32_t i = 0; i < gScreenWidth; ++i) {
        gClipBoundTop[i] = -1;                  // Allow to the ceiling
        gClipBoundBottom[i] = gScreenHeight;    // Stop at the floor
    }

    // Process all the wall segments: create the visplanes and draw the sky only
    viswall_t* const pStartWall = gVisWalls;
    viswall_t* const pEndWall = gpEndVisWall;

    for (viswall_t* pWall = pStartWall; pWall < pEndWall; ++pWall) {
        segLoop(*pWall);
    }
    
    // Now I actually draw the walls back to front to allow for clipping because of slop.
    // Each wall is only drawn if needed...
    for (viswall_t* pWall = gpEndVisWall - 1; pWall >= pStartWall; --pWall) {
        drawSeg(*pWall);
    }
}

END_NAMESPACE(Renderer)
