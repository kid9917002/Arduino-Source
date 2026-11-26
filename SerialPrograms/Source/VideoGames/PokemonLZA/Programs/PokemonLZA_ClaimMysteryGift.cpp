/*  Claim Mystery Gift
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/StartupChecks/VideoResolutionCheck.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonLZA_ClaimMysteryGift.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;

//  In PokemonLZA_AutoStoryMultiLang.cpp: the system keyboard cursor.
bool lza_find_keyboard_cursor(const ImageViewRGB32& screen, double& x, double& y);


namespace{

//  Gifts of "Get via Internet", in the order the game lists them (new
//  distributions are added at the end). The game only lists the ones being
//  given out now, so the row of a gift is its place among the gifts here
//  that are not over yet.
struct InternetGift{
    const char* slug;
    const char* label;
    long long end_utc;  //  end as YYYYMMDDhhmm (UTC), 0: no end date
};
const std::vector<InternetGift>& INTERNET_GIFTS(){
    static const std::vector<InternetGift> gifts{
        {
            "diancie",
            "Diancite + Side Mission \"Shine Bright Like a Gemstone\" (光芒耀眼如寶石 / 宝石のような輝きを)",
            0,
        },
        {
            "project-m",
            "Mewtwonite X & Y + Side Mission \"Project M\" (專案代號：M / プロジェクトコード：M)",
            0,
        },
        {
            "hyperspace-rift",
            "Special Hyperspace Rift (偵測到了特殊的異次元扭洞！ / 特別な異次元のひずみを検知！)",
            0,
        },
        {
            "spewpa",
            "Spewpa (粉蝶蛹 禮物) - until 2026-11-19",
            202611190159,
        },
        {
            "vivillon-poke-ball",
            "Poké Ball Pattern Vivillon (球球花紋的彩粉蝶 / ボールのもようの ビビヨン) - until 2027-02-26",
            202702260159,
        },
    };
    return gifts;
}
long long now_utc_yyyymmddhhmm(){
    std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    return (long long)(utc.tm_year + 1900) * 100000000LL
         + (long long)(utc.tm_mon + 1) * 1000000LL
         + (long long)utc.tm_mday * 10000LL
         + (long long)utc.tm_hour * 100LL
         + (long long)utc.tm_min;
}

}


ClaimMysteryGift_Descriptor::ClaimMysteryGift_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:ClaimMysteryGift",
        STRING_POKEMON + " LZA", "Claim Mystery Gift",
        "",
        "Claim Mystery Gifts: via the Internet (all of them or the rows you pick) or with a code.",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}


ClaimMysteryGift::ClaimMysteryGift()
    : START_IN(
        "<b>Start In:</b>",
        {
            {StartIn::Overworld,        "overworld",    "Overworld (all menus closed)"},
            {StartIn::MysteryGiftMenu,  "mystery-gift", "Mystery Gift menu (Get via Internet / Get with Code)"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        StartIn::Overworld
    )
    , METHOD(
        "<b>Method:</b><br>"
        "The Internet list only shows the gifts that are being given out now, so gifts past their end date never show up.",
        {
            {Method::InternetAll,      "internet-all",      "Get via Internet: claim every unclaimed gift"},
            {Method::InternetSelected, "internet-selected", "Get via Internet: claim only the gifts checked below"},
            {Method::Code,             "code",              "Get with Code/Password: enter the code below"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        Method::InternetAll
    )
    , GIFTS(
        "Gifts to claim (Get via Internet: claim only the gifts checked below)",
        LockMode::LOCK_WHILE_RUNNING
    )
    , OTHER_GIFTS(
        "<b>Other gifts:</b><br>Also claim the gifts below the ones above (new distributions).",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , CODE(
        false,
        "<b>Code / Password:</b><br>"
        "For Get with Code/Password. Not case sensitive; spaces and dashes are ignored.",
        LockMode::LOCK_WHILE_RUNNING,
        "", "XXXXXXXXXXXXXXXX"
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    //  A checkbox for each gift that is not over yet.
    const long long now = now_utc_yyyymmddhhmm();
    for (const InternetGift& gift : INTERNET_GIFTS()){
        if (gift.end_utc != 0 && now > gift.end_utc){
            m_gift_option_index.push_back(-1);
            continue;
        }
        m_gift_option_index.push_back((int)m_gift_options.size());
        m_gift_options.emplace_back(new BooleanCheckBoxOption(gift.label, LockMode::LOCK_WHILE_RUNNING, true));
        GIFTS.add_option(*m_gift_options.back(), gift.slug);
    }
    GIFTS.add_option(OTHER_GIFTS, "other");

    PA_ADD_OPTION(START_IN);
    PA_ADD_OPTION(METHOD);
    PA_ADD_OPTION(GIFTS);
    PA_ADD_OPTION(CODE);
    PA_ADD_OPTION(NOTIFICATIONS);
}



namespace{

//  Share of pixels in a box that pass a test.
template <typename Test>
double pixel_ratio(const ImageViewRGB32& screen, const ImageFloatBox& box, Test test){
    ImageViewRGB32 image = extract_box_reference(screen, box);
    size_t total = 0;
    size_t hits = 0;
    for (size_t y = 0; y < image.height(); y++){
        for (size_t x = 0; x < image.width(); x++){
            uint32_t pixel = image.pixel(x, y);
            int r = (pixel >> 16) & 0xff;
            int g = (pixel >>  8) & 0xff;
            int b = (pixel >>  0) & 0xff;
            total++;
            if (test(r, g, b)){
                hits++;
            }
        }
    }
    return total == 0 ? 0 : (double)hits / (double)total;
}
double white_ratio(const ImageViewRGB32& screen, const ImageFloatBox& box){
    return pixel_ratio(screen, box, [](int r, int g, int b){
        return r >= 220 && g >= 220 && b >= 220;
    });
}
double mean_brightness(const ImageViewRGB32& screen, const ImageFloatBox& box){
    ImageViewRGB32 image = extract_box_reference(screen, box);
    double sum = 0;
    size_t total = 0;
    for (size_t y = 0; y < image.height(); y++){
        for (size_t x = 0; x < image.width(); x++){
            uint32_t pixel = image.pixel(x, y);
            sum += ((pixel >> 16) & 0xff) + ((pixel >> 8) & 0xff) + (pixel & 0xff);
            total++;
        }
    }
    return total == 0 ? 0 : sum / (3.0 * (double)total);
}

//  Mystery Gift menu: Get via Internet / Get with Code/Password / Check.
//  The selected row is white from x = 0.206 to 0.794. Returns its index,
//  or -1 when the menu is not on screen.
int mystery_gift_menu_row(const ImageViewRGB32& screen){
    const double ROW_Y[3] = {0.370, 0.500, 0.630};
    for (int c = 0; c < 3; c++){
        double y = ROW_Y[c];
        if (white_ratio(screen, ImageFloatBox(0.213, y - 0.025, 0.020, 0.050)) >= 0.80 &&
            white_ratio(screen, ImageFloatBox(0.772, y - 0.025, 0.015, 0.050)) >= 0.80
        ){
            return c;
        }
    }
    return -1;
}

//  The Get via Internet list: up to 7 rows from y = 0.272, 0.0778 apart,
//  x = 0.292 to 0.708. The selected row is white. Unclaimed gifts have a
//  red dot at the top right of the row.
const int GIFT_ROW_COUNT = 7;
double gift_row_top(int row){
    return 0.2722 + 0.0778 * row;
}
//  Index of the selected row, or -1 when the list is not on screen. The
//  result card (white title from x = 0.252) and the Mystery Gift menu
//  (white from x = 0.206) are white left of the list, so they do not count.
int gift_list_selected_row(const ImageViewRGB32& screen){
    for (int c = 0; c < GIFT_ROW_COUNT; c++){
        double top = gift_row_top(c);
        if (white_ratio(screen, ImageFloatBox(0.300, top + 0.012, 0.350, 0.042)) >= 0.55 &&
            white_ratio(screen, ImageFloatBox(0.272, top + 0.012, 0.012, 0.042)) < 0.20
        ){
            return c;
        }
    }
    return -1;
}
bool gift_row_filled(const ImageViewRGB32& screen, int row){
    double top = gift_row_top(row);
    ImageFloatBox box(0.300, top + 0.012, 0.350, 0.042);
    return white_ratio(screen, box) >= 0.55 || mean_brightness(screen, box) >= 70;
}
bool gift_row_unclaimed(const ImageViewRGB32& screen, int row){
    double top = gift_row_top(row);
    return pixel_ratio(screen, ImageFloatBox(0.6975, top + 0.002, 0.009, 0.018), [](int r, int g, int b){
        return r >= 200 && g <= 120 && b <= 140 && r - g >= 90;
    }) >= 0.15;
}


//  The code keyboard (Switch system keyboard, letters/digits).
const char* CODE_KEY_ROWS[4] = {
    "1234567890@",
    "QWERTYUIOP=",
    "ASDFGHJKL&;",
    "ZXCVBNM*#!?",
};
const double KEY_X0 = 0.1021;
const double KEY_PITCH_X = 0.0698;
const double KEY_Y0 = 0.5583;
const double KEY_PITCH_Y = 0.0707;
//  Columns go around: left of the first column is the right column
//  (Backspace / New Line / OK), column 11.
const int KEY_COLUMNS = 12;

bool code_key_position(char ch, int& col, int& row){
    for (int r = 0; r < 4; r++){
        for (int c = 0; CODE_KEY_ROWS[r][c] != 0; c++){
            if (CODE_KEY_ROWS[r][c] == ch){
                col = c;
                row = r;
                return true;
            }
        }
    }
    return false;
}
void cursor_to_key(double x, double y, int& col, int& row){
    if (x > 0.85){
        col = KEY_COLUMNS - 1;
    }else{
        col = (int)std::lround((x - KEY_X0) / KEY_PITCH_X);
        col = std::max(0, std::min(col, KEY_COLUMNS - 2));
    }
    row = (int)std::lround((y - KEY_Y0) / KEY_PITCH_Y);
    row = std::max(0, std::min(row, 4));
}

}



void ClaimMysteryGift::open_mystery_gift_menu(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    for (int attempt = 0; attempt < 3; attempt++){
        env.log("Opening the Mystery Gift menu.");
        //  X: menu. Up: from the first item to the last one (Online Play).
        pbf_press_button(context, BUTTON_X, 100ms, 2000ms);
        pbf_press_dpad(context, DPAD_UP, 100ms, 600ms);
        pbf_press_button(context, BUTTON_A, 100ms, 3000ms);
        //  Online Play: left, down -> Mystery Gift.
        pbf_press_dpad(context, DPAD_LEFT, 100ms, 500ms);
        pbf_press_dpad(context, DPAD_DOWN, 100ms, 500ms);
        pbf_press_button(context, BUTTON_A, 100ms, 1000ms);

        WallClock start = current_time();
        while (current_time() - start < std::chrono::seconds(20)){
            context.wait_for_all_requests();
            if (mystery_gift_menu_row(env.console.video().snapshot()) >= 0){
                env.log("On the Mystery Gift menu.", COLOR_BLUE);
                return;
            }
            context.wait_for(500ms);
        }
        env.log("No Mystery Gift menu. Backing out and trying again.", COLOR_RED);
        for (int c = 0; c < 4; c++){
            pbf_press_button(context, BUTTON_B, 100ms, 1200ms);
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "Unable to open the Mystery Gift menu.",
        env.console
    );
}

void ClaimMysteryGift::pick_menu_option(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int option){
    //  Move to the option.
    for (int c = 0; c < 10; c++){
        context.wait_for_all_requests();
        int row = mystery_gift_menu_row(env.console.video().snapshot());
        if (row < 0){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "Not on the Mystery Gift menu.",
                env.console
            );
        }
        if (row == option){
            break;
        }
        pbf_press_dpad(context, row < option ? DPAD_DOWN : DPAD_UP, 100ms, 400ms);
    }
    pbf_press_button(context, BUTTON_A, 100ms, 500ms);

    //  "It will be saved. Continue?" (Yes is the default), then the
    //  connection, which can take a while. A advances; it is only pressed
    //  while the screen we want is not there yet.
    WallClock start = current_time();
    WallClock last_press = start;
    int seen = 0;
    while (current_time() - start < std::chrono::seconds(180)){
        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();
        double x = 0;
        double y = 0;
        bool there = option == 0
            ? gift_list_selected_row(snapshot) >= 0
            : lza_find_keyboard_cursor(snapshot, x, y);
        if (there){
            if (++seen >= 2){
                env.log(option == 0 ? "On the Get via Internet list." : "On the code keyboard.", COLOR_BLUE);
                return;
            }
            context.wait_for(500ms);
            continue;
        }
        seen = 0;
        if (current_time() - last_press >= std::chrono::milliseconds(1500)){
            pbf_press_button(context, BUTTON_A, 100ms, 100ms);
            last_press = current_time();
        }
        context.wait_for(250ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        option == 0
            ? "No Get via Internet list after 3 minutes (no connection, or no gift to get)."
            : "No code keyboard after 3 minutes.",
        env.console
    );
}

void ClaimMysteryGift::finish_receiving(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    bool back_to_list, const std::string& what
){
    //  Connection, gift animation, result card: A through them until the
    //  list (Internet) or the Mystery Gift menu (code) is back.
    context.wait_for(2000ms);
    WallClock start = current_time();
    int seen = 0;
    while (current_time() - start < std::chrono::seconds(180)){
        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();
        bool back = back_to_list
            ? gift_list_selected_row(snapshot) >= 0
            : mystery_gift_menu_row(snapshot) >= 0;
        if (back){
            if (++seen >= 2){
                env.log("Received: " + what, COLOR_BLUE);
                return;
            }
            context.wait_for(700ms);
            continue;
        }
        seen = 0;
        pbf_press_button(context, BUTTON_A, 100ms, 1400ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "Did not get back from receiving " + what + " within 3 minutes.",
        env.console
    );
}

bool ClaimMysteryGift::gift_wanted(int gift) const{
    if (gift < 0){
        return OTHER_GIFTS;
    }
    int option = m_gift_option_index[(size_t)gift];
    return option >= 0 && *m_gift_options[(size_t)option];
}

void ClaimMysteryGift::claim_via_internet(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    const bool pick = METHOD == Method::InternetSelected;

    //  The gifts being given out now, in list order: row -> gift.
    std::vector<int> active;
    {
        const long long now = now_utc_yyyymmddhhmm();
        const std::vector<InternetGift>& gifts = INTERNET_GIFTS();
        for (size_t c = 0; c < gifts.size(); c++){
            if (gifts[c].end_utc == 0 || now <= gifts[c].end_utc){
                active.push_back((int)c);
            }
        }
    }

    pick_menu_option(env, context, 0);

    context.wait_for_all_requests();
    context.wait_for(500ms);
    int rows = 0;
    {
        VideoSnapshot snapshot = env.console.video().snapshot();
        while (rows < GIFT_ROW_COUNT && gift_row_filled(snapshot, rows)){
            rows++;
        }
    }
    env.log("Gift list: " + std::to_string(rows) + " rows, " + std::to_string(active.size()) + " known gifts given out now.");
    if (pick && rows < (int)active.size()){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            "The gift list has fewer rows (" + std::to_string(rows) + ") than the known gifts (" +
            std::to_string(active.size()) + "): the program's gift list is out of date.",
            env.console
        );
    }

    auto move_to_row = [&](int target){
        for (int c = 0; c < 20; c++){
            context.wait_for_all_requests();
            int row = gift_list_selected_row(env.console.video().snapshot());
            if (row == target){
                return;
            }
            if (row < 0){
                context.wait_for(300ms);
                continue;
            }
            pbf_press_dpad(context, row < target ? DPAD_DOWN : DPAD_UP, 100ms, 400ms);
        }
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            "Unable to move to gift row " + std::to_string(target + 1) + ".",
            env.console
        );
    };

    int claimed = 0;
    for (int row = 0; row < rows; row++){
        int gift = row < (int)active.size() ? active[(size_t)row] : -1;
        const std::string what = gift >= 0
            ? std::string(INTERNET_GIFTS()[(size_t)gift].slug)
            : "gift row " + std::to_string(row + 1);

        context.wait_for_all_requests();
        context.wait_for(300ms);
        if (!gift_row_unclaimed(env.console.video().snapshot(), row)){
            env.log("Row " + std::to_string(row + 1) + " (" + what + "): already claimed.");
            continue;
        }
        if (pick && !gift_wanted(gift)){
            env.log("Row " + std::to_string(row + 1) + " (" + what + "): not checked. Skipping.");
            continue;
        }

        move_to_row(row);
        env.log("Claiming row " + std::to_string(row + 1) + " (" + what + ").", COLOR_BLUE);
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Claiming " + what + ".");
        pbf_press_button(context, BUTTON_A, 100ms, 500ms);
        finish_receiving(env, context, true, what);
        claimed++;
    }
    env.log("Gifts claimed: " + std::to_string(claimed), COLOR_BLUE);
}

void ClaimMysteryGift::claim_via_code(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    //  Normalize: upper case, no spaces or dashes. The keyboard has no
    //  I / O / Z (they read as 1 / 0 / 2).
    std::string code;
    for (char ch : std::string(CODE)){
        if (ch == ' ' || ch == '-' || ch == '\t'){
            continue;
        }
        ch = (char)std::toupper((unsigned char)ch);
        if (ch == 'I'){ ch = '1'; }
        if (ch == 'O'){ ch = '0'; }
        if (ch == 'Z'){ ch = '2'; }
        int col = 0;
        int row = 0;
        if (!code_key_position(ch, col, row)){
            throw UserSetupError(env.logger(), std::string("Code: \"") + ch + "\" is not on the code keyboard.");
        }
        code += ch;
    }
    if (code.empty() || code.size() > 16){
        throw UserSetupError(env.logger(), "Code: write 1 to 16 letters / digits.");
    }
    env.log("Code: " + code);

    pick_menu_option(env, context, 1);

    auto read_cursor = [&](int& col, int& row){
        for (int c = 0; c < 10; c++){
            context.wait_for_all_requests();
            double x = 0;
            double y = 0;
            if (lza_find_keyboard_cursor(env.console.video().snapshot(), x, y)){
                cursor_to_key(x, y, col, row);
                return;
            }
            context.wait_for(150ms);
        }
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            "Unable to find the keyboard cursor.",
            env.console
        );
    };

    for (char ch : code){
        int target_col = 0;
        int target_row = 0;
        code_key_position(ch, target_col, target_row);

        //  Shortest way: up/down first, then left/right. Left/right go
        //  around (left of the first column is the right column), so take
        //  whichever way is shorter.
        int last_col = -1;
        int last_row = -1;
        bool try_horizontal = false;
        bool reached = false;
        for (int step = 0; step < 40 && !reached; step++){
            int col = 0;
            int row = 0;
            read_cursor(col, row);
            if (col == target_col && row == target_row){
                reached = true;
                break;
            }
            bool stuck = col == last_col && row == last_row;
            last_col = col;
            last_row = row;
            if (stuck){
                try_horizontal = !try_horizontal;
            }
            //  The right column is taller than a row: leave it sideways.
            bool vertical = row != target_row && col != KEY_COLUMNS - 1 && !try_horizontal;
            if (vertical){
                pbf_press_dpad(context, row < target_row ? DPAD_DOWN : DPAD_UP, 80ms, 220ms);
                continue;
            }
            int right = ((target_col - col) % KEY_COLUMNS + KEY_COLUMNS) % KEY_COLUMNS;
            int left  = ((col - target_col) % KEY_COLUMNS + KEY_COLUMNS) % KEY_COLUMNS;
            if (right == 0 && left == 0){
                pbf_press_dpad(context, row < target_row ? DPAD_DOWN : DPAD_UP, 80ms, 220ms);
                try_horizontal = false;
                continue;
            }
            pbf_press_dpad(context, right <= left ? DPAD_RIGHT : DPAD_LEFT, 80ms, 220ms);
            try_horizontal = false;
        }
        if (!reached){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                std::string("Unable to move the cursor to \"") + ch + "\".",
                env.console
            );
        }
        pbf_press_button(context, BUTTON_A, 80ms, 320ms);
    }

    //  確定 (+), then the connection and the gift.
    env.log("Code entered. Confirming.", COLOR_BLUE);
    send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Code entered: " + code);
    pbf_press_button(context, BUTTON_PLUS, 100ms, 1000ms);
    finish_receiving(env, context, false, "code " + code);
}

void ClaimMysteryGift::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    assert_16_9_720p_min(env.logger(), env.console);

    //  Connect the controller without pressing anything.
    require_player(env.console, context, BUTTON_NONE);

    if (START_IN == StartIn::Overworld){
        open_mystery_gift_menu(env, context);
    }

    if (METHOD == Method::Code){
        claim_via_code(env, context);
    }else{
        claim_via_internet(env, context);
    }

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}



}
}
}
