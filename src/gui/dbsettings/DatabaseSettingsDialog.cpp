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

#include "DatabaseSettingsDialog.h"
#include "DatabaseSettingsWidgetDatabaseKey.h"
#include "DatabaseSettingsWidgetEncryption.h"
#include "DatabaseSettingsWidgetGeneral.h"
#ifdef KPXC_FEATURE_BROWSER
#include "DatabaseSettingsWidgetBrowser.h"
#endif
#include "../remote/DatabaseSettingsWidgetRemote.h"
#include "DatabaseSettingsWidgetMaintenance.h"
#include "keeshare/DatabaseSettingsWidgetKeeShare.h"
#ifdef KPXC_FEATURE_FDOSECRETS
#include "fdosecrets/widgets/DatabaseSettingsWidgetFdoSecrets.h"
#endif

#include "core/Database.h"
#include "core/Global.h"
#include "gui/Icons.h"

#include <QScrollArea>

DatabaseSettingsDialog::DatabaseSettingsDialog(QWidget* parent)
    : EditWidget(parent)
    , m_generalWidget(new DatabaseSettingsWidgetGeneral(this))
    , m_securityTabWidget(new QTabWidget(this))
    , m_databaseKeyWidget(new DatabaseSettingsWidgetDatabaseKey(this))
    , m_encryptionWidget(new DatabaseSettingsWidgetEncryption(this))
#ifdef KPXC_FEATURE_BROWSER
    , m_browserWidget(new DatabaseSettingsWidgetBrowser(this))
#endif
    , m_keeShareWidget(new DatabaseSettingsWidgetKeeShare(this))
#ifdef KPXC_FEATURE_FDOSECRETS
    , m_fdoSecretsWidget(new DatabaseSettingsWidgetFdoSecrets(this))
#endif
    , m_maintenanceWidget(new DatabaseSettingsWidgetMaintenance(this))
    , m_remoteWidget(new DatabaseSettingsWidgetRemote(this))
{
    connect(this, SIGNAL(accepted()), SLOT(save()));
    connect(this, SIGNAL(rejected()), SLOT(reject()));

    addPage(tr("General"), icons()->icon("preferences-other"), m_generalWidget);
    addPage(tr("Security"), icons()->icon("security-high"), m_securityTabWidget);

    auto* scrollArea = new QScrollArea(parent);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setFrameShadow(QFrame::Plain);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setSizeAdjustPolicy(QScrollArea::AdjustToContents);
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(m_databaseKeyWidget);

    m_securityTabWidget->setObjectName("securityTabWidget");
    m_securityTabWidget->addTab(scrollArea, tr("Database Credentials"));
    m_securityTabWidget->addTab(m_encryptionWidget, tr("Encryption Settings"));

    m_securityTabWidget->setCurrentIndex(0);

    addPage(tr("Remote Sync"), icons()->icon("remote-sync"), m_remoteWidget);

#ifdef KPXC_FEATURE_BROWSER
    addPage(tr("Browser Integration"), icons()->icon("internet-web-browser"), m_browserWidget);
#endif

    addPage(tr("KeeShare"), icons()->icon("preferences-system-network-sharing"), m_keeShareWidget);

#ifdef KPXC_FEATURE_FDOSECRETS
    addPage(tr("Secret Service Integration"), icons()->icon(QStringLiteral("freedesktop")), m_fdoSecretsWidget);
#endif

    addPage(tr("Maintenance"), icons()->icon("hammer-wrench"), m_maintenanceWidget);

    setCurrentPage(0);
}

DatabaseSettingsDialog::~DatabaseSettingsDialog() = default;

void DatabaseSettingsDialog::load(const QSharedPointer<Database>& db)
{
    setHeadline(tr("Database Settings: %1").arg(db->canonicalFilePath()));

    m_generalWidget->loadSettings(db);
    m_databaseKeyWidget->loadSettings(db);
    m_encryptionWidget->loadSettings(db);
    m_remoteWidget->loadSettings(db);
#ifdef KPXC_FEATURE_BROWSER
    m_browserWidget->loadSettings(db);
#endif
    m_keeShareWidget->loadSettings(db);
#ifdef KPXC_FEATURE_FDOSECRETS
    m_fdoSecretsWidget->loadSettings(db);
#endif
    m_maintenanceWidget->loadSettings(db);

    m_db = db;
}

/**
 * Show page and tab with database database key settings.
 */
void DatabaseSettingsDialog::showDatabaseKeySettings(int index)
{
    setCurrentPage(1);
    m_securityTabWidget->setCurrentIndex(index);
}

void DatabaseSettingsDialog::showRemoteSettings()
{
    setCurrentPage(2);
}

void DatabaseSettingsDialog::save()
{
    if (!m_generalWidget->saveSettings()) {
        setCurrentPage(0);
        return;
    }

    if (!m_databaseKeyWidget->saveSettings()) {
        setCurrentPage(1);
        m_securityTabWidget->setCurrentIndex(0);
        return;
    }

    if (!m_encryptionWidget->saveSettings()) {
        setCurrentPage(1);
        m_securityTabWidget->setCurrentIndex(1);
        return;
    }

    if (!m_remoteWidget->saveSettings()) {
        setCurrentPage(2);
        return;
    }

    // Browser settings don't have anything to save

    m_keeShareWidget->saveSettings();
#ifdef KPXC_FEATURE_FDOSECRETS
    m_fdoSecretsWidget->saveSettings();
#endif

    emit editFinished(true);
}

void DatabaseSettingsDialog::reject()
{
    m_generalWidget->discard();
    m_databaseKeyWidget->discard();
    m_encryptionWidget->discard();
    m_remoteWidget->discard();
#ifdef KPXC_FEATURE_BROWSER
    m_browserWidget->discard();
#endif

    emit editFinished(false);
}
