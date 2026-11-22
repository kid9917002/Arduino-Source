/*  Character Portrait Widget
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QToolButton>
#include <QButtonGroup>
#include <QPixmap>
#include <QIcon>
#include <QByteArray>
#include "PokemonLZA_CharacterPortraitWidget.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{



CharacterPortraitWidget::~CharacterPortraitWidget(){
    m_value.remove_listener(*this);
}
CharacterPortraitWidget::CharacterPortraitWidget(QWidget& parent, CharacterPortraitOption& value)
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

    QWidget* grid_holder = new QWidget(this);
    QGridLayout* grid = new QGridLayout(grid_holder);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(6);
    layout->addWidget(grid_holder, 0, Qt::AlignLeft);

    QButtonGroup* group = new QButtonGroup(this);
    group->setExclusive(true);

    const IntegerEnumDropdownDatabase& database = value.database();
    for (size_t index = 0; index < CharacterPortraitOption::COUNT; index++){
        QToolButton* button = new QToolButton(grid_holder);
        button->setCheckable(true);
        button->setAutoRaise(false);
        button->setIconSize(QSize(72, 72));
        button->setFixedSize(QSize(84, 84));
        //  A thick frame on the picked portrait.
        button->setStyleSheet(
            "QToolButton{ border: 3px solid transparent; border-radius: 8px; padding: 0px; }"
            "QToolButton:checked{ border: 3px solid #36c46a; background: rgba(54, 196, 106, 60); }"
            "QToolButton:hover:!checked{ border: 3px solid #8fb7ff; }"
        );

        QPixmap pixmap;
        const CharacterPortraitImage& image = CHARACTER_PORTRAIT_IMAGES[index];
        QByteArray bytes = QByteArray::fromRawData((const char*)image.data, (qsizetype)image.size);
        if (pixmap.loadFromData(bytes, "PNG")){
            button->setIcon(QIcon(pixmap));
        }else{
            button->setText(QString::number(index + 1));
        }

        const EnumEntry* entry = database.find(index);
        if (entry != nullptr){
            button->setToolTip(QString::fromStdString(entry->display));
        }

        grid->addWidget(button, (int)(index / CharacterPortraitOption::COLS), (int)(index % CharacterPortraitOption::COLS));
        group->addButton(button, (int)index);
        m_buttons.emplace_back(button);

        connect(
            button, &QToolButton::clicked,
            this, [this, index](bool){
                m_value.set_value(index);
            }
        );
    }

    update_value();
    value.add_listener(*this);
}
void CharacterPortraitWidget::update_value(){
    size_t current = m_value.current_value();
    for (size_t index = 0; index < m_buttons.size(); index++){
        m_buttons[index]->setChecked(index == current);
    }
}
void CharacterPortraitWidget::on_config_value_changed(void*){
    QMetaObject::invokeMethod(this, [this]{
        update_value();
    }, Qt::QueuedConnection);
}



}
}
}
