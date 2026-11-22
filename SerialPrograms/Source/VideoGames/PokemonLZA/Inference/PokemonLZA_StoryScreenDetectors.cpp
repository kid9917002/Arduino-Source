/*  Story Screen Detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <vector>
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
    const double ARROW_RADIUS = 0.016;  //  the player arrow sits inside this
    const double INNER_RADIUS = 0.012;

    double width = (double)screen.width();
    double height = (double)screen.height();
    if (width <= 0 || height <= 0){
        return false;
    }

    double cx = CENTER_X * width;
    double cy = CENTER_Y * height;
    double r_outer = OUTER_RADIUS * width;
    double r_arrow = ARROW_RADIUS * width;
    double r_inner = INNER_RADIUS * width;

    long x_min = std::max<long>(0, (long)(cx - r_outer));
    long x_max = std::min<long>((long)width - 1, (long)(cx + r_outer));
    long y_min = std::max<long>(0, (long)(cy - r_outer));
    long y_max = std::min<long>((long)height - 1, (long)(cy + r_outer));

    size_t arrow_area = 0;
    size_t arrow_pixels = 0;
    size_t disk_pixels = 0;
    size_t cone_pixels = 0;
    double sum_x = 0;
    double sum_y = 0;
    for (long y = y_min; y <= y_max; y++){
        for (long x = x_min; x <= x_max; x++){
            double dx = (double)x - cx;
            double dy = (double)y - cy;
            double distance = std::sqrt(dx * dx + dy * dy);
            if (distance >= r_outer){
                continue;
            }

            uint32_t pixel = screen.pixel((size_t)x, (size_t)y);
            int r = (pixel >> 16) & 0xff;
            int g = (pixel >>  8) & 0xff;
            int b = (pixel >>  0) & 0xff;

            //  Saturated blue = the player arrow.
            bool arrow = b > 200 && r < 90 && g > 90 && g < 190;
            if (distance < r_arrow){
                arrow_area++;
                if (arrow){
                    arrow_pixels++;
                }
            }
            if (distance <= r_inner || arrow){
                continue;
            }
            disk_pixels++;

            //  The cone is a bright light-blue overlay. Over white it is
            //  about (212,255,251), over the map colors similar. Blue must
            //  clearly beat red and stay close to green: the green Wild Zone
            //  fill (e.g. 176,255,198) has blue far below green, and the dark
            //  navy zone marker is excluded by brightness.
            if (r + g + b >= 450 && b >= 170 && b >= r + 35 && b + 25 >= g){
                cone_pixels++;
                sum_x += dx;
                sum_y += dy;
            }
        }
    }

    //  The player arrow must be there, or this is not the minimap.
    if (arrow_area == 0 || arrow_pixels * 1000 < arrow_area * 15){
        return false;
    }

    //  Overworld frames measure 2% to 5% cone pixels.
    if (disk_pixels == 0 || cone_pixels * 1000 < disk_pixels * 12){
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



bool find_weedle(const ImageViewRGB32& screen, double& x, double& y){
    size_t width = screen.width();
    size_t height = screen.height();
    if (width < 100 || height < 100){
        return false;
    }

    //  Count orange (body) and pink (nose) pixels per grid cell.
    size_t cell = std::max<size_t>(1, width / 50);
    size_t grid_w = width / cell;
    size_t grid_h = height / cell;
    std::vector<uint32_t> orange(grid_w * grid_h, 0);
    std::vector<uint32_t> pink(grid_w * grid_h, 0);

    for (size_t py = 0; py < grid_h * cell; py++){
        double fy = (double)py / (double)height;
        if (fy < 0.30 || fy > 0.80){
            continue;   //  sky / top HUD, bottom HUD
        }
        for (size_t px = 0; px < grid_w * cell; px++){
            double fx = (double)px / (double)width;
            if (fx < 0.10){
                continue;   //  quest markers on the left edge
            }
            if (fx < 0.32 && fy > 0.70){
                continue;   //  party HUD
            }
            if (fx > 0.42 && fx < 0.58 && fy > 0.33){
                continue;   //  the player (skin and red pouch look similar)
            }

            uint32_t pixel = screen.pixel(px, py);
            int r = (pixel >> 16) & 0xff;
            int g = (pixel >>  8) & 0xff;
            int b = (pixel >>  0) & 0xff;
            int mx = std::max(r, std::max(g, b));
            int mn = std::min(r, std::min(g, b));
            double sat = mx == 0 ? 0 : (double)(mx - mn) / (double)mx;

            size_t index = (py / cell) * grid_w + (px / cell);
            if (r >= 170 && r > g + 35 && g > b + 15 && sat >= 0.35 && sat <= 0.75){
                orange[index]++;
            }else if (r >= 180 && g <= 120 && b >= 80 && b <= 170 && r > b + 50){
                pink[index]++;
            }
        }
    }

    //  A Weedle is a 3x3-cell area with enough orange and some pink. The
    //  pink must be a minority (the nose), which rules out pink hair and
    //  clothing.
    bool found = false;
    uint32_t best_score = 0;
    for (size_t gy = 1; gy + 1 < grid_h; gy++){
        for (size_t gx = 1; gx + 1 < grid_w; gx++){
            uint32_t o = 0;
            uint32_t p = 0;
            for (size_t dy = 0; dy < 3; dy++){
                for (size_t dx = 0; dx < 3; dx++){
                    size_t index = (gy + dy - 1) * grid_w + (gx + dx - 1);
                    o += orange[index];
                    p += pink[index];
                }
            }
            if (o < 12 || p < 3){
                continue;
            }
            if (p * 100 < o * 8 || p * 100 > o * 80){
                continue;
            }
            if (o + p > best_score){
                best_score = o + p;
                x = ((double)gx + 0.5) * (double)cell / (double)width;
                y = ((double)gy + 0.5) * (double)cell / (double)height;
                found = true;
            }
        }
    }
    return found;
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
