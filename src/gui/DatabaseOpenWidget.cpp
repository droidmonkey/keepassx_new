/*
 *  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include "DatabaseOpenWidget.h"
#include "ui_DatabaseOpenWidget.h"

#include "gui/FileDialog.h"
#include "gui/Icons.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "keys/ChallengeResponseKey.h"
#include "keys/FileKey.h"
#ifdef WITH_XC_YUBIKEY
#include "keys/drivers/YubiKeyInterfaceUSB.h"
#endif
#include "quickunlock/QuickUnlockInterface.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QFont>

namespace
{
    constexpr int clearFormsDelay = 30000;

    bool isQuickUnlockAvailable()
    {
        if (config()->get(Config::Security_QuickUnlock).toBool()) {
            return getQuickUnlock()->isAvailable();
        }
        return false;
    }
} // namespace

DatabaseOpenWidget::DatabaseOpenWidget(QWidget* parent)
    : DialogyWidget(parent)
    , m_ui(new Ui::DatabaseOpenWidget())
    , m_db(nullptr)
#ifdef WITH_XC_YUBIKEY
    , m_deviceListener(new DeviceListener(this))
#endif
{
    m_ui->setupUi(this);

    m_ui->messageWidget->setHidden(true);

    m_hideTimer.setInterval(clearFormsDelay);
    m_hideTimer.setSingleShot(true);
    connect(&m_hideTimer, &QTimer::timeout, this, [this] {
        // Reset the password field after being hidden for a set time
        m_ui->editPassword->setText("");
        m_ui->editPassword->setShowPassword(false);
    });

    QFont font;
    font.setPointSize(font.pointSize() + 4);
    font.setBold(true);
    m_ui->labelHeadline->setFont(font);

    m_ui->quickUnlockButton->setFont(font);
    m_ui->quickUnlockButton->setIcon(
        icons()->icon("fingerprint", true, palette().color(QPalette::Active, QPalette::HighlightedText)));
    m_ui->quickUnlockButton->setIconSize({32, 32});

    connect(m_ui->buttonBrowseFile, SIGNAL(clicked()), SLOT(browseKeyFile()));

    auto okBtn = m_ui->buttonBox->button(QDialogButtonBox::Ok);
    okBtn->setText(tr("Unlock"));
    okBtn->setDefault(true);
    connect(m_ui->buttonBox, SIGNAL(accepted()), SLOT(openDatabase()));
    connect(m_ui->buttonBox, SIGNAL(rejected()), SLOT(reject()));

    connect(m_ui->addKeyFileLinkLabel, &QLabel::linkActivated, this, &DatabaseOpenWidget::browseKeyFile);
    connect(m_ui->keyFileLineEdit, &PasswordWidget::textChanged, this, [&](const QString& text) {
        bool state = !text.isEmpty();
        m_ui->addKeyFileLinkLabel->setVisible(!state);
        m_ui->selectKeyFileComponent->setVisible(state);
    });
    connect(m_ui->useHardwareKeyCheckBox, &QCheckBox::toggled, m_ui->hardwareKeyCombo, &QComboBox::setEnabled);

    m_ui->selectKeyFileComponent->setVisible(false);
    toggleHardwareKeyComponent(false);

    QSizePolicy sp = m_ui->hardwareKeyProgress->sizePolicy();
    sp.setRetainSizeWhenHidden(true);
    m_ui->hardwareKeyProgress->setSizePolicy(sp);

#ifdef WITH_XC_YUBIKEY
    connect(m_deviceListener, &DeviceListener::devicePlugged, this, [this] { pollHardwareKey(false, 500); });
    connect(YubiKey::instance(), SIGNAL(detectComplete(bool)), SLOT(hardwareKeyResponse(bool)), Qt::QueuedConnection);

    connect(YubiKey::instance(), &YubiKey::userInteractionRequest, this, [this] {
        // Show the press notification if we are in an independent window (e.g., DatabaseOpenDialog)
        if (window() != getMainWindow()) {
            m_ui->messageWidget->showMessage(tr("Please present or touch your YubiKey to continue…"),
                                             MessageWidget::Information,
                                             MessageWidget::DisableAutoHide);
        }
    });
    connect(YubiKey::instance(), &YubiKey::challengeCompleted, this, [this] { m_ui->messageWidget->hide(); });

    m_ui->noHardwareKeysFoundLabel->setVisible(false);
    m_ui->refreshHardwareKeys->setIcon(icons()->icon("yubikey-refresh", true));
    connect(m_ui->refreshHardwareKeys, &QPushButton::clicked, this, [this] { pollHardwareKey(true); });
    m_hideNoHardwareKeysFoundTimer.setInterval(2000);
    connect(&m_hideNoHardwareKeysFoundTimer, &QTimer::timeout, this, [this] {
        m_ui->noHardwareKeysFoundLabel->setVisible(false);
    });
#else
    m_ui->noHardwareKeysFoundLabel->setVisible(false);
    m_ui->refreshHardwareKeys->setVisible(false);
#endif

    // QuickUnlock actions
    connect(m_ui->quickUnlockButton, &QPushButton::pressed, this, [this] { openDatabase(); });
    connect(m_ui->resetQuickUnlockButton, &QPushButton::pressed, this, [this] { resetQuickUnlock(); });
    m_ui->resetQuickUnlockButton->setShortcut(Qt::Key_Escape);
}

DatabaseOpenWidget::~DatabaseOpenWidget() = default;

void DatabaseOpenWidget::toggleHardwareKeyComponent(bool state)
{
    m_ui->hardwareKeyProgress->setVisible(false);
    m_ui->hardwareKeyComponent->setVisible(state);
    m_ui->hardwareKeyCombo->setVisible(state && m_ui->hardwareKeyCombo->count() != 1);

    m_ui->noHardwareKeysFoundLabel->setVisible(!state && m_manualHardwareKeyRefresh);
    m_ui->noHardwareKeysFoundLabel->setText(YubiKey::instance()->connectedKeys() > 0
                                                ? tr("Hardware keys found, but no slots are configured.")
                                                : tr("No hardware keys found."));

    if (!state) {
        m_ui->useHardwareKeyCheckBox->setChecked(false);
    }
    if (m_ui->hardwareKeyCombo->count() == 1) {
        m_ui->useHardwareKeyCheckBox->setText(
            tr("Use hardware key [Serial: %1]")
                .arg(m_ui->hardwareKeyCombo->itemData(m_ui->hardwareKeyCombo->currentIndex())
                         .value<YubiKeySlot>()
                         .first));
    } else {
        m_ui->useHardwareKeyCheckBox->setText(tr("Use hardware key"));
    }
}

bool DatabaseOpenWidget::event(QEvent* event)
{
    bool ret = DialogyWidget::event(event);
    auto type = event->type();

    if (type == QEvent::Show || type == QEvent::WindowActivate) {
        if (isOnQuickUnlockScreen() && (m_db.isNull() || !canPerformQuickUnlock())) {
            resetQuickUnlock();
        }
        toggleQuickUnlockScreen();

        if (type == QEvent::Show) {
#ifdef WITH_XC_YUBIKEY
#ifdef Q_OS_WIN
            m_deviceListener->registerHotplugCallback(true,
                                                      true,
                                                      YubiKeyInterfaceUSB::YUBICO_USB_VID,
                                                      DeviceListener::MATCH_ANY,
                                                      &DeviceListenerWin::DEV_CLS_KEYBOARD);
            m_deviceListener->registerHotplugCallback(true,
                                                      true,
                                                      YubiKeyInterfaceUSB::ONLYKEY_USB_VID,
                                                      DeviceListener::MATCH_ANY,
                                                      &DeviceListenerWin::DEV_CLS_KEYBOARD);
#else
            m_deviceListener->registerHotplugCallback(true, true, YubiKeyInterfaceUSB::YUBICO_USB_VID);
            m_deviceListener->registerHotplugCallback(true, true, YubiKeyInterfaceUSB::ONLYKEY_USB_VID);
#endif
#endif
        }

        if (isVisible()) {
            m_hideTimer.stop();
            pollHardwareKey();
        }

        ret = true;
    } else if (type == QEvent::Hide || type == QEvent::WindowDeactivate) {
        // Schedule form clearing if we are hidden
        if (!m_hideTimer.isActive()) {
            m_hideTimer.start();
        }

#ifdef WITH_XC_YUBIKEY
        if (type == QEvent::Hide) {
            m_deviceListener->deregisterAllHotplugCallbacks();
        }
#endif

        ret = true;
    }

    return ret;
}

bool DatabaseOpenWidget::unlockingDatabase()
{
    return m_unlockingDatabase;
}

void DatabaseOpenWidget::showMessage(const QString& text, MessageWidget::MessageType type, int autoHideTimeout)
{
    m_ui->messageWidget->showMessage(text, type, autoHideTimeout);
}

void DatabaseOpenWidget::load(const QString& filename)
{
    clearForms();

    m_filename = filename;

    // Read public headers
    QString error;
    m_db.reset(new Database());
    m_db->open(m_filename, nullptr, &error);

    m_ui->fileNameLabel->setRawText(m_filename);

    // Set the public name if defined
    auto label = tr("Unlock KeePassXC Database");
    if (!m_db->publicName().isEmpty()) {
        label.append(QString(": %1").arg(m_db->publicName()));
    }
    m_ui->labelHeadline->setText(label);

    // Apply the public color to the central unlock stack if defined
    auto color = m_db->publicColor();
    if (!color.isEmpty()) {
        m_ui->centralStack->setStyleSheet(QString("QStackedWidget {border: 4px solid %1}").arg(color));
    } else {
        m_ui->centralStack->setStyleSheet("");
    }

    // Show the database icon if defined
    auto iconIndex = m_db->publicIcon();
    if (iconIndex >= 0 && iconIndex < databaseIcons()->count()) {
        m_ui->dbIconLabel->setPixmap(databaseIcons()->icon(iconIndex, IconSize::Large));
        m_ui->dbIconLabel->setVisible(true);
    } else {
        m_ui->dbIconLabel->setPixmap({});
        m_ui->dbIconLabel->setVisible(false);
    }

    if (config()->get(Config::RememberLastKeyFiles).toBool()) {
        auto lastKeyFiles = config()->get(Config::LastKeyFiles).toHash();
        if (lastKeyFiles.contains(m_filename)) {
            m_ui->keyFileLineEdit->setText(lastKeyFiles[m_filename].toString());
        }
    }

    toggleQuickUnlockScreen();

#ifdef WITH_XC_YUBIKEY
    // Do initial auto-poll
    pollHardwareKey();
#endif
}

void DatabaseOpenWidget::clearForms()
{
    setUserInteractionLock(false);
    m_ui->editPassword->setText("");
    m_ui->editPassword->setShowPassword(false);
    m_ui->keyFileLineEdit->clear();
    m_ui->keyFileLineEdit->setShowPassword(false);
    m_ui->keyFileLineEdit->setClearButtonEnabled(true);
    m_ui->hardwareKeyCombo->clear();
    toggleHardwareKeyComponent(false);
    toggleQuickUnlockScreen();

    m_db.reset(new Database(m_filename));
}

QSharedPointer<Database> DatabaseOpenWidget::database()
{
    return m_db;
}

QString DatabaseOpenWidget::filename()
{
    return m_filename;
}

void DatabaseOpenWidget::enterKey(const QString& pw, const QString& keyFile)
{
    if (unlockingDatabase()) {
        qWarning("Ignoring unlock request for %s because of running unlock action.", qPrintable(m_filename));
        return;
    }

    m_ui->editPassword->setText(pw);
    m_ui->keyFileLineEdit->setText(keyFile);
    m_blockQuickUnlock = true;
    openDatabase();
}

void DatabaseOpenWidget::openDatabase()
{
    // Cache this variable for future use then reset
    bool blockQuickUnlock = m_blockQuickUnlock || isOnQuickUnlockScreen();
    m_blockQuickUnlock = false;

    setUserInteractionLock(true);
    m_ui->editPassword->setShowPassword(false);
    m_ui->messageWidget->hide();
    QCoreApplication::processEvents();

    const auto databaseKey = buildDatabaseKey();
    if (!databaseKey) {
        setUserInteractionLock(false);
        return;
    }

    QString error;
    m_db.reset(new Database());
    bool ok = m_db->open(m_filename, databaseKey, &error);

    if (ok) {
        // Warn user about minor version mismatch to halt loading if necessary
        if (m_db->hasMinorVersionMismatch()) {
            QScopedPointer<QMessageBox> msgBox(new QMessageBox(this));
            msgBox->setIcon(QMessageBox::Warning);
            msgBox->setWindowTitle(tr("Database Version Mismatch"));
            msgBox->setText(tr("The database you are trying to open was most likely\n"
                               "created by a newer version of KeePassXC.\n\n"
                               "You can try to open it anyway, but it may be incomplete\n"
                               "and saving any changes may incur data loss.\n\n"
                               "We recommend you update your KeePassXC installation."));
            auto btn = msgBox->addButton(tr("Open database anyway"), QMessageBox::ButtonRole::AcceptRole);
            msgBox->setDefaultButton(btn);
            msgBox->addButton(QMessageBox::Cancel);
            msgBox->layout()->setSizeConstraint(QLayout::SetMinimumSize);
            msgBox->exec();
            if (msgBox->clickedButton() != btn) {
                m_db.reset(new Database());
                m_db->open(m_filename, nullptr, &error);

                m_ui->messageWidget->showMessage(tr("Database unlock canceled."), MessageWidget::MessageType::Error);
                setUserInteractionLock(false);
                return;
            }
        }

        // Save Quick Unlock credentials if available
        if (!blockQuickUnlock && isQuickUnlockAvailable()) {
            auto keyData = databaseKey->serialize();
            getQuickUnlock()->setKey(m_db->publicUuid(), keyData);
            m_ui->messageWidget->hideMessage();
        }

        emit dialogFinished(true);
        clearForms();
    } else {
        if (!isOnQuickUnlockScreen() && m_ui->editPassword->text().isEmpty() && !m_retryUnlockWithEmptyPassword) {
            QScopedPointer<QMessageBox> msgBox(new QMessageBox(this));
            msgBox->setIcon(QMessageBox::Critical);
            msgBox->setWindowTitle(tr("Unlock failed and no password given"));
            msgBox->setText(tr("Unlocking the database failed and you did not enter a password.\n"
                               "Do you want to retry with an \"empty\" password instead?\n\n"
                               "To prevent this error from appearing, you must go to "
                               "\"Database Settings / Security\" and reset your password."));
            auto btn = msgBox->addButton(tr("Retry with empty password"), QMessageBox::ButtonRole::AcceptRole);
            msgBox->setDefaultButton(btn);
            msgBox->addButton(QMessageBox::Cancel);
            msgBox->exec();

            if (msgBox->clickedButton() == btn) {
                m_retryUnlockWithEmptyPassword = true;
                setUserInteractionLock(false);
                openDatabase();
                return;
            }
        }

        setUserInteractionLock(false);

        m_retryUnlockWithEmptyPassword = false;
        m_ui->messageWidget->showMessage(error, MessageWidget::MessageType::Error);

        if (!isOnQuickUnlockScreen()) {
            // Focus on the password field and select the input for easy retry
            m_ui->editPassword->selectAll();
            m_ui->editPassword->setFocus();
        }
    }
}

QSharedPointer<CompositeKey> DatabaseOpenWidget::buildDatabaseKey()
{
    auto databaseKey = QSharedPointer<CompositeKey>::create();

    if (!m_db.isNull() && canPerformQuickUnlock()) {
        // try to retrieve the stored password using Windows Hello
        QByteArray keyData;
        if (!getQuickUnlock()->getKey(m_db->publicUuid(), keyData)) {
            m_ui->messageWidget->showMessage(
                tr("Failed to authenticate with Quick Unlock: %1").arg(getQuickUnlock()->errorString()),
                MessageWidget::Error);
            return {};
        }
        databaseKey->setRawKey(keyData);
        return databaseKey;
    }

    if (!m_ui->editPassword->text().isEmpty() || m_retryUnlockWithEmptyPassword) {
        databaseKey->addKey(QSharedPointer<PasswordKey>::create(m_ui->editPassword->text()));
    }

    auto lastKeyFiles = config()->get(Config::LastKeyFiles).toHash();
    lastKeyFiles.remove(m_filename);

    auto key = QSharedPointer<FileKey>::create();
    QString keyFilename = m_ui->keyFileLineEdit->text();
    if (!keyFilename.isEmpty()) {
        QString errorMsg;
        if (!key->load(keyFilename, &errorMsg)) {
            m_ui->messageWidget->showMessage(tr("Failed to open key file: %1").arg(errorMsg), MessageWidget::Error);
            return {};
        }
        if (key->type() != FileKey::KeePass2XMLv2 && key->type() != FileKey::Hashed
            && !config()->get(Config::Messages_NoLegacyKeyFileWarning).toBool()) {
            QMessageBox legacyWarning;
            legacyWarning.setWindowTitle(tr("Old key file format"));
            legacyWarning.setText(tr("You are using an old key file format which KeePassXC may<br>"
                                     "stop supporting in the future.<br><br>"
                                     "Please consider generating a new key file by going to:<br>"
                                     "<strong>Database &gt; Database Security &gt; Change Key File.</strong><br>"));
            legacyWarning.setIcon(QMessageBox::Icon::Warning);
            legacyWarning.addButton(QMessageBox::Ok);
            legacyWarning.setDefaultButton(QMessageBox::Ok);
            legacyWarning.setCheckBox(new QCheckBox(tr("Don't show this warning again")));

            connect(legacyWarning.checkBox(), &QCheckBox::stateChanged, this, [](int state) {
                config()->set(Config::Messages_NoLegacyKeyFileWarning, state == Qt::CheckState::Checked);
            });

            legacyWarning.exec();
        }
        databaseKey->addKey(key);
        lastKeyFiles.insert(m_filename, keyFilename);
    }

    if (config()->get(Config::RememberLastKeyFiles).toBool()) {
        config()->set(Config::LastKeyFiles, lastKeyFiles);
    }

#ifdef WITH_XC_YUBIKEY
    auto lastChallengeResponse = config()->get(Config::LastChallengeResponse).toHash();
    lastChallengeResponse.remove(m_filename);

    int selectionIndex = m_ui->hardwareKeyCombo->currentIndex();
    if (m_ui->useHardwareKeyCheckBox->isChecked()) {
        auto slot = m_ui->hardwareKeyCombo->itemData(selectionIndex).value<YubiKeySlot>();
        auto crKey = QSharedPointer<ChallengeResponseKey>(new ChallengeResponseKey(slot));
        databaseKey->addChallengeResponseKey(crKey);

        // Qt doesn't read custom types in settings so stuff into a QString
        lastChallengeResponse.insert(m_filename, QStringLiteral("%1:%2").arg(slot.first).arg(slot.second));
    }

    if (config()->get(Config::RememberLastKeyFiles).toBool()) {
        config()->set(Config::LastChallengeResponse, lastChallengeResponse);
    }
#endif

    return databaseKey;
}

void DatabaseOpenWidget::reject()
{
    emit dialogFinished(false);
}

bool DatabaseOpenWidget::browseKeyFile()
{
    QString filters = QString("%1 (*);;%2 (*.keyx; *.key)").arg(tr("All files"), tr("Key files"));
    QString filename =
        fileDialog()->getOpenFileName(this, tr("Select key file"), FileDialog::getLastDir("keyfile"), filters);
    if (filename.isEmpty()) {
        return false;
    }
    if (config()->get(Config::RememberLastKeyFiles).toBool()) {
        FileDialog::saveLastDir("keyfile", filename, true);
    } else {
        FileDialog::saveLastDir("keyfile", {});
    }

    if (QFileInfo(filename).canonicalFilePath() == QFileInfo(m_filename).canonicalFilePath()) {
        MessageBox::warning(this,
                            tr("Cannot use database file as key file"),
                            tr("Your database file is NOT a key file!\nIf you don't have a key file or don't know what "
                               "that is, you don't have to select one."),
                            MessageBox::Button::Ok);
        return false;
    }
    if (filename.endsWith(".kdbx")
        && MessageBox::warning(this,
                               tr("KeePassXC database file selected"),
                               tr("The file you selected looks like a database file.\nA database file is NOT a key "
                                  "file!\n\nAre you sure you want to continue with this file?."),
                               MessageBox::Button::Yes | MessageBox::Button::Cancel,
                               MessageBox::Button::Cancel)
               != MessageBox::Yes) {
        return false;
    }

    m_ui->keyFileLineEdit->setText(filename);
    return true;
}

void DatabaseOpenWidget::pollHardwareKey(bool manualTrigger, int delay)
{
    if (m_pollingHardwareKey) {
        return;
    }

    m_ui->hardwareKeyCombo->setEnabled(false);
    m_ui->useHardwareKeyCheckBox->setEnabled(false);
    m_ui->hardwareKeyProgress->setVisible(true);
    m_ui->refreshHardwareKeys->setEnabled(false);
    m_ui->noHardwareKeysFoundLabel->setVisible(false);
    m_pollingHardwareKey = true;
    m_manualHardwareKeyRefresh = manualTrigger;

    QTimer::singleShot(delay, this, [] { YubiKey::instance()->findValidKeysAsync(); });
}

void DatabaseOpenWidget::hardwareKeyResponse(bool found)
{
    m_ui->useHardwareKeyCheckBox->setEnabled(true);
    m_ui->hardwareKeyProgress->setVisible(false);
    m_ui->refreshHardwareKeys->setEnabled(true);
    m_ui->hardwareKeyCombo->clear();
    m_pollingHardwareKey = false;

    if (!found) {
        toggleHardwareKeyComponent(false);
        return;
    }

    YubiKeySlot lastUsedSlot;
    if (config()->get(Config::RememberLastKeyFiles).toBool()) {
        auto lastChallengeResponse = config()->get(Config::LastChallengeResponse).toHash();
        if (lastChallengeResponse.contains(m_filename)) {
            // Qt doesn't read custom types in settings so extract from QString
            auto split = lastChallengeResponse.value(m_filename).toString().split(":");
            if (split.size() > 1) {
                lastUsedSlot = YubiKeySlot(split[0].toUInt(), split[1].toInt());
            }
            m_ui->useHardwareKeyCheckBox->setChecked(true);
        }
    }

    int selectedIndex = 0;
    const auto foundKeys = YubiKey::instance()->foundKeys();
    for (auto i = foundKeys.cbegin(); i != foundKeys.cend(); ++i) {
        // add detected YubiKey to combo box
        m_ui->hardwareKeyCombo->addItem(i.value(), QVariant::fromValue(i.key()));
        // Select this YubiKey + Slot if we used it in the past
        if (lastUsedSlot == i.key()) {
            selectedIndex = m_ui->hardwareKeyCombo->count() - 1;
        }
    }

    toggleHardwareKeyComponent(true);
    m_ui->hardwareKeyCombo->setEnabled(m_ui->useHardwareKeyCheckBox->isChecked());
    m_ui->hardwareKeyCombo->setCurrentIndex(selectedIndex);
}

void DatabaseOpenWidget::setUserInteractionLock(bool state)
{
    if (state) {
        QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
        m_ui->centralStack->setEnabled(false);
    } else {
        // Ensure no override cursors remain
        while (QApplication::overrideCursor()) {
            QApplication::restoreOverrideCursor();
        }
        m_ui->centralStack->setEnabled(true);
    }
    m_unlockingDatabase = state;
}

bool DatabaseOpenWidget::canPerformQuickUnlock() const
{
    return !m_db.isNull() && isQuickUnlockAvailable() && getQuickUnlock()->hasKey(m_db->publicUuid());
}

bool DatabaseOpenWidget::isOnQuickUnlockScreen() const
{
    return m_ui->centralStack->currentIndex() == 1;
}

void DatabaseOpenWidget::toggleQuickUnlockScreen()
{
    if (canPerformQuickUnlock()) {
        m_ui->centralStack->setCurrentIndex(1);
        // Work around qt issue where focus is stolen even if not visible
        if (m_ui->quickUnlockButton->isVisible()) {
            m_ui->quickUnlockButton->setFocus();
        }
    } else {
        m_ui->centralStack->setCurrentIndex(0);
        // Work around qt issue where focus is stolen even if not visible
        if (m_ui->editPassword->isVisible()) {
            m_ui->editPassword->setFocus();
        }
    }
}

void DatabaseOpenWidget::triggerQuickUnlock()
{
    if (isOnQuickUnlockScreen()) {
        m_ui->quickUnlockButton->click();
    }
}

/**
 * Reset installed quick unlock secrets.
 *
 * It's safe to call this method even if quick unlock is unavailable.
 */
void DatabaseOpenWidget::resetQuickUnlock()
{
    if (!isQuickUnlockAvailable()) {
        return;
    }
    if (!m_db.isNull()) {
        getQuickUnlock()->reset(m_db->publicUuid());
    }
    load(m_filename);
}
