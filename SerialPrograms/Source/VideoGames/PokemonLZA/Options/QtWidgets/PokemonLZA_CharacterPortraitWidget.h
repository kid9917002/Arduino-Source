/*  Character Portrait Widget
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_CharacterPortraitWidget_H
#define PokemonAutomation_PokemonLZA_CharacterPortraitWidget_H

#include <vector>
#include <QWidget>
#include "Common/Qt/Options/ConfigWidget.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_CharacterPortraitOption.h"

class QToolButton;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{



class CharacterPortraitWidget : public QWidget, public ConfigWidget{
public:
    using ParentState = CharacterPortraitOption;

public:
    ~CharacterPortraitWidget();
    CharacterPortraitWidget(QWidget& parent, CharacterPortraitOption& value);

    virtual void update_value() override;
    virtual void on_config_value_changed(void* object) override;

private:
    CharacterPortraitOption& m_value;
    std::vector<QToolButton*> m_buttons;
};



}
}
}
#endif
