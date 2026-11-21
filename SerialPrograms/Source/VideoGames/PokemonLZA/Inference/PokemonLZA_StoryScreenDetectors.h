/*  Story Screen Detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Screens that appear while playing through the story:
 *
 *  - Tutorial window: a white card with a navy header in the middle of a
 *    dimmed (dark navy) screen. Closed with A, one page per press.
 *
 *  - Cutscene letterbox: black bars at the top and bottom of the screen.
 *    Story cutscenes (including the ones that interrupt a battle) use
 *    them. Normal post-battle dialogs do not.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_StoryScreenDetectors_H
#define PokemonAutomation_PokemonLZA_StoryScreenDetectors_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


class TutorialWindowDetector : public StaticScreenDetector{
public:
    TutorialWindowDetector(Color color = COLOR_CYAN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_panel;
    ImageFloatBox m_header;
    ImageFloatBox m_margin_left;
    ImageFloatBox m_margin_right;
};


class CutsceneLetterboxDetector : public StaticScreenDetector{
public:
    CutsceneLetterboxDetector(Color color = COLOR_MAGENTA);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_top;
    ImageFloatBox m_bottom_left;
    ImageFloatBox m_bottom_right;
};



}
}
}
#endif
