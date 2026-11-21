/*  Auto Story
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Runs the opening of the story from a fresh save:
 *    ZA-00  create character (vision: pick the portrait)
 *    ZA-01  train station, chase Pancham
 *    ZA-02  chase Pancham, pick starter (dropdown), first battle (vision)
 *  Stops on the post-battle dialog.
 *
 *  The button sequences between vision checkpoints are Turbo Macro JSON
 *  files read from a user-selected folder, so routes can be re-recorded
 *  without rebuilding the program.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStory_H
#define PokemonAutomation_PokemonLZA_AutoStory_H

#include "Common/Cpp/Options/StringOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_BattleAIOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


class AutoStory_Descriptor : public SingleSwitchProgramDescriptor{
public:
    AutoStory_Descriptor();

    class Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};


class AutoStory : public SingleSwitchProgramInstance{
public:
    using Descriptor = AutoStory_Descriptor;
    AutoStory();

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

    //  From the pre-battle dialog, fight the first battle and stop on the
    //  post-battle dialog.
    void run_first_battle(SingleSwitchProgramEnvironment& env, ProControllerContext& context);

    std::string starter_macro_filename() const;


private:
    StringOption MACRO_FOLDER;
    EnumDropdownOption<StartPoint> START_POINT;
    EnumDropdownOption<CharacterLook> CHARACTER;
    EnumDropdownOption<Starter> STARTER;
    BattleAIOption BATTLE_AI;

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};



}
}
}
#endif
