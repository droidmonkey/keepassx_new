//
// Created by Thomas on 1/04/2022.
//

#include "ReportsPage2FA.h"
#include "ReportsWidget2FA.h"
#include "gui/Icons.h"



QString ReportsPage2FA::name()
{
    return QObject::tr("2FA");
}

QIcon ReportsPage2FA::icon()
{
    return icons()->icon("chronometer");
}

QWidget* ReportsPage2FA::createWidget()
{
    return m_2faWidget;
}

void ReportsPage2FA::loadSettings(QWidget* widget, QSharedPointer<Database> db)
{
    ReportsWidget2FA* settingsWidget = reinterpret_cast<ReportsWidget2FA*>(widget);
    settingsWidget->loadSettings(db);
}

void ReportsPage2FA::saveSettings(QWidget* widget)
{
    ReportsWidget2FA* settingsWidget = reinterpret_cast<ReportsWidget2FA*>(widget);
    settingsWidget->saveSettings();
}

ReportsPage2FA::ReportsPage2FA()
    : m_2faWidget(new ReportsWidget2FA())
{

}
