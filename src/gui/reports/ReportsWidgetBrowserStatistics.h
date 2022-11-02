/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_REPORTSWIDGETBROWSERSTATISTICS_H
#define KEEPASSXC_REPORTSWIDGETBROWSERSTATISTICS_H

#include "gui/entry/EntryModel.h"
#include <QWidget>

class Database;
class Entry;
class Group;
class PasswordHealth;
class QSortFilterProxyModel;
class QStandardItemModel;

namespace Ui
{
    class ReportsWidgetBrowserStatistics;
}

class ReportsWidgetBrowserStatistics : public QWidget
{
    Q_OBJECT
public:
    explicit ReportsWidgetBrowserStatistics(QWidget* parent = nullptr);
    ~ReportsWidgetBrowserStatistics() override;

    void loadSettings(QSharedPointer<Database> db);
    void saveSettings();

protected:
    void showEvent(QShowEvent* event) override;

signals:
    void entryActivated(Entry*);

public slots:
    void calculateBrowserStatistics();
    void emitEntryActivated(const QModelIndex& index);
    void customMenuRequested(QPoint);
    QList<Entry*> getSelectedEntries();
    void expireSelectedEntries();
    void deleteSelectedEntries();
    void deletePluginDataFromSelectedEntries();

private:
    void addStatisticsRow(bool hasUrls, bool hasSettings, Group*, Entry*, bool);
    QList<Entry*> getSelectedEntries() const;
    QMap<QString, QStringList> getBrowserConfigFromEntry(Entry* entry) const;

    QScopedPointer<Ui::ReportsWidgetBrowserStatistics> m_ui;

    bool m_statisticsCalculated = false;
    QScopedPointer<QStandardItemModel> m_referencesModel;
    QScopedPointer<QSortFilterProxyModel> m_modelProxy;
    QSharedPointer<Database> m_db;
    QList<QPair<Group*, Entry*>> m_rowToEntry;
};

#endif // KEEPASSXC_REPORTSWIDGETBROWSERSTATISTICS_H
