#include "Renderer_Internal.h"

#include "Base/Tables.h"
#include "Blit.h"
#include "Textures.h"
#include "Video.h"

//----------------------------------------------------------------------------------------------------------------------
// Code for drawing walls and skies in the game.
//
// Notes:
//  (1) Clip values are the solid pixel bounding the range
//  (2) floorclip starts out ScreenHeight
//  (3) ceilingclip starts out -1
//----------------------------------------------------------------------------------------------------------------------
BEGIN_NAMESPACE(Renderer)

//----------------------------------------------------------------------------------------------------------------------
// Draws a single column of the sky
//----------------------------------------------------------------------------------------------------------------------
static void drawSkyColumn(const uint32_t viewX, const uint32_t maxColHeight) noexcept {
    // Figure out the angle this sky column is at
    const float colX = gNearPlaneP1x + viewX * gNearPlaneXStepPerViewCol;
    const float colY = gNearPlaneP1y + viewX * gNearPlaneYStepPerViewCol;
    const float fromVpX = colX - gViewX;
    const float fromVpY = colY - gViewY;
    const float angle = std::fmodf(FMath::ANGLE_360<float> + std::atan2f(fromVpY, fromVpX), FMath::ANGLE_360<float>);

    // Figure out the texture coordinate from the angle and wrap it.
    // Sky textures are always 256 pixels wide:
    const uint32_t texX = ((uint32_t)((angle / FMath::ANGLE_90<float>) * 256.0f)) & 255;

    // Figure out the sky column height and texel step (y)
    const Texture* const pTex = (const Texture*) Textures::getWall(Textures::getCurrentSkyTexNum());    // FIXME: don't keep doing this for each column
    const ImageData& texImg = pTex->data;

    const uint32_t skyTexH = texImg.height;
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
        texImg.pPixels,
        texImg.width,
        texImg.height,
        (float) texX,
        0.0f,
        0.0f,
        0.0f,
        Video::gFrameBuffer + gScreenYOffset * Video::SCREEN_WIDTH + gScreenXOffset,
        gScreenWidth,
        gScreenHeight,
        Video::SCREEN_WIDTH,
        viewX,
        0,
        std::min(colHeight, maxColHeight),
        0,
        texYStep
    );
}

void drawAllWallFragments() noexcept {
    for (const WallFragment& wallFrag : gWallFragments) {
        const ImageData& wallImage = *wallFrag.pImageData;

        Blit::blitColumn<
            Blit::BCF_STEP_Y |
            Blit::BCF_H_WRAP_WRAP |
            Blit::BCF_V_WRAP_WRAP |
            Blit::BCF_COLOR_MULT_RGB
        >(
            wallImage.pPixels,
            wallImage.width,
            wallImage.height,
            (float) wallFrag.texcoordX,
            wallFrag.texcoordY,
            0.0f,
            wallFrag.texcoordYSubPixelAdjust,
            Video::gFrameBuffer + gScreenYOffset * Video::SCREEN_WIDTH + gScreenXOffset,
            gScreenWidth,
            gScreenHeight,
            Video::SCREEN_WIDTH,
            wallFrag.x,
            wallFrag.y,
            wallFrag.height,
            0,
            wallFrag.texcoordYStep,
            wallFrag.lightMul,
            wallFrag.lightMul,
            wallFrag.lightMul
        );
    }
}

void drawAllSkyFragments() noexcept {
    for (const SkyFragment& skyFrag : gSkyFragments) {
        drawSkyColumn(skyFrag.x, skyFrag.height);
    }
}

END_NAMESPACE(Renderer)