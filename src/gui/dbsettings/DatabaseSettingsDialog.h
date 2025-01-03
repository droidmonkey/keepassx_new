/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSXC_DATABASESETTINGSDIALOG_H
#define KEEPASSXC_DATABASESETTINGSDIALOG_H

#include "config-keepassx.h"
#include "gui/EditWidget.h"

#include <QPointer>

class Database;
class DatabaseSettingsWidgetGeneral;
class DatabaseSettingsWidgetEncryption;
class DatabaseSettingsWidgetDatabaseKey;
#ifdef KPXC_FEATURE_BROWSER
class DatabaseSettingsWidgetBrowser;
#endif
class DatabaseSettingsWidgetKeeShare;
#ifdef KPXC_FEATURE_FDOSECRETS
class DatabaseSettingsWidgetFdoSecrets;
#endif
class DatabaseSettingsWidgetMaintenance;
class DatabaseSettingsWidgetRemote;
class QTabWidget;

class DatabaseSettingsDialog : public EditWidget
{
    Q_OBJECT

public:
    explicit DatabaseSettingsDialog(QWidget* parent = nullptr);
    ~DatabaseSettingsDialog() override;
    Q_DISABLE_COPY(DatabaseSettingsDialog);

    void load(const QSharedPointer<Database>& db);
    void showDatabaseKeySettings(int index = 0);
    void showRemoteSettings();

signals:
    void editFinished(bool accepted);

private slots:
    void save();
    void reject();

private:
    QSharedPointer<Database> m_db;
    QPointer<DatabaseSettingsWidgetGeneral> m_generalWidget;
    QPointer<QTabWidget> m_securityTabWidget;
    QPointer<DatabaseSettingsWidgetDatabaseKey> m_databaseKeyWidget;
    QPointer<DatabaseSettingsWidgetEncryption> m_encryptionWidget;
#ifdef KPXC_FEATURE_BROWSER
    QPointer<DatabaseSettingsWidgetBrowser> m_browserWidget;
#endif
    QPointer<DatabaseSettingsWidgetKeeShare> m_keeShareWidget;
#ifdef KPXC_FEATURE_FDOSECRETS
    QPointer<DatabaseSettingsWidgetFdoSecrets> m_fdoSecretsWidget;
#endif
    QPointer<DatabaseSettingsWidgetMaintenance> m_maintenanceWidget;
    QPointer<DatabaseSettingsWidgetRemote> m_remoteWidget;
};

#endif // KEEPASSXC_DATABASESETTINGSDIALOG_H
