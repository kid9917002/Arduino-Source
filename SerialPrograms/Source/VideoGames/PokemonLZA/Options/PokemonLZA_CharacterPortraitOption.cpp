/*  Character Portrait Option
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_CharacterPortraitOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{



CharacterPortraitOption::CharacterPortraitOption(std::string label, size_t default_index)
    : UiState<CharacterPortraitOption, IntegerEnumDropdownOption>(
        std::move(label),
        ::PokemonAutomation::IntegerEnumDropdownDatabase{
            {0, "top-1",    "Top row, 1st"},
            {1, "top-2",    "Top row, 2nd"},
            {2, "top-3",    "Top row, 3rd"},
            {3, "top-4",    "Top row, 4th"},
            {4, "bottom-1", "Bottom row, 1st"},
            {5, "bottom-2", "Bottom row, 2nd"},
            {6, "bottom-3", "Bottom row, 3rd"},
            {7, "bottom-4", "Bottom row, 4th"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        default_index
    )
{}



}
}
}
