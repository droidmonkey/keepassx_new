/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ReportsDialog.h"
#include "ui_ReportsDialog.h"

#include "ReportsPageHealthcheck.h"
#include "ReportsPageHibp.h"
#include "ReportsPageStatistics.h"
#include "ReportsWidgetHealthcheck.h"
#include "ReportsWidgetHibp.h"

#include "core/Global.h"
#include "touchid/TouchID.h"
#include <core/Entry.h>
#include <core/Group.h>

class ReportsDialog::ExtraPage
{
public:
    ExtraPage(QSharedPointer<IReportsPage> p, QWidget* w)
        : page(p)
        , widget(w)
    {
    }
    void loadSettings(QSharedPointer<Database> db) const
    {
        page->loadSettings(widget, db);
    }
    void saveSettings() const
    {
        page->saveSettings(widget);
    }

private:
    QSharedPointer<IReportsPage> page;
    QWidget* widget;
};

ReportsDialog::ReportsDialog(QWidget* parent)
    : DialogyWidget(parent)
    , m_ui(new Ui::ReportsDialog())
    , m_healthPage(new ReportsPageHealthcheck())
    , m_hibpPage(new ReportsPageHibp())
    , m_statPage(new ReportsPageStatistics())
    , m_editEntryWidget(new EditEntryWidget(this))
{
    m_ui->setupUi(this);

    connect(m_ui->buttonBox, SIGNAL(rejected()), SLOT(reject()));
    addPage(m_healthPage);
    addPage(m_hibpPage);
    addPage(m_statPage);

    m_ui->stackedWidget->setCurrentIndex(0);

    m_editEntryWidget->setObjectName("editEntryWidget");
    m_editEntryWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_ui->stackedWidget->addWidget(m_editEntryWidget);
    adjustSize();

    connect(m_ui->categoryList, SIGNAL(categoryChanged(int)), m_ui->stackedWidget, SLOT(setCurrentIndex(int)));
    connect(m_healthPage->m_healthWidget,
            SIGNAL(entryActivated(Entry*)),
            SLOT(entryActivationSignalReceived(Entry*)));
    connect(m_hibpPage->m_hibpWidget,
            SIGNAL(entryActivated(Entry*)),
            SLOT(entryActivationSignalReceived(Entry*)));
    connect(m_editEntryWidget, SIGNAL(editFinished(bool)), SLOT(switchToMainView(bool)));
}

ReportsDialog::~ReportsDialog()
{
}

void ReportsDialog::load(const QSharedPointer<Database>& db)
{
    m_ui->categoryList->setCurrentCategory(0);
    for (const ExtraPage& page : asConst(m_extraPages)) {
        page.loadSettings(db);
    }
    m_db = db;
}

void ReportsDialog::addPage(QSharedPointer<IReportsPage> page)
{
    const auto category = m_ui->categoryList->currentCategory();
    const auto widget = page->createWidget();
    widget->setParent(this);
    m_extraPages.append(ExtraPage(page, widget));
    m_ui->stackedWidget->addWidget(widget);
    m_ui->categoryList->addCategory(page->name(), page->icon());
    m_ui->categoryList->setCurrentCategory(category);
}

void ReportsDialog::reject()
{
    for (const ExtraPage& extraPage : asConst(m_extraPages)) {
        extraPage.saveSettings();
    }

#ifdef WITH_XC_TOUCHID
    TouchID::getInstance().reset(m_db ? m_db->filePath() : "");
#endif

    emit editFinished(true);
}

void ReportsDialog::entryActivationSignalReceived(Entry* entry)
{
    m_sender = static_cast<QWidget*>(sender());
    m_editEntryWidget->loadEntry(entry, false, false, entry->group()->hierarchy().join(" > "), m_db);
    m_ui->stackedWidget->setCurrentWidget(m_editEntryWidget);
}

void ReportsDialog::switchToMainView(bool previousDialogAccepted)
{
    // Sanity check
    if (!m_sender) {
        return;
    }

    // Return to the previous widget
    m_ui->stackedWidget->setCurrentWidget(m_sender);

    // If "OK" was clicked, and if we came from the Health Check pane,
    // re-compute Health Check
    if (previousDialogAccepted) {
        if (m_sender == m_healthPage->m_healthWidget) {
            m_healthPage->m_healthWidget->calculateHealth();
        } else if (m_sender == m_hibpPage->m_hibpWidget) {
            m_hibpPage->m_hibpWidget->refreshAfterEdit();
        }
    }

    // Don't process the same sender twice
    m_sender = nullptr;
}
