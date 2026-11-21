/*  Auto Story
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_CharacterSelectDetector.h"
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
const char* MACRO_WALK_TO_GAI       = "ZA-06-1-走向蓋伊對戰.json";

}



AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        STRING_POKEMON + " LZA", "Auto Story",
        "",
        "Play the opening of the story from a new save: create the character, "
        "pick a starter, win the first battle, escape the battle zone, stay at Hotel Z, and win the battle against Gai.",
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
            {StartPoint::WalkToGai,       "walk-to-gai",      "ZA-06-1 Walk to Gai"},
            {StartPoint::GaiBattle,       "gai-battle",       "ZA-06-2 Gai battle (pre-battle screen)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        StartPoint::CreateCharacter
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



void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    assert_16_9_720p_min(env.logger(), env.console);

    //  Connect the controller.
    require_player(env.console, context, BUTTON_NONE);

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

    //  ZA-06: Gai.
    if (start <= StartPoint::WalkToGai){
        run_macro(env, context, MACRO_WALK_TO_GAI);
    }

    //  ZA-06: Gai battle. Stops on the post-battle dialog.
    send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Gai battle.");
    run_trainer_battle(env, context, "Gai Battle");

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}



}
}
}
