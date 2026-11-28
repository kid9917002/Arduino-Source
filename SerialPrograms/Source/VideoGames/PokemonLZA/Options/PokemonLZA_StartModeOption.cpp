/*  Start Mode Option
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_StartModeOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{



StartModeOption::StartModeOption(std::string label, size_t default_index)
    : UiState<StartModeOption, IntegerEnumDropdownOption>(
        std::move(label),
        ::PokemonAutomation::IntegerEnumDropdownDatabase{
            {NEW_GAME,  "new-game",  "New game"},
            {LOAD_SAVE, "load-save", "Load save at a step"},
            {CONTINUE,  "continue",  "Continue from screen"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        default_index
    )
{}



}
}
}
