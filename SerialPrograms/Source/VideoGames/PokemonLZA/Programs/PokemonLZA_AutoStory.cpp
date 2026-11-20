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

}



AutoStory_Descriptor::AutoStory_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStory",
        STRING_POKEMON + " LZA", "Auto Story",
        "",
        "Play the opening of the story from a new save: create the character, "
        "pick a starter and win the first battle.",
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



void AutoStory::run_first_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    AutoStory_Descriptor::Stats& stats = env.current_stats<AutoStory_Descriptor::Stats>();

    env.log("Starting first battle.");
    env.console.overlay().add_log("First Battle");

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
                "run_first_battle(): Battle did not finish within 5 minutes.",
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

    send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Creating character.");

    //  ZA-00: create character.
    run_macro(env, context, MACRO_CREATE_CHAR_0);
    select_character(env, context);
    run_macro(env, context, MACRO_CREATE_CHAR_3);

    //  ZA-01: train station, chase Pancham.
    send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Train station.");
    run_macro(env, context, MACRO_STATION);

    //  ZA-02: chase Pancham, pick starter.
    send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Choosing starter.");
    run_macro(env, context, MACRO_CHASE_PANCHAM);
    run_macro(env, context, starter_macro_filename());

    //  ZA-02: first battle. Stops on the post-battle dialog.
    send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "First battle.");
    run_first_battle(env, context);

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}



}
}
}
