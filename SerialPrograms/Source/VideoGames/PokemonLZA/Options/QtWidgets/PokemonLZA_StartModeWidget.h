/*  Start Mode Widget
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_StartModeWidget_H
#define PokemonAutomation_PokemonLZA_StartModeWidget_H

#include <vector>
#include <QWidget>
#include "Common/Qt/Options/ConfigWidget.h"
#include "VideoGames/PokemonLZA/Options/PokemonLZA_StartModeOption.h"

class QLabel;
class QFrame;

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{



class StartModeWidget : public QWidget, public ConfigWidget{
public:
    using ParentState = StartModeOption;

public:
    ~StartModeWidget();
    StartModeWidget(QWidget& parent, StartModeOption& value);

    virtual void update_value() override;
    virtual void on_config_value_changed(void* object) override;

private:
    StartModeOption& m_value;
    QLabel* m_description;
    std::vector<QFrame*> m_cards;
};



}
}
}
#endif
