/*  Character Select Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Detects which of the 8 portraits is highlighted on the "LOOKS" screen
 *  that appears when creating a new character (2 rows x 4 columns).
 *
 *  The highlighted portrait has a solid white background and a green
 *  chevron marker floating above it. Non-highlighted portraits have a
 *  dark navy background.
 *
 *  Tile index layout:
 *      0 1 2 3
 *      4 5 6 7
 *
 */

#ifndef PokemonAutomation_PokemonLZA_CharacterSelectDetector_H
#define PokemonAutomation_PokemonLZA_CharacterSelectDetector_H

#include <array>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


class CharacterSelectDetector : public StaticScreenDetector{
public:
    static constexpr int ROWS = 2;
    static constexpr int COLS = 4;
    static constexpr int TILES = ROWS * COLS;

    CharacterSelectDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;

    //  Returns true if exactly one tile is highlighted (white background).
    virtual bool detect(const ImageViewRGB32& screen) override;

    //  Valid after detect(). -1 if no single highlighted tile was found.
    int highlighted_tile() const{ return m_highlighted; }

    //  Valid after detect(). True if the green marker is above the highlighted tile.
    bool highlighted_has_marker() const{ return m_has_marker; }

    static bool tile_is_white(const ImageViewRGB32& screen, const ImageFloatBox& box);
    static bool has_green_marker(const ImageViewRGB32& screen, const ImageFloatBox& box);


private:
    Color m_color;
    std::array<ImageFloatBox, TILES> m_tile_boxes;
    std::array<ImageFloatBox, TILES> m_marker_boxes;

    int m_highlighted;
    bool m_has_marker;
};



}
}
}
#endif
