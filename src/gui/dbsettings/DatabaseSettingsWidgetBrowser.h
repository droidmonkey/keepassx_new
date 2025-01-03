/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2018 Sami Vänttinen <sami.vanttinen@protonmail.com>
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

#ifndef KEEPASSXC_DATABASESETTINGSWIDGETBROWSER_H
#define KEEPASSXC_DATABASESETTINGSWIDGETBROWSER_H

#include "DatabaseSettingsWidget.h"

#include <QItemSelection>
#include <QPointer>
#include <QStandardItemModel>

class CustomData;
class Database;

namespace Ui
{
    class DatabaseSettingsWidgetBrowser;
}

class DatabaseSettingsWidgetBrowser : public DatabaseSettingsWidget
{
    Q_OBJECT

public:
    explicit DatabaseSettingsWidgetBrowser(QWidget* parent = nullptr);
    Q_DISABLE_COPY(DatabaseSettingsWidgetBrowser);
    ~DatabaseSettingsWidgetBrowser() override;

    CustomData* customData() const;

public slots:
    void initialize() override;
    void uninitialize() override;
    bool save() override;

private slots:
    void removeSelectedKey();
    void toggleRemoveButton(const QItemSelection& selected);
    void updateSharedKeyList();
    void removeSharedEncryptionKeys();
    void removeStoredPermissions();
    void convertAttributesToCustomData();
    void refreshDatabaseID();
    void editIndex(const QModelIndex& index);
    void editFinished(QStandardItem* item);

private:
    void updateModel();
    void settingsWarning();
    void replaceKey(const QString& prefix, const QString& oldName, const QString& newName) const;
    QString getKeyWithPrefix(const QString& prefix, const QString& key) const;

protected:
    void showEvent(QShowEvent* event) override;

    const QScopedPointer<Ui::DatabaseSettingsWidgetBrowser> m_ui;

private:
    QPointer<CustomData> m_customData;
    QPointer<QStandardItemModel> m_customDataModel;
    QString m_valueInEdit;
};

#endif // KEEPASSXC_DATABASESETTINGSWIDGETBROWSER_H
