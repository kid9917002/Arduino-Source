/*  Start Mode Widget
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *      Three cards side by side; click one to pick it.
 *
 */

#include <functional>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QMouseEvent>
#include "PokemonLZA_StartModeWidget.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

class ClickableCard : public QFrame{
public:
    ClickableCard(QWidget* parent, std::function<void()> on_click)
        : QFrame(parent)
        , m_on_click(std::move(on_click))
    {
        setObjectName("StartModeCard");
        setCursor(Qt::PointingHandCursor);
    }

protected:
    virtual void mousePressEvent(QMouseEvent* event) override{
        if (event->button() == Qt::LeftButton){
            m_on_click();
        }
        QFrame::mousePressEvent(event);
    }

private:
    std::function<void()> m_on_click;
};


const char* YES = "<font color=\"#2e9d50\">";
const char* NO  = "<font color=\"#8a8a8a\">";

std::string row(const char* key, const char* color, const char* value){
    return std::string("<font color=\"#8a8a8a\">") + key + ":</font> " + color + value + "</font><br>";
}

std::string card_html(size_t index){
    switch (index){
    case StartModeOption::NEW_GAME:
        return std::string("<b>New game</b><br>")
            + row("Game running", NO, "not needed")
            + row("Restarts the game", YES, "yes")
            + row("Start From", NO, "not used (ZA-00)")
            + row("Save data", YES, "must have none")
            + row("Start steps", "<font>", "ZA-00");
    case StartModeOption::LOAD_SAVE:
        return std::string("<b>Load save at a step</b><br>")
            + row("Game running", NO, "not needed")
            + row("Restarts the game", YES, "yes (loads the save)")
            + row("Start From", YES, "required")
            + row("Save data", YES, "required")
            + row("Start steps", "<font>", "overworld only");
    default:
        return std::string("<b>Continue from screen</b><br>")
            + row("Game running", YES, "yes, at the start screen")
            + row("Restarts the game", NO, "no")
            + row("Start From", YES, "required")
            + row("Save data", NO, "doesn't matter")
            + row("Start steps", "<font>", "all");
    }
}

const char* description(size_t index){
    switch (index){
    case StartModeOption::NEW_GAME:
        return "Close the game, start it with the User Profile Position below and play the whole story "
               "from a new save (ZA-00). The profile must have no save data.";
    case StartModeOption::LOAD_SAVE:
        return "Close the game, start it with the User Profile Position below, load the save "
               "and start at \"Start From\" once the overworld HUD is up. The game does not need to be running. "
               "Pick the step the save loads at (e.g. ZA-05-10 after the roll tutorial save).";
    default:
        return "The game is already running on the screen the \"Start From\" step starts from. Start right away.";
    }
}

const char* CARD_STYLE =
    "QFrame#StartModeCard{ border: 1px solid #9a9a9a; border-radius: 6px; }"
    "QFrame#StartModeCard:hover{ border: 1px solid #5b8fd9; }";
const char* CARD_STYLE_PICKED =
    "QFrame#StartModeCard{ border: 2px solid #3d7fd9; border-radius: 6px; background: rgba(61, 127, 217, 30); }";

}



StartModeWidget::~StartModeWidget(){
    m_value.remove_listener(*this);
}
StartModeWidget::StartModeWidget(QWidget& parent, StartModeOption& value)
    : QWidget(&parent)
    , ConfigWidget(value, *this)
    , m_value(value)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QLabel* text = new QLabel(QString::fromStdString(value.label()), this);
    text->setWordWrap(true);
    text->setTextFormat(Qt::RichText);
    layout->addWidget(text);

    m_description = new QLabel(this);
    m_description->setWordWrap(true);
    layout->addWidget(m_description);

    QWidget* row_holder = new QWidget(this);
    QHBoxLayout* cards = new QHBoxLayout(row_holder);
    cards->setContentsMargins(0, 4, 0, 0);
    cards->setSpacing(8);
    layout->addWidget(row_holder);

    for (size_t index = 0; index < StartModeOption::COUNT; index++){
        ClickableCard* card = new ClickableCard(row_holder, [this, index]{
            m_value.set_value(index);
        });
        QVBoxLayout* card_layout = new QVBoxLayout(card);
        card_layout->setContentsMargins(10, 8, 10, 8);
        QLabel* label = new QLabel(QString::fromStdString(card_html(index)), card);
        label->setTextFormat(Qt::RichText);
        label->setWordWrap(true);
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        card_layout->addWidget(label);
        cards->addWidget(card, 1);
        m_cards.emplace_back(card);
    }

    update_value();
    value.add_listener(*this);
}
void StartModeWidget::update_value(){
    size_t current = m_value.current_value();
    for (size_t index = 0; index < m_cards.size(); index++){
        m_cards[index]->setStyleSheet(index == current ? CARD_STYLE_PICKED : CARD_STYLE);
    }
    m_description->setText(QString(description(current)));
}
void StartModeWidget::on_config_value_changed(void*){
    QMetaObject::invokeMethod(this, [this]{
        update_value();
    }, Qt::QueuedConnection);
}



}
}
}
