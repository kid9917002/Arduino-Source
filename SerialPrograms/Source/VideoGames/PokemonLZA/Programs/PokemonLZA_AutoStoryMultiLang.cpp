/*  Auto Story - Multiple Lang
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <map>
#include <cmath>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
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
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Pokemon/Inference/Pokemon_NameReader.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "CommonTools/Images/BinaryImage_FilterRgb32.h"
#include "Kernels/Waterfill/Kernels_Waterfill.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/Tools/GlobalThreadPools.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_CharacterSelectDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_ButtonDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_StoryScreenDetectors.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_TrainerBattle.h"
#include "PokemonLZA_ChineseReadings.h"
#include "PokemonLZA_AutoStoryMultiLang.h"
#include "PokemonLZA_AutoStory.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;
using namespace Kernels::Waterfill;

bool overworld_hud(const ImageViewRGB32& screen);



//  Macro files (Turbo Macro JSON) expected in MACRO_FOLDER, or by default
//  in Resources/PokemonLZA/AutoStory/ of the program folder.
namespace{

const char* MACRO_SUBFOLDER_DEFAULT = "PokemonLZA/AutoStory/";

//  ZA-00 (multi-language): the game start, the language and the name are
//  done by the program. These two hold the button presses around the name.
const char* MACRO_STARTER_CHIKORITA = "ZA-02-1-選擇御三家(菊草葉).json";
const char* MACRO_STARTER_TEPIG     = "ZA-02-1-選擇御三家(暖暖豬).json";
const char* MACRO_STARTER_TOTODILE  = "ZA-02-1-選擇御三家(小鋸鱷).json";
//  Walking routes. Each ends standing still, facing the target.

//  Wild Zone 1 spots for minimap navigation: the player's position relative
//  to the "1" zone marker on the minimap, in frame widths (x east, y south).
//  Measured from screenshots taken at each spot.
const double WEEDLE_SPOT_X = -0.0019;
const double WEEDLE_SPOT_Y =  0.0058;
const double GATE_SPOT_X   = -0.0298;
const double GATE_SPOT_Y   =  0.0184;

}



AutoStoryMultiLang_Descriptor::AutoStoryMultiLang_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:AutoStoryMultiLang",
        STRING_POKEMON + " LZA", "Auto Story",
        "",
        "Play the opening of the story from a new save: create the character, "
        "pick a starter, win the first battle, escape the battle zone, stay at Hotel Z, beat the rival, catch Bunnelby, Scatterbug and Weedle, "
        "leave Wild Zone 1, go to the lab and open Mable's Research (茉蜜姬調查).",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}
AutoStoryMultiLang_Descriptor::AutoStoryMultiLang_Descriptor(
    std::string identifier, std::string display_name, std::string description
)
    : SingleSwitchProgramDescriptor(
        std::move(identifier),
        STRING_POKEMON + " LZA", std::move(display_name),
        "",
        std::move(description),
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}
class AutoStoryMultiLang_Descriptor::Stats : public StatsTracker{
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
std::unique_ptr<StatsTracker> AutoStoryMultiLang_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}



AutoStoryMultiLang::~AutoStoryMultiLang(){
    START_POINT.remove_listener(*this);
    START_MODE.remove_listener(*this);
}
AutoStoryMultiLang::AutoStoryMultiLang()
    : SECTION_START("<font size=4><b>Start</b></font>")
    , START_MODE(
        "<b>Start Mode:</b>",
        StartModeOption::CONTINUE
    )
    , SECTION_CHAPTER("<font size=4><b>Chapter</b></font>")
    , MACRO_FOLDER(
        false,
        "<b>Macro Folder:</b><br>"
        "Folder that contains story_flow.json and the Turbo Macro JSON files. "
        "Default: Resources\\PokemonLZA\\AutoStory of the program folder.",
        LockMode::LOCK_WHILE_RUNNING,
        "Resources/PokemonLZA/AutoStory",
        "Resources/PokemonLZA/AutoStory"
    )
    , START_POINT(
        "<b>Start From:</b><br>"
        "Which step to start at (Load save at a step / Continue from screen).",
        {
            {StartPoint::CreateCharacter, "create-character", "ZA-00 Start the game + language (beginning, from Home)"},
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
            {StartPoint::RollTutorial,    "roll-tutorial",    "ZA-05-9 Roll tutorial"},
            {StartPoint::LeaveHotel,      "leave-hotel",      "ZA-05-10 Leave Hotel Z / walk to the rival (after the roll tutorial save)"},
            {StartPoint::RivalBattle,     "urbain-battle",    "ZA-06-2 Rival (Urbain / Taunie) battle (pre-battle screen)"},
            {StartPoint::MapTutorial,     "map-tutorial",     "ZA-07 Map tutorial (after the rival battle)"},
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
            {StartPoint::LabMable,         "lab-mable",         "ZA-13-3 Lab 3F: walk to Mable (茉蜜姬) at the \"!\""},
            {StartPoint::ResearchMenu,    "research-menu",    "ZA-13-4 Next to Mable: open Mable's Research (茉蜜姬調查) (X, X, B)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        StartPoint::CreateCharacter
    )
    , SECTION_GAME("<font size=4><b>Game Information</b></font>")
    , PROFILE(
        "<b>User Profile Position:</b><br>"
        "The position, from left to right, of the Switch user profile to start the game with "
        "(New game, Load save at a step).<br>"
        "If this is set to 0, Switch 1 defaults to the last-used profile, while Switch 2 defaults to the first profile (position 1).",
        LockMode::LOCK_WHILE_RUNNING,
        0, 0, 8 // default, min, max
    )
    , GAME_LANGUAGE(
        "<b>Game Language:</b><br>"
        "Picked on the in-game LANGUAGE screen of a new save, and used to read Pokemon names.",
        {
            {GameLanguage::ChineseTraditional, "zh-Hant",  "繁體中文 (Chinese Traditional)"},
            {GameLanguage::English,            "en",       "English"},
            {GameLanguage::SpanishSpain,       "es-ES",    "Español (España)"},
            {GameLanguage::SpanishLatAm,       "es-419",   "Español (Latinoamérica)"},
            {GameLanguage::French,             "fr",       "Français"},
            {GameLanguage::German,             "de",       "Deutsch"},
            {GameLanguage::Italian,            "it",       "Italiano"},
            {GameLanguage::Japanese,           "ja",       "日本語 (Japanese)"},
            {GameLanguage::Korean,             "ko",       "한국어 (Korean)"},
            {GameLanguage::ChineseSimplified,  "zh-Hans",  "简体中文 (Chinese Simplified)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        GameLanguage::ChineseTraditional
    )
    , NEW_SAVE_NOTE("<i>Only for a new save (greyed out when the run starts after that point):</i>")
    , CHARACTER(
        "<b>Character Look:</b><br>"
        "Click the portrait to pick on the LOOKS screen.",
        2
    )
    , CHARACTER_NAME(
        false,
        "<b>Character Name:</b><br>"
        "Just paste the name. It is typed on the keyboard of the Game Language: "
        "繁體中文 注音 / 简体中文 拼音 (readings looked up automatically, the character is picked from the candidates), "
        "日本語 ひらがな・カタカナ, 한국어 한글, other languages letters (A-Z, digits). "
        "Up to 6 characters (12 for letter keyboards).<br>"
        "If a Chinese character is not found, give its reading by hand: <b>塔:ㄊㄚˇ</b> (注音) or <b>塔:ta</b> (拼音).",
        LockMode::LOCK_WHILE_RUNNING,
        "塔霓",
        "塔霓"
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
    , SECTION_PROGRAM("<font size=4><b>Program Settings</b></font>")
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_STATIC(SECTION_START);
    PA_ADD_OPTION(START_LOCATION);
    PA_ADD_OPTION(START_MODE);
    PA_ADD_STATIC(SECTION_CHAPTER);
    PA_ADD_OPTION(MACRO_FOLDER);
    PA_ADD_OPTION(START_POINT);
    PA_ADD_STATIC(SECTION_GAME);
    PA_ADD_OPTION(PROFILE);
    PA_ADD_OPTION(GAME_LANGUAGE);
    PA_ADD_STATIC(NEW_SAVE_NOTE);
    PA_ADD_OPTION(CHARACTER);
    PA_ADD_OPTION(CHARACTER_NAME);
    PA_ADD_OPTION(STARTER);
    PA_ADD_STATIC(SECTION_PROGRAM);
    PA_ADD_OPTION(BATTLE_AI);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);

    START_MODE.add_listener(*this);
    START_POINT.add_listener(*this);
    AutoStoryMultiLang::on_config_value_changed(this);
}
void AutoStoryMultiLang::on_config_value_changed(void*){
    //  Grey out what the chosen start does not use.
    const bool new_game = START_MODE.new_game();
    const bool cont = START_MODE.continue_from_screen();
    const StartPoint start = new_game ? StartPoint::CreateCharacter : START_POINT.get();
    auto state = [](bool enabled){
        return enabled ? ConfigOptionState::ENABLED : ConfigOptionState::DISABLED;
    };
    START_POINT.set_visibility(state(!new_game));
    //  The profile is used whenever the game is (re)started.
    PROFILE.set_visibility(state(!cont || start <= StartPoint::CreateCharacter));
    CHARACTER.set_visibility(state(start <= StartPoint::SelectCharacter));
    CHARACTER_NAME.set_visibility(state(start <= StartPoint::SelectCharacter));
    STARTER.set_visibility(state(start <= StartPoint::ChooseStarter));
}



std::string AutoStoryMultiLang::starter_macro_filename() const{
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



void AutoStoryMultiLang::run_macro(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& filename
){
    AutoStoryMultiLang_Descriptor::Stats& stats = env.current_stats<AutoStoryMultiLang_Descriptor::Stats>();

    context.wait_for_all_requests();

    std::string path = macro_file_path(filename);

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

    env.log("Running macro: " + filename, COLOR_BLUE);
    env.console.overlay().add_log("Macro: " + filename);

    //  Multi-language: an entry with the "Capture" button is a marker, not a
    //  button press. At a marker, press A once (talk / advance) and then
    //  clear dialogs by detecting them (clear_dialogs()) until the overworld
    //  is back, instead of recorded A presses whose timing depends on the
    //  text length of the language. The marker's "ms" is how long the
    //  overworld must stay quiet to count as done (default 4000 ms).
    //  The macro is split at the markers and the pieces run in order.
    const JsonObject& object = json.to_object_throw(path);
    std::string key = object.get_array("Schedule") != nullptr ? "Schedule" : "history";
    const JsonArray& entries = object.get_array_throw(key, path);

    auto run_piece = [&](JsonArray&& piece){
        if (piece.empty()){
            return;
        }
        JsonObject piece_object = object.clone();
        piece_object[key] = JsonValue(std::move(piece));
        ControllerCommandTable table(
            ControllerClass::NintendoSwitch_ProController,
            "",
            LockMode::LOCK_WHILE_RUNNING
        );
        table.load_json(JsonValue(std::move(piece_object)));
        table.run(context, env.console.controller());
        context.wait_for_all_requests();
    };

    auto entry_ms = [](const JsonObject& entry_object, int64_t default_ms) -> int64_t{
        const JsonValue* ms = entry_object.get_value("ms");
        if (ms == nullptr){
            return default_ms;
        }
        if (ms->is_integer()){
            return ms->to_integer_default(default_ms);
        }
        if (ms->is_string()){
            std::string digits;
            for (char ch : *ms->to_string()){
                if (ch >= '0' && ch <= '9'){
                    digits += ch;
                }
            }
            if (!digits.empty()){
                return std::stoll(digits);
            }
        }
        return default_ms;
    };

    JsonArray piece;
    size_t markers = 0;
    for (const JsonValue& entry : entries){
        const JsonObject* entry_object = entry.to_object();
        const std::string* buttons = entry_object == nullptr ? nullptr : entry_object->get_string("buttons");

        //  A recorded long + press skips a video. Only do it while a video
        //  (or scene) is playing: on the overworld, + would open the menu.
        //  clear_dialogs() may already have skipped it.
        if (buttons != nullptr && *buttons == "+" && entry_ms(*entry_object, 0) >= 1500){
            run_piece(std::move(piece));
            piece = JsonArray();
            context.wait_for_all_requests();
            int64_t hold_ms = entry_ms(*entry_object, 0);
            if (m_hud_expected && overworld_hud(env.console.video().snapshot())){
                env.log("Recorded + (skip video): on the overworld, so not pressing it.", COLOR_BLUE);
                context.wait_for(Milliseconds(std::min<int64_t>(hold_ms, 1000)));
            }else{
                env.log("Recorded + (skip video): holding + for " + std::to_string(hold_ms) + " ms.");
                pbf_press_button(context, BUTTON_PLUS, Milliseconds(hold_ms), 0ms);
                context.wait_for_all_requests();
            }
            continue;
        }

        if (buttons == nullptr || *buttons != "Capture"){
            piece.push_back(entry.clone());
            continue;
        }

        run_piece(std::move(piece));
        piece = JsonArray();

        int64_t quiet_ms = entry_ms(*entry_object, 4000);
        quiet_ms = std::max<int64_t>(1000, std::min<int64_t>(quiet_ms, 30000));

        markers++;
        clear_dialogs(
            env, context,
            filename + " #" + std::to_string(markers),
            true, Milliseconds(quiet_ms), true, std::chrono::seconds(20)
        );
    }
    run_piece(std::move(piece));

    stats.macros++;
    env.update_stats();
}



std::string AutoStoryMultiLang::macro_file_path(const std::string& filename) const{
    std::string path = MACRO_FOLDER;
    if (path.empty()){
        path = RESOURCE_PATH() + MACRO_SUBFOLDER_DEFAULT;
    }else if (path.rfind("Resources/", 0) == 0 || path.rfind("Resources\\", 0) == 0){
        //  "Resources/..." is the Resources folder of the program, wherever
        //  the program was started from.
        path = RESOURCE_PATH() + path.substr(10);
    }
    if (!path.empty() && path.back() != '/' && path.back() != '\\'){
        path += "/";
    }
    return path + filename;
}

void AutoStoryMultiLang::story_dialog(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& name, Milliseconds wait_before, Milliseconds wait_after
){
    //  A step that is only story dialog: talk / advance (A), clear the
    //  dialogs by detection, then wait like the recorded macro did.
    //
    //  story_dialog.json in the macro folder may override the numbers:
    //      { "ZA-03-0 battle zone": { "before": 10, "after": 0, "quiet": 4000 }, ... }
    //  A missing file, step or key keeps the default below.
    Milliseconds quiet = 4000ms;
    const char* source = "default";
    JsonValue holder;
    const JsonObject* step = story_dialog_step(env, name, holder);
    if (step != nullptr){
        wait_before = Milliseconds(step->get_integer_default("before", wait_before.count()));
        wait_after  = Milliseconds(step->get_integer_default("after",  wait_after.count()));
        quiet       = Milliseconds(step->get_integer_default("quiet",  quiet.count()));
        source = "story_dialog.json";
    }
    env.log(
        "Story dialog " + name + " (" + source + "): before " + std::to_string(wait_before.count()) +
        " ms, after " + std::to_string(wait_after.count()) +
        " ms, quiet " + std::to_string(quiet.count()) + " ms"
    );

    context.wait_for_all_requests();
    context.wait_for(wait_before);
    clear_dialogs(env, context, name, true, quiet, true, std::chrono::seconds(20));
    context.wait_for(wait_after);
}


const JsonObject* AutoStoryMultiLang::story_dialog_step(
    SingleSwitchProgramEnvironment& env, const std::string& name, JsonValue& holder
) const{
    std::string text;
    if (!file_to_string(macro_file_path("story_dialog.json"), text)){
        return nullptr;
    }
    if (text.size() >= 3 &&
        (uint8_t)text[0] == 0xef &&
        (uint8_t)text[1] == 0xbb &&
        (uint8_t)text[2] == 0xbf
    ){
        text.erase(0, 3);
    }
    try{
        holder = parse_json(text);
    }catch (Exception& e){
        env.log("story_dialog.json is not valid JSON, using the defaults: " + e.message(), COLOR_RED);
        return nullptr;
    }
    const JsonObject* all = holder.to_object();
    return all == nullptr ? nullptr : all->get_object(name);
}

Milliseconds AutoStoryMultiLang::dialog_quiet(
    SingleSwitchProgramEnvironment& env, const std::string& name, Milliseconds quiet
) const{
    const char* source = "default";
    auto override_iter = m_quiet_overrides.find(name);
    if (override_iter != m_quiet_overrides.end()){
        quiet = override_iter->second;
        source = "story_flow.json";
    }else{
        JsonValue holder;
        const JsonObject* step = story_dialog_step(env, name, holder);
        if (step != nullptr){
            quiet = Milliseconds(step->get_integer_default("quiet", quiet.count()));
            source = "story_dialog.json";
        }
    }
    env.log("Dialog quiet " + name + " (" + source + "): quiet " + std::to_string(quiet.count()) + " ms");
    return quiet;
}



namespace{

//  LOOKS screen: which portrait is highlighted (white tile), or -1.
//  Looks at a strip along the left edge of each tile over its full height
//  and counts white pixels, so hair reaching into part of the strip (e.g.
//  the blonde looks) does not hide the highlight.
int highlighted_portrait(const ImageViewRGB32& screen, bool& has_marker){
    const double CX[4] = {0.2562, 0.4196, 0.5830, 0.7451};
    const double CY[2] = {0.4112, 0.6776};
    const double HW = 0.0623;
    const double HH = 0.1110;
    int found = -1;
    int count = 0;
    for (int r = 0; r < 2; r++){
        for (int c = 0; c < 4; c++){
            ImageFloatBox strip(CX[c] - HW * 0.99, CY[r] - HH * 0.85, HW * 0.11, HH * 1.70);
            ImageViewRGB32 image = extract_box_reference(screen, strip);
            size_t white = 0;
            size_t total = 0;
            for (size_t y = 0; y < image.height(); y++){
                for (size_t x = 0; x < image.width(); x++){
                    uint32_t pixel = image.pixel(x, y);
                    int pr = (pixel >> 16) & 0xff;
                    int pg = (pixel >>  8) & 0xff;
                    int pb = (pixel >>  0) & 0xff;
                    total++;
                    if (pr >= 215 && pg >= 215 && pb >= 215){
                        white++;
                    }
                }
            }
            if (total > 0 && white * 10 >= total * 6){
                count++;
                found = r * 4 + c;
            }
        }
    }
    if (count != 1){
        has_marker = false;
        return -1;
    }
    double cx = CX[found % 4];
    double cy = CY[found / 4];
    has_marker = CharacterSelectDetector::has_green_marker(
        screen, ImageFloatBox(cx - 0.020, cy - HH - 0.045, 0.040, 0.040)
    );
    return found;
}

}



void AutoStoryMultiLang::select_character(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const int target = (int)CHARACTER.current_value();
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
        bool has_marker = false;
        int current = highlighted_portrait(snapshot, has_marker);

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
            //  The green marker normally shows over the highlighted portrait.
            //  If it cannot be seen, accept a highlight that stays put.
            confirmations++;
            if (confirmations >= (has_marker ? 3 : 8)){
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



namespace{
const char* dialog_kind(const ImageViewRGB32& screen);
}

void AutoStoryMultiLang::run_trainer_battle(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& name
){
    AutoStoryMultiLang_Descriptor::Stats& stats = env.current_stats<AutoStoryMultiLang_Descriptor::Stats>();

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
    std::string last_pre_battle_kind;

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

        //  Before the battle: story dialogs can still be up (the battle may
        //  have been handed over early, e.g. when the battle-text detector
        //  fires on a story dialog). Advance them at dialog speed instead of
        //  holding ZL first. The battle text itself goes the usual way.
        if (!seen_battle){
            const char* kind = dialog_kind(env.console.video().snapshot());
            if (kind != nullptr && std::string(kind) != "TransparentBattleDialog"){
                if (last_pre_battle_kind != kind){
                    env.log(std::string("Pre-battle dialog (") + kind + "). Pressing A.");
                    last_pre_battle_kind = kind;
                }
                pbf_press_button(context, BUTTON_A, 100ms, 200ms);
                continue;
            }
            last_pre_battle_kind.clear();
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






void AutoStoryMultiLang::throw_ball(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Throwing a ball.", COLOR_BLUE);
    env.console.overlay().add_log("Throw Ball");
    //  Hold ZL for 1.5s to lock on. ZR is tapped while ZL is still held.
    ssf_press_button(context, BUTTON_ZL, 700ms, 1500ms, 0ms);
    pbf_press_button(context, BUTTON_ZR, 120ms, 1500ms);
    context.wait_for_all_requests();
}



void AutoStoryMultiLang::catch_wild_pokemon(
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
                env.console.logger(), ocr_language(),
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
        }else{
            env.log("Nothing to lock onto. " + debug);
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



std::pair<double, double> AutoStoryMultiLang::navigate_by_minimap(
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



void AutoStoryMultiLang::leave_wild_zone(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
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



void AutoStoryMultiLang::run_capture_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    AutoStoryMultiLang_Descriptor::Stats& stats = env.current_stats<AutoStoryMultiLang_Descriptor::Stats>();

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
//  Story dialog boxes whose shape varies (e.g. the jagged white "shout"
//  bubble) all have a dark "next" chevron (a hollow V) at the bottom right
//  of the white box.
bool white_box_chevron(const ImageViewRGB32& screen);

}

//  Overworld HUD: the party bar at the bottom left. With a party of one
//  there are no small slots, so also look for the bar's light-blue left
//  arrow and lead-slot panel.
bool overworld_hud(const ImageViewRGB32& screen){
    if (read_party_slot_count(screen) >= 1){
        return true;
    }
    auto fraction = [&](const ImageFloatBox& box, bool arrow){
        ImageViewRGB32 image = extract_box_reference(screen, box);
        size_t hits = 0;
        size_t total = 0;
        for (size_t y = 0; y < image.height(); y++){
            for (size_t x = 0; x < image.width(); x++){
                uint32_t pixel = image.pixel(x, y);
                int r = (pixel >> 16) & 0xff;
                int g = (pixel >>  8) & 0xff;
                int b = (pixel >>  0) & 0xff;
                total++;
                bool hit = arrow
                    ? (b >= 190 && g >= 150 && b >= r + 40)
                    : (b >= 190 && g >= 160 && r >= 80 && r <= 210 && b >= r + 25);
                hits += hit ? 1 : 0;
            }
        }
        return total == 0 ? 0.0 : (double)hits / (double)total;
    };
    double arrow = fraction(ImageFloatBox(0.018, 0.895, 0.016, 0.035), true);
    double lead = fraction(ImageFloatBox(0.040, 0.875, 0.038, 0.090), false);
    if (arrow >= 0.15 && lead >= 0.08){
        return true;
    }

    //  Only one Pokemon (e.g. right after the first battle): no small slots
    //  and no left arrow, but the lead panel has its own HP bar. The bar can
    //  be green, yellow or red and partly empty, and its exact height was
    //  measured on a scaled screenshot, so look for it in a band. Only its
    //  left end is checked (the bar starts at x = 0.043 and empties from the
    //  right), so even ~10% HP counts.
    if (lead < 0.08){
        return false;
    }
    for (int i = 0; i <= 12; i++){
        double y = 0.925 + 0.004 * i;
        ImageViewRGB32 bar = extract_box_reference(screen, ImageFloatBox(0.0435, y, 0.0045, 0.008));
        size_t colored = 0;
        size_t bar_total = 0;
        for (size_t py = 0; py < bar.height(); py++){
            for (size_t px = 0; px < bar.width(); px++){
                uint32_t pixel = bar.pixel(px, py);
                int r = (pixel >> 16) & 0xff;
                int g = (pixel >>  8) & 0xff;
                int b = (pixel >>  0) & 0xff;
                bar_total++;
                bool green  = g >= 150 && g >= r + 40 && g >= b + 40;
                bool yellow = r >= 200 && g >= 150 && b <= 90;
                bool red    = r >= 190 && g <= 100 && b <= 100;
                colored += (green || yellow || red) ? 1 : 0;
            }
        }
        if (bar_total != 0 && (double)colored / (double)bar_total >= 0.40){
            return true;
        }
    }
    return false;
}

namespace{

//  The movement tutorial that shows up once the player can walk after the
//  station story (ZA-01 -> ZA-02-0 only): a light blue "?" book icon in the
//  lower middle of the screen. Text is not read, so any language works.
bool movement_tutorial_icon(const ImageViewRGB32& screen){
    auto cyan = [&](double x, double y, double w, double h){
        ImageViewRGB32 image = extract_box_reference(screen, ImageFloatBox(x, y, w, h));
        size_t hits = 0;
        size_t total = 0;
        for (size_t py = 0; py < image.height(); py++){
            for (size_t px = 0; px < image.width(); px++){
                uint32_t pixel = image.pixel(px, py);
                int r = (pixel >> 16) & 0xff;
                int g = (pixel >>  8) & 0xff;
                int b = (pixel >>  0) & 0xff;
                total++;
                hits += (b >= 180 && g >= 140 && r >= 40 && r <= 190 && b - r >= 60 && b >= g) ? 1 : 0;
            }
        }
        return total == 0 ? 0.0 : (double)hits / (double)total;
    };
    //  The icon moves up or down with the number of tutorial lines, so look
    //  for it around the lower middle. It is a small light blue square with
    //  nothing light blue around it (a light blue Pokemon is much bigger).
    for (int i = 0; i <= 30; i++){
        double y = 0.74 + 0.004 * i;
        for (int j = 0; j <= 15; j++){
            double x = 0.475 + 0.003 * j;
            if (cyan(x, y, 0.010, 0.024) < 0.75){
                continue;
            }
            if (cyan(x - 0.017, y - 0.005, 0.010, 0.034) <= 0.03 &&
                cyan(x + 0.017, y - 0.005, 0.010, 0.034) <= 0.03 &&
                cyan(x - 0.003, y - 0.030, 0.016, 0.020) <= 0.03 &&
                cyan(x - 0.003, y + 0.040, 0.016, 0.020) <= 0.03
            ){
                return true;
            }
        }
    }
    return false;
}

//  The yellow hexagon "!" of a new quest at the top left (any language).
bool quest_icon(const ImageViewRGB32& screen){
    ImageViewRGB32 image = extract_box_reference(screen, ImageFloatBox(0.036, 0.066, 0.014, 0.027));
    size_t hits = 0;
    size_t total = 0;
    for (size_t y = 0; y < image.height(); y++){
        for (size_t x = 0; x < image.width(); x++){
            uint32_t pixel = image.pixel(x, y);
            int r = (pixel >> 16) & 0xff;
            int g = (pixel >>  8) & 0xff;
            int b = (pixel >>  0) & 0xff;
            total++;
            hits += (r >= 200 && g >= 150 && g <= 225 && b <= 140 && r - b >= 90) ? 1 : 0;
        }
    }
    return total != 0 && (double)hits / (double)total >= 0.30;
}

bool white_box_chevron(const ImageViewRGB32& screen){
    ImageFloatBox search(0.70, 0.86, 0.10, 0.10);
    ImageViewRGB32 area = extract_box_reference(screen, search);
    double width = (double)screen.width();

    PackedBinaryMatrix matrix = compress_rgb32_to_binary_range(
        area,
        0, 140,
        0, 140,
        0, 140
    );
    std::vector<WaterfillObject> objects = find_objects_inplace(matrix, 15);
    auto is_dark = [](uint32_t pixel){
        int r = (pixel >> 16) & 0xff;
        int g = (pixel >>  8) & 0xff;
        int b = (pixel >>  0) & 0xff;
        return r + g + b < 300;
    };
    for (const WaterfillObject& object : objects){
        size_t w = object.width();
        size_t h = object.height();
        if ((double)w < 0.010 * width || (double)w > 0.022 * width || h == 0){
            continue;
        }
        double aspect = (double)w / (double)h;
        if (aspect < 1.3 || aspect > 2.2){
            continue;
        }
        size_t count = 0;
        size_t top_hits = 0;
        size_t top_total = 0;
        size_t bottom_hits = 0;
        size_t bottom_total = 0;
        for (size_t y = object.min_y; y < object.max_y; y++){
            size_t row = y - object.min_y;
            for (size_t x = object.min_x; x < object.max_x; x++){
                size_t col = x - object.min_x;
                bool hit = is_dark(area.pixel(x, y));
                count += hit ? 1 : 0;
                if (row < std::max<size_t>(1, h / 4) && col >= w * 35 / 100 && col < w * 65 / 100){
                    top_total++;
                    top_hits += hit ? 1 : 0;
                }
                if (row >= h * 6 / 10 && col >= w * 4 / 10 && col < w * 6 / 10){
                    bottom_total++;
                    bottom_hits += hit ? 1 : 0;
                }
            }
        }
        double fill = (double)count / (double)(w * h);
        if (fill < 0.2 || fill > 0.5 || top_total == 0 || bottom_total == 0){
            continue;
        }
        if ((double)top_hits > 0.1 * (double)top_total || (double)bottom_hits < 0.7 * (double)bottom_total){
            continue;
        }
        //  On a white box.
        size_t x0 = object.min_x > w / 2 ? object.min_x - w / 2 : 0;
        size_t x1 = std::min(area.width(), object.max_x + w / 2);
        size_t y0 = object.min_y > h ? object.min_y - h : 0;
        size_t y1 = std::min(area.height(), object.max_y + h);
        double sum = 0;
        size_t background = 0;
        for (size_t y = y0; y < y1; y++){
            for (size_t x = x0; x < x1; x++){
                uint32_t pixel = area.pixel(x, y);
                if (is_dark(pixel)){
                    continue;
                }
                sum += (double)(((pixel >> 16) & 0xff) + ((pixel >> 8) & 0xff) + (pixel & 0xff));
                background++;
            }
        }
        if (background > 0 && sum / (double)background >= 550){
            return true;
        }
    }
    return false;
}

//  "MAIN MISSION" / "COMPLETE" yellow band across the middle of the screen.
bool mission_banner(const ImageViewRGB32& screen){
    const double ROWS[4] = {0.40, 0.44, 0.48, 0.52};
    size_t width = screen.width();
    size_t height = screen.height();
    size_t x0 = (size_t)(0.2 * width);
    size_t x1 = (size_t)(0.8 * width);
    for (double ry : ROWS){
        size_t y = (size_t)(ry * height);
        size_t yellow = 0;
        size_t total = 0;
        for (size_t x = x0; x < x1; x += 2){
            uint32_t pixel = screen.pixel(x, y);
            int r = (pixel >> 16) & 0xff;
            int g = (pixel >>  8) & 0xff;
            int b = (pixel >>  0) & 0xff;
            total++;
            if (r >= 200 && g >= 150 && b <= 140 && r - b >= 90){
                yellow++;
            }
        }
        if (total == 0 || yellow * 10 < total * 6){
            return false;
        }
    }
    return true;
}

//  A white dialog box at the bottom (with or without its "next" mark,
//  e.g. while the text is still being written or a choice is shown).
bool white_dialog_box(const ImageViewRGB32& screen){
    auto white_ratio = [&](const ImageFloatBox& box){
        ImageViewRGB32 image = extract_box_reference(screen, box);
        size_t white = 0;
        size_t total = 0;
        for (size_t y = 0; y < image.height(); y += 2){
            for (size_t x = 0; x < image.width(); x += 2){
                uint32_t pixel = image.pixel(x, y);
                int r = (pixel >> 16) & 0xff;
                int g = (pixel >>  8) & 0xff;
                int b = (pixel >>  0) & 0xff;
                total++;
                if (r >= 230 && g >= 230 && b >= 230){
                    white++;
                }
            }
        }
        return total == 0 ? 0.0 : (double)white / (double)total;
    };
    //  The box's left margin (never text) is solid white; long lines with
    //  furigana can cover much of the middle, so check the margin first.
    if (white_ratio(ImageFloatBox(0.235, 0.80, 0.030, 0.12)) >= 0.80){
        return true;
    }
    return white_ratio(ImageFloatBox(0.30, 0.80, 0.40, 0.10)) >= 0.55;
}

//  The company logo video (gold emblem on a dark blue vignette) in the
//  opening. It can be skipped by holding +.
bool logo_video(const ImageViewRGB32& screen){
    auto stats_of = [&](const ImageFloatBox& box){
        return image_stats(extract_box_reference(screen, box));
    };
    //  Dark corners.
    double corners =
        stats_of(ImageFloatBox(0.00, 0.00, 0.12, 0.12)).average.sum() +
        stats_of(ImageFloatBox(0.88, 0.00, 0.12, 0.12)).average.sum() +
        stats_of(ImageFloatBox(0.00, 0.88, 0.12, 0.12)).average.sum() +
        stats_of(ImageFloatBox(0.88, 0.88, 0.12, 0.12)).average.sum();
    if (corners / 4 > 80){
        return false;
    }
    //  Plain dark blue on both sides.
    for (double x : {0.06, 0.80}){
        ImageStats side = stats_of(ImageFloatBox(x, 0.40, 0.14, 0.20));
        if (side.average.sum() > 170 || side.average.b < side.average.r + 10 || side.stddev.sum() > 50){
            return false;
        }
    }
    //  Gold emblem in the middle.
    ImageViewRGB32 center = extract_box_reference(screen, ImageFloatBox(0.38, 0.22, 0.24, 0.40));
    size_t gold = 0;
    size_t total = 0;
    for (size_t y = 0; y < center.height(); y += 2){
        for (size_t x = 0; x < center.width(); x += 2){
            uint32_t pixel = center.pixel(x, y);
            int r = (pixel >> 16) & 0xff;
            int g = (pixel >>  8) & 0xff;
            int b = (pixel >>  0) & 0xff;
            total++;
            if (r >= 170 && g >= 130 && b <= 140 && r - b >= 70){
                gold++;
            }
        }
    }
    double ratio = total == 0 ? 0 : (double)gold / (double)total;
    return ratio >= 0.03 && ratio <= 0.30;
}

//  A full-screen menu (ZA Royale, MAP, POKEDEX, main menu ...) shows a
//  white "B" button icon at the bottom right to close it. It is the last
//  button prompt of the row, and where it sits depends on how long the
//  label after it is (關閉 / 閉じる / 닫는다 / Close ...), so look along the
//  row from the right.
bool button_icon_at(const ImageViewRGB32& screen, double box_x);
bool b_close_screen(const ImageViewRGB32& screen){
    for (double box_x = 0.950; box_x >= 0.860; box_x -= 0.004){
        if (button_icon_at(screen, box_x)){
            return true;
        }
    }
    return false;
}
bool button_icon_at(const ImageViewRGB32& screen, double box_x){
    ImageViewRGB32 image = extract_box_reference(screen, ImageFloatBox(box_x, 0.945, 0.022, 0.035));
    size_t white = 0;
    size_t dark = 0;
    size_t total = 0;
    for (size_t y = 0; y < image.height(); y++){
        for (size_t x = 0; x < image.width(); x++){
            uint32_t pixel = image.pixel(x, y);
            int r = (pixel >> 16) & 0xff;
            int g = (pixel >>  8) & 0xff;
            int b = (pixel >>  0) & 0xff;
            total++;
            if (r >= 220 && g >= 220 && b >= 220){
                white++;
            }else if (r <= 90 && g <= 90 && b <= 90){
                dark++;
            }
        }
    }
    if (total == 0){
        return false;
    }
    double white_ratio = (double)white / (double)total;
    double dark_ratio = (double)dark / (double)total;
    return white_ratio >= 0.18 && white_ratio <= 0.60 && dark_ratio >= 0.03;
}

//  Which detector sees a dialog (for the log), or nullptr.
const char* dialog_kind(const ImageViewRGB32& screen){
    if (white_box_chevron(screen)){ return "white box chevron"; }
    if (mission_banner(screen)){ return "mission banner"; }
    if (white_dialog_box(screen)){ return "white dialog box"; }
    //  A choice (e.g. 是的 / 你说得对!): green pointer + white row.
    double pointer_y = 0;
    if (find_choice_menu_pointer(screen, pointer_y)){ return "choice pointer"; }
    FlatWhiteDialogDetector flat_white;
    if (flat_white.detect(screen)){ return "FlatWhiteDialog"; }
    BlueDialogDetector blue;
    if (blue.detect(screen)){ return "BlueDialog"; }
    TealDialogDetector teal;
    if (teal.detect(screen)){ return "TealDialog"; }
    LightBlueDialogDetector light_blue;
    if (light_blue.detect(screen)){ return "LightBlueDialog"; }
    TransparentBattleDialogDetector transparent;
    if (transparent.detect(screen)){ return "TransparentBattleDialog"; }
    ItemReceiveDetector item;
    if (item.detect(screen)){ return "ItemReceive"; }
    TutorialWindowDetector tutorial;
    if (tutorial.detect(screen)){ return "TutorialWindow"; }
    return nullptr;
}

bool dialog_on_screen(const ImageViewRGB32& screen){
    return dialog_kind(screen) != nullptr;
}

//  Only meaningful on Mable's Research screen (bright teal): Mable's dark
//  dialog box at the bottom, or the dark reward card in the middle, is up.
//  Catches them while the text is still being written, before the "next"
//  chevron shows.
bool research_box_up(const ImageViewRGB32& screen){
    double dialog = image_stats(extract_box_reference(screen, ImageFloatBox(0.30, 0.80, 0.40, 0.06))).average.sum();
    if (dialog < 260){
        return true;
    }
    double card_left  = image_stats(extract_box_reference(screen, ImageFloatBox(0.33, 0.42, 0.04, 0.12))).average.sum();
    double card_right = image_stats(extract_box_reference(screen, ImageFloatBox(0.62, 0.62, 0.04, 0.12))).average.sum();
    return card_left < 400 && card_right < 380;
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



void AutoStoryMultiLang::clear_dialogs(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& name, bool require_dialog,
    Milliseconds quiet, bool press_first, std::chrono::seconds max_wait_for_dialog
){
    env.log("Clearing dialogs: " + name);
    env.console.overlay().add_log("Dialog: " + name);

    //  Before the starter there is no party bar, so "quiet" only means no
    //  dialog box: wait a bit longer.
    //  (Only when waiting for a dialog: a plain "wait for things to settle"
    //  call keeps the quiet time it asked for.)
    const Milliseconds QUIET_NEEDED = (m_hud_expected || !require_dialog)
        ? quiet
        : std::max<Milliseconds>(quiet, 6000ms);

    //  Waiting for the logo video: gaps before it do not end the dialogs.
    //  (After it, the Pancham cutscene has no dialog box for a while, so the
    //  normal quiet time still applies.)
    //  The story is over as soon as the movement tutorial ("?" icon) shows
    //  up; if it is missed, the normal quiet time still ends it.
    //  After the logo, the Pancham cutscene has gaps longer than the quiet
    //  time, so only the tutorial icon ends it (or 45 s after the logo).
    const bool logo_needed = m_logo_video_expected && require_dialog;
    bool logo_seen = false;
    WallClock logo_time = current_time();

    //  Waiting for the new quest: once the dialogs are over and the yellow
    //  "!" shows up, the story part is done. (Otherwise the quiet time rule
    //  still ends it.)
    const bool quest_shortcut = m_quest_icon_expected && require_dialog;

    //  Right before a trainer battle: stop on the "... wants to battle!"
    //  text (battle text with no overworld HUD) without pressing A, and let
    //  run_trainer_battle() take over.
    const bool challenge_stop = m_battle_challenge_expected && require_dialog;

    WallClock start = current_time();
    WallClock last_activity = start;
    size_t presses = 0;
    bool activity_seen = false;     //  a dialog, or the HUD went away
    bool waited_long = false;
    WallClock last_blind_press = start - std::chrono::seconds(10);
    WallClock last_dialog = start;
    WallClock last_skip = start - std::chrono::seconds(30);
    //  Holding + twice without any dialog in between means this is not a
    //  video: the HUD is there, it just was not recognized. From then on,
    //  "not black and no cutscene bars" counts as the overworld.
    size_t skips_without_dialog = 0;
    static const char* const BLIND = "blind";
    const char* last_kind = nullptr;
    WallClock last_kind_log = start;
    bool hud_fallback = false;

    if (press_first){
        //  Talk to someone / advance the first line.
        pbf_press_button(context, BUTTON_A, 100ms, 300ms);
        last_activity = current_time();
    }

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

        if (logo_video(snapshot)){
            env.log("Logo video: holding + to skip it.");
            pbf_press_button(context, BUTTON_PLUS, 3000ms, 500ms);
            if (logo_needed && !logo_seen){
                logo_seen = true;
                logo_time = current_time();
            }
            activity_seen = true;
            last_activity = current_time();
            last_dialog = last_activity;
            continue;
        }

        //  Only after the logo video: the icon never shows before it.
        if (logo_seen && movement_tutorial_icon(snapshot)){
            env.log(
                "Movement tutorial shown: dialogs cleared (" + std::to_string(presses) + " A presses): " + name,
                COLOR_BLUE
            );
            m_logo_video_expected = false;
            m_movement_tutorial_seen = true;
            return;
        }

        const char* kind = dialog_kind(snapshot);
        //  The battle-text detector also fires on bright overworld ground
        //  (e.g. after the map tutorial). Battle text never shows with the
        //  overworld HUD, so ignore it then.
        if (kind != nullptr && m_hud_expected && std::string(kind) == "TransparentBattleDialog" && overworld_hud(snapshot)){
            kind = nullptr;
        }
        if (kind != nullptr && challenge_stop && std::string(kind) == "TransparentBattleDialog" && !overworld_hud(snapshot)){
            env.log(
                "Battle challenge shown: handing over to the battle (" + std::to_string(presses) + " A presses): " + name,
                COLOR_BLUE
            );
            return;
        }
        if (kind != nullptr){
            //  Log which detector fired (once per change, or every 5 s), so
            //  a false detection can be found in the log.
            if (last_kind != kind || current_time() - last_kind_log >= std::chrono::seconds(5)){
                env.log(std::string("A: dialog (") + kind + ")");
                last_kind = kind;
                last_kind_log = current_time();
            }
            skips_without_dialog = 0;
            pbf_press_button(context, BUTTON_A, 100ms, 200ms);
            presses++;
            activity_seen = true;
            last_activity = current_time();
            last_dialog = last_activity;
            continue;
        }

        //  A menu screen the story opened (ZA Royale, MAP, POKEDEX...):
        //  nothing to read on it any more, so close it with B.
        if (b_close_screen(snapshot)){
            env.log("Menu screen with a B (close) button. Pressing B.");
            pbf_press_button(context, BUTTON_B, 100ms, 700ms);
            activity_seen = true;
            last_activity = current_time();
            last_dialog = last_activity;
            continue;
        }

        CutsceneLetterboxDetector letterbox_check;
        bool hud = false;
        if (m_hud_expected && !hud_fallback){
            hud = overworld_hud(snapshot);
        }else{
            //  No party bar yet (before the starter): no dialog, not black
            //  and no cutscene bars counts as the overworld.
            hud = !is_black(snapshot, 80, 40) && !letterbox_check.detect(snapshot);
        }
        if (!hud){
            activity_seen = true;
        }else{
            skips_without_dialog = 0;
        }
        CutsceneLetterboxDetector letterbox;
        if (m_hud_expected && !hud && !is_black(snapshot, 80, 40) &&
            current_time() - last_dialog >= std::chrono::seconds(30) &&
            !letterbox.detect(snapshot)
        ){
            //  After the starter, the scenes without the HUD are cutscenes
            //  that cannot be skipped (videos are skipped by the + recorded in
            //  the macros), so + is not pressed here. If nothing changes for
            //  30 s, the HUD is there but was not recognized.
            env.log("No HUD and no dialog for 30 s: treating this screen as the overworld.", COLOR_RED);
            hud_fallback = true;
            last_activity = current_time();
            continue;
        }
        if (!m_hud_expected && !hud && !is_black(snapshot, 80, 40) &&
            current_time() - last_dialog >= 2500ms &&
            current_time() - last_skip >= 8000ms &&
            !letterbox.detect(snapshot)
        ){
            //  Before the starter: no HUD and no dialog for a while is a video.
            //  Hold + to skip it.
            if (m_hud_expected && skips_without_dialog >= 2){
                env.log("+ did not change anything: treating this screen as the overworld.", COLOR_RED);
                hud_fallback = true;
                last_activity = current_time();
                continue;
            }
            skips_without_dialog++;
            env.log("No HUD and no dialog: holding + to skip the video.");
            pbf_press_button(context, BUTTON_PLUS, 2500ms, 300ms);
            last_skip = current_time();
            last_activity = last_skip;
            activity_seen = true;
            continue;
        }
        if (!hud && !is_black(snapshot, 80, 40)){
            //  Story scene without the HUD. Some dialog boxes have no fixed
            //  shape (e.g. the jagged white "shout" bubble), so press A
            //  slowly while the HUD is gone. A does nothing in a cutscene.
            if (current_time() - last_blind_press >= 700ms){
                if (last_kind != BLIND || current_time() - last_kind_log >= std::chrono::seconds(5)){
                    env.log("A: blind (no HUD, no dialog)");
                    last_kind = BLIND;
                    last_kind_log = current_time();
                }
                pbf_press_button(context, BUTTON_A, 100ms, 200ms);
                presses++;
                last_blind_press = current_time();
            }
            last_activity = current_time();
        }else if (!hud || letterbox.detect(snapshot)){
            //  Fade, loading or a cutscene: not done yet.
            last_activity = current_time();
        }else if (quest_shortcut && presses > 0 && now - last_dialog >= 800ms && quest_icon(snapshot)){
            env.log(
                "New quest shown: dialogs cleared (" + std::to_string(presses) + " A presses): " + name,
                COLOR_BLUE
            );
            m_quest_icon_expected = false;
            return;
        }else if (logo_needed && !logo_seen && now - start < std::chrono::seconds(150)){
            //  The logo video has not played yet: the story is not over.
            //  (Give up after 150 s and fall back to the quiet time.)
            last_activity = now;
        }else if (logo_seen && now - logo_time < std::chrono::seconds(45)){
            //  Pancham cutscene: wait for the movement tutorial icon.
            last_activity = now;
        }else if (now - last_activity >= QUIET_NEEDED){
            if (activity_seen || !require_dialog || waited_long){
                env.log(
                    "Dialogs cleared (" + std::to_string(presses) + " A presses): " + name,
                    COLOR_BLUE
                );
                return;
            }
            if (now - start > max_wait_for_dialog){
                env.log("No dialog showed up. Moving on: " + name, COLOR_RED);
                waited_long = true;
            }
        }
        context.wait_for(100ms);
    }
}



AutoStoryMultiLang::WalkResult AutoStoryMultiLang::walk_to_quest_marker(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& name, bool outdoors
){
    env.log("Walking to the quest marker: " + name);
    m_last_marker_distance = -1;
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
        bool hud = true;
    };
    auto read_state = [&](){
        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();
        State state;
        state.dialog = dialog_on_screen(snapshot);
        state.prompt = a_prompt_on_screen(snapshot);
        state.hud = overworld_hud(snapshot);
        if (outdoors){
            state.minimap = find_minimap_quest_marker(snapshot, state.mdx, state.mdy)
                && read_camera_heading(snapshot, state.heading);
        }
        state.world = find_world_quest_marker(snapshot, state.world_box);
        //  The distance (OCR, slow) is only read once the marker is near
        //  the middle: off to the side it is often cut or misread.
        if (outdoors && state.world &&
            std::abs(state.world_box.x + state.world_box.width / 2 - 0.5) <= 0.25
        ){
            state.distance = read_quest_marker_distance(env.logger(), snapshot, state.world_box);
            if (state.distance >= 0){
                m_last_marker_distance = state.distance;
            }
        }
        return state;
    };

    int lost = 0;
    int stuck = 0;
    int best_distance = -1;
    int no_progress = 0;
    int detours = 0;
    int last_turn_side = 0;
    double turn_scale = 1.0;
    int no_hud = 0;
    double stick_x = 0;
    double stick_y = 1;

    for (int step = 0; step < 200; step++){
        State state = read_state();
        //  Only the HUD says whether the player can move: while it is up,
        //  keep walking to the marker (a "dialog" read with the HUD up is a
        //  false hit: signs, shop fronts...). The story / a dialog hides it.
        if (!state.hud){
            //  The HUD goes away when a story scene starts (and some dialog
            //  boxes have no fixed shape). It also flickers for a moment
            //  (area name, new mission banner, party bar opening), so it has
            //  to stay away for about 1.5 seconds.
            if (++no_hud >= 5){
                env.log("HUD gone on the way to " + name + ". Story scene.");
                return WalkResult::Dialog;
            }
            context.wait_for(300ms);
            continue;
        }
        no_hud = 0;

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
            if (std::abs(offset) > 0.25){
                //  Far to the side: turn the camera toward it, in short
                //  pushes. Turned past it (the side flipped): push half as
                //  long, so it settles instead of swinging out of view.
                const int side = offset > 0 ? 1 : -1;
                if (last_turn_side != 0 && side != last_turn_side){
                    turn_scale = std::max(0.25, turn_scale * 0.5);
                }
                last_turn_side = side;
                double push_ms = std::max(30.0, std::min(std::abs(offset) * 200.0, 120.0) * turn_scale);
                env.log(
                    name + ": turning the camera, marker at x = " + std::to_string(cx) +
                    ", push " + std::to_string((int)push_ms) + " ms"
                );
                pbf_move_right_joystick(
                    context, {(double)side, 0},
                    Milliseconds((int64_t)push_ms), 300ms
                );
                continue;
            }
            last_turn_side = 0;
            turn_scale = 1.0;
            //  Near the middle: no camera turn, just walk / run at a slant
            //  toward it (the marker a bit up-left: go up-left).
            double slant = std::abs(offset) <= 0.04 ? 0.0 : std::max(-0.6, std::min(offset * 2.5, 0.6));
            double forward = std::sqrt(1.0 - slant * slant);

            //  Far from the marker outdoors: run (B once while moving
            //  forward). Close to it, walk so the prompt is not overshot.
            const bool run = outdoors && state.distance >= 12;
            double walk_ms = 400;
            if (state.distance > 0){
                walk_ms = run
                    ? std::max(400.0, std::min(state.distance * 45.0, 1500.0))
                    : std::max(400.0, std::min(state.distance * 80.0, 1500.0));
            }
            env.log(
                name + ": marker at x = " + std::to_string(cx) +
                ", distance " + (state.distance >= 0 ? std::to_string(state.distance) + " m" : std::string("?")) +
                (run ? ", run " : ", walk ") + std::to_string((int)walk_ms) + " ms" +
                (slant != 0 ? ", slant " + std::to_string(slant) : std::string())
            );
            if (run){
                pbf_controller_state(context, BUTTON_NONE, DPAD_NONE, {slant, forward}, {0, 0}, 150ms);
                pbf_controller_state(context, BUTTON_B, DPAD_NONE, {slant, forward}, {0, 0}, 80ms);
                pbf_controller_state(context, BUTTON_NONE, DPAD_NONE, {slant, forward}, {0, 0}, Milliseconds((int64_t)walk_ms));
                pbf_controller_state(context, BUTTON_NONE, DPAD_NONE, {0, 0}, {0, 0}, 200ms);
            }else{
                pbf_move_left_joystick(context, {slant, forward}, Milliseconds((int64_t)walk_ms), 200ms);
            }
            stick_x = slant;
            stick_y = forward;

            //  Progress: with a distance, it has to drop below the best so far
            //  (it reads 12 / 13 back and forth while pushing into a fence:
            //  that is not progress). Without one (indoors), the marker has
            //  to move.
            State after = read_state();
            if (after.world && after.hud){
                bool progressed;
                if (after.distance > 0){
                    progressed = best_distance < 0 || after.distance < best_distance;
                    if (progressed){
                        best_distance = after.distance;
                    }
                }else{
                    double after_cx = after.world_box.x + after.world_box.width / 2;
                    double after_cy = after.world_box.y + after.world_box.height / 2;
                    double before_cy = state.world_box.y + state.world_box.height / 2;
                    double size_change = std::abs(after.world_box.width - state.world_box.width) / state.world_box.width;
                    progressed = std::abs(after_cx - cx) >= 0.004 || std::abs(after_cy - before_cy) >= 0.004 || size_change >= 0.03;
                }
                no_progress = progressed ? 0 : no_progress + 1;
                if (progressed){
                    detours = 0;
                }
            }
            if (no_progress >= 3){
                //  Blocked (a fence, a wall): slide along it diagonally,
                //  right first, then left, a little longer each time.
                detours++;
                if (detours > 6){
                    OperationFailedExceptionWithScreenshot::fire(
                        ErrorReportMode::NO_ERROR_REPORT,
                        "walk_to_quest_marker(): Stuck on the way to " + name + ".",
                        env.console
                    );
                }
                const bool right = detours % 2 == 1;
                const int64_t detour_ms = 1000 * detours;
                env.log(
                    "Blocked (" + std::to_string(no_progress) + " tries without getting closer). Going around to the " +
                    (right ? "right" : "left") + " for " + std::to_string(detour_ms) + " ms (" + std::to_string(detours) + ").",
                    COLOR_RED
                );
                pbf_move_left_joystick(context, {right ? 0.75 : -0.75, 0.65}, Milliseconds(detour_ms), 200ms);
                no_progress = 0;
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



void AutoStoryMultiLang::walk_to_lab(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Walking to the Pokemon lab.");

    //  Put the camera behind the player.
    pbf_press_button(context, BUTTON_L, 160ms, 500ms);

    for (int attempt = 0; attempt < 20; attempt++){
        WalkResult result = walk_to_quest_marker(env, context, "Lab", true);
        if (result == WalkResult::Dialog){
            //  Getting close to the lab starts a story scene.
            { const std::string dialog_name = "ZA-13-1 Lab story"; clear_dialogs(env, context, dialog_name, false, dialog_quiet(env, dialog_name, 4000ms)); }
            pbf_press_button(context, BUTTON_L, 160ms, 500ms);
            continue;
        }

        //  "A 進入" at the door.
        env.log("Entering the lab.");
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);

        //  Going through the door fades to black. Once that happens we are
        //  inside (the minimap is gone indoors). A scene may play right after.
        WallClock start = current_time();
        bool faded = false;
        bool dialog = false;
        while (current_time() - start < std::chrono::seconds(8)){
            context.wait_for_all_requests();
            VideoSnapshot snapshot = env.console.video().snapshot();
            if (is_black(snapshot, 80, 40)){
                faded = true;
                break;
            }
            if (dialog_on_screen(snapshot)){
                dialog = true;
                break;
            }
            context.wait_for(200ms);
        }
        if (faded){
            env.log("Went through the lab door.", COLOR_BLUE);
            { const std::string dialog_name = "ZA-13-1 Lab entrance"; clear_dialogs(env, context, dialog_name, false, dialog_quiet(env, dialog_name, 4000ms)); }
            env.log("Inside the lab.", COLOR_BLUE);
            env.console.overlay().add_log("Inside the lab");
            return;
        }
        if (dialog){
            { const std::string dialog_name = "ZA-13-1 Lab door"; clear_dialogs(env, context, dialog_name, false, dialog_quiet(env, dialog_name, 4000ms)); }
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



void AutoStoryMultiLang::take_lab_elevator(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Taking the lab elevator to 3F.");

    for (int attempt = 0; attempt < 10; attempt++){
        WalkResult result = walk_to_quest_marker(env, context, "Elevator", false);
        if (result == WalkResult::Dialog){
            { const std::string dialog_name = "ZA-13-2 Lab 1F"; clear_dialogs(env, context, dialog_name, false, dialog_quiet(env, dialog_name, 4000ms)); }
            continue;
        }

        //  "A 搭電梯". A "which floor?" line comes up with the floor menu.
        pbf_press_button(context, BUTTON_A, 160ms, 340ms);

        //  Floor menu: the pointer starts on the 1st row. Move it down one
        //  row (2nd row = 3F) and confirm. Judged by the green pointer and the
        //  white highlight only, not the text.
        WallClock start = current_time();
        bool picked = false;
        bool menu_seen = false;
        double first_y = -1;
        int downs = 0;
        while (current_time() - start < std::chrono::seconds(12)){
            context.wait_for_all_requests();
            VideoSnapshot snapshot = env.console.video().snapshot();
            double y = 0;
            if (!find_choice_menu_pointer(snapshot, y)){
                if (!menu_seen && dialog_on_screen(snapshot)){
                    //  The question text before the menu shows up.
                    pbf_press_button(context, BUTTON_A, 160ms, 340ms);
                    continue;
                }
                context.wait_for(200ms);
                continue;
            }
            if (!menu_seen){
                menu_seen = true;
                first_y = y;
                env.log("Elevator menu: pointer at y = " + std::to_string(y) + ". Moving down one row.");
                pbf_press_dpad(context, DPAD_DOWN, 160ms, 500ms);
                downs++;
                continue;
            }
            double moved = y - first_y;
            env.log("Elevator menu: pointer at y = " + std::to_string(y) + " (moved " + std::to_string(moved) + ").");
            if (moved > 0.03 && moved < 0.12){
                pbf_press_button(context, BUTTON_A, 160ms, 840ms);
                picked = true;
                break;
            }
            if (moved <= 0.03 && downs < 3){
                //  The press was missed.
                pbf_press_dpad(context, DPAD_DOWN, 160ms, 500ms);
                downs++;
                continue;
            }
            if (moved >= 0.12){
                pbf_press_dpad(context, DPAD_UP, 160ms, 500ms);
                continue;
            }
            context.wait_for(200ms);
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
        { const std::string dialog_name = "ZA-13-2 Lab 1F"; clear_dialogs(env, context, dialog_name, false, dialog_quiet(env, dialog_name, 4000ms)); }
    }

    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "take_lab_elevator(): Unable to use the elevator.",
        env.console
    );
}



void AutoStoryMultiLang::walk_to_mable(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Walking to Mable on 3F.");

    //  There is a desk right in front of the elevator. Go around it to the
    //  left first.
    pbf_move_left_joystick(context, {-0.7, 0.7}, 1200ms, 300ms);

    WalkResult result = walk_to_quest_marker(env, context, "Mable", false);
    if (result == WalkResult::Prompt){
        //  Next to her before the scene started: talk.
        pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    }
    { const std::string dialog_name = "ZA-13-3 Mable"; clear_dialogs(env, context, dialog_name, true, dialog_quiet(env, dialog_name, 4000ms)); }
}



void AutoStoryMultiLang::open_research_list(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Opening Mable's Research (茉蜜姬調查).");
    env.console.overlay().add_log("Menu: Mable's Research");

    pbf_press_button(context, BUTTON_X, 160ms, 1840ms);    //  main menu
    pbf_press_button(context, BUTTON_X, 160ms, 840ms);     //  Mable's Research

    //  1. Wait for the research screen (bright teal).
    {
        WallClock start = current_time();
        bool seen = false;
        while (current_time() - start < std::chrono::seconds(10)){
            context.wait_for_all_requests();
            if (is_research_screen(env.console.video().snapshot())){
                seen = true;
                break;
            }
            context.wait_for(200ms);
        }
        if (!seen){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "open_research_list(): Mable's Research did not open.",
                env.console
            );
        }
        env.log("On Mable's Research.");
    }

    //  2. On the research screen, Mable talks and the reward card pops up.
    //     Press A while anything there wants it (green "next" chevron or a
    //     dialog box). The next line takes a moment to appear, so only press
    //     B once nothing has needed A for a while.
    const Milliseconds QUIET_BEFORE_B = 3000ms;
    size_t a_presses = 0;
    size_t b_presses = 0;
    {
        WallClock start = current_time();
        WallClock last_activity = current_time();
        int away = 0;
        while (true){
            if (current_time() - start > std::chrono::seconds(180)){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::NO_ERROR_REPORT,
                    "open_research_list(): Stuck on Mable's Research.",
                    env.console
                );
            }
            context.wait_for_all_requests();
            VideoSnapshot snapshot = env.console.video().snapshot();
            if (!is_research_screen(snapshot)){
                if (++away >= 3){
                    break;  //  Backed out to the main menu.
                }
                context.wait_for(200ms);
                continue;
            }
            away = 0;
            //  Press A as soon as a box is up (also skips the text animation),
            //  not only once its "next" mark shows.
            if (find_green_next_chevron(snapshot) || dialog_on_screen(snapshot) || research_box_up(snapshot)){
                pbf_press_button(context, BUTTON_A, 100ms, 300ms);
                a_presses++;
                last_activity = current_time();
                continue;
            }
            if (current_time() - last_activity >= QUIET_BEFORE_B){
                env.log("Nothing to press A on. Pressing B.");
                pbf_press_button(context, BUTTON_B, 160ms, 840ms);
                b_presses++;
                last_activity = current_time();
                continue;
            }
            context.wait_for(100ms);
        }
        env.log(
            "Left Mable's Research (" + std::to_string(a_presses) + " A, " +
            std::to_string(b_presses) + " B presses).",
            COLOR_BLUE
        );
    }

    //  3. Main menu: B to close it. Closing it starts the story; stop there.
    for (int c = 0; c < 4; c++){
        pbf_press_button(context, BUTTON_B, 160ms, 1340ms);
        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();
        CutsceneLetterboxDetector letterbox;
        if (overworld_hud(snapshot) ||
            letterbox.detect(snapshot) ||
            dialog_on_screen(snapshot) ||
            is_black(snapshot, 80, 40)
        ){
            env.log("Menu closed.", COLOR_BLUE);
            env.console.overlay().add_log("Menu closed");
            return;
        }
    }
    env.log("Unable to confirm the menu closed.", COLOR_RED);
}



namespace{

//  LANGUAGE screen (new save): 2 columns x 5 rows of tiles. The selected one
//  is white, the others dark. Returns the selected tile (column-major: left
//  column 0-4, right column 5-9), or -1 if this is not the LANGUAGE screen.
int read_language_screen(const ImageViewRGB32& screen){
    const double COL_X[2] = {0.3309, 0.6666};
    const double ROW_Y[5] = {0.3549, 0.4544, 0.5530, 0.6536, 0.7532};
    int white = -1;
    int dark = 0;
    for (int c = 0; c < 2; c++){
        for (int r = 0; r < 5; r++){
            ImageFloatBox box(COL_X[c] - 0.12, ROW_Y[r] - 0.02, 0.24, 0.04);
            double sum = image_stats(extract_box_reference(screen, box)).average.sum();
            if (sum > 500){
                if (white >= 0){
                    return -1;
                }
                white = c * 5 + r;
            }else if (sum < 350){
                dark++;
            }
        }
    }
    return (white >= 0 && dark >= 8) ? white : -1;
}


//  System keyboard. The selected key / candidate has a colorful glowing
//  frame (blue / purple / pink). Returns the frame center (0..1), or false.
//  Cyan text (the selected tab, e.g. 注音 / ABC / ア) is not the frame.
bool find_keyboard_cursor(const ImageViewRGB32& screen, double& x, double& y){
    const size_t STEP = 2;
    size_t width = screen.width();
    size_t height = screen.height();
    size_t x0 = (size_t)(0.02 * width);
    size_t x1 = (size_t)(0.98 * width);
    size_t y0 = (size_t)(0.40 * height);
    size_t y1 = (size_t)(0.90 * height);
    size_t gw = (x1 - x0) / STEP;
    size_t gh = (y1 - y0) / STEP;
    if (gw == 0 || gh == 0){
        return false;
    }
    std::vector<uint8_t> mask(gw * gh, 0);
    for (size_t gy = 0; gy < gh; gy++){
        for (size_t gx = 0; gx < gw; gx++){
            uint32_t pixel = screen.pixel(x0 + gx * STEP, y0 + gy * STEP);
            int r = (pixel >> 16) & 0xff;
            int g = (pixel >>  8) & 0xff;
            int b = (pixel >>  0) & 0xff;
            int mx = std::max(r, std::max(g, b));
            int mn = std::min(r, std::min(g, b));
            if (mx >= 170 && mx - mn >= 70){
                mask[gy * gw + gx] = 1;
            }
        }
    }

    //  Connected pieces (8-neighbors).
    size_t min_x = SIZE_MAX;
    size_t max_x = 0;
    size_t min_y = SIZE_MAX;
    size_t max_y = 0;
    bool found = false;
    std::vector<size_t> stack;
    for (size_t start = 0; start < mask.size(); start++){
        if (mask[start] != 1){
            continue;
        }
        size_t count = 0;
        size_t bx0 = SIZE_MAX, bx1 = 0, by0 = SIZE_MAX, by1 = 0;
        double sum_r = 0;
        double sum_g = 0;
        stack.clear();
        stack.push_back(start);
        mask[start] = 2;
        while (!stack.empty()){
            size_t index = stack.back();
            stack.pop_back();
            size_t gx = index % gw;
            size_t gy = index / gw;
            count++;
            bx0 = std::min(bx0, gx);
            bx1 = std::max(bx1, gx);
            by0 = std::min(by0, gy);
            by1 = std::max(by1, gy);
            uint32_t pixel = screen.pixel(x0 + gx * STEP, y0 + gy * STEP);
            sum_r += (pixel >> 16) & 0xff;
            sum_g += (pixel >>  8) & 0xff;
            for (int dy = -1; dy <= 1; dy++){
                for (int dx = -1; dx <= 1; dx++){
                    if (dx == 0 && dy == 0){
                        continue;
                    }
                    if ((dx < 0 && gx == 0) || (dy < 0 && gy == 0)){
                        continue;
                    }
                    size_t nx = gx + dx;
                    size_t ny = gy + dy;
                    if (nx >= gw || ny >= gh){
                        continue;
                    }
                    size_t n = ny * gw + nx;
                    if (mask[n] == 1){
                        mask[n] = 2;
                        stack.push_back(n);
                    }
                }
            }
        }
        double w = (double)(bx1 - bx0 + 1) * STEP / (double)width;
        double h = (double)(by1 - by0 + 1) * STEP / (double)width;
        double avg_r = sum_r / (double)count;
        double avg_g = sum_g / (double)count;
        if (count * STEP * STEP < 40){
            continue;
        }
        if (std::max(w, h) < 0.020 || std::min(w, h) < 0.002){
            continue;
        }
        if (avg_g >= 185 && avg_r < 115){
            continue;   //  cyan tab label / underline
        }
        found = true;
        min_x = std::min(min_x, bx0);
        max_x = std::max(max_x, bx1);
        min_y = std::min(min_y, by0);
        max_y = std::max(max_y, by1);
    }
    if (!found){
        return false;
    }
    x = (x0 + (min_x + max_x) / 2.0 * STEP) / (double)width;
    y = (y0 + (min_y + max_y) / 2.0 * STEP) / (double)height;
    return true;
}

//  Candidate bar above the keys (Chinese keyboards).
const double CAND_Y = 0.4676;
const double CAND_X0 = 0.1312;
const double CAND_PITCH = 0.04055;
const int CAND_COUNT = 19;
const double CAND_BAR_LIMIT_Y = 0.51;   //  cursor above this = candidate bar

struct KeyboardKey{
    std::string label;
    double x;
    double y;
};
using KeyboardLayout = std::vector<KeyboardKey>;

void add_grid(
    KeyboardLayout& layout,
    const std::vector<std::vector<const char*>>& rows,
    double x0, double pitch_x, double y0, double pitch_y
){
    for (size_t r = 0; r < rows.size(); r++){
        for (size_t c = 0; c < rows[r].size(); c++){
            if (rows[r][c] == nullptr || rows[r][c][0] == 0){
                continue;
            }
            layout.push_back({rows[r][c], x0 + pitch_x * c, y0 + pitch_y * r});
        }
    }
}

//  注音 (繁體中文).
KeyboardLayout make_zhuyin_layout(){
    KeyboardLayout layout;
    add_grid(layout, {
        {"ㄅ", "ㄉ", "ˇ", "ˋ", "ㄓ", "ˊ", "˙", "ㄚ", "ㄞ", "ㄢ", "ㄦ"},
        {"ㄆ", "ㄊ", "ㄍ", "ㄐ", "ㄔ", "ㄗ", "ㄧ", "ㄛ", "ㄟ", "ㄣ", "，"},
        {"ㄇ", "ㄋ", "ㄎ", "ㄑ", "ㄕ", "ㄘ", "ㄨ", "ㄜ", "ㄠ", "ㄤ", "。"},
        {"ㄈ", "ㄌ", "ㄏ", "ㄒ", "ㄖ", "ㄙ", "ㄩ", "ㄝ", "ㄡ", "ㄥ", "？"},
    }, 0.1026, 0.0697, 0.5600, 0.0701);
    return layout;
}
//  Letters (拼音 / English / French / German / Italian / Spanish).
KeyboardLayout make_latin_layout(){
    KeyboardLayout layout;
    add_grid(layout, {
        {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-"},
        {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "/"},
        {"a", "s", "d", "f", "g", "h", "j", "k", "l", ":", "'"},
        {"z", "x", "c", "v", "b", "n", "m", ",", ".", "?", "!"},
    }, 0.1021, 0.0698, 0.5583, 0.0707);
    return layout;
}
//  한글.
KeyboardLayout make_korean_layout(){
    KeyboardLayout layout;
    add_grid(layout, {
        {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-"},
        {"ㅂ", "ㅈ", "ㄷ", "ㄱ", "ㅅ", "ㅛ", "ㅕ", "ㅑ", "ㅐ", "ㅔ", "/"},
        {"ㅁ", "ㄴ", "ㅇ", "ㄹ", "ㅎ", "ㅗ", "ㅓ", "ㅏ", "ㅣ", ":", "'"},
        {"ㅋ", "ㅌ", "ㅊ", "ㅍ", "ㅠ", "ㅜ", "ㅡ", ",", ".", "?", "!"},
    }, 0.1021, 0.0698, 0.5583, 0.0707);
    return layout;
}
//  かな (日本語). Columns right to left like the 50-sound table. The same
//  positions hold hiragana in あ mode and katakana in ア mode; the layout
//  uses katakana.
KeyboardLayout make_japanese_layout(){
    KeyboardLayout layout;
    add_grid(layout, {
        {"「", "ワ", "ラ", "ヤ", "マ", "ハ", "ナ", "タ", "サ", "カ", "ア"},
        {"」", "ヲ", "リ", "",   "ミ", "ヒ", "ニ", "チ", "シ", "キ", "イ"},
        {"ー", "ン", "ル", "ユ", "ム", "フ", "ヌ", "ツ", "ス", "ク", "ウ"},
        {"？", "、", "レ", "",   "メ", "ヘ", "ネ", "テ", "セ", "ケ", "エ"},
        {"！", "。", "ロ", "ヨ", "モ", "ホ", "ノ", "ト", "ソ", "コ", "オ"},
    }, 0.1979, 0.0603, 0.5593, 0.0722);
    layout.push_back({"MODE_HIRAGANA", 0.0917, 0.5593});
    layout.push_back({"MODE_KATAKANA", 0.1380, 0.5593});
    layout.push_back({"MODE_SMALL", 0.1146, 0.7759});
    return layout;
}

const KeyboardKey* find_key(const KeyboardLayout& layout, const std::string& label){
    for (const KeyboardKey& key : layout){
        if (key.label == label){
            return &key;
        }
    }
    return nullptr;
}
const KeyboardKey* nearest_key(const KeyboardLayout& layout, double x, double y){
    const KeyboardKey* best = nullptr;
    double best_distance = 1e9;
    for (const KeyboardKey& key : layout){
        double dx = key.x - x;
        double dy = (key.y - y) * 0.5625;
        double distance = dx * dx + dy * dy;
        if (distance < best_distance){
            best_distance = distance;
            best = &key;
        }
    }
    return best;
}

//  Split a UTF-8 string into code points.
std::vector<std::string> utf8_chars(const std::string& text){
    std::vector<std::string> ret;
    size_t i = 0;
    while (i < text.size()){
        uint8_t ch = (uint8_t)text[i];
        size_t len = 1;
        if (ch >= 0xf0){
            len = 4;
        }else if (ch >= 0xe0){
            len = 3;
        }else if (ch >= 0xc0){
            len = 2;
        }
        ret.emplace_back(text.substr(i, len));
        i += len;
    }
    return ret;
}
uint32_t utf8_code_point(const std::string& ch){
    if (ch.empty()){
        return 0;
    }
    uint8_t c0 = (uint8_t)ch[0];
    if (c0 < 0x80 || ch.size() == 1){
        return c0;
    }
    if (ch.size() == 2){
        return ((c0 & 0x1f) << 6) | ((uint8_t)ch[1] & 0x3f);
    }
    if (ch.size() == 3){
        return ((c0 & 0x0f) << 12) | (((uint8_t)ch[1] & 0x3f) << 6) | ((uint8_t)ch[2] & 0x3f);
    }
    return ((c0 & 0x07) << 18) | (((uint8_t)ch[1] & 0x3f) << 12) | (((uint8_t)ch[2] & 0x3f) << 6) | ((uint8_t)ch[3] & 0x3f);
}
std::string utf8_encode(uint32_t cp){
    std::string ret;
    if (cp < 0x80){
        ret += (char)cp;
    }else if (cp < 0x800){
        ret += (char)(0xc0 | (cp >> 6));
        ret += (char)(0x80 | (cp & 0x3f));
    }else if (cp < 0x10000){
        ret += (char)(0xe0 | (cp >> 12));
        ret += (char)(0x80 | ((cp >> 6) & 0x3f));
        ret += (char)(0x80 | (cp & 0x3f));
    }else{
        ret += (char)(0xf0 | (cp >> 18));
        ret += (char)(0x80 | ((cp >> 12) & 0x3f));
        ret += (char)(0x80 | ((cp >> 6) & 0x3f));
        ret += (char)(0x80 | (cp & 0x3f));
    }
    return ret;
}

std::string trim_spaces(const std::string& text){
    std::string ret;
    for (const std::string& ch : utf8_chars(text)){
        if (ch == " " || ch == "\t" || ch == "\n" || ch == "\r" || ch == "　"){
            continue;
        }
        ret += ch;
    }
    return ret;
}

//  Chinese readings: character -> (注音 readings, pinyin readings).
struct ChineseReading{
    std::vector<std::string> zhuyin;
    std::vector<std::string> pinyin;
};
const std::map<std::string, ChineseReading>& chinese_readings(){
    static const std::map<std::string, ChineseReading> table = [](){
        std::map<std::string, ChineseReading> ret;
        auto split = [](const std::string& text, char delimiter){
            std::vector<std::string> parts;
            std::string current;
            for (char ch : text){
                if (ch == delimiter){
                    parts.push_back(current);
                    current.clear();
                }else{
                    current += ch;
                }
            }
            parts.push_back(current);
            return parts;
        };
        for (size_t i = 0; i < CHINESE_READINGS_COUNT; i++){
            std::vector<std::string> fields = split(CHINESE_READINGS[i], '|');
            if (fields.size() != 3){
                continue;
            }
            ChineseReading reading;
            for (std::string& item : split(fields[1], ',')){
                if (!item.empty()){
                    reading.zhuyin.push_back(std::move(item));
                }
            }
            for (std::string& item : split(fields[2], ',')){
                if (!item.empty()){
                    reading.pinyin.push_back(std::move(item));
                }
            }
            ret.emplace(fields[0], std::move(reading));
        }
        return ret;
    }();
    return table;
}

//  Hangul syllable -> key presses (compatibility jamo; "+" prefix = Shift).
bool hangul_keys(uint32_t cp, std::vector<std::string>& keys){
    static const char* L[19] = {"ㄱ","+ㄱ","ㄴ","ㄷ","+ㄷ","ㄹ","ㅁ","ㅂ","+ㅂ","ㅅ","+ㅅ","ㅇ","ㅈ","+ㅈ","ㅊ","ㅋ","ㅌ","ㅍ","ㅎ"};
    static const char* V[21] = {"ㅏ","ㅐ","ㅑ","+ㅐ","ㅓ","ㅔ","ㅕ","+ㅔ","ㅗ","ㅗ ㅏ","ㅗ ㅐ","ㅗ ㅣ","ㅛ","ㅜ","ㅜ ㅓ","ㅜ ㅔ","ㅜ ㅣ","ㅠ","ㅡ","ㅡ ㅣ","ㅣ"};
    static const char* T[28] = {"","ㄱ","+ㄱ","ㄱ ㅅ","ㄴ","ㄴ ㅈ","ㄴ ㅎ","ㄷ","ㄹ","ㄹ ㄱ","ㄹ ㅁ","ㄹ ㅂ","ㄹ ㅅ","ㄹ ㅌ","ㄹ ㅍ","ㄹ ㅎ","ㅁ","ㅂ","ㅂ ㅅ","ㅅ","+ㅅ","ㅇ","ㅈ","ㅊ","ㅋ","ㅌ","ㅍ","ㅎ"};
    auto push = [&](const char* text){
        std::string current;
        for (const char* p = text; ; p++){
            if (*p == ' ' || *p == 0){
                if (!current.empty()){
                    keys.push_back(current);
                }
                current.clear();
                if (*p == 0){
                    break;
                }
            }else{
                current += *p;
            }
        }
    };
    if (cp >= 0xac00 && cp <= 0xd7a3){
        uint32_t s = cp - 0xac00;
        push(L[s / 588]);
        push(V[(s % 588) / 28]);
        push(T[s % 28]);
        return true;
    }
    //  A lone jamo.
    if (cp >= 0x3131 && cp <= 0x3163){
        static const std::map<uint32_t, const char*> SHIFTED = {
            {0x3132, "+ㄱ"}, {0x3138, "+ㄷ"}, {0x3143, "+ㅂ"}, {0x3146, "+ㅅ"}, {0x3149, "+ㅈ"},
            {0x3152, "+ㅐ"}, {0x3156, "+ㅔ"},
        };
        auto iter = SHIFTED.find(cp);
        keys.push_back(iter != SHIFTED.end() ? iter->second : utf8_encode(cp));
        return true;
    }
    return false;
}

//  Kana -> (katakana key, hiragana?, presses of 小字゛゜ after it).
//  小字゛゜ cycles: small -> voiced -> semi-voiced (only the ones that exist).
bool kana_keys(uint32_t cp, std::string& key, bool& hiragana, int& modifier){
    hiragana = false;
    if (cp >= 0x3041 && cp <= 0x3096){
        hiragana = true;
        cp += 0x60;     //  to katakana
    }
    if (cp == 0x30fc){
        key = "ー";
        modifier = 0;
        return true;
    }
    if (cp < 0x30a1 || cp > 0x30f6){
        return false;
    }
    struct Variant{ uint32_t base; int presses; };
    static const std::map<uint32_t, Variant> VARIANTS = [](){
        std::map<uint32_t, Variant> ret;
        //  Small vowels / small ya yu yo / small wa: 1 press.
        const uint32_t small[] = {0x30a1, 0x30a3, 0x30a5, 0x30a7, 0x30a9, 0x30e3, 0x30e5, 0x30e7, 0x30ee};
        for (uint32_t s : small){
            ret[s] = {s + 1, 1};
        }
        //  ッ: small (1), ヅ: voiced (2).
        ret[0x30c3] = {0x30c4, 1};
        ret[0x30c5] = {0x30c4, 2};
        //  ヴ: ゥ (1) then ヴ (2).
        ret[0x30f4] = {0x30a6, 2};
        //  カ..ド voiced: base + 1 (1 press). Skip ツ handled above.
        const uint32_t voiced_k[] = {
            0x30ac, 0x30ae, 0x30b0, 0x30b2, 0x30b4,     //  ガギグゲゴ
            0x30b6, 0x30b8, 0x30ba, 0x30bc, 0x30be,     //  ザジズゼゾ
            0x30c0, 0x30c2, 0x30c7, 0x30c9,             //  ダヂデド
        };
        for (uint32_t s : voiced_k){
            ret[s] = {s - 1, 1};
        }
        //  ハ row: バ (1), パ (2).
        const uint32_t ha[] = {0x30cf, 0x30d2, 0x30d5, 0x30d8, 0x30db};
        for (uint32_t h : ha){
            ret[h + 1] = {h, 1};
            ret[h + 2] = {h, 2};
        }
        return ret;
    }();
    auto iter = VARIANTS.find(cp);
    uint32_t base = cp;
    modifier = 0;
    if (iter != VARIANTS.end()){
        base = iter->second.base;
        modifier = iter->second.presses;
    }
    if (base == 0x30f2){
        key = "ヲ";
    }else{
        key = utf8_encode(base);
    }
    return true;
}

}



Language AutoStoryMultiLang::ocr_language() const{
    switch (GAME_LANGUAGE.get()){
    case GameLanguage::ChineseTraditional:  return Language::ChineseTraditional;
    case GameLanguage::English:             return Language::English;
    case GameLanguage::SpanishSpain:        return Language::Spanish;
    case GameLanguage::SpanishLatAm:        return Language::Spanish;
    case GameLanguage::French:              return Language::French;
    case GameLanguage::German:              return Language::German;
    case GameLanguage::Italian:             return Language::Italian;
    case GameLanguage::Japanese:            return Language::Japanese;
    case GameLanguage::Korean:              return Language::Korean;
    case GameLanguage::ChineseSimplified:   return Language::ChineseSimplified;
    }
    return Language::English;
}



void AutoStoryMultiLang::launch_and_pick_language(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Starting the game with user profile " + std::to_string((int)PROFILE) + ".");
    env.console.overlay().add_log("Start game");

    //  At Home: close the game and start it again with the chosen profile.
    close_game_from_home(env.console, context);
    start_game_from_home(
        env.console, context,
        ConsoleSettings::instance().TOLERATE_SYSTEM_UPDATE_MENU_FAST,
        0, PROFILE
    );

    //  New save: the LANGUAGE screen comes up after the logos.
    WallClock start = current_time();
    WallClock last_press = start;
    while (true){
        if (current_time() - start > std::chrono::minutes(3)){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "launch_and_pick_language(): The LANGUAGE screen did not show up.",
                env.console
            );
        }
        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();
        if (read_language_screen(snapshot) >= 0){
            break;
        }
        bool looks_marker = false;
        if (highlighted_portrait(snapshot, looks_marker) >= 0){
            env.log("Already on the LOOKS screen. No LANGUAGE screen (existing save?).", COLOR_RED);
            return;
        }
        //  After a while, press A in case a title screen is waiting.
        if (current_time() - start > std::chrono::seconds(40) &&
            current_time() - last_press > std::chrono::seconds(3)
        ){
            pbf_press_button(context, BUTTON_A, 100ms, 300ms);
            last_press = current_time();
        }
        context.wait_for(300ms);
    }

    pick_game_language(env, context);

    //  Then on to the LOOKS screen. Press A slowly through whatever is in
    //  between (checking for LOOKS before each press).
    start = current_time();
    last_press = start;
    while (true){
        if (current_time() - start > std::chrono::minutes(4)){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "launch_and_pick_language(): The LOOKS screen did not show up.",
                env.console
            );
        }
        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();
        bool looks_marker = false;
        if (read_language_screen(snapshot) < 0 && highlighted_portrait(snapshot, looks_marker) >= 0){
            env.log("On the LOOKS screen.", COLOR_BLUE);
            return;
        }
        if (current_time() - last_press > std::chrono::milliseconds(2000)){
            if (read_language_screen(snapshot) >= 0){
                //  Still on the LANGUAGE screen (A dropped?): pick again.
                pick_game_language(env, context);
                last_press = current_time();
                continue;
            }
            //  The LOOKS screen shows no highlighted portrait until the
            //  stick is touched. Nudge it and look again before pressing A.
            pbf_move_left_joystick(context, {+1, 0}, 120ms, 0ms);
            context.wait_for_all_requests();
            bool looks_seen = false;
            for (int c = 0; c < 4 && !looks_seen; c++){
                context.wait_for(250ms);
                bool check_marker = false;
                looks_seen = highlighted_portrait(env.console.video().snapshot(), check_marker) >= 0;
            }
            if (looks_seen){
                env.log("On the LOOKS screen.", COLOR_BLUE);
                return;
            }
            pbf_press_button(context, BUTTON_A, 100ms, 300ms);
            last_press = current_time();
        }
        context.wait_for(200ms);
    }
}

void AutoStoryMultiLang::pick_game_language(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const int target = (int)GAME_LANGUAGE.get();
    const int target_col = target / 5;
    const int target_row = target % 5;
    env.log("Picking game language #" + std::to_string(target) + " on the LANGUAGE screen.");
    env.console.overlay().add_log("Language");

    for (int step = 0; step < 30; step++){
        context.wait_for_all_requests();
        int current = -1;
        for (int c = 0; c < 5 && current < 0; c++){
            current = read_language_screen(env.console.video().snapshot());
            if (current < 0){
                context.wait_for(200ms);
            }
        }
        if (current < 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "pick_game_language(): Unable to read the LANGUAGE screen.",
                env.console
            );
        }
        int col = current / 5;
        int row = current % 5;
        if (current == target){
            env.log("Language selected. Confirming.", COLOR_BLUE);
            pbf_press_button(context, BUTTON_A, 100ms, 900ms);
            return;
        }
        if (col < target_col){
            pbf_press_dpad(context, DPAD_RIGHT, 100ms, 300ms);
        }else if (col > target_col){
            pbf_press_dpad(context, DPAD_LEFT, 100ms, 300ms);
        }else if (row < target_row){
            pbf_press_dpad(context, DPAD_DOWN, 100ms, 300ms);
        }else{
            pbf_press_dpad(context, DPAD_UP, 100ms, 300ms);
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "pick_game_language(): Unable to move to the language.",
        env.console
    );
}



void AutoStoryMultiLang::enter_character_name(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    enum class Keyboard{ Zhuyin, Pinyin, Japanese, Korean, Latin };
    Keyboard keyboard = Keyboard::Latin;
    Language candidate_language = Language::ChineseTraditional;
    switch (GAME_LANGUAGE.get()){
    case GameLanguage::ChineseTraditional:
        keyboard = Keyboard::Zhuyin;
        break;
    case GameLanguage::ChineseSimplified:
        keyboard = Keyboard::Pinyin;
        candidate_language = Language::ChineseSimplified;
        break;
    case GameLanguage::Japanese:
        keyboard = Keyboard::Japanese;
        break;
    case GameLanguage::Korean:
        keyboard = Keyboard::Korean;
        break;
    default:
        keyboard = Keyboard::Latin;
    }
    const KeyboardLayout layout =
        keyboard == Keyboard::Zhuyin   ? make_zhuyin_layout() :
        keyboard == Keyboard::Japanese ? make_japanese_layout() :
        keyboard == Keyboard::Korean   ? make_korean_layout() :
        make_latin_layout();

    auto fail = [&](const std::string& message){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            "enter_character_name(): " + message,
            env.console
        );
    };

    //  Parse the name. A "字:注音" item gives a reading by hand (Chinese).
    struct NameChar{
        std::string character;
        std::vector<std::string> manual_reading;    //  注音 / pinyin symbols
    };
    std::vector<NameChar> name;
    {
        std::vector<std::string> chars = utf8_chars(std::string(CHARACTER_NAME));
        for (size_t i = 0; i < chars.size(); i++){
            const std::string& ch = chars[i];
            if (ch == "\t" || ch == "\r" || ch == "\n"){
                continue;
            }
            if ((ch == ":" || ch == "：") && !name.empty() && keyboard != Keyboard::Latin){
                //  Reading by hand for the previous character, up to a space.
                i++;
                while (i < chars.size() && chars[i] != " " && chars[i] != "　" && chars[i] != "," && chars[i] != "，"){
                    name.back().manual_reading.push_back(chars[i]);
                    i++;
                }
                continue;
            }
            if ((ch == " " || ch == "　" || ch == "," || ch == "，") && keyboard != Keyboard::Latin){
                continue;
            }
            name.push_back({ch, {}});
        }
    }
    size_t max_length = keyboard == Keyboard::Latin ? 12 : 6;
    if (name.empty() || name.size() > max_length){
        throw UserSetupError(env.logger(), "Character Name: write 1 to " + std::to_string(max_length) + " characters.");
    }

    env.log("Entering the character name: " + std::string(CHARACTER_NAME));
    env.console.overlay().add_log("Name");

    //  Where is the cursor? Returns the key under it, or nullptr with
    //  bar_index set when it is on the candidate bar.
    auto read_cursor = [&](const KeyboardKey*& key, int& bar_index) -> bool{
        for (int c = 0; c < 5; c++){
            context.wait_for_all_requests();
            double x = 0;
            double y = 0;
            if (find_keyboard_cursor(env.console.video().snapshot(), x, y)){
                if (y < CAND_BAR_LIMIT_Y){
                    key = nullptr;
                    bar_index = (int)std::lround((x - CAND_X0) / CAND_PITCH);
                }else{
                    key = nearest_key(layout, x, y);
                    bar_index = -1;
                }
                return true;
            }
            context.wait_for(150ms);
        }
        return false;
    };

    auto move_to = [&](const KeyboardKey& target){
        //  Sideways first, then up/down: some columns have gaps (e.g. the
        //  ヤ column on the kana keyboard), so moving along a row and then
        //  down the target's column is the safe way. If a press does not
        //  move the cursor, try the other direction next.
        const KeyboardKey* last = nullptr;
        bool prefer_vertical = false;
        for (int step = 0; step < 60; step++){
            const KeyboardKey* key = nullptr;
            int bar_index = -1;
            if (!read_cursor(key, bar_index)){
                fail("Unable to find the keyboard cursor.");
            }
            if (key == nullptr){
                pbf_press_dpad(context, DPAD_DOWN, 80ms, 220ms);
                continue;
            }
            if (key->label == target.label){
                return;
            }
            double dx = target.x - key->x;
            double dy = target.y - key->y;
            bool need_x = std::abs(dx) > 0.02;
            bool need_y = std::abs(dy) > 0.035;
            if (key == last){
                //  Did not move.
                if (!need_x){
                    //  Blocked going straight up/down: step aside first.
                    pbf_press_dpad(context, DPAD_RIGHT, 80ms, 220ms);
                    last = nullptr;
                    continue;
                }
                prefer_vertical = !prefer_vertical;
            }
            last = key;
            bool vertical = need_y && (!need_x || prefer_vertical);
            if (vertical){
                pbf_press_dpad(context, dy > 0 ? DPAD_DOWN : DPAD_UP, 80ms, 220ms);
            }else{
                pbf_press_dpad(context, dx > 0 ? DPAD_RIGHT : DPAD_LEFT, 80ms, 220ms);
            }
        }
        fail("Unable to move the cursor to \"" + target.label + "\".");
    };
    auto press_key = [&](const std::string& label){
        const KeyboardKey* key = find_key(layout, label);
        if (key == nullptr){
            fail("No key \"" + label + "\" on this keyboard.");
        }
        move_to(*key);
        pbf_press_button(context, BUTTON_A, 80ms, 320ms);
    };

    //  Candidate list (Chinese keyboards).
    auto find_candidate = [&](const std::string& character) -> int{
        context.wait_for_all_requests();
        context.wait_for(300ms);
        VideoSnapshot snapshot = env.console.video().snapshot();
        std::string all;
        int found = -1;
        for (int i = 0; i < CAND_COUNT; i++){
            ImageFloatBox box(CAND_X0 + CAND_PITCH * i - CAND_PITCH / 2, CAND_Y - 0.028, CAND_PITCH, 0.056);
            std::string text = trim_spaces(OCR::ocr_read(candidate_language, extract_box_reference(snapshot, box)));
            all += text.empty() ? std::string("_") : text;
            all += " ";
            if (found < 0 && text.find(character) != std::string::npos){
                found = i;
            }
        }
        env.log("Candidates: " + all);
        return found;
    };
    auto pick_candidate = [&](int index){
        for (int step = 0; step < 40; step++){
            const KeyboardKey* key = nullptr;
            int bar_index = -1;
            if (!read_cursor(key, bar_index)){
                fail("Unable to find the cursor near the candidate list.");
            }
            if (key != nullptr){
                pbf_press_dpad(context, DPAD_UP, 80ms, 220ms);
                continue;
            }
            if (bar_index == index){
                pbf_press_button(context, BUTTON_A, 80ms, 420ms);
                return;
            }
            pbf_press_dpad(context, bar_index < index ? DPAD_RIGHT : DPAD_LEFT, 80ms, 220ms);
        }
        fail("Unable to move to the candidate.");
    };
    auto back_to_keys = [&](){
        const KeyboardKey* key = nullptr;
        int bar_index = -1;
        if (read_cursor(key, bar_index) && key == nullptr){
            pbf_press_dpad(context, DPAD_DOWN, 80ms, 220ms);
        }
    };

    //  Type a Chinese character with one reading. Returns true when it was
    //  picked from the candidates.
    auto type_chinese = [&](const std::string& character, const std::vector<std::string>& symbols) -> bool{
        size_t typed = 0;
        for (const std::string& symbol : symbols){
            if (symbol == "ˉ"){
                pbf_press_button(context, BUTTON_Y, 80ms, 320ms);
            }else{
                press_key(symbol);
            }
            typed++;
            int index = find_candidate(character);
            if (index >= 0){
                env.log("Found " + character + " at candidate #" + std::to_string(index) + ".", COLOR_BLUE);
                pick_candidate(index);
                back_to_keys();
                return true;
            }
        }
        //  Whole reading typed and not on the first page: page through the
        //  candidates. R only pages while the cursor is on the candidate bar;
        //  moving right past the last candidate also scrolls.
        auto read_first = [&]() -> std::string{
            context.wait_for_all_requests();
            context.wait_for(300ms);
            VideoSnapshot snapshot = env.console.video().snapshot();
            ImageFloatBox box(CAND_X0 - CAND_PITCH / 2, CAND_Y - 0.028, CAND_PITCH * 3, 0.056);
            return trim_spaces(OCR::ocr_read(candidate_language, extract_box_reference(snapshot, box)));
        };
        for (int c = 0; c < 6; c++){
            const KeyboardKey* key = nullptr;
            int bar_index = -1;
            if (read_cursor(key, bar_index) && key == nullptr){
                break;
            }
            pbf_press_dpad(context, DPAD_UP, 80ms, 220ms);
        }
        for (int page = 0; page < 12; page++){
            std::string before = read_first();
            pbf_press_button(context, BUTTON_R, 80ms, 320ms);
            if (read_first() == before){
                //  R did nothing: walk right past the end of the list.
                for (int c = 0; c < CAND_COUNT + 1; c++){
                    pbf_press_dpad(context, DPAD_RIGHT, 80ms, 170ms);
                }
                if (read_first() == before){
                    env.log("No more candidate pages.");
                    break;
                }
            }
            int index = find_candidate(character);
            if (index >= 0){
                env.log("Found " + character + " on page " + std::to_string(page + 2) + ".", COLOR_BLUE);
                pick_candidate(index);
                back_to_keys();
                return true;
            }
        }
        //  Not this reading: delete what was typed (only the typed symbols,
        //  so the characters already entered stay).
        back_to_keys();
        for (size_t c = 0; c < typed; c++){
            pbf_press_button(context, BUTTON_B, 80ms, 220ms);
        }
        return false;
    };

    bool japanese_hiragana = false;
    bool japanese_mode_set = false;

    for (const NameChar& item : name){
        env.log("Name character: " + item.character);
        uint32_t cp = utf8_code_point(item.character);

        switch (keyboard){
        case Keyboard::Zhuyin:
        case Keyboard::Pinyin:{
            std::vector<std::vector<std::string>> readings;
            if (!item.manual_reading.empty()){
                readings.push_back(item.manual_reading);
            }else{
                const auto& table = chinese_readings();
                auto iter = table.find(item.character);
                if (iter == table.end()){
                    throw UserSetupError(env.logger(), "Character Name: no reading known for \"" + item.character + "\". Write it as 字:注音.");
                }
                const std::vector<std::string>& list = keyboard == Keyboard::Zhuyin ? iter->second.zhuyin : iter->second.pinyin;
                for (const std::string& reading : list){
                    readings.push_back(utf8_chars(reading));
                }
            }
            bool done = false;
            for (const std::vector<std::string>& symbols : readings){
                if (type_chinese(item.character, symbols)){
                    done = true;
                    break;
                }
            }
            if (!done){
                fail("Unable to find \"" + item.character + "\" in the candidates.");
            }
            break;
        }
        case Keyboard::Korean:{
            std::vector<std::string> keys;
            if (!hangul_keys(cp, keys)){
                if (find_key(layout, item.character) == nullptr){
                    throw UserSetupError(env.logger(), "Character Name: \"" + item.character + "\" cannot be typed on the Korean keyboard.");
                }
                keys.push_back(item.character);
            }
            for (const std::string& key : keys){
                if (key.size() > 1 && key[0] == '+'){
                    //  Shift (left stick press), then the key.
                    const KeyboardKey* target = find_key(layout, key.substr(1));
                    if (target == nullptr){
                        fail("No key for \"" + key + "\".");
                    }
                    move_to(*target);
                    pbf_press_button(context, BUTTON_LCLICK, 80ms, 220ms);
                    pbf_press_button(context, BUTTON_A, 80ms, 320ms);
                }else{
                    press_key(key);
                }
            }
            break;
        }
        case Keyboard::Japanese:{
            std::string key;
            bool hiragana = false;
            int modifier = 0;
            if (!kana_keys(cp, key, hiragana, modifier)){
                throw UserSetupError(env.logger(), "Character Name: \"" + item.character + "\" is not hiragana / katakana.");
            }
            if (key != "ー" && (!japanese_mode_set || hiragana != japanese_hiragana)){
                press_key(hiragana ? "MODE_HIRAGANA" : "MODE_KATAKANA");
                japanese_hiragana = hiragana;
                japanese_mode_set = true;
            }
            press_key(key);
            for (int c = 0; c < modifier; c++){
                press_key("MODE_SMALL");
            }
            break;
        }
        case Keyboard::Latin:{
            if (item.character == " "){
                pbf_press_button(context, BUTTON_Y, 80ms, 320ms);
                break;
            }
            std::string lower = item.character;
            bool upper = false;
            if (lower.size() == 1 && lower[0] >= 'A' && lower[0] <= 'Z'){
                lower[0] = (char)(lower[0] - 'A' + 'a');
                upper = true;
            }
            const KeyboardKey* target = find_key(layout, lower);
            if (target == nullptr){
                throw UserSetupError(env.logger(), "Character Name: \"" + item.character + "\" is not on the keyboard (letters, digits and - / : ' , . ? ! only).");
            }
            move_to(*target);
            if (upper){
                pbf_press_button(context, BUTTON_LCLICK, 80ms, 220ms);
            }
            pbf_press_button(context, BUTTON_A, 80ms, 320ms);
            break;
        }
        }
    }

    //  確定 (+). On the Korean (system) keyboard the first + only finishes
    //  the syllable being composed and the keyboard stays open, so press it
    //  again while the keyboard cursor is still there.
    env.log("Name entered. Confirming.", COLOR_BLUE);
    for (int attempt = 0; attempt < 3; attempt++){
        pbf_press_button(context, BUTTON_PLUS, 100ms, 900ms);
        context.wait_for_all_requests();
        context.wait_for(600ms);
        double cursor_x = 0;
        double cursor_y = 0;
        if (!find_keyboard_cursor(env.console.video().snapshot(), cursor_x, cursor_y)){
            return;
        }
        env.log("The keyboard is still open. Pressing + again.", COLOR_RED);
    }
}



//  The original fixed-order program was removed: Auto Story now runs from
//  story_flow.json (AutoStory).


//
//  Auto Story: the order of the steps comes from story_flow.json.
//
namespace{

const char* STORY_FLOW_FILE = "story_flow.json";

//  Start From slugs, in the order of AutoStoryMultiLang::StartPoint. A step
//  with "start": <slug> is where that choice starts.
const char* START_SLUGS[] = {
    "create-character", "select-character", "train-station", "chase-pancham",
    "choose-starter", "first-battle", "battle-zone", "escape-zone",
    "waitress-battle", "escape-zone-2", "escape-zone-3", "walk-into-hotel",
    "hotel-intro", "hotel-upstairs", "hotel-night", "hotel-morning",
    "roll-tutorial", "leave-hotel", "urbain-battle", "map-tutorial",
    "rock-scene", "rooftop", "pc", "wild-zone-1",
    "first-catch", "bunnelby", "after-bunnelby", "scatterbug",
    "weedle", "leave-wild-zone", "walk-to-lab", "lab-elevator",
    "lab-mable", "research-menu",
};
static_assert(
    sizeof(START_SLUGS) / sizeof(START_SLUGS[0]) ==
    (size_t)AutoStoryMultiLang::StartPoint::ResearchMenu + 1,
    "One slug per start point."
);

//  Used when story_flow.json is missing or not valid.
const char* DEFAULT_STORY_FLOW =
    R"JSON({)JSON" "\n"
    R"JSON(    "flow": [)JSON" "\n"
    R"JSON(        { "id": "ZA-00 Start the game + language", "action": "start_game", "start": "create-character" },)JSON" "\n"
    R"JSON(        { "id": "ZA-00 Character look + name", "action": "create_character", "start": "select-character" },)JSON" "\n"
    R"JSON(        { "id": "ZA-01 Train station", "macro": "ZA-01-車站.json", "expect": "logo_video", "start": "train-station" },)JSON" "\n"
    R"JSON(        { "id": "Before chasing Pancham", "action": "before_chasing_pancham", "start": "chase-pancham" },)JSON" "\n"
    R"JSON(        { "id": "ZA-02-0 Chase Pancham", "macro": "ZA-02-0-追頑皮熊貓.json", "expect": "quest_icon" },)JSON" "\n"
    R"JSON(        { "id": "Before choosing the starter", "action": "before_choosing_starter", "start": "choose-starter" },)JSON" "\n"
    R"JSON(        { "id": "ZA-02-1 Choose starter", "action": "choose_starter", "expect": "battle_challenge" },)JSON" "\n"
    R"JSON(        { "id": "First Battle", "action": "trainer_battle", "start": "first-battle" },)JSON" "\n"
    R"JSON(        { "id": "ZA-03-0 battle zone", "story": { "before": 10, "after": 0, "quiet": 1200 }, "start": "battle-zone" },)JSON" "\n"
    R"JSON(        { "id": "ZA-03-1 Escape the battle zone 0", "macro": "ZA-03-1-逃出對戰特區0.json", "start": "escape-zone" },)JSON" "\n"
    R"JSON(        { "id": "ZA-03-1 escape 1", "story": { "before": 10, "after": 10, "quiet": 1200 }, "expect": "battle_challenge" },)JSON" "\n"
    R"JSON(        { "id": "Waitress Battle", "action": "trainer_battle", "start": "waitress-battle" },)JSON" "\n"
    R"JSON(        { "id": "ZA-03-3 escape 2", "story": { "before": 10, "after": 10, "quiet": 1200 }, "start": "escape-zone-2" },)JSON" "\n"
    R"JSON(        { "id": "ZA-03-3 Escape the battle zone 2-2", "macro": "ZA-03-3-逃出對戰特區2-2.json" },)JSON" "\n"
    R"JSON(        { "id": "ZA-03-4 Escape the battle zone 3", "macro": "ZA-03-4-逃出對戰特區3.json", "start": "escape-zone-3" },)JSON" "\n"
    R"JSON(        { "id": "ZA-04-0 Hotel Z", "story": { "before": 10, "after": 10, "quiet": 1200 }, "start": "walk-into-hotel" },)JSON" "\n"
    R"JSON(        { "id": "ZA-04-1 Walk into Hotel Z", "macro": "ZA-04-1-走進Z飯店.json" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-0 Hotel Z intro", "story": { "before": 10, "after": 10, "quiet": 1200 }, "start": "hotel-intro" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-1 Talk to AZ 1", "macro": "ZA-05-1-Z飯店-向AZ對話1.json" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-2 AZ", "story": { "before": 10, "after": 10, "quiet": 1200 } },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-3 Elevator to 2F", "macro": "ZA-05-3-Z飯店-走向電梯門口上2F.json", "start": "hotel-upstairs" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-4 Room 202", "macro": "ZA-05-4-Z飯店-走進202客房.json" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-5 To the bed", "macro": "ZA-05-5-Z飯店-202房間走向床邊.json" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-6 To the door", "macro": "ZA-05-6-Z飯店-202房間走向房門.json" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-7 night", "story": { "before": 10, "after": 10, "quiet": 1200 }, "start": "hotel-night" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-8 Next morning", "macro": "ZA-05-8-Z飯店-隔天早上起床.json", "start": "hotel-morning" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-9 roll tutorial", "story": { "before": 10, "after": 10, "quiet": 1200 }, "start": "roll-tutorial" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-10 To the hotel door", "macro": "ZA-05-10-Z飯店-翻滾教學結束走向ZA飯店門口.json", "start": "leave-hotel" },)JSON" "\n"
    R"JSON(        { "id": "ZA-05-11 Leave the hotel", "macro": "ZA-05-11-Z飯店-出ZA飯店.json" },)JSON" "\n"
    R"JSON(        { "id": "ZA-06-1 Walk to the rival", "macro": "ZA-06-1-走向蓋伊對戰.json", "wait_hud": 20, "expect": "battle_challenge" },)JSON" "\n"
    R"JSON(        { "id": "Rival Battle", "action": "trainer_battle", "start": "urbain-battle" },)JSON" "\n"
    R"JSON(        { "id": "ZA-07 map tutorial", "story": { "before": 10, "after": 10, "quiet": 1200 }, "start": "map-tutorial" },)JSON" "\n"
    R"JSON(        { "id": "ZA-08-1 Walk to the rocks", "macro": "ZA-08-1-往前走觸發岩石劇情.json", "start": "rock-scene" },)JSON" "\n"
    R"JSON(        { "id": "ZA-08-2 rocks", "story": { "before": 10, "after": 10, "quiet": 1200 } },)JSON" "\n"
    R"JSON(        { "id": "ZA-09-1 Ladder", "macro": "ZA-09-1-走向大樓梯子與上梯子.json", "start": "rooftop" },)JSON" "\n"
    R"JSON(        { "id": "ZA-09-2 rooftop", "story": { "before": 10, "after": 10, "quiet": 1200 } },)JSON" "\n"
    R"JSON(        { "id": "ZA-09-3 Jump down", "macro": "ZA-09-3-跳下大樓.json" },)JSON" "\n"
    R"JSON(        { "id": "ZA-10-1 PC scene", "story": { "before": 10, "after": 10, "quiet": 1200 }, "start": "pc" },)JSON" "\n"
    R"JSON(        { "id": "ZA-10-2 Walk to the PC", "macro": "ZA-10-2-走向PC.json" },)JSON" "\n"
    R"JSON(        { "id": "ZA-10-3 Pokedex tutorial", "story": { "before": 0, "after": 10, "quiet": 2000 } },)JSON" "\n"
    R"JSON(        { "id": "ZA-11-1 Walk to Wild Zone 1", "action": "walk_to_marker", "marker": "Wild Zone 1", "start": "wild-zone-1" },)JSON" "\n"
    R"JSON(        { "id": "ZA-11-2 Wild Zone 1", "story": { "before": 10, "after": 10, "quiet": 1200 } },)JSON" "\n"
    R"JSON(        { "id": "ZA-11-3 Into Wild Zone 1", "macro": "ZA-11-3-走向WildZone1-3.json" },)JSON" "\n"
    R"JSON(        { "id": "ZA-12-1 First catch", "macro": "ZA-12-1-捕捉教學1.json", "start": "first-catch" },)JSON" "\n"
    R"JSON(        { "id": "ZA-12-2 Bunnelby battle + catch", "action": "capture_battle", "start": "bunnelby" },)JSON" "\n"
    R"JSON(        { "id": "ZA-12-3 after Bunnelby", "story": { "before": 10, "after": 10, "quiet": 1200 }, "start": "after-bunnelby" },)JSON" "\n"
    R"JSON(        { "id": "ZA-12-4 Walk to Scatterbug", "macro": "ZA-12-4-走向粉蝶蟲.json", "start": "scatterbug" },)JSON" "\n"
    R"JSON(        { "id": "ZA-12-4 Catch Scatterbug", "action": "catch_scatterbug" },)JSON" "\n"
    R"JSON(        { "id": "ZA-12-5 Catch Weedle", "action": "catch_weedle", "start": "weedle" },)JSON" "\n"
    R"JSON(        { "id": "ZA-12-6 Leave Wild Zone 1", "action": "leave_wild_zone", "start": "leave-wild-zone" },)JSON" "\n"
    R"JSON(        { "id": "ZA-13-1 Walk to the lab", "action": "walk_to_lab", "dialog_quiet": { "ZA-13-1 Lab story": 1200, "ZA-13-1 Lab door": 1200, "ZA-13-1 Lab entrance": 1200 }, "start": "walk-to-lab" },)JSON" "\n"
    R"JSON(        { "id": "ZA-13-2 Lab elevator", "action": "lab_elevator", "dialog_quiet": { "ZA-13-2 Lab 1F": 1200 }, "start": "lab-elevator" },)JSON" "\n"
    R"JSON(        { "id": "ZA-13-2 3F story", "story": { "before": 0, "after": 0, "quiet": 1200, "press_first": false, "max_wait": 60 } },)JSON" "\n"
    R"JSON(        { "id": "ZA-13-3 Walk to Mable", "action": "walk_to_mable", "dialog_quiet": { "ZA-13-3 Mable": 1200 }, "start": "lab-mable" },)JSON" "\n"
    R"JSON(        { "id": "ZA-13-4 Mable's Research", "action": "open_research_list", "start": "research-menu" },)JSON" "\n"
    R"JSON(        { "id": "ZA-13-4 After Mable's Research", "story": { "before": 0, "after": 0, "quiet": 1200, "press_first": false, "max_wait": 60 } })JSON" "\n"
    R"JSON(    ])JSON" "\n"
    R"JSON(})JSON" "\n";

}


AutoStory_Descriptor::AutoStory_Descriptor()
    : AutoStoryMultiLang_Descriptor(
        "PokemonLZA:AutoStory",
        "Auto Story",
        "Play the story from the start (or a chosen step). The order of the steps is read from story_flow.json "
        "in the macro folder. Each step runs a macro, clears a story dialog, or runs a built-in action."
    )
{}

AutoStory::AutoStory(){}


//  Wait until the overworld HUD has been on screen for about a second (after
//  a loading screen, a door or HOME). Returns false after max_wait.
static bool wait_for_overworld_hud(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    std::chrono::seconds max_wait
){
    WallClock wait_start = current_time();
    int hud_seen = 0;
    while (current_time() - wait_start < max_wait){
        context.wait_for(300ms);
        context.wait_for_all_requests();
        if (overworld_hud(env.console.video().snapshot())){
            if (++hud_seen >= 3){
                return true;
            }
        }else{
            hud_seen = 0;
        }
    }
    return false;
}


void AutoStory::run_flow_action(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::string& action, const std::string& id, const JsonObject& step
){
    if (action == "start_game"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Starting the game.");
        launch_and_pick_language(env, context);
        return;
    }
    if (action == "create_character"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Creating character.");
        select_character(env, context);
        //  Confirm the look; the name keyboard comes up.
        pbf_press_button(context, BUTTON_A, 160ms, 340ms);
        WallClock keyboard_wait = current_time();
        bool keyboard = false;
        while (current_time() - keyboard_wait < std::chrono::seconds(10)){
            context.wait_for_all_requests();
            double kx = 0;
            double ky = 0;
            if (find_keyboard_cursor(env.console.video().snapshot(), kx, ky)){
                keyboard = true;
                break;
            }
            context.wait_for(300ms);
        }
        if (!keyboard){
            env.log("No name keyboard yet. Pressing A again.", COLOR_RED);
            pbf_press_button(context, BUTTON_A, 160ms, 1840ms);
        }
        enter_character_name(env, context);

        //  After the name: pick the upper answer of the confirmation, then
        //  skip the opening video.
        pbf_wait(context, 1000ms);
        pbf_press_dpad(context, DPAD_UP, 200ms, 200ms);
        pbf_press_button(context, BUTTON_A, 200ms, 15000ms);
        pbf_press_button(context, BUTTON_PLUS, 5000ms, 3000ms);
        pbf_press_button(context, BUTTON_A, 200ms, 200ms);
        context.wait_for_all_requests();
        return;
    }
    if (action == "before_chasing_pancham"){
        //  Not needed when the movement tutorial already showed up.
        if (!m_movement_tutorial_seen){
            clear_dialogs(env, context, "Before chasing Pancham", false, 1500ms, false, std::chrono::seconds(20));
        }
        return;
    }
    if (action == "before_choosing_starter"){
        clear_dialogs(env, context, "Before choosing the starter", false, 1500ms, false, std::chrono::seconds(20));
        return;
    }
    if (action == "choose_starter"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Choosing starter.");
        run_macro(env, context, starter_macro_filename());
        m_hud_expected = true;
        return;
    }
    if (action == "trainer_battle"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, id + ".");
        run_trainer_battle(env, context, step.get_string_default("name", id.c_str()));
        return;
    }
    if (action == "capture_battle"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Bunnelby battle.");
        run_capture_battle(env, context);
        return;
    }
    if (action == "catch_scatterbug"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Catching Scatterbug.");
        catch_wild_pokemon(env, context, "scatterbug", "Scatterbug");
        return;
    }
    if (action == "catch_weedle"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Catching Weedle.");
        navigate_by_minimap(env, context, "Weedle spot", WEEDLE_SPOT_X, WEEDLE_SPOT_Y, 0.004);
        catch_wild_pokemon(env, context, "weedle", "Weedle");
        return;
    }
    if (action == "leave_wild_zone"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Leaving Wild Zone 1.");
        std::pair<double, double> stick = navigate_by_minimap(
            env, context, "Gate", GATE_SPOT_X, GATE_SPOT_Y, 0.004
        );
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
        leave_wild_zone(env, context);
        return;
    }
    if (action == "walk_to_marker"){
        //  Walk to the quest marker (camera + minimap, runs when far), until
        //  a dialog / story starts or an A prompt is up at the marker.
        //  Step fields: "marker" (name for the log, default: the step id),
        //  "outdoors" (default true: read the distance, run when far).
        const std::string marker = step.get_string_default("marker", id.c_str());
        const bool outdoors = step.get_boolean_default("outdoors", true);
        //  "story_within": a dialog farther than this (m) from the marker is
        //  someone on the street (a trainer, a shop): back out with B and
        //  keep walking. Default 20.
        const int story_within = (int)step.get_integer_default("story_within", 20);
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Walking to " + marker + ".");
        //  Right after a tutorial / story the screen is still settling (the
        //  new mission and area name banners cover the HUD and the marker).
        //  Wait for a steady HUD, then let the banners go away.
        if (!wait_for_overworld_hud(env, context, std::chrono::seconds(15))){
            env.log("No steady HUD before walking to " + marker + ". Walking anyway.", COLOR_RED);
        }
        context.wait_for(2500ms);
        for (int interruptions = 0; ; interruptions++){
            WalkResult result = walk_to_quest_marker(env, context, marker, outdoors);
            if (result == WalkResult::Prompt){
                pbf_press_button(context, BUTTON_A, 100ms, 400ms);
                return;
            }
            if (!outdoors || m_last_marker_distance < 0 || m_last_marker_distance <= story_within || interruptions >= 5){
                return;
            }
            env.log(
                "Dialog " + std::to_string(m_last_marker_distance) + " m from " + marker +
                ": not the story (someone on the street). Backing out with B.",
                COLOR_RED
            );
            //  B closes the talk / picks "no", without starting anything.
            WallClock back_start = current_time();
            WallClock quiet_since = back_start;
            while (current_time() - back_start < std::chrono::seconds(30)){
                context.wait_for_all_requests();
                VideoSnapshot snapshot = env.console.video().snapshot();
                if (dialog_on_screen(snapshot) || !overworld_hud(snapshot)){
                    pbf_press_button(context, BUTTON_B, 100ms, 400ms);
                    quiet_since = current_time();
                    continue;
                }
                if (current_time() - quiet_since > std::chrono::milliseconds(1500)){
                    break;
                }
                context.wait_for(200ms);
            }
        }
    }
    if (action == "walk_to_lab"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Walking to the lab.");
        walk_to_lab(env, context);
        return;
    }
    if (action == "lab_elevator"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Lab elevator.");
        take_lab_elevator(env, context);
        return;
    }
    if (action == "walk_to_mable"){
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Walking to Mable.");
        walk_to_mable(env, context);
        return;
    }
    if (action == "open_research_list"){
        open_research_list(env, context);
        return;
    }
    throw UserSetupError(env.logger(), "story_flow.json: step \"" + id + "\" has an unknown action \"" + action + "\".");
}


void AutoStory::run_flow_step(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const JsonObject& step, size_t index, size_t count
){
    const std::string id = step.get_string_default("id", "(no id)");

    //  "expect": the special endings of a step, as a string or a list.
    std::vector<std::string> expects;
    if (const std::string* expect = step.get_string("expect")){
        expects.emplace_back(*expect);
    }else if (const JsonArray* list = step.get_array("expect")){
        for (const JsonValue& item : *list){
            if (const std::string* text = item.to_string()){
                expects.emplace_back(*text);
            }
        }
    }
    bool logo_video = false;
    bool quest_icon = false;
    bool battle_challenge = false;
    for (const std::string& expect : expects){
        if (expect == "logo_video"){
            logo_video = true;
        }else if (expect == "quest_icon"){
            quest_icon = true;
        }else if (expect == "battle_challenge"){
            battle_challenge = true;
        }else{
            env.log("story_flow.json: unknown \"expect\": " + expect + " (step " + id + ")", COLOR_RED);
        }
    }

    //  "dialog_quiet": quiet times of the dialogs inside an action.
    m_quiet_overrides.clear();
    if (const JsonObject* quiet_list = step.get_object("dialog_quiet")){
        for (const auto& item : *quiet_list){
            m_quiet_overrides[item.first] = Milliseconds(item.second.to_integer_default(4000));
        }
    }

    env.log("Flow step " + std::to_string(index + 1) + "/" + std::to_string(count) + ": " + id, COLOR_BLUE);
    env.console.overlay().add_log(id);

    m_logo_video_expected = logo_video;
    m_quest_icon_expected = quest_icon;
    m_battle_challenge_expected = battle_challenge;

    //  "wait_hud": seconds to wait for the overworld HUD before the step
    //  (after a door / loading screen, which can take longer right after
    //  the game was started). Button presses during the loading are lost.
    const int64_t wait_hud = step.get_integer_default("wait_hud", 0);
    if (wait_hud > 0){
        if (wait_for_overworld_hud(env, context, std::chrono::seconds(wait_hud))){
            env.log("HUD seen. Starting " + id + ".");
        }else{
            env.log("No HUD within " + std::to_string(wait_hud) + " seconds. Starting " + id + " anyway.", COLOR_RED);
        }
        context.wait_for(300ms);
    }

    if (const std::string* macro = step.get_string("macro")){
        run_macro(env, context, *macro);
    }else if (const JsonObject* story = step.get_object("story")){
        Milliseconds before(story->get_integer_default("before", 0));
        Milliseconds after(story->get_integer_default("after", 0));
        Milliseconds quiet(story->get_integer_default("quiet", 4000));
        bool press_first = story->get_boolean_default("press_first", true);
        std::chrono::seconds max_wait(story->get_integer_default("max_wait", 20));
        env.log(
            "Story dialog " + id + " (story_flow.json): before " + std::to_string(before.count()) +
            " ms, after " + std::to_string(after.count()) +
            " ms, quiet " + std::to_string(quiet.count()) + " ms"
        );
        context.wait_for_all_requests();
        context.wait_for(before);
        clear_dialogs(env, context, id, true, quiet, press_first, max_wait);
        context.wait_for(after);
    }else if (const std::string* action = step.get_string("action")){
        run_flow_action(env, context, *action, id, step);
    }else{
        throw UserSetupError(env.logger(), "story_flow.json: step \"" + id + "\" has no \"macro\", \"story\" or \"action\".");
    }

    m_logo_video_expected = false;
    m_quest_icon_expected = false;
    m_battle_challenge_expected = false;
    m_quiet_overrides.clear();
}


//  At Home: close the game, start it with the chosen profile, and press A
//  (title screen, "PRESS A" in every language) until the save has loaded and
//  the overworld HUD is up. A loaded save always lands on the overworld.
void AutoStory::restart_and_load_save(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    env.log("Restarting the game with user profile " + std::to_string((int)PROFILE) + " and loading the save.");
    env.console.overlay().add_log("Restart game");
    close_game_from_home(env.console, context);
    start_game_from_home(
        env.console, context,
        ConsoleSettings::instance().TOLERATE_SYSTEM_UPDATE_MENU_FAST,
        0, PROFILE
    );

    WallClock start = current_time();
    WallClock last_press = start;
    int hud_seen = 0;
    while (current_time() - start < std::chrono::minutes(3)){
        context.wait_for_all_requests();
        if (overworld_hud(env.console.video().snapshot())){
            if (++hud_seen >= 3){
                env.log("Save loaded (HUD seen).", COLOR_BLUE);
                context.wait_for(500ms);
                return;
            }
            context.wait_for(300ms);
            continue;
        }
        hud_seen = 0;
        //  Title screen / loading: A every 2 seconds (ignored while loading).
        if (current_time() - last_press > std::chrono::seconds(2)){
            pbf_press_button(context, BUTTON_A, 100ms, 0ms);
            last_press = current_time();
        }
        context.wait_for(300ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "restart_and_load_save(): No overworld HUD within 3 minutes of starting the game.",
        env.console
    );
}


void AutoStory::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    assert_16_9_720p_min(env.logger(), env.console);

    const StartPoint start = START_MODE.new_game() ? StartPoint::CreateCharacter : START_POINT.get();

    //  No party bar on the overworld until the starter is picked.
    m_hud_expected = start > StartPoint::ChooseStarter;
    m_logo_video_expected = false;
    m_quest_icon_expected = false;
    m_movement_tutorial_seen = false;
    m_battle_challenge_expected = false;
    m_quiet_overrides.clear();
    env.log("Macro folder: " + macro_file_path(""));

    //  Load the flow. Without a usable file, use the built-in one.
    JsonValue json;
    std::string source = STORY_FLOW_FILE;
    {
        bool loaded = false;
        const std::string path = macro_file_path(STORY_FLOW_FILE);
        std::string text;
        if (file_to_string(path, text)){
            if (text.size() >= 3 &&
                (uint8_t)text[0] == 0xef &&
                (uint8_t)text[1] == 0xbb &&
                (uint8_t)text[2] == 0xbf
            ){
                text.erase(0, 3);
            }
            try{
                json = parse_json(text);
                const JsonObject* root = json.to_object();
                loaded = root != nullptr && root->get_array("flow") != nullptr;
                if (!loaded){
                    env.log("story_flow.json has no \"flow\" list.", COLOR_RED);
                }
            }catch (Exception& e){
                env.log("story_flow.json is not valid JSON: " + e.message(), COLOR_RED);
            }
        }else{
            env.log("story_flow.json not found: " + path, COLOR_RED);
        }
        if (!loaded){
            env.log("Using the built-in story flow.", COLOR_RED);
            json = parse_json(DEFAULT_STORY_FLOW);
            source = "built-in flow";
        }
    }
    std::vector<const JsonObject*> steps;
    for (const JsonValue& item : json.to_object_throw().get_array_throw("flow")){
        const JsonObject* step = item.to_object();
        if (step == nullptr){
            throw UserSetupError(env.logger(), source + ": every step of \"flow\" must be an object.");
        }
        steps.emplace_back(step);
    }

    //  Start From: the first step marked with its slug.
    const std::string slug = START_SLUGS[(size_t)start];
    size_t first = steps.size();
    for (size_t c = 0; c < steps.size(); c++){
        if (steps[c]->get_string_default("start") == slug){
            first = c;
            break;
        }
    }
    if (first == steps.size()){
        throw UserSetupError(env.logger(), source + ": no step has \"start\": \"" + slug + "\".");
    }
    env.log("Story flow: " + source + ", " + std::to_string(steps.size()) + " steps, starting at step " + std::to_string(first + 1) + ".");

    //  Connect the controller.
    const bool from_beginning = start <= StartPoint::CreateCharacter;

    //  Only points that start on the overworld with the party bar have a HUD
    //  to wait for. The others (before the starter, or on a dialog /
    //  pre-battle / tutorial / LOOKS screen) get a fixed wait.
    bool overworld_start = start > StartPoint::ChooseStarter;
    switch (start){
    case StartPoint::FirstBattle:
    case StartPoint::BattleZone:
    case StartPoint::WaitressBattle:
    case StartPoint::EscapeBattleZone2:
    case StartPoint::RivalBattle:
    case StartPoint::MapTutorial:
    case StartPoint::BunnelbyCapture:
    case StartPoint::AfterBunnelby:
        overworld_start = false;
        break;
    default:
        break;
    }
    const bool restart = !from_beginning && START_MODE.load_save();
    if (restart && !overworld_start){
        throw UserSetupError(
            env.logger(),
            "Load save at a step: \"" + START_POINT.database().find(START_POINT.current_value())->display + "\" does not start on the overworld, "
            "so a loaded save cannot be there. Pick the step the save loads at (an earlier one)."
        );
    }

    if (START_LOCATION.start_in_grip_menu()){
        grip_menu_connect_go_home(context);
        if (!from_beginning && !restart){
            resume_game_from_home(env.console, context);
            //  The game takes a moment to come back from HOME: button
            //  presses before that are lost and the first macro walks short.
            if (!overworld_start){
                env.log("Back in the game. Waiting 3 seconds (this start point has no HUD).");
                context.wait_for(3000ms);
            }else if (wait_for_overworld_hud(env, context, std::chrono::seconds(15))){
                env.log("Back in the game (HUD seen).", COLOR_BLUE);
            }else{
                env.log("Back in the game? No HUD seen in 15 seconds. Starting anyway.", COLOR_RED);
            }
        }
    }else{
        require_player(env.console, context, BUTTON_NONE);
        if (from_beginning || restart){
            go_home(env.console, context);
        }
    }

    if (restart){
        restart_and_load_save(env, context);
    }

    for (size_t c = first; c < steps.size(); c++){
        run_flow_step(env, context, *steps[c], c, steps.size());
    }

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}



//  For other LZA programs (Claim Mystery Gift): the system keyboard cursor.
bool lza_find_keyboard_cursor(const ImageViewRGB32& screen, double& x, double& y){
    return find_keyboard_cursor(screen, x, y);
}



}
}
}
