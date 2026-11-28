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
#include "CommonFramework/GlobalAutoPaths.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Filesystem/FileIO.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "CommonTools/StartupChecks/VideoResolutionCheck.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "VideoGames/PokemonLZA/PokemonLZA_Settings.h"
#include "PokemonLZA_GameEntry.h"
#include "PokemonLZA_MenuNavigation.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonLZA_ClaimMysteryGift.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;

//  In PokemonLZA_AutoStoryMultiLang.cpp: the system keyboard cursor.
bool lza_find_keyboard_cursor(const ImageViewRGB32& screen, double& x, double& y);


namespace{

const char* GIFT_LIST_FILE = "PokemonLZA/MysteryGift/internet_gifts.json";

//  Used when the json file is missing or not valid.
//  titles: the gift's title on the Get via Internet list, per game language
//  (OCR language codes: eng, jpn, chi_sim, chi_tra, kor, fra, deu, ita,
//  spa), to find the checked gifts on the list. Several per language are
//  fine. Checked on game screens (user's screenshot / game videos): all of
//  chi_sim, chi_tra, eng, jpn, fra, kor, spa (es-MX; es-ES all but the
//  Vivillon); ita / deu all but the Vivillon. Those Vivillon titles are
//  guesses from news text.
const char* DEFAULT_GIFT_LIST = R"JSON({
    "gifts": [
        { "id": "diancie", "name": "Diancite + Side Mission \"Shine Bright Like a Gemstone\"", "end_utc": "",
          "titles": {"eng": ["Shine Bright like a Gemstone"], "jpn": ["\u300c\u5b9d\u77f3\u306e\u3088\u3046\u306a\u8f1d\u304d\u3092\u300d", "\u5b9d\u77f3\u306e\u3088\u3046\u306a\u8f1d\u304d\u3092"], "fra": ["\u00ab Un scintillement sans pareil \u00bb", "Un scintillement sans pareil"], "ita": ["Brillante come una gemma"], "deu": ["Wie ein leuchtender Edelstein"], "spa": ["Brillante como una joya"], "kor": ["'\ubcf4\uc11d\uacfc\ub3c4 \uac19\uc740 \ubc18\uc9dd\uc784'", "\u2018\ubcf4\uc11d\uacfc\ub3c4 \uac19\uc740 \ubc18\uc9dd\uc784\u2019", "\ubcf4\uc11d\uacfc\ub3c4 \uac19\uc740 \ubc18\uc9dd\uc784"], "chi_sim": ["\u201c\u5982\u5b9d\u77f3\u822c\u7684\u5149\u8292\u201d", "\u5982\u5b9d\u77f3\u822c\u7684\u5149\u8292"], "chi_tra": ["\u300c\u5149\u8292\u8000\u773c\u5982\u5bf6\u77f3\u300d", "\u5149\u8292\u8000\u773c\u5982\u5bf6\u77f3"]} },
        { "id": "project-m", "name": "Mewtwonite X & Y + Side Mission \"Project M\"", "end_utc": "",
          "titles": {"eng": ["Project M"], "jpn": ["\u300c\u30d7\u30ed\u30b8\u30a7\u30af\u30c8\u30b3\u30fc\u30c9\uff1aM\u300d", "\u300c\u30d7\u30ed\u30b8\u30a7\u30af\u30c8\u30b3\u30fc\u30c9\uff1a\uff2d\u300d", "\u30d7\u30ed\u30b8\u30a7\u30af\u30c8\u30b3\u30fc\u30c9\uff1aM"], "fra": ["\u00ab Le projet M \u00bb", "Le projet M"], "ita": ["Nome in codice: Progetto M"], "deu": ["Projekt M"], "spa": ["Proyecto M"], "kor": ["'\ud504\ub85c\uc81d\ud2b8 M'", "\u2018\ud504\ub85c\uc81d\ud2b8 M\u2019", "\ud504\ub85c\uc81d\ud2b8 M"], "chi_sim": ["\u201cM\u9879\u76ee\u201d", "M\u9879\u76ee"], "chi_tra": ["\u300c\u5c08\u6848\u4ee3\u865f\uff1aM\u300d", "\u300c\u5c08\u6848\u4ee3\u865f\uff1a\uff2d\u300d", "\u5c08\u6848\u4ee3\u865f\uff1aM"]} },
        { "id": "hyperspace-rift", "name": "Special Hyperspace Rift", "end_utc": "",
          "titles": {"eng": ["Special distortion detected!"], "jpn": ["\u7279\u5225\u306a\u7570\u6b21\u5143\u306e\u3072\u305a\u307f\u3092\u691c\u77e5\uff01"], "fra": ["Faille extradimensionnelle particuli\u00e8re d\u00e9tect\u00e9e !"], "ita": ["Rilevata distorsione dimensionale speciale!"], "deu": ["Spezielle Dimensionsverzerrung geortet!"], "spa": ["\u00a1Se ha detectado una distorsi\u00f3n dimensional especial!", "\u00a1Se detect\u00f3 una distorsi\u00f3n dimensional especial!"], "kor": ["\ud2b9\ubcc4\ud55c \uc774\ucc28\uc6d0\uc758 \ub4a4\ud2c0\ub9bc\uc744 \uac10\uc9c0!"], "chi_sim": ["\u68c0\u6d4b\u5230\u7279\u6b8a\u7684\u5f02\u6b21\u5143\u626d\u6d1e\uff01"], "chi_tra": ["\u5075\u6e2c\u5230\u4e86\u7279\u6b8a\u7684\u7570\u6b21\u5143\u626d\u6d1e\uff01"]} },
        { "id": "spewpa", "name": "Spewpa", "end_utc": "2026-11-19 01:59",
          "titles": {"eng": ["Spewpa Gift"], "jpn": ["\u30b3\u30d5\u30fc\u30e9\u30a4\u3000\u30d7\u30ec\u30bc\u30f3\u30c8"], "fra": ["P\u00e9r\u00e9grain (Spewpa)"], "spa": ["Regalo: Spewpa"], "kor": ["\ubd84\ub5a0\ub3c4\ub9ac \uc120\ubb3c"], "chi_sim": ["\u7c89\u8776\u86f9 \u793c\u7269", "\u7c89\u8776\u86f9\u793c\u7269"], "chi_tra": ["\u7c89\u8776\u86f9\u79ae\u7269", "\u7c89\u8776\u86f9 \u79ae\u7269"]} },
        { "id": "vivillon-poke-ball", "name": "Poke Ball Pattern Vivillon", "end_utc": "2027-02-26 01:59",
          "titles": {"eng": ["Vivillon (Pok\u00e9 Ball Pattern) Gift"], "jpn": ["\u30dc\u30fc\u30eb\u306e\u3082\u3088\u3046\u306e\u3000\u30d3\u30d3\u30e8\u30f3"], "fra": ["Prismillon (Motif Pok\u00e9 Ball)"], "spa": ["Vivillon (Motivo Pok\u00e9bola)", "Vivillon (Motivo Pok\u00e9 Ball)"], "ita": ["Vivillon (Motivo Pok\u00e9 Ball)"], "deu": ["Vivillon (Pok\u00e9ball-Muster)", "Vivillon: Pok\u00e9ball-Muster"], "kor": ["\ube44\ube44\uc6a9(\ubcfc\uc758 \ubaa8\uc591) \uc120\ubb3c"], "chi_sim": ["\u7403\u7403\u82b1\u7eb9\u7684\u5f69\u7c89\u8776"], "chi_tra": ["\u7403\u7403\u82b1\u7d0b\u7684\u5f69\u7c89\u8776"]} }
    ]
})JSON";

//  "titles": { "chi_sim": ["..."], ... } -> language code -> titles.
std::map<std::string, std::vector<std::string>> read_titles(const JsonObject* json){
    std::map<std::string, std::vector<std::string>> titles;
    if (json == nullptr){
        return titles;
    }
    for (const auto& item : *json){
        const JsonArray* list = item.second.to_array();
        if (list == nullptr){
            continue;
        }
        for (const JsonValue& title : *list){
            const std::string* text = title.to_string();
            if (text != nullptr && !text->empty()){
                titles[item.first].push_back(*text);
            }
        }
    }
    return titles;
}

//  "2027-02-26 01:59" -> 202702260159. Empty: 0.
long long parse_end_utc(const std::string& text){
    std::string digits;
    for (char ch : text){
        if (ch >= '0' && ch <= '9'){
            digits += ch;
        }
    }
    if (digits.empty()){
        return 0;
    }
    while (digits.size() < 12){
        digits += digits.size() < 10 ? "0" : "9";
    }
    return std::atoll(digits.substr(0, 12).c_str());
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
    : PROFILE(
        "<b>User Profile Position:</b><br>"
        "The position, from left to right, of the Switch user profile to start the game with "
        "(the game is closed and started again with it).<br>"
        "If this is set to 0, Switch 1 defaults to the last-used profile, while Switch 2 defaults to the first profile (position 1).",
        LockMode::LOCK_WHILE_RUNNING,
        0, 0, 8 // default, min, max
    )
    , GIFTS(
        "Get via Internet",
        LockMode::LOCK_WHILE_RUNNING,
        GroupOption::EnableMode::DEFAULT_ENABLED
    )
    , LANGUAGE(
        "<b>Game Language:</b><br>"
        "Used to read the gift names on the list, so only the checked gifts are claimed. "
        "Not needed with Select all.",
        {
            Language::English, Language::Japanese, Language::Spanish, Language::French, Language::German,
            Language::Italian, Language::Korean, Language::ChineseSimplified, Language::ChineseTraditional,
        },
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , SELECT_ALL(
        "<b>Select all</b> (claim every unclaimed gift)",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , m_syncing_select_all(false)
    , OTHER_GIFTS(
        "<b>Other gifts:</b><br>Also claim the gifts below the ones above (new distributions).",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , CODE_GROUP(
        "Get with Code/Password (done after Get via Internet when both are on)",
        LockMode::LOCK_WHILE_RUNNING,
        GroupOption::EnableMode::DEFAULT_DISABLED
    )
    , CODE(
        "<b>Code / Password:</b><br>"
        "One code per line. "
        "Not case sensitive; spaces and dashes are ignored.",
        LockMode::LOCK_WHILE_RUNNING,
        "", "CODE1\nCODE2"
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    //  The gift list: the json file in Resources, or the built-in one.
    {
        std::string text;
        bool loaded = false;
        if (file_to_string(RESOURCE_PATH() + GIFT_LIST_FILE, text)){
            if (text.size() >= 3 && (uint8_t)text[0] == 0xef && (uint8_t)text[1] == 0xbb && (uint8_t)text[2] == 0xbf){
                text.erase(0, 3);
            }
            try{
                JsonValue json = parse_json(text);
                const JsonObject* root = json.to_object();
                const JsonArray* list = root == nullptr ? nullptr : root->get_array("gifts");
                if (list != nullptr){
                    for (const JsonValue& item : *list){
                        const JsonObject* gift = item.to_object();
                        if (gift == nullptr){
                            continue;
                        }
                        m_gifts.push_back({
                            gift->get_string_default("id"),
                            gift->get_string_default("name"),
                            parse_end_utc(gift->get_string_default("end_utc")),
                            read_titles(gift->get_object("titles")),
                        });
                    }
                    loaded = true;
                }
            }catch (Exception&){
                m_gifts.clear();
            }
        }
        if (!loaded){
            m_gifts.clear();
            JsonValue json = parse_json(DEFAULT_GIFT_LIST);
            for (const JsonValue& item : json.to_object_throw().get_array_throw("gifts")){
                const JsonObject& gift = item.to_object_throw();
                m_gifts.push_back({
                    gift.get_string_default("id"),
                    gift.get_string_default("name"),
                    parse_end_utc(gift.get_string_default("end_utc")),
                    read_titles(gift.get_object("titles")),
                });
            }
        }
    }

    //  The title reader: language -> gift id -> titles.
    {
        std::map<std::string, JsonObject> languages;
        for (const InternetGift& gift : m_gifts){
            for (const auto& item : gift.titles){
                JsonArray list;
                for (const std::string& title : item.second){
                    list.push_back(JsonValue(title));
                }
                languages[item.first][gift.id] = std::move(list);
            }
        }
        JsonObject dictionary;
        for (auto& item : languages){
            dictionary[item.first] = std::move(item.second);
        }
        try{
            m_title_reader.reset(new OCR::SmallDictionaryMatcher(dictionary));
        }catch (Exception&){
            m_title_reader.reset();
        }
    }

    GIFTS.add_option(LANGUAGE, "language");
    GIFTS.add_option(SELECT_ALL, "select-all");

    //  A checkbox for each gift that is not over yet.
    const long long now = now_utc_yyyymmddhhmm();
    for (const InternetGift& gift : m_gifts){
        if (gift.end_utc != 0 && now > gift.end_utc){
            m_gift_option_index.push_back(-1);
            continue;
        }
        std::string label = gift.name;
        if (gift.end_utc != 0){
            std::string end = std::to_string(gift.end_utc);
            label += " (until " + end.substr(0, 4) + "-" + end.substr(4, 2) + "-" + end.substr(6, 2) +
                " " + end.substr(8, 2) + ":" + end.substr(10, 2) + " UTC)";
        }
        m_gift_option_index.push_back((int)m_gift_options.size());
        m_gift_options.emplace_back(new BooleanCheckBoxOption(label, LockMode::LOCK_WHILE_RUNNING, true));
        GIFTS.add_option(*m_gift_options.back(), gift.id);
    }
    GIFTS.add_option(OTHER_GIFTS, "other");

    PA_ADD_OPTION(START_LOCATION);
    PA_ADD_OPTION(PROFILE);
    CODE_GROUP.add_option(CODE, "code");

    PA_ADD_OPTION(GIFTS);
    PA_ADD_OPTION(CODE_GROUP);
    PA_ADD_OPTION(NOTIFICATIONS);

    SELECT_ALL = all_gifts_checked();
    SELECT_ALL.add_listener(*this);
    for (const std::unique_ptr<BooleanCheckBoxOption>& option : m_gift_options){
        option->add_listener(*this);
    }
    OTHER_GIFTS.add_listener(*this);
}
ClaimMysteryGift::~ClaimMysteryGift(){
    OTHER_GIFTS.remove_listener(*this);
    for (const std::unique_ptr<BooleanCheckBoxOption>& option : m_gift_options){
        option->remove_listener(*this);
    }
    SELECT_ALL.remove_listener(*this);
}
bool ClaimMysteryGift::all_gifts_checked() const{
    for (const std::unique_ptr<BooleanCheckBoxOption>& option : m_gift_options){
        if (!*option){
            return false;
        }
    }
    return OTHER_GIFTS;
}
bool ClaimMysteryGift::any_gift_checked() const{
    for (const std::unique_ptr<BooleanCheckBoxOption>& option : m_gift_options){
        if (*option){
            return true;
        }
    }
    return OTHER_GIFTS;
}
void ClaimMysteryGift::on_config_value_changed(void* object){
    if (m_syncing_select_all.load()){
        return;
    }
    m_syncing_select_all.store(true);
    if (object == static_cast<void*>(static_cast<BooleanCheckBoxCell*>(&SELECT_ALL))){
        const bool checked = SELECT_ALL;
        for (const std::unique_ptr<BooleanCheckBoxOption>& option : m_gift_options){
            *option = checked;
        }
        OTHER_GIFTS = checked;
    }else{
        SELECT_ALL = all_gifts_checked();
    }
    m_syncing_select_all.store(false);
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
//  The selected row is white from x = 0.206 to 0.794, the other two are
//  dark. Returns the selected index, or -1 when the menu is not on screen.
//  The other rows must be dark too: a white card while receiving a gift
//  (2026-11-27 23:10, after the Diancie gift) looked like a selected row.
int mystery_gift_menu_row(const ImageViewRGB32& screen){
    const double ROW_Y[3] = {0.370, 0.500, 0.630};
    //  Right part of the rows only: the left end has the green arrow and
    //  the icon, and the text length depends on the language.
    double left[3];
    double right[3];
    for (int c = 0; c < 3; c++){
        double y = ROW_Y[c];
        left[c] = white_ratio(screen, ImageFloatBox(0.745, y - 0.025, 0.020, 0.050));
        right[c] = white_ratio(screen, ImageFloatBox(0.772, y - 0.025, 0.015, 0.050));
    }
    for (int c = 0; c < 3; c++){
        if (left[c] < 0.80 || right[c] < 0.80){
            continue;
        }
        double y = ROW_Y[c];
        //  The row is white across (top margin, above the text) and ends at
        //  x = 0.794. The "stop receiving?" Yes / No box on the gift list
        //  (x = 0.735 to 0.895) sits where row 2 is: it is white past
        //  0.794 and dark at x = 0.3 (2026-11-28 11:43).
        if (white_ratio(screen, ImageFloatBox(0.300, y - 0.048, 0.400, 0.010)) < 0.80 ||
            white_ratio(screen, ImageFloatBox(0.805, y - 0.025, 0.030, 0.050)) > 0.20
        ){
            continue;
        }
        bool others_dark = true;
        for (int i = 0; i < 3; i++){
            if (i != c && (left[i] > 0.10 || right[i] > 0.10)){
                others_dark = false;
            }
        }
        return others_dark ? c : -1;
    }
    return -1;
}

//  The Get via Internet list: up to 7 rows from y = 0.272, 0.0778 apart,
//  x = 0.292 to 0.708. The selected row is white. Claimed gifts leave the
//  list.
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
//  The "Choose the gift to receive." message box under the gift list. It
//  takes A before the list moves at all, so it is closed first (A only
//  while it is up: an A on the list itself would claim the selected row).
//  Dark navy box from x = 0.224 to 0.776, y = 0.774 to 0.956; without it
//  that band is bright cyan. User's screenshots: 0.94 with it, 0.17
//  without.
bool gift_list_message_open(const ImageViewRGB32& screen){
    auto navy = [](int r, int g, int b){
        return r < 50 && g < 60 && b >= 60 && b <= 110 && b - r >= 35;
    };
    return pixel_ratio(screen, ImageFloatBox(0.26, 0.80, 0.48, 0.13), navy) >= 0.70 &&
        pixel_ratio(screen, ImageFloatBox(0.20, 0.85, 0.015, 0.08), navy) < 0.20;
}
//  The green arrow left of the selected row: shown only while the list
//  takes input (not while the message box is up). x = 0.283 to 0.305.
//  User's screenshots: 0.48 with it, 0.00 without.
bool gift_list_arrow(const ImageViewRGB32& screen, int row){
    double top = gift_row_top(row);
    return pixel_ratio(screen, ImageFloatBox(0.284, top + 0.010, 0.016, 0.045), [](int r, int g, int b){
        return g >= 180 && r >= 120 && b <= 110 && g - b >= 90;
    }) >= 0.20;
}
//  B on the gift list asks "Stop receiving?" with Yes / No at the right
//  (Yes selected by default: white, green arrow left of it). True while
//  Yes is selected. User's screenshot: white 1.00 / arrow 0.51 with it,
//  0.00 / 0.00 on the other screens.
bool quit_receiving_yes_selected(const ImageViewRGB32& screen){
    return white_ratio(screen, ImageFloatBox(0.780, 0.610, 0.100, 0.050)) >= 0.80 &&
        pixel_ratio(screen, ImageFloatBox(0.730, 0.615, 0.015, 0.040), [](int r, int g, int b){
            return g >= 180 && r >= 120 && b <= 110 && g - b >= 90;
        }) >= 0.20;
}
bool gift_row_filled(const ImageViewRGB32& screen, int row){
    double top = gift_row_top(row);
    ImageFloatBox box(0.300, top + 0.012, 0.350, 0.042);
    return white_ratio(screen, box) >= 0.55 || mean_brightness(screen, box) >= 70;
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



//  Wait for the Mystery Gift menu, checking every 200ms, up to timeout.
//  Returns as soon as it is seen, so a press is followed by only as much
//  wait as the screen change needs.
static bool wait_for_mystery_gift_menu(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    Milliseconds timeout
){
    WallClock start = current_time();
    while (true){
        context.wait_for_all_requests();
        if (mystery_gift_menu_row(env.console.video().snapshot()) >= 0){
            return true;
        }
        if (current_time() - start >= timeout){
            return false;
        }
        context.wait_for(200ms);
    }
}

void ClaimMysteryGift::open_mystery_gift_menu(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    for (int attempt = 0; attempt < 3; attempt++){
        env.log("Opening the Mystery Gift menu.");
        //  X: menu, waited for by detection (same as the other LZA programs).
        overworld_to_main_menu(env.console, context);
        //  Up: from the first item to the last one (Online Play).
        pbf_press_dpad(context, DPAD_UP, 100ms, 300ms);
        //  Online Play opens its menu right away (no connection yet).
        pbf_press_button(context, BUTTON_A, 100ms, 900ms);
        //  Online Play: left, down -> Mystery Gift.
        pbf_press_dpad(context, DPAD_LEFT, 100ms, 300ms);
        pbf_press_dpad(context, DPAD_DOWN, 100ms, 300ms);
        pbf_press_button(context, BUTTON_A, 100ms, 100ms);

        if (wait_for_mystery_gift_menu(env, context, 20000ms)){
            env.log("On the Mystery Gift menu.", COLOR_BLUE);
            return;
        }
        env.log("No Mystery Gift menu. Backing out and trying again.", COLOR_RED);
        for (int c = 0; c < 4; c++){
            pbf_press_button(context, BUTTON_B, 100ms, 700ms);
        }
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "Unable to open the Mystery Gift menu.",
        env.console
    );
}

void ClaimMysteryGift::pick_menu_option(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int option){
    //  Move to the option with the d-pad (only the gift list needs the left
    //  stick, checked by the user 2026-11-27). If a press does not move the
    //  cursor, switch to the stick.
    bool use_stick = false;
    int last_row = -1;
    for (int c = 0; c < 10; c++){
        context.wait_for_all_requests();
        int row = mystery_gift_menu_row(env.console.video().snapshot());
        if (row < 0){
            //  The cursor may still be moving, or a message is closing.
            env.log("Mystery Gift menu not seen. Waiting for it.", COLOR_ORANGE);
            if (!wait_for_mystery_gift_menu(env, context, 5000ms)){
                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::NO_ERROR_REPORT,
                    "Not on the Mystery Gift menu.",
                    env.console
                );
            }
            row = mystery_gift_menu_row(env.console.video().snapshot());
        }
        if (row == option){
            break;
        }
        if (row == last_row){
            use_stick = !use_stick;
            env.log(
                std::string("Mystery Gift menu: the cursor did not move. Trying the ") +
                (use_stick ? "left stick." : "d-pad."),
                COLOR_RED
            );
        }
        last_row = row;
        if (use_stick){
            pbf_move_left_joystick(context, {0, row < option ? -1.0 : +1.0}, 150ms, 400ms);
        }else{
            pbf_press_dpad(context, row < option ? DPAD_DOWN : DPAD_UP, 150ms, 400ms);
        }
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
        //  Internet: back on the list, or on the Mystery Gift menu when that
        //  was the last gift (the game says there is no gift and leaves).
        bool back = mystery_gift_menu_row(snapshot) >= 0 ||
            (back_to_list && gift_list_selected_row(snapshot) >= 0);
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

int ClaimMysteryGift::read_selected_gift(
    SingleSwitchProgramEnvironment& env, const ImageViewRGB32& screen, int row
) const{
    if (!m_title_reader || !m_title_reader->languages()[LANGUAGE]){
        return -1;
    }
    //  The title: dark text on the white (selected) row, left of the red dot.
    ImageViewRGB32 image = extract_box_reference(
        screen, ImageFloatBox(0.300, gift_row_top(row) + 0.010, 0.390, 0.048)
    );
    OCR::StringMatchResult result = m_title_reader->match_substring_from_image_multifiltered(
        &env.logger(), LANGUAGE, image, OCR::BLACK_TEXT_FILTERS(), -1.40, 0.50
    );
    if (result.results.empty()){
        return -1;
    }
    const std::string& id = result.results.begin()->second.token;
    for (size_t c = 0; c < m_gifts.size(); c++){
        if (m_gifts[c].id == id){
            return (int)c;
        }
    }
    return -1;
}

void ClaimMysteryGift::claim_via_internet(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    //  Everything checked: claim every unclaimed row.
    const bool pick = !all_gifts_checked();

    pick_menu_option(env, context, 0);

    //  Claimed gifts are not on the list (they leave it as soon as they are
    //  claimed), so a row is not always the same gift: the title of each
    //  row is read (OCR) unless every gift is wanted.
    auto count_rows = [&](){
        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();
        int rows = 0;
        while (rows < GIFT_ROW_COUNT && gift_row_filled(snapshot, rows)){
            rows++;
        }
        return rows;
    };
    context.wait_for_all_requests();
    context.wait_for(500ms);
    env.log("Gift list: " + std::to_string(count_rows()) + " rows.");

    //  Close the message box under the list. The list takes input once the
    //  green arrow is next to the selected row. A only when there is no
    //  arrow and the message box is up: an A with the arrow showing would
    //  claim the selected row.
    auto close_list_message = [&](){
        for (int c = 0; c < 30; c++){
            context.wait_for_all_requests();
            VideoSnapshot snapshot = env.console.video().snapshot();
            int row = gift_list_selected_row(snapshot);
            if (row >= 0 && gift_list_arrow(snapshot, row)){
                return;
            }
            if (gift_list_message_open(snapshot)){
                env.log("Gift list: closing the message box (A).");
                pbf_press_button(context, BUTTON_A, 100ms, 400ms);
            }else{
                context.wait_for(200ms);
            }
        }
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            "Unable to close the message box under the gift list.",
            env.console
        );
    };

    //  The d-pad moves the list once the message box is closed (2026-11-27:
    //  20 x down with the box up did nothing). If a press does not move the
    //  cursor, switch between the d-pad and the left stick, and log it.
    bool use_stick = false;
    auto move_to_row = [&](int target){
        close_list_message();
        int last_row = -1;
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
            if (row == last_row){
                use_stick = !use_stick;
                env.log(
                    std::string("Gift list: the cursor did not move. Trying the ") +
                    (use_stick ? "left stick." : "d-pad."),
                    COLOR_RED
                );
            }else if (last_row >= 0){
                env.log(std::string("Gift list: the ") + (use_stick ? "left stick" : "d-pad") + " moves the cursor.");
            }
            last_row = row;
            const bool down = row < target;
            if (use_stick){
                pbf_move_left_joystick(context, {0, down ? -1.0 : +1.0}, 150ms, 400ms);
            }else{
                pbf_press_dpad(context, down ? DPAD_DOWN : DPAD_UP, 150ms, 400ms);
            }
        }
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            "Unable to move to gift row " + std::to_string(target + 1) + ".",
            env.console
        );
    };

    //  Top to bottom. A claimed row leaves the list and the rows below move
    //  up, so the same row is looked at again after a claim.
    int claimed = 0;
    int row = 0;
    for (int guard = 0; guard < 4 * GIFT_ROW_COUNT; guard++){
        if (mystery_gift_menu_row(env.console.video().snapshot()) >= 0){
            env.log("Gift list: no gift left (back on the Mystery Gift menu).");
            break;
        }
        if (row >= count_rows()){
            break;
        }
        move_to_row(row);
        context.wait_for_all_requests();
        int gift = -1;
        std::string what = "list row " + std::to_string(row + 1);
        if (pick || LANGUAGE != Language::None){
            gift = read_selected_gift(env, env.console.video().snapshot(), row);
            what += gift >= 0
                ? " (" + m_gifts[(size_t)gift].id + ")"
                : " (not a known gift)";
        }
        if (pick && !gift_wanted(gift)){
            env.log(what + ": not checked. Skipping.");
            row++;
            continue;
        }
        env.log("Claiming " + what + ".", COLOR_BLUE);
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Claiming " + what + ".");
        pbf_press_button(context, BUTTON_A, 100ms, 500ms);
        finish_receiving(env, context, true, what);
        claimed++;
    }
    env.log("Gifts claimed: " + std::to_string(claimed), COLOR_BLUE);
}

std::vector<std::string> ClaimMysteryGift::read_codes(SingleSwitchProgramEnvironment& env) const{
    //  Several codes: one per line (commas and semicolons also work).
    //  Each: upper case, no spaces or dashes. The keyboard has no I / O / Z
    //  (they read as 1 / 0 / 2).
    std::vector<std::string> codes;
    {
        std::string code;
        auto flush = [&](){
            if (code.empty()){
                return;
            }
            if (code.size() > 16){
                throw UserSetupError(env.logger(), "Code \"" + code + "\": at most 16 letters / digits.");
            }
            codes.push_back(code);
            code.clear();
        };
        for (char ch : std::string(CODE)){
            if (ch == ',' || ch == ';' || ch == '\n' || ch == '\r'){
                flush();
                continue;
            }
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
        flush();
    }
    if (codes.empty()){
        throw UserSetupError(env.logger(), "Code / Password: write at least one code.");
    }
    {
        std::string all;
        for (const std::string& code : codes){
            all += (all.empty() ? "" : ", ") + code;
        }
        env.log("Codes (" + std::to_string(codes.size()) + "): " + all);
    }
    return codes;
}
void ClaimMysteryGift::claim_via_code(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const std::vector<std::string>& codes
){

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

    auto type_key = [&](char ch){
        int target_col = 0;
        int target_row = 0;
        code_key_position(ch, target_col, target_row);

        //  Shortest way: up/down first, then left/right. Left/right go
        //  around (left of the first column is the right column), so take
        //  whichever way is shorter.
        int last_col = -1;
        int last_row = -1;
        bool try_horizontal = false;
        for (int step = 0; step < 40; step++){
            int col = 0;
            int row = 0;
            read_cursor(col, row);
            if (col == target_col && row == target_row){
                pbf_press_button(context, BUTTON_A, 80ms, 320ms);
                return;
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
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            std::string("Unable to move the cursor to \"") + ch + "\".",
            env.console
        );
    };

    int received = 0;
    std::string failed;
    for (size_t index = 0; index < codes.size(); index++){
        const std::string& code = codes[index];
        const std::string what = "code " + std::to_string(index + 1) + "/" + std::to_string(codes.size()) + " " + code;
        env.log("Entering " + what + ".", COLOR_BLUE);
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE, "Entering " + what + ".");

        pick_menu_option(env, context, 1);
        for (char ch : code){
            type_key(ch);
        }

        //  確定 (+), then the connection and the gift, back to the Mystery
        //  Gift menu. A wrong / used code puts the keyboard back up: leave
        //  it with X and go on with the next code.
        pbf_press_button(context, BUTTON_PLUS, 100ms, 2000ms);
        WallClock start = current_time();
        int menu_seen = 0;
        int keyboard_seen = 0;
        bool ok = false;
        bool rejected = false;
        while (current_time() - start < std::chrono::seconds(180)){
            context.wait_for_all_requests();
            VideoSnapshot snapshot = env.console.video().snapshot();
            if (mystery_gift_menu_row(snapshot) >= 0){
                keyboard_seen = 0;
                if (++menu_seen >= 2){
                    ok = true;
                    break;
                }
                context.wait_for(700ms);
                continue;
            }
            menu_seen = 0;
            double x = 0;
            double y = 0;
            if (lza_find_keyboard_cursor(snapshot, x, y)){
                if (++keyboard_seen >= 3){
                    rejected = true;
                    break;
                }
                context.wait_for(700ms);
                continue;
            }
            keyboard_seen = 0;
            pbf_press_button(context, BUTTON_A, 100ms, 1400ms);
        }
        if (rejected){
            env.log("Code " + code + " was not accepted (wrong, used or expired). Leaving the keyboard.", COLOR_RED);
            failed += (failed.empty() ? "" : ", ") + code;
            for (int c = 0; c < 10; c++){
                pbf_press_button(context, BUTTON_X, 100ms, 100ms);
                if (wait_for_mystery_gift_menu(env, context, 1500ms)){
                    break;
                }
                pbf_press_button(context, BUTTON_B, 100ms, 100ms);
                if (wait_for_mystery_gift_menu(env, context, 1500ms)){
                    break;
                }
            }
            continue;
        }
        if (!ok){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "Did not get back from " + what + " within 3 minutes.",
                env.console
            );
        }
        env.log("Received: " + what, COLOR_BLUE);
        received++;
    }
    env.log(
        "Codes received: " + std::to_string(received) + "/" + std::to_string(codes.size()) +
        (failed.empty() ? std::string() : ". Not accepted: " + failed),
        failed.empty() ? COLOR_BLUE : COLOR_RED
    );
}

void ClaimMysteryGift::back_to_mystery_gift_menu(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    //  B on the list asks "Stop receiving?" (Yes / No): A on Yes. Then the
    //  Mystery Gift menu.
    WallClock start = current_time();
    WallClock last_press = start - std::chrono::seconds(10);
    while (current_time() - start < std::chrono::seconds(30)){
        context.wait_for_all_requests();
        VideoSnapshot snapshot = env.console.video().snapshot();
        if (mystery_gift_menu_row(snapshot) >= 0){
            env.log("Back on the Mystery Gift menu.", COLOR_BLUE);
            return;
        }
        if (quit_receiving_yes_selected(snapshot)){
            env.log("Stop receiving? -> Yes (A).");
            pbf_press_button(context, BUTTON_A, 100ms, 100ms);
            last_press = current_time();
            context.wait_for(400ms);
            continue;
        }
        if (current_time() - last_press >= std::chrono::milliseconds(1500)){
            pbf_press_button(context, BUTTON_B, 100ms, 100ms);
            last_press = current_time();
        }
        context.wait_for(200ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::NO_ERROR_REPORT,
        "Unable to get back to the Mystery Gift menu from the Get via Internet list.",
        env.console
    );
}

void ClaimMysteryGift::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    assert_16_9_720p_min(env.logger(), env.console);

    //  Check the settings before pressing anything.
    if (!GIFTS.enabled() && !CODE_GROUP.enabled()){
        throw UserSetupError(env.logger(), "Turn on Get via Internet, Get with Code/Password, or both.");
    }
    if (GIFTS.enabled() && !all_gifts_checked()){
        if (LANGUAGE == Language::None){
            throw UserSetupError(env.logger(), "Get via Internet: set Game Language to find the checked gifts on the list (not needed with Select all).");
        }
        if (!m_title_reader || !m_title_reader->languages()[LANGUAGE]){
            throw UserSetupError(env.logger(), "Get via Internet: no gift names for this Game Language yet (internet_gifts.json, \"titles\"). Use Select all, or add the names.");
        }
    }
    if (GIFTS.enabled() && !any_gift_checked()){
        throw UserSetupError(env.logger(), "Get via Internet: check at least one gift (or turn Get via Internet off).");
    }
    std::vector<std::string> codes;
    if (CODE_GROUP.enabled()){
        codes = read_codes(env);
    }

    //  Connect the controller, then Home -> close the game -> start it with
    //  the chosen profile (same as Auto Story).
    if (START_LOCATION.start_in_grip_menu()){
        grip_menu_connect_go_home(context);
    }else{
        go_home(env.console, context);
    }
    env.log("Starting the game with user profile " + std::to_string((int)PROFILE) + ".", COLOR_BLUE);
    env.console.overlay().add_log("Start game: profile " + std::to_string((int)PROFILE));
    close_game_from_home(env.console, context);
    start_game_from_home(
        env.console, context,
        ConsoleSettings::instance().TOLERATE_SYSTEM_UPDATE_MENU_FAST,
        0, PROFILE
    );
    {
        bool ok = openedgame_to_gamemenu(env.console, context, GameSettings::instance().START_GAME_WAIT);
        ok = ok && gamemenu_to_ingame(env.console, context, GameSettings::instance().ENTER_GAME_WAIT0);
        if (!ok){
            OperationFailedExceptionWithScreenshot::fire(
                ErrorReportMode::NO_ERROR_REPORT,
                "Unable to start the game with user profile " + std::to_string((int)PROFILE) + ".",
                env.console
            );
        }
    }
    env.log("In the game.", COLOR_BLUE);
    context.wait_for_all_requests();
    context.wait_for(1000ms);
    require_player(env.console, context, BUTTON_NONE);

    //  From the overworld (all menus closed).
    open_mystery_gift_menu(env, context);

    if (GIFTS.enabled()){
        claim_via_internet(env, context);
        if (CODE_GROUP.enabled()){
            back_to_mystery_gift_menu(env, context);
        }
    }
    if (CODE_GROUP.enabled()){
        claim_via_code(env, context, codes);
    }

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}



}
}
}
