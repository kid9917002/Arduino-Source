/*  Claim Mystery Gift
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  From the overworld: X menu -> Online Play -> Mystery Gift, then
 *  - Get via Internet: claim every unclaimed gift, or only the rows picked, or
 *  - Get with Code/Password: type the code on the system keyboard.
 */

#ifndef PokemonAutomation_PokemonLZA_ClaimMysteryGift_H
#define PokemonAutomation_PokemonLZA_ClaimMysteryGift_H

#include <vector>
#include "Common/Cpp/Options/StringOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{



class ClaimMysteryGift_Descriptor : public SingleSwitchProgramDescriptor{
public:
    ClaimMysteryGift_Descriptor();
};

class ClaimMysteryGift : public SingleSwitchProgramInstance{
public:
    using Descriptor = ClaimMysteryGift_Descriptor;
    ClaimMysteryGift();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
    enum class StartIn{
        Overworld,
        MysteryGiftMenu,
    };
    enum class Method{
        InternetAll,
        InternetSelected,
        Code,
    };

    void open_mystery_gift_menu(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    //  Select a row of the Mystery Gift menu (0: Internet, 1: Code) and
    //  get through the "save" question and the connection.
    void pick_menu_option(SingleSwitchProgramEnvironment& env, ProControllerContext& context, int option);
    void claim_via_internet(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    void claim_via_code(SingleSwitchProgramEnvironment& env, ProControllerContext& context);
    //  Press A through the gift animation and result until the given screen
    //  is back.
    void finish_receiving(
        SingleSwitchProgramEnvironment& env, ProControllerContext& context,
        bool back_to_list, const std::string& what
    );
    std::vector<int> selected_rows(SingleSwitchProgramEnvironment& env) const;

private:
    EnumDropdownOption<StartIn> START_IN;
    EnumDropdownOption<Method> METHOD;
    StringOption GIFT_ROWS;
    StringOption CODE;

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};





}
}
}
#endif
