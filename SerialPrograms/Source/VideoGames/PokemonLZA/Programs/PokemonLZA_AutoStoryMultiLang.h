/*  Auto Story - Multiple Lang
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Runs the opening of the story from a fresh save:
 *    ZA-00  create character (vision: pick the portrait)
 *    ZA-01  train station, chase Pancham
 *    ZA-02  chase Pancham, pick starter (dropdown), first battle (vision)
 *    ZA-03  battle zone, escape, waitress battle (vision), escape
 *    ZA-04  walk into Hotel Z
 *    ZA-05  Hotel Z story, night, morning, roll tutorial, leave hotel
 *    ZA-06  walk to the rival (Urbain / Taunie), rival battle (vision)
 *    ZA-07 .. ZA-11  map tutorial, rocks, rooftop, PC, Pokedex, Wild Zone 1
 *    ZA-12  first catch, Bunnelby battle + catch (vision), catch Scatterbug
 *           and Weedle without battling (vision), leave Wild Zone 1 (vision)
 *  ZA-13  walk to the Pokemon lab (minimap "!"), elevator to 3F, talk to
 *         Mable (world "!"), open the research list in the menu, close it,
 *         and clear the scene after it (ends on the "back to Hotel Z" quest).
 *  Dialogs in the lab part are cleared by vision, not recorded A presses,
 *  so it works in any game language.
 *
 *  The button sequences between vision checkpoints are Turbo Macro JSON
 *  files read from a user-selected folder, so routes can be re-recorded
 *  without rebuilding the program.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMultiLang_H
#define PokemonAutomation_PokemonLZA_AutoStoryMultiLang_H

#include "Common/Cpp/Time.h"
#include "Common/Cpp/Options/StringOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "NintendoSwitch/Options/NintendoSwitch_StartInGripMenuOption.h"
#include "CommonFramework/Language.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "NintendoSwitch/Options/NintendoSwitch_GoHomeWhenDoneOption.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


class AutoStoryMultiLang_Descriptor : public SingleSwitchProgramDescriptor{
public:
    AutoStoryMultiLang_Descriptor();

    class Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class AutoStoryMultiLang : public SingleSwitchProgramInstance, public ConfigOption::Listener{
public:
    using Descriptor = AutoStoryMultiLang_Descriptor;
    ~AutoStoryMultiLang();
    AutoStoryMultiLang();

    virtual void on_config_value_changed(void* object) override;

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;


public:
    //  Portrait index on the LOOKS screen. Row-major, 2 rows x 4 columns.
    enum class CharacterLook{
        Top1, Top2, Top3, Top4,
        Bottom1, Bottom2, Bottom3, Bottom4,
    };

    //  Where to start the run. Each later step assumes the game is on the
    //  screen the previous step ends on.
    enum class StartPoint{
        CreateCharacter,    //  ZA-00, from the beginning
        SelectCharacter,    //  on the LOOKS screen (after ZA-00-createChar0)
        TrainStation,       //  ZA-01
        ChasePancham,       //  ZA-02-0
        ChooseStarter,      //  ZA-02-1
        FirstBattle,        //  on the pre-battle screen (ZA-02-戰鬥前)
        BattleZone,         //  ZA-03-0, on the dialog after the first battle
        EscapeBattleZone,   //  ZA-03-1
        WaitressBattle,     //  on the pre-battle screen (ZA-03-2-女服務生戰鬥前)
        EscapeBattleZone2,  //  ZA-03-3, on the dialog after the waitress battle
        EscapeBattleZone3,  //  ZA-03-4
        WalkIntoHotel,      //  ZA-04
        HotelIntro,         //  ZA-05-0 .. ZA-05-2
        HotelUpstairs,      //  ZA-05-3 .. ZA-05-6
        HotelNight,         //  ZA-05-7
        HotelMorning,       //  ZA-05-8
        RollTutorial,       //  ZA-05-9 .. ZA-05-11
        WalkToRival,        //  ZA-06-1
        RivalBattle,        //  on the pre-battle screen (ZA-06-2-蓋伊戰鬥前)
        MapTutorial,        //  ZA-07, on the dialog after the rival battle
        RockScene,          //  ZA-08
        Rooftop,            //  ZA-09
        PokemonCenterPC,    //  ZA-10
        WildZone1,          //  ZA-11
        FirstCatch,         //  ZA-12-1
        BunnelbyCapture,    //  on the tutorial screen (ZA-12-2-掘掘兔戰鬥捕捉教學前)
        AfterBunnelby,      //  ZA-12-3, on the dialog after catching Bunnelby
        CatchScatterbug,    //  ZA-12-4, on ZA-12-4-捕捉粉蝶蟲前
        CatchWeedle,        //  ZA-12-5, right after catching Scatterbug
        LeaveWildZone,      //  ZA-12-6, right after catching Weedle
        WalkToLab,          //  ZA-13-1, outside Wild Zone 1: walk to the lab and go in
        LabElevator,        //  ZA-13-2, lab 1F (saved): elevator to 3F, 3F story
        LabMable,            //  ZA-13-3, lab 3F (saved): walk to the "!" and the story
        ResearchMenu,       //  ZA-13-4, next to Mable: X, X (research list), B out
    };

    //  Game language. Order = the in-game LANGUAGE screen, left column top to
    //  bottom, then right column.
    enum class GameLanguage{
        ChineseTraditional, //  繁體中文
        English,
        SpanishSpain,       //  Español (España)
        SpanishLatAm,       //  Español (Latinoamérica)
        French,
        German,
        Italian,
        Japanese,
        Korean,
        ChineseSimplified,  //  简体中文
    };

    enum class Starter{
        Chikorita,  //  菊草葉
        Tepig,      //  暖暖豬
        Totodile,   //  小鋸鱷
    };


private:
    //  Load a Turbo Macro JSON file from MACRO_FOLDER and run it.
    void run_macro(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const std::string& filename
    );

    //  On the "LOOKS" screen, move the cursor to the chosen portrait and
    //  confirm it is highlighted (white tile + green marker).
    void select_character(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

    //  From the pre-battle dialog, fight a trainer battle and stop on the
    //  post-battle dialog.
    void run_trainer_battle(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const std::string& name
    );

    //  Wild battle tutorial: send out a Pokemon, weaken Bunnelby, then
    //  throw a ball when told to. Stops on the dialog after the catch.
    void run_capture_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

    //  Catch a specific wild Pokemon without battling. Hold ZL to lock on,
    //  read the name above the target (OCR), and throw (ZR, still holding
    //  ZL) only if it is the one we want. Otherwise turn the camera a little
    //  and lock onto the next one. The catch is confirmed from the party bar.
    //  slug: Pokemon slug, e.g. "scatterbug".
    void catch_wild_pokemon(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const std::string& slug, const std::string& display_name
    );

    //  Walk to a spot in Wild Zone 1 by the minimap. The spot is the
    //  player's position relative to the "1" zone marker, in frame widths
    //  (x east, y south). Returns the last stick direction used.
    //  Throws if it cannot get there.
    std::pair<double, double> navigate_by_minimap(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const std::string& name, double target_x, double target_y, double tolerance
    );

    //  Press A on the "A 離開" prompt at the Wild Zone gate.
    void leave_wild_zone(SingleSwitchProgramEnvironment& env, ProControllerContext& context);


    //  Hold ZL to lock on and tap ZR to throw a ball.
    void throw_ball(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

    std::string starter_macro_filename() const;

    //  Before the starter is picked there is no party bar on the overworld.
    //  Until then, dialog clearing cannot use it to tell the overworld from
    //  a scene.
    bool m_hud_expected = true;

    //  OCR language for the chosen game language (both Spanish -> Spanish).
    Language ocr_language() const;

    //  ZA-00 (multi-language): from Home, close the game, start it with the
    //  chosen user profile, pick the game language on the LANGUAGE screen,
    //  and wait for the LOOKS screen.
    //  A story step that is only dialog (no walking): wait, clear the dialogs
    //  by detection, wait. Replaces macros that held only a dialog marker.
    void story_dialog(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const std::string& name, Milliseconds wait_before, Milliseconds wait_after
    );

    void launch_and_pick_language(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

    //  On the LANGUAGE screen, move to the chosen language and confirm.
    void pick_game_language(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

    //  Type the character name on the system keyboard (注音) and confirm.
    void enter_character_name(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

    //  Lab part (ZA-13). No recorded macros: steer by the yellow "!" quest
    //  marker and clear dialogs by detecting the dialog boxes.
    enum class WalkResult{
        Dialog,     //  a dialog / story started
        Prompt,     //  an "A" prompt is up next to the player at the target
    };

    //  Press A while a dialog is up, until the overworld HUD is back and no
    //  dialog shows for a few seconds. If require_dialog is set, keep waiting
    //  (up to a minute) until at least one dialog has been seen.
    void clear_dialogs(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const std::string& name, bool require_dialog,
        Milliseconds quiet = std::chrono::milliseconds(4000),
        bool press_first = false,
        std::chrono::seconds max_wait_for_dialog = std::chrono::seconds(60)
    );

    //  Walk toward the yellow "!" marker. Outdoors it steers by the minimap
    //  marker and camera heading; indoors (no minimap) it turns the camera to
    //  put the world marker in the middle of the screen and walks forward.
    WalkResult walk_to_quest_marker(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        const std::string& name, bool outdoors
    );

    void walk_to_lab(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    void take_lab_elevator(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    void walk_to_mable(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    void open_research_list(SingleSwitchProgramEnvironment& env, ProControllerContext& context);


private:
    BooleanCheckBoxOption FULL_GAME;
    GroupOption CHAPTER;
    StringOption MACRO_FOLDER;
    EnumDropdownOption<StartPoint> START_POINT;
    StartInGripOrGameOption START_LOCATION;
    SimpleIntegerOption<uint8_t> PROFILE;
    EnumDropdownOption<GameLanguage> GAME_LANGUAGE;
    StringOption CHARACTER_NAME;
    GroupOption CHARACTER_GROUP;
    EnumDropdownOption<CharacterLook> CHARACTER;
    EnumDropdownOption<Starter> STARTER;
    BattleAIOption BATTLE_AI;
    GoHomeWhenDoneOption GO_HOME_WHEN_DONE;

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};



}
}
}
#endif
