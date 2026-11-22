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
#include "Kernels/Waterfill/Kernels_Waterfill.h"
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "PokemonLZA_StoryScreenDetectors.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Kernels::Waterfill;


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



bool find_minimap_zone_marker(const ImageViewRGB32& screen, double& dx, double& dy){
    const double CENTER_X = 0.0960;
    const double CENTER_Y = 0.1655;
    const double RADIUS = 0.055;    //  in frame widths

    double width = (double)screen.width();
    double height = (double)screen.height();
    if (width < 100 || height < 100){
        return false;
    }

    //  Square around the minimap, in normalized coordinates.
    double r_x = RADIUS;
    double r_y = RADIUS * width / height;
    ImageFloatBox box(CENTER_X - r_x, CENTER_Y - r_y, 2 * r_x, 2 * r_y);
    ImageViewRGB32 area = extract_box_reference(screen, box);

    //  The marker is dark and unsaturated. The map itself (beige, white,
    //  green) is bright, and the player arrow is bright blue.
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(
        area,
        0, 150,
        0, 150,
        0, 150
    );
    std::vector<WaterfillObject> objects = find_objects_inplace(matrix, 20);

    double cx_px = CENTER_X * width - (double)(size_t)(box.x * width);
    double cy_px = CENTER_Y * height - (double)(size_t)(box.y * height);
    double min_area = 0.00004 * width * width;
    double max_area = 0.0015 * width * width;

    bool found = false;
    size_t best_area = 0;
    for (const WaterfillObject& object : objects){
        if ((double)object.area < min_area || (double)object.area > max_area){
            continue;
        }
        double ox = object.center_of_gravity_x() - cx_px;
        double oy = object.center_of_gravity_y() - cy_px;
        double distance = std::sqrt(ox * ox + oy * oy) / width;
        if (distance > 0.052){
            continue;   //  outside the minimap disk (game scene in the corners)
        }

        //  Must be unsaturated: average the object's pixels.
        double sum_r = 0;
        double sum_g = 0;
        double sum_b = 0;
        size_t count = 0;
        //  (Pixels in the bounding box that pass the same dark filter.)
        for (size_t y = object.min_y; y < object.max_y; y++){
            for (size_t x = object.min_x; x < object.max_x; x++){
                uint32_t pixel = area.pixel(x, y);
                uint32_t r = (pixel >> 16) & 0xff;
                uint32_t g = (pixel >>  8) & 0xff;
                uint32_t b = (pixel >>  0) & 0xff;
                if (r > 150 || g > 150 || b > 150){
                    continue;
                }
                sum_r += r;
                sum_g += g;
                sum_b += b;
                count++;
            }
        }
        if (count == 0){
            continue;
        }
        double ar = sum_r / count;
        double ag = sum_g / count;
        double ab = sum_b / count;
        double mx = std::max(ar, std::max(ag, ab));
        double mn = std::min(ar, std::min(ag, ab));
        if (mx - mn > 60){
            continue;
        }

        if (object.area > best_area){
            best_area = object.area;
            dx = ox / width;
            dy = oy / width;
            found = true;
        }
    }
    return found;
}



bool find_locked_target_hp_bar(const ImageViewRGB32& screen, ImageFloatBox& bar, std::string* debug){
    size_t width = screen.width();
    size_t height = screen.height();
    if (width < 100 || height < 100){
        return false;
    }

    //  Skip the bottom of the screen: the party HUD has green HP bars too.
    const ImageFloatBox SEARCH_AREA(0.0, 0.0, 1.0, 0.80);
    ImageViewRGB32 area = extract_box_reference(screen, SEARCH_AREA);

    //  The bar is a flat, strong green, measured at about (57, 219, 112) in
    //  a screenshot. Allow a wide range: the capture card and the label's
    //  fade-in shift it. Grass (low green or high red) and the yellow-green
    //  Wild Zone walls (high red, low blue) stay out of this range.
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(
        area,
        0, 115,
        175, 255,
        50, 170
    );
    std::vector<WaterfillObject> objects = find_objects_inplace(matrix, 30);

    if (debug){
        //  Describe the 3 biggest green objects.
        std::vector<const WaterfillObject*> sorted;
        for (const WaterfillObject& object : objects){
            sorted.push_back(&object);
        }
        std::sort(sorted.begin(), sorted.end(), [](const WaterfillObject* a, const WaterfillObject* b){
            return a->area > b->area;
        });
        *debug = std::to_string(objects.size()) + " green objects.";
        for (size_t i = 0; i < sorted.size() && i < 3; i++){
            const WaterfillObject& object = *sorted[i];
            *debug += " [x=" + std::to_string(object.min_x) + " y=" + std::to_string(object.min_y)
                + " w=" + std::to_string(object.width()) + " h=" + std::to_string(object.height())
                + " fill=" + std::to_string((int)(object.area_ratio() * 100)) + "%]";
        }
    }

    bool found = false;
    size_t best_area = 0;
    for (const WaterfillObject& object : objects){
        double w = (double)object.width() / (double)width;
        double h = (double)object.height() / (double)height;
        if (w < 0.03 || w > 0.12){
            continue;
        }
        if (h < 0.006 || h > 0.025){
            continue;
        }
        if (object.width() < 4 * object.height()){
            continue;   //  must be a flat bar
        }
        if (object.area_ratio() < 0.6){
            continue;   //  must be solid
        }
        if (object.area > best_area){
            best_area = object.area;
            bar = ImageFloatBox(
                (double)object.min_x / (double)width,
                (double)object.min_y / (double)height,
                w, h
            );
            found = true;
        }
    }
    return found;
}

ImageFloatBox locked_target_name_box(const ImageFloatBox& bar){
    //  Measured: the name sits 0.004 to 0.055 above the bar top and is about
    //  as wide as the bar. The "Lv.4" line above it is left out.
    double center_x = bar.x + bar.width / 2;
    double width = bar.width * 1.6;
    return ImageFloatBox(center_x - width / 2, bar.y - 0.057, width, 0.053);
}


namespace{

//  Average color of the pixels in an object's bounding box that pass the
//  same range filter the object was found with.
bool average_in_range(
    const ImageViewRGB32& area, const WaterfillObject& object,
    uint32_t rmin, uint32_t rmax, uint32_t gmin, uint32_t gmax, uint32_t bmin, uint32_t bmax,
    double& ar, double& ag, double& ab
){
    double sum_r = 0;
    double sum_g = 0;
    double sum_b = 0;
    size_t count = 0;
    for (size_t y = object.min_y; y < object.max_y; y++){
        for (size_t x = object.min_x; x < object.max_x; x++){
            uint32_t pixel = area.pixel(x, y);
            uint32_t r = (pixel >> 16) & 0xff;
            uint32_t g = (pixel >>  8) & 0xff;
            uint32_t b = (pixel >>  0) & 0xff;
            if (r < rmin || r > rmax || g < gmin || g > gmax || b < bmin || b > bmax){
                continue;
            }
            sum_r += r;
            sum_g += g;
            sum_b += b;
            count++;
        }
    }
    if (count == 0){
        return false;
    }
    ar = sum_r / count;
    ag = sum_g / count;
    ab = sum_b / count;
    return true;
}

//  Yellow blobs (quest "!" markers). Bright yellow, not white.
std::vector<WaterfillObject> find_yellow_objects(const ImageViewRGB32& screen){
    std::vector<WaterfillObject> ret;
    double width = (double)screen.width();
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(
        screen,
        200, 255,
        170, 255,
        0, 150
    );
    std::vector<WaterfillObject> objects = find_objects_inplace(matrix, 20);
    double min_area = 0.00004 * width * width;
    for (WaterfillObject& object : objects){
        if ((double)object.area < min_area){
            continue;
        }
        double ar = 0;
        double ag = 0;
        double ab = 0;
        if (!average_in_range(screen, object, 200, 255, 170, 255, 0, 150, ar, ag, ab)){
            continue;
        }
        if (ar - ab < 90 || std::abs(ar - ag) > 55){
            continue;
        }
        ret.emplace_back(std::move(object));
    }
    return ret;
}

const double MINIMAP_CENTER_X = 0.0960;
const double MINIMAP_CENTER_Y = 0.1655;

}



bool find_minimap_quest_marker(const ImageViewRGB32& screen, double& dx, double& dy){
    double width = (double)screen.width();
    double height = (double)screen.height();
    if (width < 100 || height < 100){
        return false;
    }
    double best_area = 0;
    for (const WaterfillObject& object : find_yellow_objects(screen)){
        double ox = object.center_of_gravity_x() / width - MINIMAP_CENTER_X;
        double oy = (object.center_of_gravity_y() / height - MINIMAP_CENTER_Y) * height / width;
        if (std::sqrt(ox * ox + oy * oy) > 0.058){
            continue;
        }
        if ((double)object.area > best_area){
            best_area = (double)object.area;
            dx = ox;
            dy = oy;
        }
    }
    return best_area > 0;
}

bool find_world_quest_marker(const ImageViewRGB32& screen, ImageFloatBox& box){
    double width = (double)screen.width();
    double height = (double)screen.height();
    if (width < 100 || height < 100){
        return false;
    }
    double best_area = 0;
    for (const WaterfillObject& object : find_yellow_objects(screen)){
        double cx = (object.min_x + object.max_x) / 2.0 / width;
        double cy = (object.min_y + object.max_y) / 2.0 / height;
        double ox = cx - MINIMAP_CENTER_X;
        double oy = (cy - MINIMAP_CENTER_Y) * height / width;
        if (std::sqrt(ox * ox + oy * oy) <= 0.062){
            continue;   //  minimap
        }
        if (cx < 0.09 && cy < 0.13){
            continue;   //  quest panel icon
        }
        if (cy > 0.80){
            continue;   //  bottom HUD
        }
        double w = (double)object.width();
        double h = (double)object.height();
        if (w < 0.013 * width || w > 0.030 * width){
            continue;
        }
        if (w < 0.75 * h || w > 1.33 * h){
            continue;
        }
        if ((double)object.area < 0.4 * w * h){
            continue;
        }
        if ((double)object.area > best_area){
            best_area = (double)object.area;
            box = ImageFloatBox(object.min_x / width, object.min_y / height, w / width, h / height);
        }
    }
    return best_area > 0;
}

int read_choice_menu_row(const ImageViewRGB32& screen){
    const double ROW0_Y = 0.559;
    const double ROW_PITCH = 0.0735;

    double width = (double)screen.width();
    double height = (double)screen.height();
    if (width < 100 || height < 100){
        return -1;
    }

    ImageFloatBox search(0.55, 0.40, 0.35, 0.45);
    ImageViewRGB32 area = extract_box_reference(screen, search);
    size_t off_x = (size_t)(search.x * width);
    size_t off_y = (size_t)(search.y * height);

    //  Green pointer: yellow-green triangle left of the highlighted row.
    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(
        area,
        100, 215,
        170, 255,
        0, 110
    );
    std::vector<WaterfillObject> objects = find_objects_inplace(matrix, 20);

    double best_area = 0;
    double best_cy = 0;
    double best_right = 0;
    for (const WaterfillObject& object : objects){
        if ((double)object.area < 0.0001 * width * width){
            continue;
        }
        double ar = 0;
        double ag = 0;
        double ab = 0;
        if (!average_in_range(area, object, 100, 215, 170, 255, 0, 110, ar, ag, ab)){
            continue;
        }
        if (ag < ar + 15 || ag - ab < 90){
            continue;
        }
        if ((double)object.area > best_area){
            best_area = (double)object.area;
            best_cy = (off_y + (object.min_y + object.max_y) / 2.0) / height;
            best_right = (off_x + object.max_x) / width;
        }
    }
    if (best_area <= 0){
        return -1;
    }

    //  The highlighted row is white right of the pointer.
    ImageFloatBox row(best_right + 0.008, best_cy - 0.015, 0.10, 0.03);
    ImageViewRGB32 row_image = extract_box_reference(screen, row);
    size_t white = 0;
    size_t total = 0;
    for (size_t y = 0; y < row_image.height(); y++){
        for (size_t x = 0; x < row_image.width(); x++){
            uint32_t pixel = row_image.pixel(x, y);
            uint32_t r = (pixel >> 16) & 0xff;
            uint32_t g = (pixel >>  8) & 0xff;
            uint32_t b = (pixel >>  0) & 0xff;
            total++;
            if (r >= 200 && g >= 200 && b >= 200){
                white++;
            }
        }
    }
    if (total == 0 || white < total * 4 / 10){
        return -1;
    }

    double index = (best_cy - ROW0_Y) / ROW_PITCH;
    if (index < -0.5 || index > 2.5){
        return -1;
    }
    return (int)std::lround(index);
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
