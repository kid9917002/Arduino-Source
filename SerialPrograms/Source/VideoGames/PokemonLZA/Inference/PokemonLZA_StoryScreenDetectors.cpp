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
