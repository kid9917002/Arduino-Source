/*  Story Screen Detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "PokemonLZA_StoryScreenDetectors.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The dimmed background and the card header are dark with a blue tint.
bool is_dark_navy(const ImageViewRGB32& screen, const ImageFloatBox& box){
    ImageStats stats = image_stats(extract_box_reference(screen, box));
    return stats.average.sum() <= 260
        && stats.average.b >= stats.average.r + 5
        && stats.stddev.sum() <= 60;
}

}



TutorialWindowDetector::TutorialWindowDetector(Color color)
    : m_color(color)
    , m_panel(0.55, 0.50, 0.15, 0.30)
    , m_header(0.50, 0.085, 0.20, 0.05)
    , m_margin_left(0.03, 0.30, 0.15, 0.40)
    , m_margin_right(0.80, 0.30, 0.15, 0.40)
{}
void TutorialWindowDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_panel);
    items.add(m_color, m_header);
    items.add(m_color, m_margin_left);
    items.add(m_color, m_margin_right);
}
bool TutorialWindowDetector::detect(const ImageViewRGB32& screen){
    if (!is_white(extract_box_reference(screen, m_panel), 650, 40)){
        return false;
    }
    if (!is_dark_navy(screen, m_header)){
        return false;
    }
    if (!is_dark_navy(screen, m_margin_left)){
        return false;
    }
    return is_dark_navy(screen, m_margin_right);
}



namespace{

ImageFloatBox party_slot_hp_bar_box(int index){
    //  HP bars under the small party icons, measured on 16:9 frames.
    double center_x = 0.097 + 0.0410 * index;
    return ImageFloatBox(center_x - 0.010, 0.946, 0.020, 0.012);
}

}

int read_party_slot_count(const ImageViewRGB32& screen){
    int count = 0;
    for (int index = 0; index < 5; index++){
        ImageViewRGB32 image = extract_box_reference(screen, party_slot_hp_bar_box(index));
        size_t width = image.width();
        size_t height = image.height();
        if (width == 0 || height == 0){
            continue;
        }
        size_t green_pixels = 0;
        for (size_t y = 0; y < height; y++){
            for (size_t x = 0; x < width; x++){
                uint32_t pixel = image.pixel(x, y);
                int r = (pixel >> 16) & 0xff;
                int g = (pixel >>  8) & 0xff;
                int b = (pixel >>  0) & 0xff;
                if (g >= 150 && g >= r + 40 && g >= b + 40){
                    green_pixels++;
                }
            }
        }
        //  Filled slots measure 30%+ green (the bar has a dark outline).
        //  Empty slots measure 0%.
        if (green_pixels * 100 >= width * height * 20){
            count++;
        }
    }
    return count;
}

void make_party_slot_overlays(VideoOverlaySet& items, Color color){
    for (int index = 0; index < 5; index++){
        items.add(color, party_slot_hp_bar_box(index));
    }
}



CutsceneLetterboxDetector::CutsceneLetterboxDetector(Color color)
    : m_color(color)
    , m_top(0.15, 0.01, 0.50, 0.06)
    , m_bottom_left(0.02, 0.92, 0.18, 0.06)
    , m_bottom_right(0.80, 0.92, 0.18, 0.06)
{}
void CutsceneLetterboxDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, m_top);
    items.add(m_color, m_bottom_left);
    items.add(m_color, m_bottom_right);
}
bool CutsceneLetterboxDetector::detect(const ImageViewRGB32& screen){
    if (!is_black(extract_box_reference(screen, m_top), 60, 30)){
        return false;
    }
    //  The dialog box can cover part of the bottom bar.
    return is_black(extract_box_reference(screen, m_bottom_left), 60, 30)
        || is_black(extract_box_reference(screen, m_bottom_right), 60, 30);
}



}
}
}
