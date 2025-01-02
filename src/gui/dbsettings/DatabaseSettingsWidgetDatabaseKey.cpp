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

#include "DatabaseSettingsWidgetDatabaseKey.h"

#include "core/Config.h"
#include "core/Database.h"
#include "core/PasswordHealth.h"
#include "gui/MessageBox.h"
#include "gui/databasekey/KeyFileEditWidget.h"
#include "gui/databasekey/PasswordEditWidget.h"
#include "gui/databasekey/YubiKeyEditWidget.h"
#include "keys/ChallengeResponseKey.h"
#include "keys/FileKey.h"
#include "keys/PasswordKey.h"
#include "quickunlock/QuickUnlockInterface.h"

#include <QLayout>
#include <QPushButton>

DatabaseSettingsWidgetDatabaseKey::DatabaseSettingsWidgetDatabaseKey(QWidget* parent)
    : DatabaseSettingsWidget(parent)
    , m_additionalKeyOptionsToggle(new QPushButton(tr("Add additional protection…"), this))
    , m_additionalKeyOptions(new QWidget(this))
    , m_passwordEditWidget(new PasswordEditWidget(this))
    , m_keyFileEditWidget(new KeyFileEditWidget(this))
#ifdef WITH_XC_YUBIKEY
    , m_yubiKeyEditWidget(new YubiKeyEditWidget(this))
#endif
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setSizeConstraint(QLayout::SetMinimumSize);
    vbox->setSpacing(20);

    // Primary password option
    vbox->addWidget(m_passwordEditWidget);

    // Additional key options
    m_additionalKeyOptionsToggle->setObjectName("additionalKeyOptionsToggle");
    vbox->addWidget(m_additionalKeyOptionsToggle);
    vbox->addWidget(m_additionalKeyOptions);
    vbox->setSizeConstraint(QLayout::SetMinimumSize);
    m_additionalKeyOptions->setLayout(new QVBoxLayout());
    m_additionalKeyOptions->layout()->setMargin(0);
    m_additionalKeyOptions->layout()->setSpacing(20);
    m_additionalKeyOptions->layout()->addWidget(m_keyFileEditWidget);
#ifdef WITH_XC_YUBIKEY
    m_additionalKeyOptions->layout()->addWidget(m_yubiKeyEditWidget);
#endif
    m_additionalKeyOptions->setVisible(false);

    connect(m_additionalKeyOptionsToggle, SIGNAL(clicked()), SLOT(showAdditionalKeyOptions()));

    vbox->addStretch();
    setLayout(vbox);
}

DatabaseSettingsWidgetDatabaseKey::~DatabaseSettingsWidgetDatabaseKey() = default;

void DatabaseSettingsWidgetDatabaseKey::loadSettings(QSharedPointer<Database> db)
{
    DatabaseSettingsWidget::loadSettings(db);

    if (!m_db->key() || m_db->key()->keys().isEmpty()) {
        // Database has no key, we are about to add a new one
        m_passwordEditWidget->changeVisiblePage(KeyComponentWidget::Page::Edit);
        // Focus won't work until the UI settles
        QTimer::singleShot(0, m_passwordEditWidget, SLOT(setFocus()));
    } else {
        bool hasAdditionalKeys = false;
        for (const auto& key : m_db->key()->keys()) {
            if (key->uuid() == PasswordKey::UUID) {
                m_passwordEditWidget->setComponentAdded(true);
            } else if (key->uuid() == FileKey::UUID) {
                m_keyFileEditWidget->setComponentAdded(true);
                hasAdditionalKeys = true;
            }
        }

#ifdef WITH_XC_YUBIKEY
        for (const auto& key : m_db->key()->challengeResponseKeys()) {
            if (key->uuid() == ChallengeResponseKey::UUID) {
                m_yubiKeyEditWidget->setComponentAdded(true);
                hasAdditionalKeys = true;
            }
        }
#endif

        setAdditionalKeyOptionsVisible(hasAdditionalKeys);
    }

    connect(m_passwordEditWidget->findChild<QPushButton*>("removeButton"), SIGNAL(clicked()), SLOT(markDirty()));
    connect(m_keyFileEditWidget->findChild<QPushButton*>("removeButton"), SIGNAL(clicked()), SLOT(markDirty()));
#ifdef WITH_XC_YUBIKEY
    connect(m_yubiKeyEditWidget->findChild<QPushButton*>("removeButton"), SIGNAL(clicked()), SLOT(markDirty()));
#endif
}

void DatabaseSettingsWidgetDatabaseKey::initialize()
{
    bool blocked = blockSignals(true);
    m_passwordEditWidget->setComponentAdded(false);
    m_keyFileEditWidget->setComponentAdded(false);
#ifdef WITH_XC_YUBIKEY
    m_yubiKeyEditWidget->setComponentAdded(false);
#endif
    blockSignals(blocked);
}

void DatabaseSettingsWidgetDatabaseKey::uninitialize()
{
}

bool DatabaseSettingsWidgetDatabaseKey::saveSettings()
{
    m_isDirty |= (m_passwordEditWidget->visiblePage() == KeyComponentWidget::Page::Edit);
    m_isDirty |= (m_keyFileEditWidget->visiblePage() == KeyComponentWidget::Page::Edit);
#ifdef WITH_XC_YUBIKEY
    m_isDirty |= (m_yubiKeyEditWidget->visiblePage() == KeyComponentWidget::Page::Edit);
#endif

    if (m_db->key() && !m_db->key()->keys().isEmpty() && !m_isDirty) {
        // Key unchanged
        return true;
    }

    auto newKey = QSharedPointer<CompositeKey>::create();

    QSharedPointer<Key> oldPasswordKey;
    QSharedPointer<Key> oldFileKey;
    QSharedPointer<ChallengeResponseKey> oldChallengeResponse;

    for (const auto& key : m_db->key()->keys()) {
        if (key->uuid() == PasswordKey::UUID) {
            oldPasswordKey = key;
        } else if (key->uuid() == FileKey::UUID) {
            oldFileKey = key;
        }
    }

    for (const auto& key : m_db->key()->challengeResponseKeys()) {
        if (key->uuid() == ChallengeResponseKey::UUID) {
            oldChallengeResponse = key;
        }
    }

    // Show warning if database password has not been set
    if (m_passwordEditWidget->visiblePage() == KeyComponentWidget::Page::AddNew
        || (m_passwordEditWidget->visiblePage() == KeyComponentWidget::Page::Edit && m_passwordEditWidget->isEmpty())) {

        QScopedPointer<QMessageBox> msgBox(new QMessageBox(this));
        msgBox->setIcon(QMessageBox::Warning);
        msgBox->setWindowTitle(tr("No password set"));
        msgBox->setText(tr("WARNING! You have not set a password. Using a database without "
                           "a password is strongly discouraged!\n\n"
                           "Are you sure you want to continue without a password?"));
        auto btn = msgBox->addButton(tr("Continue without password"), QMessageBox::ButtonRole::AcceptRole);
        msgBox->addButton(QMessageBox::Cancel);
        msgBox->setDefaultButton(QMessageBox::Cancel);
        msgBox->layout()->setSizeConstraint(QLayout::SetMinimumSize);
        msgBox->exec();
        if (msgBox->clickedButton() != btn) {
            return false;
        }
    } else if (!addToCompositeKey(m_passwordEditWidget, newKey, oldPasswordKey)) {
        return false;
    }

    if (m_passwordEditWidget->visiblePage() == KeyComponentWidget::Page::Edit && !m_passwordEditWidget->isEmpty()) {
        // Prevent setting password with a quality less than the minimum required
        auto minQuality = qBound(0, config()->get(Config::Security_DatabasePasswordMinimumQuality).toInt(), 4);
        if (m_passwordEditWidget->getPasswordQuality() < static_cast<PasswordHealth::Quality>(minQuality)) {
            MessageBox::critical(this,
                                 tr("Weak password"),
                                 tr("The provided password does not meet the minimum quality requirement."),
                                 MessageBox::Ok,
                                 MessageBox::Ok);
            return false;
        }

        // Show warning if database password is weak or poor
        if (m_passwordEditWidget->getPasswordQuality() < PasswordHealth::Quality::Good) {
            auto dialogResult =
                MessageBox::warning(this,
                                    tr("Weak password"),
                                    tr("This is a weak password! For better protection of your secrets, "
                                       "you should choose a stronger password."),
                                    MessageBox::ContinueWithWeakPass | MessageBox::Cancel,
                                    MessageBox::Cancel);

            if (dialogResult == MessageBox::Cancel) {
                return false;
            }
        }
    }

    if (!addToCompositeKey(m_keyFileEditWidget, newKey, oldFileKey)) {
        return false;
    }

#ifdef WITH_XC_YUBIKEY
    if (!addToCompositeKey(m_yubiKeyEditWidget, newKey, oldChallengeResponse)) {
        return false;
    }
#endif

    if (newKey->keys().isEmpty() && newKey->challengeResponseKeys().isEmpty()) {
        MessageBox::critical(this,
                             tr("No encryption key added"),
                             tr("You must add at least one encryption key to secure your database!"),
                             MessageBox::Ok,
                             MessageBox::Ok);
        return false;
    }

    m_db->setKey(newKey, true, false, false);

    getQuickUnlock()->interface()->reset(m_db->publicUuid());

    emit editFinished(true);
    if (m_isDirty) {
        m_db->markAsModified();
    }

    // Reset fields
    initialize();

    return true;
}

void DatabaseSettingsWidgetDatabaseKey::discard()
{
    // Reset fields
    initialize();
    emit editFinished(false);
}

void DatabaseSettingsWidgetDatabaseKey::showAdditionalKeyOptions()
{
    setAdditionalKeyOptionsVisible(true);
}

void DatabaseSettingsWidgetDatabaseKey::setAdditionalKeyOptionsVisible(bool show)
{
    m_additionalKeyOptionsToggle->setVisible(!show);
    m_additionalKeyOptions->setVisible(show);
}

bool DatabaseSettingsWidgetDatabaseKey::addToCompositeKey(KeyComponentWidget* widget,
                                                          QSharedPointer<CompositeKey>& newKey,
                                                          QSharedPointer<Key>& oldKey)
{
    if (widget->visiblePage() == KeyComponentWidget::Edit) {
        QString error = tr("Unknown error");
        if (!widget->validate(error) || !widget->addToCompositeKey(newKey)) {
            MessageBox::critical(this, tr("Failed to change database credentials"), error, MessageBox::Ok);
            return false;
        }
    } else if (widget->visiblePage() == KeyComponentWidget::LeaveOrRemove) {
        Q_ASSERT(oldKey);
        newKey->addKey(oldKey);
    }
    return true;
}

bool DatabaseSettingsWidgetDatabaseKey::addToCompositeKey(KeyComponentWidget* widget,
                                                          QSharedPointer<CompositeKey>& newKey,
                                                          QSharedPointer<ChallengeResponseKey>& oldKey)
{
    if (widget->visiblePage() == KeyComponentWidget::Edit) {
        QString error = tr("Unknown error");
        if (!widget->validate(error) || !widget->addToCompositeKey(newKey)) {
            MessageBox::critical(this, tr("Failed to change database credentials"), error, MessageBox::Ok);
            return false;
        }
    } else if (widget->visiblePage() == KeyComponentWidget::LeaveOrRemove) {
        Q_ASSERT(oldKey);
        newKey->addChallengeResponseKey(oldKey);
    }
    return true;
}

void DatabaseSettingsWidgetDatabaseKey::markDirty()
{
    m_isDirty = true;
}
