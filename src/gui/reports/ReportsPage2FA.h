//
// Created by Thomas on 1/04/2022.
//

#ifndef KEEPASSXC_REPORTSPAGE2FA_H
#define KEEPASSXC_REPORTSPAGE2FA_H

#include "ReportsDialog.h"
#include "ReportsWidget2FA.h"

class ReportsPage2FA : public IReportsPage
{

    QString name() override;
    QIcon icon() override;
    QWidget* createWidget() override;
    void loadSettings(QWidget* widget, QSharedPointer<Database> db) override;
    void saveSettings(QWidget* widget) override;

public:
    ReportsWidget2FA* m_2faWidget;
    ReportsPage2FA();
};

#endif // KEEPASSXC_REPORTSPAGE2FA_H
