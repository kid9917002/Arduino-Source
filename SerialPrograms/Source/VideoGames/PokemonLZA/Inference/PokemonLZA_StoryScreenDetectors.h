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
 *  - Camera heading: the minimap is north-up and the player is always at
 *    its center. The light-blue view cone shows which way the camera
 *    faces.
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

//  Camera heading in degrees from the minimap view cone. 0 = north,
//  90 = east, 180 = south, 270 = west. Returns false if the minimap is not
//  on screen.
bool read_camera_heading(const ImageViewRGB32& screen, double& degrees);

//  Find a Weedle (orange body with a pink nose) in the game view. Returns
//  false if none is found. x/y are normalized screen coordinates. The area
//  around the player and the HUD are skipped. Only meant for the Wild Zone.
bool find_weedle(const ImageViewRGB32& screen, double& x, double& y);


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
