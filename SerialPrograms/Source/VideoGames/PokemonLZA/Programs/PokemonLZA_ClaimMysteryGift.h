/*  Claim Mystery Gift
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  From the overworld: X menu -> Online Play -> Mystery Gift, then
 *  - Get via Internet: claim every unclaimed gift, or only the gifts picked, or
 *  - Get with Code/Password: type the code on the system keyboard.
 */

#ifndef PokemonAutomation_PokemonLZA_ClaimMysteryGift_H
#define PokemonAutomation_PokemonLZA_ClaimMysteryGift_H

#include <vector>
#include <atomic>
#include <memory>
#include "Common/Cpp/Options/TextEditOption.h"
#include "Common/Cpp/Options/GroupOption.h"
#include "Common/Cpp/Options/BooleanCheckBoxOption.h"
#include "Common/Cpp/Options/EnumDropdownOption.h"
#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "NintendoSwitch/Options/NintendoSwitch_StartInGripMenuOption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{



class ClaimMysteryGift_Descriptor : public SingleSwitchProgramDescriptor{
public:
    ClaimMysteryGift_Descriptor();
};

class ClaimMysteryGift : public SingleSwitchProgramInstance, public ConfigOption::Listener{
public:
    using Descriptor = ClaimMysteryGift_Descriptor;
    ~ClaimMysteryGift();
    ClaimMysteryGift();

    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

private:
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
    bool gift_wanted(int gift) const;

    //  "Select all" checks / unchecks every gift; it shows checked only
    //  while every gift is checked.
    virtual void on_config_value_changed(void* object) override;
    bool all_gifts_checked() const;

private:
    //  Gifts of "Get via Internet", in list order, from
    //  Resources/PokemonLZA/MysteryGift/internet_gifts.json.
    struct InternetGift{
        std::string id;
        std::string name;
        long long end_utc;  //  YYYYMMDDhhmm (UTC), 0: no end date
    };
    std::vector<InternetGift> m_gifts;

    StartInGripOrGameOption START_LOCATION;
    EnumDropdownOption<Method> METHOD;
    GroupOption GIFTS;
    BooleanCheckBoxOption SELECT_ALL;
    std::atomic<bool> m_syncing_select_all;
    //  One per gift still being given out (see the gift list in the .cpp).
    std::vector<std::unique_ptr<BooleanCheckBoxOption>> m_gift_options;
    std::vector<int> m_gift_option_index;  //  gift index -> m_gift_options, or -1
    BooleanCheckBoxOption OTHER_GIFTS;
    TextEditOption CODE;

    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};





}
}
}
#endif
