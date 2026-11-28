/*  Start Mode Option
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *      Auto Story: how the run starts (new game / load the save at a step /
 *      continue from the current screen). Picked by clicking one of three
 *      cards; stored like an enum dropdown.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_StartModeOption_H
#define PokemonAutomation_PokemonLZA_StartModeOption_H

#include <cstddef>
#include "Common/Cpp/Options/EnumDropdownOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


class StartModeOption : public UiState<StartModeOption, IntegerEnumDropdownOption>{
public:
    static constexpr size_t NEW_GAME = 0;
    static constexpr size_t LOAD_SAVE = 1;
    static constexpr size_t CONTINUE = 2;
    static constexpr size_t COUNT = 3;

    StartModeOption(std::string label, size_t default_index);

    bool new_game() const{ return current_value() == NEW_GAME; }
    bool load_save() const{ return current_value() == LOAD_SAVE; }
    bool continue_from_screen() const{ return current_value() == CONTINUE; }
};



}
}
}
#endif
