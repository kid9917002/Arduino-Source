/*  Auto Story
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "Controllers/ControllerStateTable.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/Async/InterruptableCommands.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "CommonTools/StartupChecks/VideoResolutionCheck.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Pokemon/Inference/Pokemon_NameReader.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_CharacterSelectDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_ButtonDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_StoryScreenDetectors.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "PokemonLZA_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;



//  Macro files (Turbo Macro JSON) expected in MACRO_FOLDER.
namespace{

const char* MACRO_CREATE_CHAR_0     = "ZA-00-createChar0.json";
const char* MACRO_CREATE_CHAR_3     = "ZA-00-createChar3.json";
const char* MACRO_STATION           = "ZA-01-車站-追頑皮熊貓-without-restart.json";
const char* MACRO_CHASE_PANCHAM     = "ZA-02-0-追頑皮熊貓4-without-restart.json";
const char* MACRO_STARTER_CHIKORITA = "ZA-02-1-選擇御三家(菊草葉).json";
const char* MACRO_STARTER_TEPIG     = "ZA-02-1-選擇御三家(暖暖豬).json";
const char* MACRO_STARTER_TOTODILE  = "ZA-02-1-選擇御三家(小鋸鱷).json";
const char* MACRO_BATTLE_ZONE       = "ZA-03-0-對戰特區前.json";
const char* MACRO_ESCAPE_0          = "ZA-03-1-逃出對戰特區0.json";
const char* MACRO_ESCAPE_1          = "ZA-03-1-逃出對戰特區1.json";
const char* MACRO_ESCAPE_2_1        = "ZA-03-3-逃出對戰特區2-1.json";
const char* MACRO_ESCAPE_2_2        = "ZA-03-3-逃出對戰特區2-2.json";
const char* MACRO_ESCAPE_3          = "ZA-03-4-逃出對戰特區3.json";
const char* MACRO_HOTEL_WALK_0      = "ZA-04-0-走進Z飯店.json";
const char* MACRO_HOTEL_WALK_1      = "ZA-04-1-走進Z飯店.json";
const char* MACRO_HOTEL_INTRO       = "ZA-05-0-Z飯店-介紹.json";
const char* MACRO_HOTEL_AZ_1        = "ZA-05-1-Z飯店-向AZ對話1.json";
const char* MACRO_HOTEL_AZ_2        = "ZA-05-2-Z飯店-向AZ對話2.json";
const char* MACRO_HOTEL_ELEVATOR    = "ZA-05-3-Z飯店-走向電梯門口上2F.json";
const char* MACRO_HOTEL_ROOM        = "ZA-05-4-Z飯店-走進202客房.json";
const char* MACRO_HOTEL_BED         = "ZA-05-5-Z飯店-202房間走向床邊.json";
const char* MACRO_HOTEL_DOOR        = "ZA-05-6-Z飯店-202房間走向房門.json";
const char* MACRO_HOTEL_NIGHT       = "ZA-05-7-Z飯店-晚上劇情.json";
const char* MACRO_HOTEL_MORNING     = "ZA-05-8-Z飯店-隔天早上起床後出房門到2F電梯門口.json";
const char* MACRO_ROLL_TUTORIAL     = "ZA-05-9-Z飯店-翻滾教學.json";
const char* MACRO_ROLL_TO_DOOR      = "ZA-05-10-Z飯店-翻滾教學結束走向ZA飯店門口.json";
const char* MACRO_LEAVE_HOTEL       = "ZA-05-11-Z飯店-出ZA飯店.json";
const char* MACRO_WALK_TO_URBAIN    = "ZA-06-1-走向蓋伊對戰.json";
const char* MACRO_MAP_TUTORIAL      = "ZA-07-地圖教學.json";
const char* MACRO_ROCKS_WALK        = "ZA-08-1-往前走觸發岩石劇情.json";
const char* MACRO_ROCKS_SCENE       = "ZA-08-2-岩石劇情.json";
const char* MACRO_LADDER            = "ZA-09-1-走向大樓梯子與上梯子.json";
const char* MACRO_ROOFTOP           = "ZA-09-2-頂樓劇情.json";
const char* MACRO_JUMP_DOWN         = "ZA-09-3-跳下大樓.json";
const char* MACRO_PC_SCENE          = "ZA-10-1-往PC前劇情.json";
const char* MACRO_WALK_TO_PC        = "ZA-10-2-走向PC.json";
const char* MACRO_POKEDEX_TUTORIAL  = "ZA-10-3-圖鑑教學.json";
const char* MACRO_WILD_ZONE_1       = "ZA-11-1-走向WildZone1-1.json";
const char* MACRO_WILD_ZONE_2       = "ZA-11-2-走向WildZone1-2.json";
const char* MACRO_WILD_ZONE_3       = "ZA-11-3-走向WildZone1-3.json";
const char* MACRO_FIRST_CATCH       = "ZA-12-1-捕捉1.json";
const char* MACRO_AFTER_BUNNELBY    = "ZA-12-3-掘掘兔捕捉結束劇情.json";
//  Walking routes. Each ends standing still, facing the target.
const char* MACRO_WALK_TO_SCATTERBUG = "ZA-12-4-走向粉蝶蟲.json";
const char* MACRO_WALK_TO_WEEDLE    = "ZA-12-5-走向獨角蟲.json";
const char* MACRO_WALK_TO_GATE      = "ZA-12-6-走向大門.json";

//  Wild Zone 1 spots for minimap navigation: the player's position relative
//  to the "1" zone marker on the minimap, in frame widths (x east, y south).
//  Measured from screenshots taken at each spot.
const double WEEDLE_SPOT_X = -0.0019;
const double WEEDLE_SPOT_Y =  0.0058;
const double GATE_SPOT_X   = -0.0298;
const double GATE_SPOT_Y   =  0.0184;

}



AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        STRING_POKEMON + " LZA", "Auto Story",
        "",
        "Play the opening of the story from a new save: create the character, "
        "pick a starter, win the first battle, escape the battle zone, stay at Hotel Z, beat Urbain, catch Bunnelby, Scatterbug and Weedle, "
        "leave Wild Zone 1, go to the lab and open Mimi's research list.",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}
class AutoStory_Descriptor::Stats : public StatsTracker{
public:
    Stats()
        : macros(m_stats["Macros Run"])
        , attacks(m_stats["Attacks"])
        , errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Macros Run");
        m_display_order.emplace_back("Attacks");
        m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
    }

    std::atomic<uint64_t>& macros;
    std::atomic<uint64_t>& attacks;
    std::atomic<uint64_t>& errors;
};
std::unique_ptr<StatsTracker> AutoStory_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}



AutoStory::AutoStory()
    : MACRO_FOLDER(
        false,
        "<b>Macro Folder:</b><br>"
        "Folder that contains the ZA-00 / ZA-01 / ZA-02 Turbo Macro JSON files.",
        LockMode::LOCK_WHILE_RUNNING,
        "F:\\Game\\za_auto_2",
        "F:\\Game\\za_auto_2"
    )
    , START_POINT(
        "<b>Start From:</b><br>"
        "Which step to start at. The game must already be on the screen that step starts from.",
        {
            {StartPoint::CreateCharacter, "create-character", "ZA-00 Create character (beginning)"},
            {StartPoint::SelectCharacter, "select-character", "ZA-00 LOOKS screen (pick character)"},
            {StartPoint::TrainStation,    "train-station",    "ZA-01 Train station"},
            {StartPoint::ChasePancham,    "chase-pancham",    "ZA-02-0 Chase Pancham"},
            {StartPoint::ChooseStarter,   "choose-starter",   "ZA-02-1 Choose starter"},
            {StartPoint::FirstBattle,     "first-battle",     "ZA-02 First battle (pre-battle screen)"},
            {StartPoint::BattleZone,      "battle-zone",      "ZA-03-0 Battle zone (after first battle)"},
            {StartPoint::EscapeBattleZone,"escape-zone",      "ZA-03-1 Escape the battle zone"},
            {StartPoint::WaitressBattle,  "waitress-battle",  "ZA-03-2 Waitress battle (pre-battle screen)"},
            {StartPoint::EscapeBattleZone2, "escape-zone-2",  "ZA-03-3 Escape the battle zone 2 (after waitress battle)"},
            {StartPoint::EscapeBattleZone3, "escape-zone-3",  "ZA-03-4 Escape the battle zone 3"},
            {StartPoint::WalkIntoHotel,   "walk-into-hotel",  "ZA-04 Walk into Hotel Z"},
            {StartPoint::HotelIntro,      "hotel-intro",      "ZA-05-0 Hotel Z intro / talk to AZ"},
            {StartPoint::HotelUpstairs,   "hotel-upstairs",   "ZA-05-3 Elevator to 2F / room 202"},
            {StartPoint::HotelNight,      "hotel-night",      "ZA-05-7 Night scene"},
            {StartPoint::HotelMorning,    "hotel-morning",    "ZA-05-8 Next morning"},
            {StartPoint::RollTutorial,    "roll-tutorial",    "ZA-05-9 Roll tutorial / leave hotel"},
            {StartPoint::WalkToUrbain,    "walk-to-urbain",   "ZA-06-1 Walk to Urbain (蓋伊)"},
            {StartPoint::UrbainBattle,    "urbain-battle",    "ZA-06-2 Urbain (蓋伊) battle (pre-battle screen)"},
            {StartPoint::MapTutorial,     "map-tutorial",     "ZA-07 Map tutorial (after Urbain battle)"},
            {StartPoint::RockScene,       "rock-scene",       "ZA-08 Rocks scene"},
            {StartPoint::Rooftop,         "rooftop",          "ZA-09 Ladder / rooftop / jump down"},
            {StartPoint::PokemonCenterPC, "pc",               "ZA-10 PC / Pokedex tutorial"},
            {StartPoint::WildZone1,       "wild-zone-1",      "ZA-11 Walk to Wild Zone 1"},
            {StartPoint::FirstCatch,      "first-catch",      "ZA-12-1 First catch"},
            {StartPoint::BunnelbyCapture, "bunnelby",         "ZA-12-2 Bunnelby (掘掘兔) battle + catch (tutorial screen)"},
            {StartPoint::AfterBunnelby,   "after-bunnelby",   "ZA-12-3 After catching Bunnelby (做得好 dialog)"},
            {StartPoint::CatchScatterbug, "scatterbug",       "ZA-12-4 Walk to + catch Scatterbug (粉蝶蟲)"},
            {StartPoint::CatchWeedle,     "weedle",           "ZA-12-5 Walk to + catch Weedle (獨角蟲)"},
            {StartPoint::LeaveWildZone,   "leave-wild-zone",  "ZA-12-6 Walk to the gate and leave Wild Zone 1"},
            {StartPoint::WalkToLab,       "walk-to-lab",      "ZA-13-1 Walk to the lab and go in (outside Wild Zone 1)"},
            {StartPoint::LabElevator,     "lab-elevator",     "ZA-13-2 Lab 1F: elevator to 3F (所長室)"},
            {StartPoint::LabMimi,         "lab-mimi",         "ZA-13-3 Lab 3F: walk to the \"!\" (茉蜜姬)"},
            {StartPoint::ResearchMenu,    "research-menu",    "ZA-13-4 Next to Mimi: open the research list (X, X, B)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        StartPoint::CreateCharacter
    )
    , LANGUAGE(
        "<b>Game Language:</b><br>"
        "Used to read the names of wild Pokemon when catching Scatterbug and Weedle.",
        Pokemon::PokemonNameReader::instance().languages(),
        LockMode::LOCK_WHILE_RUNNING
    )
    , WILD_ZONE_WALK(
        "<b>Wild Zone Walking (ZA-12-5 / ZA-12-6):</b><br>"
        "Minimap navigation finds the player's position from the \"1\" zone marker on the "
        "minimap and the camera direction from the view cone, then steers to Weedle and to "
        "the gate. Recorded macros play ZA-12-5 / ZA-12-6 instead.",
        {
            {WildZoneWalk::MinimapNavigation, "minimap", "Minimap navigation"},
            {WildZoneWalk::RecordedMacros,    "macros",  "Recorded macros (ZA-12-5 / ZA-12-6)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        WildZoneWalk::MinimapNavigation
    )
    , CAMERA_MODE(
        "<b>Camera Mode:</b><br>"
        "Catching turns the camera toward the target, so normal runs face the camera north "
        "before the ZA-12-5 / ZA-12-6 walks. Record those two macros starting from north too: "
        "pick the second choice, run once, then start recording.",
        {
            {CameraMode::FaceNorthBeforeWalks, "face-north-before-walks", "Face north before each walk (ZA-12-5 / 12-6)"},
            {CameraMode::OnlyFaceNorthAndStop, "only-face-north",         "Only face north, then stop (for recording)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        CameraMode::FaceNorthBeforeWalks
    )
    , CHARACTER(
        "<b>Character Look:</b><br>"
        "Which portrait to pick on the LOOKS screen.",
        {
            {CharacterLook::Top1,    "top-1",    "Top row, 1st"},
            {CharacterLook::Top2,    "top-2",    "Top row, 2nd"},
            {CharacterLook::Top3,    "top-3",    "Top row, 3rd"},
            {CharacterLook::Top4,    "top-4",    "Top row, 4th"},
            {CharacterLook::Bottom1, "bottom-1", "Bottom row, 1st"},
            {CharacterLook::Bottom2, "bottom-2", "Bottom row, 2nd"},
            {CharacterLook::Bottom3, "bottom-3", "Bottom row, 3rd"},
            {CharacterLook::Bottom4, "bottom-4", "Bottom row, 4th"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        CharacterLook::Top3
    )
    , STARTER(
        "<b>Starter:</b>",
        {
            {Starter::Chikorita, "chikorita", "菊草葉 (Chikorita)"},
            {Starter::Tepig,     "tepig",     "暖暖豬 (Tepig)"},
            {Starter::Totodile,  "totodile",  "小鋸鱷 (Totodile)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        Starter::Tepig
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    LANGUAGE.set(Language::ChineseTraditional);

    PA_ADD_OPTION(MACRO_FOLDER);
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(START_POINT);
    PA_ADD_OPTION(WILD_ZONE_WALK);
    PA_ADD_OPTION(CAMERA_MODE);
    PA_ADD_OPTION(CHARACTER);
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}



std::string AutoStory::starter_macro_filename() const{
    switch (STARTER.get()){
    case Starter::Chikorita:
        return MACRO_STARTER_CHIKORITA;
    case Starter::Tepig:
        return MACRO_STARTER_TEPIG;
    case Starter::Totodile:
        return MACRO_STARTER_TOTODILE;
    }
    return MACRO_STARTER_TEPIG;
}



void AutoStory::run_macro(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& filename
){
    AutoStory_Descriptor::Stats& stats = env.current_stats<AutoStory_Descriptor::Stats>();

    context.wait_for_all_requests();

    std::string folder = MACRO_FOLDER;
    std::string path = folder;
    if (!path.empty() && path.back() != '/' && path.back() != '\\'){
        path += "/";
    }
    path += filename;

    std::string text;
    if (!file_to_string(path, text)){
        throw FileException(&env.logger(), PA_CURRENT_FUNCTION, "Unable to open macro file.", path);
    }

    //  Files saved by Windows Notepad start with a UTF-8 BOM.
    if (text.size() >= 3 &&
        (uint8_t)text[0] == 0xef &&
        (uint8_t)text[1] == 0xbb &&
        (uint8_t)text[2] == 0xbf
    ){
        text.erase(0, 3);
    }

    JsonValue json = parse_json(text);
    if (!json.is_object()){
        throw FileException(&env.logger(), PA_CURRENT_FUNCTION, "Macro file is not valid JSON.", path);
    }

    ControllerCommandTable table(
        ControllerClass::NintendoSwitch_ProController,
        "",
        LockMode::LOCK_WHILE_RUNNING
    );
    table.load_json(json);

    env.log("Running macro: " + filename, COLOR_BLUE);
    env.console.overlay().add_log("Macro: " + filename);

    table.run(context, env.console.controller());
    context.wait_for_all_requests();

    stats.macros++;
    env.update_stats();
}



void AutoStory::select_character(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const int target = (int)CHARACTER.get();
    const int target_row = target / CharacterSelectDetector::COLS;
    const int target_col = target % CharacterSelectDetector::COLS;

    env.log("Selecting character portrait #" + std::to_string(target));
    env.console.overlay().add_log("Select Character");

    CharacterSelectDetector detector(COLOR_GREEN);
    VideoOverlaySet overlays(env.console.overlay());
    detector.make_overlays(overlays);

    WallClock start = current_time();
    WallClock last_highlight = start;
    int confirmations = 0;

    while (true){
        context.wait_for_all_requests();

        if (current_time() - start > std::chrono::seconds(60)){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "select_character(): Unable to select the character portrait after 60 seconds.",
                env.console
            );
        }

        VideoSnapshot snapshot = env.console.video().snapshot();
        detector.detect(snapshot);
        int current = detector.highlighted_tile();

        if (current < 0){
            confirmations = 0;
            //  No portrait highlighted. The cursor may not show until the stick
            //  is touched. Wait a bit first in case the screen is fading in.
            if (current_time() - last_highlight > std::chrono::seconds(3)){
                env.log("No portrait highlighted. Nudging the stick.");
                pbf_move_left_joystick(context, {+1, 0}, 120ms, 500ms);
                last_highlight = current_time();
            }else{
                context.wait_for(200ms);
            }
            continue;
        }
        last_highlight = current_time();

        if (current == target){
            if (!detector.highlighted_has_marker()){
                confirmations = 0;
                context.wait_for(200ms);
                continue;
            }
            confirmations++;
            if (confirmations >= 3){
                env.log("Character portrait #" + std::to_string(target) + " selected.", COLOR_BLUE);
                env.console.overlay().add_log("Character Selected");
                return;
            }
            context.wait_for(150ms);
            continue;
        }

        confirmations = 0;
        int row = current / CharacterSelectDetector::COLS;
        int col = current % CharacterSelectDetector::COLS;
        env.log("Highlighted portrait #" + std::to_string(current) + ", moving toward #" + std::to_string(target));

        //  Move one step at a time, then re-check.
        if (row < target_row){
            pbf_move_left_joystick(context, {0, -1}, 120ms, 400ms);
        }else if (row > target_row){
            pbf_move_left_joystick(context, {0, +1}, 120ms, 400ms);
        }else if (col < target_col){
            pbf_move_left_joystick(context, {+1, 0}, 120ms, 400ms);
        }else{
            pbf_move_left_joystick(context, {-1, 0}, 120ms, 400ms);
        }
    }
}



void AutoStory::run_trainer_battle(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& name
){
    AutoStory_Descriptor::Stats& stats = env.current_stats<AutoStory_Descriptor::Stats>();

    env.log("Starting battle: " + name);
    env.console.overlay().add_log("Battle: " + name);

    //  The story battles have no "- Run" prompt, so RunFromBattleWatcher
    //  cannot be used to tell that we are in battle. Instead, every loop
    //  holds ZL and looks for the move palette. Seeing move symbols means
    //  we are in battle.
    TrainerBattleState battle_state(BATTLE_AI);

    WallClock start = current_time();
    WallClock last_attack = start;
    bool seen_battle = false;

    while (true){
        context.wait_for_all_requests();

        if (current_time() - start > std::chrono::minutes(5)){
            stats.errors++;
            env.update_stats();
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "run_trainer_battle(): " + name + " did not finish within 5 minutes.",
                env.console
            );
        }

        if (battle_state.attempt_one_attack(env, env.console, context)){
            if (!seen_battle){
                env.log("Battle started.", COLOR_BLUE);
                env.console.overlay().add_log("Battle Started");
            }
            seen_battle = true;
            last_attack = current_time();
            stats.attacks++;
            env.update_stats();
            continue;
        }

        //  No move palette. Either we are not in battle yet, the battle is
        //  over, or something is covering the screen.
        context.wait_for_all_requests();
        {
            VideoSnapshot snapshot = env.console.video().snapshot();

            //  Tutorial window (one page per A press). Can appear before or
            //  in the middle of a battle.
            TutorialWindowDetector tutorial;
            if (tutorial.detect(snapshot)){
                env.log("Tutorial window. Pressing A.");
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                continue;
            }

            //  Letterboxed cutscene in the middle of a battle (for example
            //  when the opponent sends out the next Pokemon). Its dialog
            //  looks like the post-battle dialog, so advance it here instead
            //  of treating it as the end of the battle.
            CutsceneLetterboxDetector letterbox;
            if (seen_battle && letterbox.detect(snapshot)){
                env.log("Mid-battle cutscene. Pressing A.");
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                last_attack = current_time();
                continue;
            }
        }
        FlatWhiteDialogWatcher dialog(COLOR_RED, &env.console.overlay(), 500ms);
        int ret = wait_until(
            env.console, context,
            1000ms,
            {dialog}
        );

        if (ret == 0 && seen_battle){
            //  Post-battle dialog. Leave it on screen and stop.
            env.log("Post-battle dialog detected. Battle won.", COLOR_BLUE);
            env.console.overlay().add_log("Battle Finished");
            return;
        }

        if (!seen_battle){
            //  Pre-battle transparent dialog ("... wants to battle!") and
            //  the first-battle tutorial window: both advance with A.
            env.log("Waiting for battle. Pressing A.");
            pbf_press_button(context, BUTTON_A, 160ms, 240ms);
            continue;
        }

        //  Between the final hit and the post-battle dialog there is a
        //  FINISH animation. Do not press A here or we could skip past the
        //  dialog we want to stop on. Only nudge with A if nothing has been
        //  recognized for a long time.
        if (current_time() - last_attack > std::chrono::seconds(30)){
            env.log("No move palette or dialog for 30 seconds. Pressing A.", COLOR_RED);
            pbf_press_button(context, BUTTON_A, 160ms, 240ms);
            last_attack = current_time();
        }
    }
}



void AutoStory::face_camera_north(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Turning the camera to face north.");
    env.console.overlay().add_log("Face North");

    const double TOLERANCE = 6.0;

    //  Signed degrees of heading change per millisecond of right stick
    //  pushed right. Starts as a guess and is re-measured after each push.
    double rate = 0.15;

    auto wrap = [](double degrees){
        while (degrees > 180){
            degrees -= 360;
        }
        while (degrees <= -180){
            degrees += 360;
        }
        return degrees;
    };

    //  Median of 3 reads to ride out icons passing over the minimap.
    auto read_heading = [&](double& heading) -> bool{
        double reads[3] = {0, 0, 0};
        int count = 0;
        for (int c = 0; c < 6 && count < 3; c++){
            context.wait_for_all_requests();
            double value = 0;
            if (read_camera_heading(env.console.video().snapshot(), value)){
                reads[count++] = value;
            }
            context.wait_for(100ms);
        }
        if (count == 0){
            return false;
        }
        //  Median on the circle: pick the read closest to the others.
        int best = 0;
        double best_cost = 1e9;
        for (int i = 0; i < count; i++){
            double cost = 0;
            for (int j = 0; j < count; j++){
                cost += std::abs(wrap(reads[i] - reads[j]));
            }
            if (cost < best_cost){
                best_cost = cost;
                best = i;
            }
        }
        heading = reads[best];
        return true;
    };

    double heading = 0;
    int read_failures = 0;
    for (int iteration = 0; iteration < 24; iteration++){
        if (!read_heading(heading)){
            //  An icon or a popup may be covering the minimap, or the cone
            //  may be over colors it blends into. Turn a little and retry.
            read_failures++;
            env.log("Unable to read the minimap view cone. Turning a bit and retrying.", COLOR_RED);
            if (read_failures > 6){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::NO_ERROR_REPORT,
                    "face_camera_north(): Unable to read the minimap view cone.",
                    env.console
                );
            }
            pbf_move_right_joystick(context, {1, 0}, 250ms, 500ms);
            continue;
        }

        double error = wrap(0 - heading);
        env.log("Camera heading: " + std::to_string((int)heading) + " degrees");
        if (std::abs(error) <= TOLERANCE){
            env.log("Camera faces north.", COLOR_BLUE);
            return;
        }

        //  Push right (+x) to change the heading by +rate per ms.
        double push_ms = std::abs(error / rate);
        push_ms = std::max(40.0, std::min(push_ms, 600.0));
        double direction = (error / rate) > 0 ? 1.0 : -1.0;
        pbf_move_right_joystick(
            context, {direction, 0},
            Milliseconds((int64_t)push_ms), 400ms
        );

        //  Re-measure the rate from what actually happened.
        double after = 0;
        if (read_heading(after)){
            double change = wrap(after - heading);
            if (std::abs(change) >= 3){
                rate = change / (direction * push_ms);
            }
        }
    }

    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "face_camera_north(): Unable to turn the camera to face north.",
        env.console
    );
}



void AutoStory::throw_ball(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Throwing a ball.", COLOR_BLUE);
    env.console.overlay().add_log("Throw Ball");
    //  Hold ZL for 1.5s to lock on. ZR is tapped while ZL is still held.
    ssf_press_button(context, BUTTON_ZL, 700ms, 1500ms, 0ms);
    pbf_press_button(context, BUTTON_ZR, 120ms, 1500ms);
    context.wait_for_all_requests();
}



void AutoStory::catch_wild_pokemon(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& slug, const std::string& display_name
){
    env.log("Catching " + display_name + ".");
    env.console.overlay().add_log("Catch: " + display_name);

    VideoOverlaySet overlays(env.console.overlay());
    make_party_slot_overlays(overlays);

    //  Party size now. Take the highest of a few reads in case something
    //  briefly covers the HUD.
    context.wait_for_all_requests();
    int before = 0;
    for (int c = 0; c < 3; c++){
        before = std::max(before, read_party_slot_count(env.console.video().snapshot()));
        context.wait_for(200ms);
    }
    env.log("Party slots before catching " + display_name + ": " + std::to_string(before));

    const int MAX_LOCK_TRIES = 30;  //  about 1.5 turns of the camera
    const int MAX_THROWS = 6;
    int throws = 0;
    std::string previous_read;
    std::string last_read;

    for (int attempt = 1; attempt <= MAX_LOCK_TRIES; attempt++){
        context.wait_for_all_requests();

        //  Hold ZL in a background command while this thread watches the
        //  screen (same pattern as TrainerBattleState::attempt_one_attack).
        AsyncCommandSession<ProController> command(context, env.console.logger(), context);
        command.dispatch([](ProControllerContext& context){
            pbf_press_button(context, BUTTON_ZL, 5000ms, 0ms);
        });

        //  The label fades in, and the capture lags the console. Keep looking
        //  for the HP bar for up to about 2.5 seconds of the hold.
        WallClock hold_start = current_time();
        VideoSnapshot snapshot;
        ImageFloatBox bar;
        bool locked = false;
        std::string debug;
        context.wait_for(300ms);
        while (current_time() - hold_start < 2500ms){
            snapshot = env.console.video().snapshot();
            if (find_locked_target_hp_bar(snapshot, bar, &debug)){
                locked = true;
                //  Give the name a moment to finish fading in.
                context.wait_for(150ms);
                VideoSnapshot again = env.console.video().snapshot();
                ImageFloatBox again_bar;
                if (find_locked_target_hp_bar(again, again_bar)){
                    snapshot = std::move(again);
                    bar = again_bar;
                }
                break;
            }
            context.wait_for(150ms);
        }

        bool is_target = false;
        if (locked){
            ImageFloatBox name_box = locked_target_name_box(bar);
            OverlayBoxScope name_overlay(env.console.overlay(), name_box, COLOR_BLUE);
            OCR::StringMatchResult result = Pokemon::PokemonNameReader::instance().read_substring(
                env.console.logger(), LANGUAGE,
                extract_box_reference(snapshot, name_box),
                OCR::WHITE_TEXT_FILTERS()
            );
            std::string names;
            for (const auto& item : result.results){
                names += item.second.token + " ";
                if (item.second.token == slug){
                    is_target = true;
                }
            }
            last_read = names;
            env.log("Locked onto: " + (names.empty() ? std::string("(unreadable)") : names));
            if (!is_target && attempt <= 3 && snapshot.frame){
                std::string path = "AutoStory-ZL-" + slug + "-name-" + std::to_string(attempt) + ".png";
                snapshot.frame->save(path);
                env.log("Saved " + path);
            }
        }else{
            env.log("Nothing to lock onto. " + debug);
            //  Save a few of these frames so the detection can be checked.
            if (attempt <= 3 && snapshot.frame){
                std::string path = "AutoStory-ZL-" + slug + "-" + std::to_string(attempt) + ".png";
                snapshot.frame->save(path);
                env.log("Saved " + path);
            }
        }

        if (!is_target){
            //  Let go of ZL, turn the camera and try the next one. Lock-on
            //  sticks to the nearest Pokemon, so if the same wrong one comes
            //  back, turn further to get it out of view.
            std::string current = locked ? last_read : std::string();
            bool same_again = locked && current == previous_read;
            previous_read = current;
            command.stop_session_and_rethrow();
            context.wait_for_all_requests();
            pbf_move_right_joystick(context, {1, 0}, same_again ? 400ms : 120ms, 400ms);
            continue;
        }

        //  It is the one. Keep holding ZL and press ZR to throw.
        throws++;
        env.log(display_name + " locked. Throwing (" + std::to_string(throws) + ").", COLOR_BLUE);
        env.console.overlay().add_log("Throw at " + display_name);
        command.dispatch([](ProControllerContext& context){
            ssf_press_button(context, BUTTON_ZL, 0ms, 1200ms, 0ms);
            pbf_wait(context, 300ms);
            pbf_press_button(context, BUTTON_ZR, 160ms, 1000ms);
        });
        command.wait();
        command.stop_session_and_rethrow();
        context.wait_for_all_requests();

        //  The ball flies and shakes, then the Pokemon shows up in the party
        //  bar.
        WallClock start = current_time();
        int confirmations = 0;
        while (current_time() - start < std::chrono::seconds(12)){
            context.wait_for_all_requests();
            VideoSnapshot frame = env.console.video().snapshot();

            TutorialWindowDetector tutorial;
            if (tutorial.detect(frame)){
                env.log("Tutorial window. Pressing A.");
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                continue;
            }

            int now = read_party_slot_count(frame);
            if (now > before){
                confirmations++;
                if (confirmations >= 2){
                    env.log(display_name + " caught. Party slots: " + std::to_string(now), COLOR_BLUE);
                    env.console.overlay().add_log(display_name + " Caught");
                    context.wait_for(1000ms);
                    return;
                }
            }else{
                confirmations = 0;
            }
            context.wait_for(400ms);
        }
        env.log(display_name + ": not caught yet.", COLOR_RED);

        if (throws >= MAX_THROWS){
            break;
        }
    }

    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "catch_wild_pokemon(): Failed to catch " + display_name + ". "
        "Check that the walking macro ends with it in view.",
        env.console
    );
}



std::pair<double, double> AutoStory::navigate_by_minimap(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& name, double target_x, double target_y, double tolerance
){
    env.log("Navigating to " + name + " by the minimap.");
    env.console.overlay().add_log("Go to " + name);

    //  Lock-on (ZL) tilts the camera toward the target. Press L to put the
    //  camera back behind the player at the normal angle. The heading is
    //  read from the minimap every step, so where it ends up facing is fine.
    pbf_press_button(context, BUTTON_L, 160ms, 500ms);

    const double PI = 3.14159265358979323846;

    //  Read the player position (relative to the zone marker) and the camera
    //  heading. A few tries to ride out icons and popups.
    auto read_state = [&](double& x, double& y, double& heading) -> bool{
        for (int c = 0; c < 5; c++){
            context.wait_for_all_requests();
            VideoSnapshot snapshot = env.console.video().snapshot();
            double marker_dx = 0;
            double marker_dy = 0;
            double h = 0;
            if (find_minimap_zone_marker(snapshot, marker_dx, marker_dy) &&
                read_camera_heading(snapshot, h)
            ){
                x = -marker_dx;
                y = -marker_dy;
                heading = h;
                return true;
            }
            context.wait_for(150ms);
        }
        return false;
    };

    double speed = 0.00002;     //  frame widths per ms of walking (re-measured)
    double stick_x = 0;
    double stick_y = 1;
    int read_failures = 0;
    int stuck = 0;

    for (int step = 0; step < 60; step++){
        double x = 0;
        double y = 0;
        double heading = 0;
        if (!read_state(x, y, heading)){
            read_failures++;
            env.log("Unable to read the minimap. Stepping the same way.", COLOR_RED);
            if (read_failures > 8){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::NO_ERROR_REPORT,
                    "navigate_by_minimap(): Unable to read the minimap on the way to " + name + ".",
                    env.console
                );
            }
            pbf_move_left_joystick(context, {stick_x, stick_y}, 250ms, 250ms);
            continue;
        }
        read_failures = 0;

        double dx = target_x - x;
        double dy = target_y - y;
        double distance = std::sqrt(dx * dx + dy * dy);
        env.log(
            name + ": at (" + std::to_string(x) + ", " + std::to_string(y) +
            "), heading " + std::to_string((int)heading) +
            ", distance " + std::to_string(distance)
        );
        if (distance <= tolerance){
            env.log("Arrived at " + name + ".", COLOR_BLUE);
            env.console.overlay().add_log("At " + name);
            return {stick_x, stick_y};
        }

        //  Bearing to the target (0 = north, clockwise), then relative to
        //  the camera. Up on the left stick walks the way the camera faces.
        double bearing = std::atan2(dx, -dy) * 180.0 / PI;
        double relative = (bearing - heading) * PI / 180.0;

        //  Blocked last time: step sideways (alternating) to get around it.
        if (stuck > 0){
            relative += (stuck % 2 == 1 ? 1 : -1) * PI / 2;
        }
        stick_x = std::sin(relative);
        stick_y = std::cos(relative);

        double push_ms = stuck > 0 ? 500.0 : distance / speed * 0.7;
        push_ms = std::max(150.0, std::min(push_ms, 1200.0));
        pbf_move_left_joystick(context, {stick_x, stick_y}, Milliseconds((int64_t)push_ms), 300ms);

        //  How far did that get us?
        double new_x = 0;
        double new_y = 0;
        double new_heading = 0;
        if (read_state(new_x, new_y, new_heading)){
            double moved = std::sqrt((new_x - x) * (new_x - x) + (new_y - y) * (new_y - y));
            if (moved < 0.0015 && push_ms >= 300){
                stuck++;
                env.log("Barely moved. Trying to go around (" + std::to_string(stuck) + ").", COLOR_RED);
                if (stuck > 8){
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::NO_ERROR_REPORT,
                        "navigate_by_minimap(): Stuck on the way to " + name + ".",
                        env.console
                    );
                }
            }else{
                if (stuck == 0 && moved > 0.001){
                    speed = 0.5 * speed + 0.5 * (moved / push_ms);
                }
                stuck = 0;
            }
        }
    }

    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "navigate_by_minimap(): Did not reach " + name + ".",
        env.console
    );
}



void AutoStory::leave_wild_zone(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Leaving the Wild Zone.");
    env.console.overlay().add_log("Leave Wild Zone");

    //  The "A 離開" prompt floats next to the player, and where it shows up
    //  depends on the camera angle, so look over most of the screen. It is
    //  only used to confirm: we are standing on the gate, so press A anyway.
    const ImageFloatBox PROMPT_BOX(0.25, 0.0, 0.5, 0.85);

    for (int c = 0; c < 3; c++){
        context.wait_for_all_requests();
        ButtonWatcher prompt(COLOR_RED, ButtonType::ButtonA, PROMPT_BOX, &env.console.overlay());
        int ret = wait_until(env.console, context, 2000ms, {prompt});
        if (ret == 0){
            env.log("Exit prompt found. Pressing A.", COLOR_BLUE);
        }else{
            env.log("No exit prompt seen. At the gate, so pressing A anyway.", COLOR_RED);
        }
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        context.wait_for_all_requests();

        //  Left once the prompt is gone and stays gone.
        ButtonWatcher still_there(COLOR_RED, ButtonType::ButtonA, PROMPT_BOX, &env.console.overlay());
        ret = wait_until(env.console, context, 3000ms, {still_there});
        if (ret < 0){
            break;
        }
        env.log("Exit prompt still there. Pressing A again.");
    }

    context.wait_for(5000ms);
    env.log("Left the Wild Zone.", COLOR_BLUE);
    env.console.overlay().add_log("Left Wild Zone");
}



void AutoStory::run_capture_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    AutoStory_Descriptor::Stats& stats = env.current_stats<AutoStory_Descriptor::Stats>();

    env.log("Starting Bunnelby battle.");
    env.console.overlay().add_log("Battle: Bunnelby");

    WallClock start = current_time();
    auto check_timeout = [&](const char* phase){
        if (current_time() - start > std::chrono::minutes(5)){
            stats.errors++;
            env.update_stats();
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                std::string("run_capture_battle(): Took more than 5 minutes. Phase: ") + phase,
                env.console
            );
        }
    };


    //  Phase 1: close the "wild battle" tutorial window (one page per A).
    {
        int clear_checks = 0;
        while (clear_checks < 2){
            check_timeout("tutorial");
            context.wait_for_all_requests();
            TutorialWindowDetector tutorial;
            if (tutorial.detect(env.console.video().snapshot())){
                env.log("Tutorial window. Pressing A.");
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                clear_checks = 0;
            }else{
                clear_checks++;
                context.wait_for(500ms);
            }
        }
    }


    //  Phase 2: send out a Pokemon (D-pad Up) and fight until Bunnelby's
    //  HP is 0. That brings up a white dialog asking us to catch it.
    {
        env.log("Sending out Pokemon (D-pad Up).");
        pbf_press_dpad(context, DPAD_UP, 160ms, 1500ms);
        WallClock last_send_out = current_time();

        TrainerBattleState battle_state(BATTLE_AI);
        bool seen_battle = false;

        while (true){
            check_timeout("battle");
            context.wait_for_all_requests();

            if (battle_state.attempt_one_attack(env, env.console, context)){
                seen_battle = true;
                stats.attacks++;
                env.update_stats();
                continue;
            }

            context.wait_for_all_requests();
            {
                TutorialWindowDetector tutorial;
                if (tutorial.detect(env.console.video().snapshot())){
                    env.log("Tutorial window. Pressing A.");
                    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                    continue;
                }
            }

            FlatWhiteDialogWatcher dialog(COLOR_RED, &env.console.overlay(), 500ms);
            int ret = wait_until(env.console, context, 1000ms, {dialog});
            if (ret == 0){
                env.log("Dialog after battle: catch prompt.", COLOR_BLUE);
                break;
            }

            //  No move palette and no dialog. If we never got into battle,
            //  the Pokemon is probably not out yet. Send it out again.
            if (!seen_battle && current_time() - last_send_out > std::chrono::seconds(3)){
                env.log("Not in battle yet. Sending out Pokemon again (D-pad Up).");
                pbf_press_dpad(context, DPAD_UP, 160ms, 1500ms);
                last_send_out = current_time();
            }
        }
    }


    //  Phase 3: "捕捉機會來了！" / "用ZR來投出精靈球吧！". Press A until the
    //  dialog is gone.
    {
        env.console.overlay().add_log("Catch Prompt");
        int clear_checks = 0;
        while (clear_checks < 2){
            check_timeout("catch prompt");
            context.wait_for_all_requests();
            FlatWhiteDialogWatcher dialog(COLOR_RED, &env.console.overlay(), 300ms);
            int ret = wait_until(env.console, context, 1000ms, {dialog});
            if (ret == 0){
                pbf_press_button(context, BUTTON_A, 160ms, 640ms);
                clear_checks = 0;
            }else{
                clear_checks++;
            }
        }
    }


    //  Phase 4: throw balls until the post-catch dialog ("做得好！") shows up.
    while (true){
        check_timeout("catch");
        throw_ball(env, context);

        //  Wait for the catch to resolve. A white dialog means it worked.
        FlatWhiteDialogWatcher dialog(COLOR_RED, &env.console.overlay(), 500ms);
        int ret = wait_until(env.console, context, 15000ms, {dialog});
        if (ret == 0){
            env.log("Dialog after catch. Bunnelby caught.", COLOR_BLUE);
            env.console.overlay().add_log("Bunnelby Caught");
            return;
        }

        context.wait_for_all_requests();
        TutorialWindowDetector tutorial;
        if (tutorial.detect(env.console.video().snapshot())){
            env.log("Tutorial window. Pressing A.");
            pbf_press_button(context, BUTTON_A, 160ms, 840ms);
            continue;
        }
        env.log("No dialog 15 seconds after throwing. Throwing again.", COLOR_RED);
    }
}



namespace{

//  Any dialog box that wants A (story text, item received, tutorial).
//  These look for the dialog's "next" arrow or box shape, not the text, so
//  they work in every language.
bool dialog_on_screen(const ImageViewRGB32& screen){
    FlatWhiteDialogDetector flat_white;
    BlueDialogDetector blue;
    TealDialogDetector teal;
    LightBlueDialogDetector light_blue;
    TransparentBattleDialogDetector transparent;
    ItemReceiveDetector item;
    TutorialWindowDetector tutorial;
    return flat_white.detect(screen)
        || blue.detect(screen)
        || teal.detect(screen)
        || light_blue.detect(screen)
        || transparent.detect(screen)
        || item.detect(screen)
        || tutorial.detect(screen);
}

//  The "A" prompt (進入 / 搭電梯 / 搭話) floats right next to the player.
const ImageFloatBox LAB_PROMPT_BOX(0.35, 0.25, 0.45, 0.55);

bool a_prompt_on_screen(const ImageViewRGB32& screen){
    ButtonDetector prompt(COLOR_RED, ButtonType::ButtonA, LAB_PROMPT_BOX);
    return prompt.detect(screen);
}

//  Outdoors, the distance (meters) is written in white right above the
//  world quest marker. Indoors there is no number. Returns -1 if unreadable.
int read_quest_marker_distance(Logger& logger, const ImageViewRGB32& screen, const ImageFloatBox& marker){
    ImageFloatBox box(
        marker.x - marker.width * 0.6,
        marker.y - marker.height * 0.85,
        marker.width * 2.2,
        marker.height * 0.7
    );
    if (box.x < 0 || box.y < 0 || box.x + box.width > 1 || box.y + box.height > 1){
        return -1;
    }
    ImageViewRGB32 image = extract_box_reference(screen, box);
    if (image.width() < 4 || image.height() < 4){
        return -1;
    }
    const std::vector<std::pair<uint32_t, uint32_t>> filters = {
        {0xffb0b0b0, 0xffffffff},
        {0xffc8c8c8, 0xffffffff},
        {0xffe0e0e0, 0xffffffff},
    };
    size_t min_digit_area = image.height() * image.height() / 25;
    int number = OCR::read_number_waterfill_multifilter(
        logger, GlobalThreadPools::computation_realtime(),
        image, filters,
        true, true, SIZE_MAX, min_digit_area
    );
    if (number < 0 || number > 999){
        return -1;
    }
    return number;
}

}



void AutoStory::clear_dialogs(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& name, bool require_dialog
){
    env.log("Clearing dialogs: " + name);
    env.console.overlay().add_log("Dialog: " + name);

    const Milliseconds QUIET_NEEDED = 4000ms;

    WallClock start = current_time();
    WallClock last_activity = start;
    size_t presses = 0;
    bool waited_long = false;

    while (true){
        WallClock now = current_time();
        if (now - start > std::chrono::minutes(6)){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "clear_dialogs(): Dialogs did not end after 6 minutes: " + name,
                env.console
            );
        }

        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();

        if (dialog_on_screen(snapshot)){
            pbf_press_button(context, BUTTON_A, 160ms, 340ms);
            presses++;
            last_activity = current_time();
            continue;
        }

        CutsceneLetterboxDetector letterbox;
        if (letterbox.detect(snapshot) || read_party_slot_count(snapshot) < 1){
            //  Cutscene, fade, loading or a menu: not done yet.
            last_activity = current_time();
        }else if (now - last_activity >= QUIET_NEEDED){
            if (presses > 0 || !require_dialog || waited_long){
                env.log(
                    "Dialogs cleared (" + std::to_string(presses) + " A presses): " + name,
                    COLOR_BLUE
                );
                return;
            }
            if (now - start > std::chrono::seconds(60)){
                env.log("No dialog showed up in a minute. Moving on: " + name, COLOR_RED);
                waited_long = true;
            }
        }
        context.wait_for(250ms);
    }
}



AutoStory::WalkResult AutoStory::walk_to_quest_marker(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& name, bool outdoors
){
    env.log("Walking to the quest marker: " + name);
    env.console.overlay().add_log("Go to " + name);

    const double PI = 3.14159265358979323846;

    struct State{
        bool dialog = false;
        bool prompt = false;
        bool minimap = false;
        double mdx = 0;
        double mdy = 0;
        double heading = 0;
        bool world = false;
        ImageFloatBox world_box;
        int distance = -1;      //  meters, outdoors only
    };
    auto read_state = [&](){
        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();
        State state;
        state.dialog = dialog_on_screen(snapshot);
        state.prompt = a_prompt_on_screen(snapshot);
        if (outdoors){
            state.minimap = find_minimap_quest_marker(snapshot, state.mdx, state.mdy)
                && read_camera_heading(snapshot, state.heading);
        }
        state.world = find_world_quest_marker(snapshot, state.world_box);
        if (outdoors && state.world){
            state.distance = read_quest_marker_distance(env.logger(), snapshot, state.world_box);
        }
        return state;
    };

    int lost = 0;
    int stuck = 0;
    double stick_x = 0;
    double stick_y = 1;

    for (int step = 0; step < 200; step++){
        State state = read_state();
        if (state.dialog){
            env.log("Dialog on the way to " + name + ".");
            return WalkResult::Dialog;
        }

        double mdist = state.minimap
            ? std::sqrt(state.mdx * state.mdx + state.mdy * state.mdy)
            : 1.0;

        if (state.prompt){
            //  Outdoors, ignore prompts from people on the street: only stop
            //  when the marker is close.
            bool near_marker = state.world && (state.distance < 0 || state.distance <= 8);
            if (!outdoors || mdist <= 0.03 || near_marker){
                env.log("A prompt at " + name + ".", COLOR_BLUE);
                return WalkResult::Prompt;
            }
        }

        //  Steer by the marker in the world (more exact than the minimap):
        //  turn the camera to put it in the middle, then walk forward.
        if (state.world){
            lost = 0;
            double cx = state.world_box.x + state.world_box.width / 2;
            double offset = cx - 0.5;
            if (std::abs(offset) > 0.04){
                double push_ms = std::max(40.0, std::min(std::abs(offset) * 450.0, 300.0));
                pbf_move_right_joystick(
                    context, {offset > 0 ? 1.0 : -1.0, 0},
                    Milliseconds((int64_t)push_ms), 300ms
                );
                continue;
            }

            if (stuck > 0){
                //  Blocked: step sideways (alternating), then try again.
                double side = (stuck % 2 == 1) ? -1.0 : 1.0;
                env.log("Blocked. Stepping sideways (" + std::to_string(stuck) + ").", COLOR_RED);
                pbf_move_left_joystick(context, {side, 0.3}, 500ms, 200ms);
            }
            double walk_ms = 400;
            if (state.distance > 0){
                walk_ms = std::max(400.0, std::min(state.distance * 80.0, 1500.0));
            }
            env.log(
                name + ": marker at x = " + std::to_string(cx) +
                ", distance " + (state.distance >= 0 ? std::to_string(state.distance) + " m" : std::string("?")) +
                ", walk " + std::to_string((int)walk_ms) + " ms"
            );
            pbf_move_left_joystick(context, {0, 1}, Milliseconds((int64_t)walk_ms), 200ms);
            stick_x = 0;
            stick_y = 1;

            State after = read_state();
            if (after.world && !after.dialog && !after.prompt){
                double after_cx = after.world_box.x + after.world_box.width / 2;
                double after_cy = after.world_box.y + after.world_box.height / 2;
                double before_cy = state.world_box.y + state.world_box.height / 2;
                double size_change = std::abs(after.world_box.width - state.world_box.width) / state.world_box.width;
                bool not_closer = (state.distance > 0 && after.distance > 0)
                    ? after.distance >= state.distance
                    : (std::abs(after_cx - cx) < 0.004 && std::abs(after_cy - before_cy) < 0.004 && size_change < 0.03);
                if (not_closer){
                    stuck++;
                    if (stuck >= 12){
                        OperationFailedExceptionWithScreenshot::fire(
                            ErrorReportMode::NO_ERROR_REPORT,
                            "walk_to_quest_marker(): Stuck on the way to " + name + ".",
                            env.console
                        );
                    }
                }else{
                    stuck = 0;
                }
            }
            continue;
        }

        //  Marker not on screen (behind the camera, far away): steer by the
        //  minimap. Not when it is right under the player arrow (the
        //  direction is meaningless there): look around instead.
        if (state.minimap && mdist > 0.004){
            lost = 0;
            double bearing = std::atan2(state.mdx, -state.mdy) * 180.0 / PI;
            double relative = (bearing - state.heading) * PI / 180.0;
            if (stuck > 0){
                relative += (stuck % 2 == 1 ? 1 : -1) * PI / 2;
            }
            stick_x = std::sin(relative);
            stick_y = std::cos(relative);
            double push_ms = stuck > 0 ? 500.0 : mdist / 0.00002 * 0.5;
            push_ms = std::max(200.0, std::min(push_ms, 900.0));
            env.log(
                name + ": minimap marker (" + std::to_string(state.mdx) + ", " + std::to_string(state.mdy) +
                "), heading " + std::to_string((int)state.heading) + ", push " + std::to_string((int)push_ms) + " ms"
            );
            pbf_move_left_joystick(context, {stick_x, stick_y}, Milliseconds((int64_t)push_ms), 200ms);

            State after = read_state();
            if (after.minimap && stuck == 0 && push_ms >= 300){
                double moved = std::sqrt(
                    (after.mdx - state.mdx) * (after.mdx - state.mdx) +
                    (after.mdy - state.mdy) * (after.mdy - state.mdy)
                );
                //  When the marker is pinned to the minimap edge it only moves
                //  sideways, so only count "stuck" when it is on the map.
                if (moved < 0.0008 && mdist < 0.05){
                    stuck++;
                    env.log("Barely moved. Going around (" + std::to_string(stuck) + ").", COLOR_RED);
                }
            }else if (stuck > 0){
                stuck = (stuck >= 12) ? stuck : 0;
            }
            if (stuck >= 12){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::NO_ERROR_REPORT,
                    "walk_to_quest_marker(): Stuck on the way to " + name + ".",
                    env.console
                );
            }
            continue;
        }

        //  Nothing in sight.
        lost++;
        if (lost > 20){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "walk_to_quest_marker(): Unable to find the quest marker for " + name + ".",
                env.console
            );
        }
        env.log("No quest marker on screen. Turning the camera to look for it.", COLOR_RED);
        pbf_move_right_joystick(context, {1, 0}, 250ms, 400ms);
    }

    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "walk_to_quest_marker(): Did not reach " + name + ".",
        env.console
    );
}



void AutoStory::walk_to_lab(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Walking to the Pokemon lab.");

    //  Put the camera behind the player.
    pbf_press_button(context, BUTTON_L, 160ms, 500ms);

    for (int attempt = 0; attempt < 20; attempt++){
        WalkResult result = walk_to_quest_marker(env, context, "Lab", true);
        if (result == WalkResult::Dialog){
            //  Getting close to the lab starts a story scene.
            clear_dialogs(env, context, "Lab story", false);
            pbf_press_button(context, BUTTON_L, 160ms, 500ms);
            continue;
        }

        //  "A 進入" at the door.
        env.log("Entering the lab.");
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);

        //  Inside once the HUD is back without the minimap.
        WallClock start = current_time();
        int inside = 0;
        bool dialog = false;
        while (current_time() - start < std::chrono::seconds(15)){
            context.wait_for_all_requests();
            VideoSnapshot snapshot = env.console.video().snapshot();
            if (dialog_on_screen(snapshot)){
                dialog = true;
                break;
            }
            double heading = 0;
            if (read_party_slot_count(snapshot) >= 1 && !read_camera_heading(snapshot, heading)){
                if (++inside >= 3){
                    env.log("Inside the lab.", COLOR_BLUE);
                    env.console.overlay().add_log("Inside the lab");
                    return;
                }
            }else{
                inside = 0;
            }
            context.wait_for(500ms);
        }
        if (dialog){
            clear_dialogs(env, context, "Lab door", false);
        }else{
            env.log("Still outside the lab. Trying again.", COLOR_RED);
        }
    }

    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "walk_to_lab(): Unable to get into the lab.",
        env.console
    );
}



void AutoStory::take_lab_elevator(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Taking the lab elevator to 3F.");

    for (int attempt = 0; attempt < 10; attempt++){
        WalkResult result = walk_to_quest_marker(env, context, "Elevator", false);
        if (result == WalkResult::Dialog){
            clear_dialogs(env, context, "Lab 1F", false);
            continue;
        }

        //  "A 搭電梯".
        pbf_press_button(context, BUTTON_A, 160ms, 340ms);

        //  Floor menu: pick the 2nd row (3F). Judge by the green pointer and
        //  the white highlight, not the text.
        WallClock start = current_time();
        bool picked = false;
        bool menu_seen = false;
        while (current_time() - start < std::chrono::seconds(8)){
            context.wait_for_all_requests();
            VideoSnapshot snapshot = env.console.video().snapshot();
            int row = read_choice_menu_row(snapshot);
            if (row < 0){
                if (!menu_seen && dialog_on_screen(snapshot)){
                    break;  //  Talked to someone instead.
                }
                context.wait_for(200ms);
                continue;
            }
            menu_seen = true;
            env.log("Elevator menu: pointer on row " + std::to_string(row + 1) + ".");
            if (row == 1){
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                picked = true;
                break;
            }
            if (row == 0){
                pbf_press_dpad(context, DPAD_DOWN, 160ms, 340ms);
            }else{
                pbf_press_dpad(context, DPAD_UP, 160ms, 340ms);
            }
        }
        if (picked){
            env.log("Picked 3F.", COLOR_BLUE);
            env.console.overlay().add_log("Elevator: 3F");
            return;
        }
        if (menu_seen){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "take_lab_elevator(): Unable to pick the 2nd floor in the elevator menu.",
                env.console
            );
        }
        env.log("No elevator menu. Trying again.", COLOR_RED);
        clear_dialogs(env, context, "Lab 1F", false);
    }

    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "take_lab_elevator(): Unable to use the elevator.",
        env.console
    );
}



void AutoStory::walk_to_mimi(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Walking to Mimi on 3F.");

    //  There is a desk right in front of the elevator. Go around it to the
    //  left first.
    pbf_move_left_joystick(context, {-0.7, 0.7}, 1200ms, 300ms);

    WalkResult result = walk_to_quest_marker(env, context, "Mimi", false);
    if (result == WalkResult::Prompt){
        //  Next to her before the scene started: talk.
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    }
    clear_dialogs(env, context, "Mimi", true);
}



void AutoStory::open_research_list(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Opening Mimi's research list.");
    env.console.overlay().add_log("Menu: research list");

    pbf_press_button(context, BUTTON_X, 160ms, 1840ms);    //  main menu
    pbf_press_button(context, BUTTON_X, 160ms, 2840ms);    //  reward list (Mimi's research)

    //  Back out to the overworld. Closing the menu starts the story.
    for (int c = 0; c < 8; c++){
        pbf_press_button(context, BUTTON_B, 160ms, 1240ms);
        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();
        CutsceneLetterboxDetector letterbox;
        if (dialog_on_screen(snapshot) || letterbox.detect(snapshot) || read_party_slot_count(snapshot) >= 1){
            env.log("Menu closed.", COLOR_BLUE);
            env.console.overlay().add_log("Menu closed");
            return;
        }
    }
    env.log("Unable to confirm the menu closed.", COLOR_RED);
}



void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    assert_16_9_720p_min(env.logger(), env.console);

    //  Connect the controller.
    require_player(env.console, context, BUTTON_NONE);

    if (CAMERA_MODE.get() == CameraMode::OnlyFaceNorthAndStop){
        env.log("Camera Mode is \"Only face north, then stop\". Not running the story.", COLOR_BLUE);
        face_camera_north(env, context);
        return;
    }

    const StartPoint start = START_POINT.get();

    //  ZA-00: create character.
    if (start <= StartPoint::CreateCharacter){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Creating character.");
        run_macro(env, context, MACRO_CREATE_CHAR_0);
    }
    if (start <= StartPoint::SelectCharacter){
        select_character(env, context);
        run_macro(env, context, MACRO_CREATE_CHAR_3);
    }

    //  ZA-01: train station, chase Pancham.
    if (start <= StartPoint::TrainStation){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Train station.");
        run_macro(env, context, MACRO_STATION);
    }

    //  ZA-02: chase Pancham, pick starter.
    if (start <= StartPoint::ChasePancham){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Chasing Pancham.");
        run_macro(env, context, MACRO_CHASE_PANCHAM);
    }
    if (start <= StartPoint::ChooseStarter){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Choosing starter.");
        run_macro(env, context, starter_macro_filename());
    }

    //  ZA-02: first battle. Ends on the post-battle dialog.
    if (start <= StartPoint::FirstBattle){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "First battle.");
        run_trainer_battle(env, context, "First Battle");
    }

    //  ZA-03: battle zone, escape.
    if (start <= StartPoint::BattleZone){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Battle zone.");
        run_macro(env, context, MACRO_BATTLE_ZONE);
    }
    if (start <= StartPoint::EscapeBattleZone){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Escaping the battle zone.");
        run_macro(env, context, MACRO_ESCAPE_0);
        run_macro(env, context, MACRO_ESCAPE_1);
    }

    //  ZA-03: waitress battle. Ends on the post-battle dialog.
    if (start <= StartPoint::WaitressBattle){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Waitress battle.");
        run_trainer_battle(env, context, "Waitress Battle");
    }

    //  ZA-03: escape the battle zone.
    if (start <= StartPoint::EscapeBattleZone2){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Escaping the battle zone.");
        run_macro(env, context, MACRO_ESCAPE_2_1);
        run_macro(env, context, MACRO_ESCAPE_2_2);
    }
    if (start <= StartPoint::EscapeBattleZone3){
        run_macro(env, context, MACRO_ESCAPE_3);
    }

    //  ZA-04: walk into Hotel Z.
    if (start <= StartPoint::WalkIntoHotel){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Walking into Hotel Z.");
        run_macro(env, context, MACRO_HOTEL_WALK_0);
        run_macro(env, context, MACRO_HOTEL_WALK_1);
    }

    //  ZA-05: Hotel Z.
    if (start <= StartPoint::HotelIntro){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Hotel Z.");
        run_macro(env, context, MACRO_HOTEL_INTRO);
        run_macro(env, context, MACRO_HOTEL_AZ_1);
        run_macro(env, context, MACRO_HOTEL_AZ_2);
    }
    if (start <= StartPoint::HotelUpstairs){
        run_macro(env, context, MACRO_HOTEL_ELEVATOR);
        run_macro(env, context, MACRO_HOTEL_ROOM);
        run_macro(env, context, MACRO_HOTEL_BED);
        run_macro(env, context, MACRO_HOTEL_DOOR);
    }
    if (start <= StartPoint::HotelNight){
        run_macro(env, context, MACRO_HOTEL_NIGHT);
    }
    if (start <= StartPoint::HotelMorning){
        run_macro(env, context, MACRO_HOTEL_MORNING);
    }
    if (start <= StartPoint::RollTutorial){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Roll tutorial.");
        run_macro(env, context, MACRO_ROLL_TUTORIAL);
        run_macro(env, context, MACRO_ROLL_TO_DOOR);
        run_macro(env, context, MACRO_LEAVE_HOTEL);
    }

    //  ZA-06: Urbain.
    if (start <= StartPoint::WalkToUrbain){
        run_macro(env, context, MACRO_WALK_TO_URBAIN);
    }

    //  ZA-06: Urbain battle. Ends on the post-battle dialog.
    if (start <= StartPoint::UrbainBattle){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Urbain battle.");
        run_trainer_battle(env, context, "Urbain Battle");
    }

    //  ZA-07 .. ZA-11.
    if (start <= StartPoint::MapTutorial){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Map tutorial.");
        run_macro(env, context, MACRO_MAP_TUTORIAL);
    }
    if (start <= StartPoint::RockScene){
        run_macro(env, context, MACRO_ROCKS_WALK);
        run_macro(env, context, MACRO_ROCKS_SCENE);
    }
    if (start <= StartPoint::Rooftop){
        run_macro(env, context, MACRO_LADDER);
        run_macro(env, context, MACRO_ROOFTOP);
        run_macro(env, context, MACRO_JUMP_DOWN);
    }
    if (start <= StartPoint::PokemonCenterPC){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "PC / Pokedex.");
        run_macro(env, context, MACRO_PC_SCENE);
        run_macro(env, context, MACRO_WALK_TO_PC);
        run_macro(env, context, MACRO_POKEDEX_TUTORIAL);
    }
    if (start <= StartPoint::WildZone1){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Wild Zone 1.");
        run_macro(env, context, MACRO_WILD_ZONE_1);
        run_macro(env, context, MACRO_WILD_ZONE_2);
        run_macro(env, context, MACRO_WILD_ZONE_3);
    }

    //  ZA-12: catching.
    if (start <= StartPoint::FirstCatch){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "First catch.");
        run_macro(env, context, MACRO_FIRST_CATCH);
    }

    //  ZA-12: Bunnelby battle + catch. Ends on the dialog after the catch.
    if (start <= StartPoint::BunnelbyCapture){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Bunnelby battle.");
        run_capture_battle(env, context);
    }
    if (start <= StartPoint::AfterBunnelby){
        run_macro(env, context, MACRO_AFTER_BUNNELBY);
    }

    //  ZA-12: catch Scatterbug and Weedle without battling.
    if (start <= StartPoint::CatchScatterbug){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Catching Scatterbug.");
        //  ZA-12-3 always ends in the same spot with the same camera, so no
        //  camera reset is needed before this walk.
        run_macro(env, context, MACRO_WALK_TO_SCATTERBUG);
        catch_wild_pokemon(env, context, "scatterbug", "Scatterbug");
    }
    if (start <= StartPoint::CatchWeedle){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Catching Weedle.");
        if (WILD_ZONE_WALK.get() == WildZoneWalk::MinimapNavigation){
            navigate_by_minimap(env, context, "Weedle spot", WEEDLE_SPOT_X, WEEDLE_SPOT_Y, 0.004);
        }else{
            //  Catching Scatterbug turned the camera. Reset it first.
            face_camera_north(env, context);
            run_macro(env, context, MACRO_WALK_TO_WEEDLE);
        }
        catch_wild_pokemon(env, context, "weedle", "Weedle");
    }

    //  ZA-12: leave Wild Zone 1.
    if (start <= StartPoint::LeaveWildZone){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Leaving Wild Zone 1.");
        if (WILD_ZONE_WALK.get() == WildZoneWalk::MinimapNavigation){
            std::pair<double, double> stick = navigate_by_minimap(
                env, context, "Gate", GATE_SPOT_X, GATE_SPOT_Y, 0.004
            );

            //  Standing on the gate marker. If the exit prompt is not up yet,
            //  keep going the same way a little at a time (a few tries; then
            //  leave_wild_zone() presses A anyway).
            const ImageFloatBox PROMPT_BOX(0.25, 0.0, 0.5, 0.85);
            for (int c = 0; c < 3; c++){
                context.wait_for_all_requests();
                ButtonWatcher prompt(COLOR_RED, ButtonType::ButtonA, PROMPT_BOX, &env.console.overlay());
                if (wait_until(env.console, context, 1000ms, {prompt}) == 0){
                    break;
                }
                env.log("No exit prompt yet. Stepping toward the gate.");
                pbf_move_left_joystick(context, {stick.first, stick.second}, 300ms, 300ms);
            }
        }else{
            face_camera_north(env, context);
            run_macro(env, context, MACRO_WALK_TO_GATE);
        }
        leave_wild_zone(env, context);
    }

    //  ZA-13: the lab. Each step starts where the game saves.
    if (start <= StartPoint::WalkToLab){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Walking to the lab.");
        walk_to_lab(env, context);
    }
    if (start <= StartPoint::LabElevator){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Lab elevator.");
        take_lab_elevator(env, context);
        clear_dialogs(env, context, "3F story", true);
    }
    if (start <= StartPoint::LabMimi){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Walking to Mimi.");
        walk_to_mimi(env, context);
    }
    open_research_list(env, context);

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



}
}
}
