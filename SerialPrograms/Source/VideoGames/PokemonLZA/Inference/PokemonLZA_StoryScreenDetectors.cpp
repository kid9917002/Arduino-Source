/*  Story Screen Detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
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



bool read_camera_heading(const ImageViewRGB32& screen, double& degrees){
    //  Minimap center and radii, measured on 16:9 frames. Distances are in
    //  units of the frame width so the disk stays round.
    const double CENTER_X = 0.0960;
    const double CENTER_Y = 0.1655;
    const double OUTER_RADIUS = 0.058;
    const double INNER_RADIUS = 0.012;   //  skip the player arrow

    double width = (double)screen.width();
    double height = (double)screen.height();
    if (width <= 0 || height <= 0){
        return false;
    }

    double cx = CENTER_X * width;
    double cy = CENTER_Y * height;
    double r_outer = OUTER_RADIUS * width;
    double r_inner = INNER_RADIUS * width;

    long x_min = std::max<long>(0, (long)(cx - r_outer));
    long x_max = std::min<long>((long)width - 1, (long)(cx + r_outer));
    long y_min = std::max<long>(0, (long)(cy - r_outer));
    long y_max = std::min<long>((long)height - 1, (long)(cy + r_outer));

    size_t disk_pixels = 0;
    size_t cone_pixels = 0;
    double sum_x = 0;
    double sum_y = 0;
    for (long y = y_min; y <= y_max; y++){
        for (long x = x_min; x <= x_max; x++){
            double dx = (double)x - cx;
            double dy = (double)y - cy;
            double distance = std::sqrt(dx * dx + dy * dy);
            if (distance >= r_outer || distance <= r_inner){
                continue;
            }
            disk_pixels++;

            uint32_t pixel = screen.pixel((size_t)x, (size_t)y);
            int r = (pixel >> 16) & 0xff;
            int g = (pixel >>  8) & 0xff;
            int b = (pixel >>  0) & 0xff;

            //  Saturated blue = the player arrow itself, not the cone.
            bool arrow = b > 200 && r < 90 && g > 90 && g < 190;
            if (arrow){
                continue;
            }
            if (b >= r + 45 && b >= 170){
                cone_pixels++;
                sum_x += dx;
                sum_y += dy;
            }
        }
    }

    //  Overworld frames measure 2.7% to 6% cone pixels. Frames without the
    //  minimap (dialogs, cutscenes) measure under 0.5%.
    if (disk_pixels == 0 || cone_pixels * 1000 < disk_pixels * 15){
        return false;
    }

    double mean_x = sum_x / (double)cone_pixels;
    double mean_y = sum_y / (double)cone_pixels;

    //  A cone spreads to one side of the center. If the mean is near the
    //  center the reading is not reliable.
    if (std::sqrt(mean_x * mean_x + mean_y * mean_y) < 0.15 * r_outer){
        return false;
    }

    const double PI = 3.14159265358979323846;
    degrees = std::atan2(mean_x, -mean_y) * 180.0 / PI;
    if (degrees < 0){
        degrees += 360.0;
    }
    return true;
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
