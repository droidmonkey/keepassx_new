/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSX_REPORTSWIDGET_H
#define KEEPASSX_REPORTSWIDGET_H

#include "config-keepassx.h"
#include "gui/DialogyWidget.h"
#include "gui/entry/EditEntryWidget.h"

class Database;
class Entry;
class Group;
class QTabWidget;
class ReportsPageHealthcheck;
class ReportsPageHibp;
class ReportsPageStatistics;
#ifdef KPXC_FEATURE_BROWSER
class ReportsPageBrowserStatistics;
class ReportsPagePasskeys;
#endif

namespace Ui
{
    class ReportsDialog;
}

class IReportsPage
{
public:
    virtual ~IReportsPage() = default;
    virtual QString name() = 0;
    virtual QIcon icon() = 0;
    virtual QWidget* createWidget() = 0;
    virtual void loadSettings(QWidget* widget, QSharedPointer<Database> db) = 0;
    virtual void saveSettings(QWidget* widget) = 0;
};

class ReportsDialog : public DialogyWidget
{
    Q_OBJECT

public:
    explicit ReportsDialog(QWidget* parent = nullptr);
    ~ReportsDialog() override;
    Q_DISABLE_COPY(ReportsDialog);

    void load(const QSharedPointer<Database>& db);
    void addPage(QSharedPointer<IReportsPage> page);
    
#ifdef KPXC_FEATURE_BROWSER
    void activatePasskeysPage();
    bool onPassKeysPage();
#endif

signals:
    void editFinished(bool accepted);

private slots:
    void reject();
    void entryActivationSignalReceived(Entry* entry);
    void switchToMainView(bool previousDialogAccepted);

private:
    QSharedPointer<Database> m_db;
    const QScopedPointer<Ui::ReportsDialog> m_ui;
    const QSharedPointer<ReportsPageHealthcheck> m_healthPage;
    const QSharedPointer<ReportsPageHibp> m_hibpPage;
    const QSharedPointer<ReportsPageStatistics> m_statPage;
#ifdef KPXC_FEATURE_BROWSER
    const QSharedPointer<ReportsPageBrowserStatistics> m_browserStatPage;
    const QSharedPointer<ReportsPagePasskeys> m_passkeysPage;
#endif
    QPointer<EditEntryWidget> m_editEntryWidget;
    QWidget* m_sender = nullptr;

    class ExtraPage;
    QList<ExtraPage> m_extraPages;
};

#endif // KEEPASSX_REPORTSWIDGET_H
