/*  Claim Mystery Gift
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Placeholder: not implemented yet.
 */

#ifndef PokemonAutomation_PokemonLZA_ClaimMysteryGift_H
#define PokemonAutomation_PokemonLZA_ClaimMysteryGift_H

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
};





}
}
}
#endif
