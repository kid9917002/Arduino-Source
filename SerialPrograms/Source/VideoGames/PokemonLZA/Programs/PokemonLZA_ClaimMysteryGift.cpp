/*  Claim Mystery Gift
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Placeholder: not implemented yet.
 */

#include "Pokemon/Pokemon_Strings.h"
#include "PokemonLZA_ClaimMysteryGift.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace Pokemon;

ClaimMysteryGift_Descriptor::ClaimMysteryGift_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonLZA:ClaimMysteryGift",
        STRING_POKEMON + " LZA", "Claim Mystery Gift",
        "",
        "Claim the Mystery Gift in LZA. (Not implemented yet.)",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

ClaimMysteryGift::ClaimMysteryGift(){}

void ClaimMysteryGift::program(SingleSwitchProgramEnvironment& env, ProControllerContext&){
    env.log("Claim Mystery Gift is not implemented yet.");
}



}
}
}
