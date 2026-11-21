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
 *  - Party bar: in the overworld HUD, each party member after the lead has
 *    a small icon at the bottom left with a green HP bar under it. Counting
 *    the green bars tells how many Pokemon are in the party, which is used
 *    to confirm a catch.
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


//  Number of filled small party slots (party size minus the lead), 0 to 5.
//  Only meaningful while the overworld HUD is on screen.
int read_party_slot_count(const ImageViewRGB32& screen);
void make_party_slot_overlays(VideoOverlaySet& items, Color color = COLOR_GREEN);


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
