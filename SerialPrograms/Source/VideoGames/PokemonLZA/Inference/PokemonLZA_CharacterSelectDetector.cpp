/*  Character Select Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "PokemonLZA_CharacterSelectDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Measured on a 16:9 frame of the LOOKS screen.
//  Tile centers (normalized) and tile half sizes.
namespace{

const double TILE_CENTER_X[CharacterSelectDetector::COLS] = {0.2562, 0.4196, 0.5830, 0.7451};
const double TILE_CENTER_Y[CharacterSelectDetector::ROWS] = {0.4112, 0.6776};
const double TILE_HALF_WIDTH  = 0.0623;
const double TILE_HALF_HEIGHT = 0.1110;

}


CharacterSelectDetector::CharacterSelectDetector(Color color)
    : m_color(color)
    , m_highlighted(-1)
    , m_has_marker(false)
{
    for (int r = 0; r < ROWS; r++){
        for (int c = 0; c < COLS; c++){
            int index = r * COLS + c;
            double cx = TILE_CENTER_X[c];
            double cy = TILE_CENTER_Y[r];

            //  A thin vertical strip just inside the left edge of the tile at
            //  mid height. This area is portrait background (never face/hair
            //  for any of the 8 looks), so it is pure white when highlighted
            //  and dark navy otherwise.
            m_tile_boxes[index] = ImageFloatBox(
                cx - TILE_HALF_WIDTH * 0.92,
                cy - TILE_HALF_HEIGHT * 0.55,
                0.0112,
                0.0555
            );

            //  The green chevron above the tile.
            m_marker_boxes[index] = ImageFloatBox(
                cx - 0.020,
                cy - TILE_HALF_HEIGHT - 0.045,
                0.040,
                0.040
            );
        }
    }
}

void CharacterSelectDetector::make_overlays(VideoOverlaySet& items) const{
    for (int i = 0; i < TILES; i++){
        items.add(m_color, m_tile_boxes[i]);
        items.add(m_color, m_marker_boxes[i]);
    }
}


bool CharacterSelectDetector::tile_is_white(const ImageViewRGB32& screen, const ImageFloatBox& box){
    ImageStats stats = image_stats(extract_box_reference(screen, box));
    return is_white(stats, 650, 40);
}

bool CharacterSelectDetector::has_green_marker(const ImageViewRGB32& screen, const ImageFloatBox& box){
    ImageViewRGB32 image = extract_box_reference(screen, box);
    size_t width = image.width();
    size_t height = image.height();
    if (width == 0 || height == 0){
        return false;
    }

    //  Marker is a yellow-green chevron: high green, moderate red, low blue.
    size_t green_pixels = 0;
    for (size_t y = 0; y < height; y++){
        for (size_t x = 0; x < width; x++){
            uint32_t pixel = image.pixel(x, y);
            int r = (pixel >> 16) & 0xff;
            int g = (pixel >>  8) & 0xff;
            int b = (pixel >>  0) & 0xff;
            if (g >= 140 && g >= r + 30 && g >= b + 60){
                green_pixels++;
            }
        }
    }
    return green_pixels * 100 >= width * height * 3;
}


bool CharacterSelectDetector::detect(const ImageViewRGB32& screen){
    m_highlighted = -1;
    m_has_marker = false;

    int count = 0;
    int found = -1;
    for (int i = 0; i < TILES; i++){
        if (tile_is_white(screen, m_tile_boxes[i])){
            count++;
            found = i;
        }
    }
    if (count != 1){
        return false;
    }

    m_highlighted = found;
    m_has_marker = has_green_marker(screen, m_marker_boxes[found]);
    return true;
}



}
}
}
