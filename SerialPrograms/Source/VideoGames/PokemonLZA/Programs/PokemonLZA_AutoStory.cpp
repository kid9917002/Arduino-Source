/*  Auto Story
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
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
#include "CommonTools/StartupChecks/VideoResolutionCheck.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/Pokemon_Strings.h"
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

}



AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        STRING_POKEMON + " LZA", "Auto Story",
        "",
        "Play the opening of the story from a new save: create the character, "
        "pick a starter, win the first battle, escape the battle zone, stay at Hotel Z, beat Urbain, catch Bunnelby, Scatterbug and Weedle, and leave Wild Zone 1.",
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
        },
        LockMode::LOCK_WHILE_RUNNING,
        StartPoint::CreateCharacter
    )
    , FACE_NORTH_ONLY(
        "<b>Only Face Camera North:</b><br>"
        "Turn the camera to face north and stop. Use this right before recording "
        "ZA-12-4 / ZA-12-5 / ZA-12-6, because the program does the same before playing them.",
        LockMode::LOCK_WHILE_RUNNING,
        false
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
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(MACRO_FOLDER);
    PA_ADD_OPTION(START_POINT);
    PA_ADD_OPTION(FACE_NORTH_ONLY);
    PA_ADD_OPTION(CHARACTER);
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(BATTLE_AI);
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
    for (int iteration = 0; iteration < 20; iteration++){
        if (!read_heading(heading)){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "face_camera_north(): Unable to read the minimap view cone.",
                env.console
            );
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
    const std::string& name
){
    env.log("Catching " + name + ".");
    env.console.overlay().add_log("Catch: " + name);

    VideoOverlaySet overlays(env.console.overlay());
    make_party_slot_overlays(overlays);

    //  Read how many Pokemon are in the party now. Take the highest of a few
    //  reads in case something briefly covers the HUD.
    context.wait_for_all_requests();
    int before = 0;
    for (int c = 0; c < 3; c++){
        before = std::max(before, read_party_slot_count(env.console.video().snapshot()));
        context.wait_for(200ms);
    }
    env.log("Party slots before catching " + name + ": " + std::to_string(before));

    for (int attempt = 1; attempt <= 6; attempt++){
        env.log(name + ": throw attempt " + std::to_string(attempt));
        throw_ball(env, context);

        //  The ball flies, shakes, then the Pokemon shows up in the party
        //  bar (with the "登錄完畢" badge for a new Pokedex entry).
        WallClock start = current_time();
        int confirmations = 0;
        while (current_time() - start < std::chrono::seconds(12)){
            context.wait_for_all_requests();
            VideoSnapshot snapshot = env.console.video().snapshot();

            TutorialWindowDetector tutorial;
            if (tutorial.detect(snapshot)){
                env.log("Tutorial window. Pressing A.");
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                continue;
            }

            int now = read_party_slot_count(snapshot);
            if (now > before){
                confirmations++;
                if (confirmations >= 2){
                    env.log(name + " caught. Party slots: " + std::to_string(now), COLOR_BLUE);
                    env.console.overlay().add_log(name + " Caught");
                    context.wait_for(1000ms);
                    return;
                }
            }else{
                confirmations = 0;
            }
            context.wait_for(400ms);
        }
        env.log(name + ": not caught yet.", COLOR_RED);
    }

    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "catch_wild_pokemon(): Failed to catch " + name + " after 6 throws. "
        "Check that the walking macro ends facing it.",
        env.console
    );
}



void AutoStory::leave_wild_zone(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Leaving the Wild Zone.");
    env.console.overlay().add_log("Leave Wild Zone");

    //  "A 離開" prompt next to the player at the gate.
    const ImageFloatBox PROMPT_BOX(0.50, 0.53, 0.10, 0.12);

    context.wait_for_all_requests();
    ButtonWatcher prompt(COLOR_RED, ButtonType::ButtonA, PROMPT_BOX, &env.console.overlay());
    int ret = wait_until(env.console, context, 10000ms, {prompt});
    if (ret < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            "leave_wild_zone(): No \"A 離開\" prompt. Check that the walking macro ends at the gate.",
            env.console
        );
    }

    env.log("Exit prompt found. Pressing A.", COLOR_BLUE);
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    context.wait_for_all_requests();

    //  Wait until the prompt is gone and the transition has finished.
    for (int c = 0; c < 10; c++){
        ButtonWatcher still_there(COLOR_RED, ButtonType::ButtonA, PROMPT_BOX, &env.console.overlay());
        ret = wait_until(env.console, context, 1000ms, {still_there});
        if (ret < 0){
            break;
        }
        env.log("Exit prompt still there. Pressing A again.");
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
        context.wait_for_all_requests();
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



void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    assert_16_9_720p_min(env.logger(), env.console);

    //  Connect the controller.
    require_player(env.console, context, BUTTON_NONE);

    if (FACE_NORTH_ONLY){
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
        face_camera_north(env, context);
        run_macro(env, context, MACRO_WALK_TO_SCATTERBUG);
        catch_wild_pokemon(env, context, "Scatterbug");
    }
    if (start <= StartPoint::CatchWeedle){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Catching Weedle.");
        face_camera_north(env, context);
        run_macro(env, context, MACRO_WALK_TO_WEEDLE);
        catch_wild_pokemon(env, context, "Weedle");
    }

    //  ZA-12: leave Wild Zone 1. Stops once outside.
    send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Leaving Wild Zone 1.");
    face_camera_north(env, context);
    run_macro(env, context, MACRO_WALK_TO_GATE);
    leave_wild_zone(env, context);

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}



}
}
}
