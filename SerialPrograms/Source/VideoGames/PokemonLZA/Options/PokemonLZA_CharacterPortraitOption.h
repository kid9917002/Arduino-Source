/*  Character Portrait Option
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *      Pick one of the 8 portraits on the LOOKS screen by clicking it.
 *      Stored like the old "Character Look" dropdown (slugs top-1 ... bottom-4).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_CharacterPortraitOption_H
#define PokemonAutomation_PokemonLZA_CharacterPortraitOption_H

#include <cstddef>
#include "Common/Cpp/Options/EnumDropdownOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  PNG data of the 8 portraits, in screen order (top row left to right,
//  then bottom row).
struct CharacterPortraitImage{
    const unsigned char* data;
    size_t size;
};
extern const CharacterPortraitImage CHARACTER_PORTRAIT_IMAGES[8];


class CharacterPortraitOption : public UiState<CharacterPortraitOption, IntegerEnumDropdownOption>{
public:
    static constexpr size_t COUNT = 8;
    static constexpr size_t COLS = 4;

    CharacterPortraitOption(std::string label, size_t default_index);
};



}
}
}
#endif
