/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2017 Sami Vänttinen <sami.vanttinen@protonmail.com>
 *  Copyright (C) 2013 Francois Ferrand
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "BrowserService.h"
#include "BrowserAction.h"
#include "BrowserEntryConfig.h"
#include "BrowserEntrySaveDialog.h"
#include "BrowserHost.h"
#include "BrowserMessageBuilder.h"
#include "BrowserSettings.h"
#include "core/EntryAttributes.h"
#include "core/Tools.h"
#include "gui/MainWindow.h"
#include "gui/MessageBox.h"
#include "gui/UrlTools.h"
#include "gui/osutils/OSUtils.h"
#ifdef WITH_XC_BROWSER_PASSKEYS
#include "BrowserPasskeys.h"
#include "BrowserPasskeysClient.h"
#include "BrowserPasskeysConfirmationDialog.h"
#include "PasskeyUtils.h"
#include "gui/passkeys/PasskeyImporter.h"
#endif
#ifdef Q_OS_MACOS
#include "gui/osutils/macutils/MacUtils.h"
#endif

#include <QCheckBox>
#include <QCryptographicHash>
#include <QHostAddress>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QListWidget>
#include <QLocalSocket>
#include <QLocale>
#include <QProgressDialog>
#include <QUrl>

const QString BrowserService::KEEPASSXCBROWSER_NAME = QStringLiteral("KeePassXC-Browser Settings");
const QString BrowserService::KEEPASSXCBROWSER_OLD_NAME = QStringLiteral("keepassxc-browser Settings");
static const QString KEEPASSXCBROWSER_GROUP_NAME = QStringLiteral("KeePassXC-Browser Passwords");
static int KEEPASSXCBROWSER_DEFAULT_ICON = 1;
#ifdef WITH_XC_BROWSER_PASSKEYS
static int KEEPASSXCBROWSER_PASSKEY_ICON = 13;
static const QString PASSKEYS_DEFAULT_GROUP_NAME = QStringLiteral("KeePassXC-Browser Passkeys");
#endif
// These are for the settings and password conversion
static const QString KEEPASSHTTP_NAME = QStringLiteral("KeePassHttp Settings");
static const QString KEEPASSHTTP_GROUP_NAME = QStringLiteral("KeePassHttp Passwords");
// Extra entry related options saved in custom data
const QString BrowserService::OPTION_SKIP_AUTO_SUBMIT = QStringLiteral("BrowserSkipAutoSubmit");
const QString BrowserService::OPTION_HIDE_ENTRY = QStringLiteral("BrowserHideEntry");
const QString BrowserService::OPTION_ONLY_HTTP_AUTH = QStringLiteral("BrowserOnlyHttpAuth");
const QString BrowserService::OPTION_NOT_HTTP_AUTH = QStringLiteral("BrowserNotHttpAuth");
const QString BrowserService::OPTION_OMIT_WWW = QStringLiteral("BrowserOmitWww");
const QString BrowserService::OPTION_RESTRICT_KEY = QStringLiteral("BrowserRestrictKey");

Q_GLOBAL_STATIC(BrowserService, s_browserService);

BrowserService::BrowserService()
    : QObject()
    , m_browserHost(new BrowserHost)
    , m_dialogActive(false)
    , m_bringToFrontRequested(false)
    , m_prevWindowState(WindowState::Normal)
    , m_keepassBrowserUUID(Tools::hexToUuid("de887cc3036343b8974b5911b8816224"))
{
    connect(m_browserHost, &BrowserHost::clientMessageReceived, this, &BrowserService::processClientMessage);
    connect(getMainWindow(), &MainWindow::databaseUnlocked, this, &BrowserService::databaseUnlocked);
    connect(getMainWindow(), &MainWindow::databaseLocked, this, &BrowserService::databaseLocked);
    connect(getMainWindow(), &MainWindow::activeDatabaseChanged, this, &BrowserService::activeDatabaseChanged);
    connect(getMainWindow(),
            &MainWindow::databaseUnlockDialogFinished,
            this,
            &BrowserService::handleDatabaseUnlockDialogFinished);

    setEnabled(browserSettings()->isEnabled());
}

BrowserService* BrowserService::instance()
{
    return s_browserService;
}

void BrowserService::setEnabled(bool enabled)
{
    if (enabled) {
        // Update KeePassXC/keepassxc-proxy binary paths to Native Messaging scripts
        if (browserSettings()->updateBinaryPath()) {
            browserSettings()->updateBinaryPaths();
        }

        m_browserHost->start();
    } else {
        m_browserHost->stop();
    }
}

bool BrowserService::isDatabaseOpened() const
{
    if (m_currentDatabaseWidget) {
        return !m_currentDatabaseWidget->isLocked();
    }
    return false;
}

bool BrowserService::openDatabase(bool triggerUnlock)
{
    if (!browserSettings()->unlockDatabase()) {
        return false;
    }

    if (m_currentDatabaseWidget && !m_currentDatabaseWidget->isLocked()) {
        return true;
    }

    if (triggerUnlock && !m_bringToFrontRequested) {
        m_bringToFrontRequested = true;
        updateWindowState();
        emit requestUnlock();
    }

    return false;
}

void BrowserService::lockDatabase()
{
    if (m_currentDatabaseWidget) {
        m_currentDatabaseWidget->lock();
    }
}

QString BrowserService::getDatabaseHash(bool legacy)
{
    if (legacy) {
        return QCryptographicHash::hash(
                   (browserService()->getDatabaseRootUuid() + browserService()->getDatabaseRecycleBinUuid()).toUtf8(),
                   QCryptographicHash::Sha256)
            .toHex();
    }
    return QCryptographicHash::hash(getDatabaseRootUuid().toUtf8(), QCryptographicHash::Sha256).toHex();
}

QString BrowserService::getDatabaseRootUuid()
{
    auto db = getDatabase();
    if (!db) {
        return {};
    }

    Group* rootGroup = db->rootGroup();
    if (!rootGroup) {
        return {};
    }

    return rootGroup->uuidToHex();
}

QString BrowserService::getDatabaseRecycleBinUuid()
{
    auto db = getDatabase();
    if (!db) {
        return {};
    }

    Group* recycleBin = db->metadata()->recycleBin();
    if (!recycleBin) {
        return {};
    }
    return recycleBin->uuidToHex();
}

QJsonArray BrowserService::getChildrenFromGroup(Group* group)
{
    QJsonArray groupList;

    if (!group) {
        return groupList;
    }

    for (const auto& c : group->children()) {
        if (c == group->database()->metadata()->recycleBin()) {
            continue;
        }

        QJsonObject jsonGroup;
        jsonGroup["name"] = c->name();
        jsonGroup["uuid"] = Tools::uuidToHex(c->uuid());
        jsonGroup["children"] = getChildrenFromGroup(c);
        groupList.push_back(jsonGroup);
    }
    return groupList;
}

QJsonObject BrowserService::getDatabaseGroups()
{
    auto db = getDatabase();
    if (!db) {
        return {};
    }

    Group* rootGroup = db->rootGroup();
    if (!rootGroup) {
        return {};
    }

    QJsonObject root;
    root["name"] = rootGroup->name();
    root["uuid"] = Tools::uuidToHex(rootGroup->uuid());
    root["children"] = getChildrenFromGroup(rootGroup);

    QJsonArray groups;
    groups.push_back(root);

    QJsonObject result;
    result["groups"] = groups;

    return result;
}

QJsonArray BrowserService::getDatabaseEntries()
{
    auto db = getDatabase();
    if (!db) {
        return {};
    }

    Group* rootGroup = db->rootGroup();
    if (!rootGroup) {
        return {};
    }

    QJsonArray entries;
    for (const auto& group : rootGroup->groupsRecursive(true)) {
        if (group == db->metadata()->recycleBin()) {
            continue;
        }

        for (const auto& entry : group->entries()) {
            QJsonObject jentry;
            jentry["title"] = entry->resolveMultiplePlaceholders(entry->title());
            jentry["uuid"] = entry->resolveMultiplePlaceholders(entry->uuidToHex());
            jentry["url"] = entry->resolveMultiplePlaceholders(entry->url());
            entries.push_back(jentry);
        }
    }
    return entries;
}

QJsonObject BrowserService::createNewGroup(const QString& groupName, bool isPasskeysGroup)
{
    if (groupName.isEmpty()) {
        return {};
    }

    auto db = getDatabase();
    if (!db) {
        return {};
    }

    Group* rootGroup = db->rootGroup();
    if (!rootGroup) {
        return {};
    }

    auto group = rootGroup->findGroupByPath(groupName);

    // Group already exists
    if (group) {
        QJsonObject result;
        result["name"] = group->name();
        result["uuid"] = Tools::uuidToHex(group->uuid());
        return result;
    }

    auto dialogResult = MessageBox::warning(m_currentDatabaseWidget,
                                            tr("KeePassXC - Create a new group"),
                                            tr("A request for creating a new group \"%1\" has been received.\n"
                                               "Do you want to create this group?\n")
                                                .arg(groupName),
                                            MessageBox::Yes | MessageBox::No);

    if (dialogResult != MessageBox::Yes) {
        return {};
    }

    QString name, uuid;
    Group* previousGroup = rootGroup;
    auto groups = groupName.split("/");

    // Returns the group name based on depth
    auto getGroupName = [&](int depth) {
        QString gName;
        for (int i = 0; i < depth + 1; ++i) {
            gName.append((i == 0 ? "" : "/") + groups[i]);
        }
        return gName;
    };

    // Create new group(s) always when the path is not found
    for (int i = 0; i < groups.length(); ++i) {
        QString gName = getGroupName(i);
        auto tempGroup = rootGroup->findGroupByPath(gName);
        if (!tempGroup) {
            auto newGroup = new Group();
            newGroup->setName(groups[i]);
            newGroup->setUuid(QUuid::createUuid());
            newGroup->setParent(previousGroup);
#ifdef WITH_XC_BROWSER_PASSKEYS
            if (isPasskeysGroup && i == groups.length() - 1) {
                newGroup->setIcon(KEEPASSXCBROWSER_PASSKEY_ICON);
            }
#endif
            name = newGroup->name();
            uuid = Tools::uuidToHex(newGroup->uuid());
            previousGroup = newGroup;
            continue;
        }

        previousGroup = tempGroup;
    }

    QJsonObject result;
    result["name"] = name;
    result["uuid"] = uuid;
    return result;
}

QString BrowserService::getCurrentTotp(const QString& uuid)
{
    QList<QSharedPointer<Database>> databases;
    if (browserSettings()->searchInAllDatabases()) {
        for (auto dbWidget : getMainWindow()->getOpenDatabases()) {
            auto db = dbWidget->database();
            if (db) {
                databases << db;
            }
        }
    } else {
        databases << getDatabase();
    }

    auto entryUuid = Tools::hexToUuid(uuid);
    for (const auto& db : databases) {
        auto entry = db->rootGroup()->findEntryByUuid(entryUuid, true);
        if (entry) {
            return entry->totp();
        }
    }

    return {};
}

QJsonArray
BrowserService::findEntries(const EntryParameters& entryParameters, const StringPairList& keyList, bool* entriesFound)
{
    if (entriesFound) {
        *entriesFound = false;
    }

    const bool alwaysAllowAccess = browserSettings()->alwaysAllowAccess();
    const bool ignoreHttpAuth = browserSettings()->httpAuthPermission();
    const QString siteHost = QUrl(entryParameters.siteUrl).host();
    const QString formHost = QUrl(entryParameters.formUrl).host();

    // Check entries for authorization
    QList<Entry*> entriesToConfirm;
    QList<Entry*> allowedEntries;
    for (auto* entry : searchEntries(entryParameters.siteUrl, entryParameters.formUrl, keyList)) {
        auto entryCustomData = entry->customData();

        if (!entryParameters.httpAuth
            && ((entryCustomData->contains(BrowserService::OPTION_ONLY_HTTP_AUTH)
                 && entryCustomData->value(BrowserService::OPTION_ONLY_HTTP_AUTH) == TRUE_STR)
                || entry->group()->resolveCustomDataTriState(BrowserService::OPTION_ONLY_HTTP_AUTH) == Group::Enable)) {
            continue;
        }

        if (entryParameters.httpAuth
            && ((entryCustomData->contains(BrowserService::OPTION_NOT_HTTP_AUTH)
                 && entryCustomData->value(BrowserService::OPTION_NOT_HTTP_AUTH) == TRUE_STR)
                || entry->group()->resolveCustomDataTriState(BrowserService::OPTION_NOT_HTTP_AUTH) == Group::Enable)) {
            continue;
        }

        // HTTP Basic Auth always needs a confirmation
        if (!ignoreHttpAuth && entryParameters.httpAuth) {
            entriesToConfirm.append(entry);
            continue;
        }

        switch (checkAccess(entry, siteHost, formHost, entryParameters.realm)) {
        case Denied:
            continue;

        case Unknown:
            if (alwaysAllowAccess) {
                allowedEntries.append(entry);
            } else {
                entriesToConfirm.append(entry);
            }
            break;

        case Allowed:
            allowedEntries.append(entry);
            break;
        }
    }

    if (entriesToConfirm.isEmpty() && allowedEntries.isEmpty()) {
        return {};
    }

    // Confirm entries
    auto selectedEntriesToConfirm =
        confirmEntries(entriesToConfirm, entryParameters, siteHost, formHost, entryParameters.httpAuth);
    if (!selectedEntriesToConfirm.isEmpty()) {
        allowedEntries.append(selectedEntriesToConfirm);
    }

    // Ensure that database is not locked when the popup was visible
    if (!isDatabaseOpened()) {
        return {};
    }

    // Sort results
    allowedEntries = sortEntries(allowedEntries, entryParameters.siteUrl, entryParameters.formUrl);

    // Fill the list
    QJsonArray entries;
    for (auto* entry : allowedEntries) {
        entries.append(prepareEntry(entry));
    }

    if (entriesFound != nullptr) {
        *entriesFound = true;
    }

    return entries;
}

QList<Entry*> BrowserService::confirmEntries(QList<Entry*>& entriesToConfirm,
                                             const EntryParameters& entryParameters,
                                             const QString& siteHost,
                                             const QString& formUrl,
                                             const bool httpAuth)
{
    if (entriesToConfirm.isEmpty() || m_dialogActive) {
        return {};
    }

    m_dialogActive = true;
    updateWindowState();
    BrowserAccessControlDialog accessControlDialog(m_currentDatabaseWidget);

    connect(m_currentDatabaseWidget, SIGNAL(databaseLockRequested()), &accessControlDialog, SLOT(reject()));

    connect(&accessControlDialog, &BrowserAccessControlDialog::disableAccess, [&](QTableWidgetItem* item) {
        auto entry = entriesToConfirm[item->row()];
        denyEntry(entry, siteHost, formUrl, entryParameters.realm);
    });

    accessControlDialog.setEntries(entriesToConfirm, entryParameters.siteUrl, httpAuth);

    QList<Entry*> allowedEntries;
    auto ret = accessControlDialog.exec();
    auto remember = accessControlDialog.remember();

    // All are denied
    if (ret == QDialog::Rejected && remember) {
        for (auto& entry : entriesToConfirm) {
            denyEntry(entry, siteHost, formUrl, entryParameters.realm);
        }
    }

    // Some/all are accepted
    if (ret == QDialog::Accepted) {
        auto selectedEntries = accessControlDialog.getEntries(SelectionType::Selected);
        for (auto& item : selectedEntries) {
            auto entry = entriesToConfirm[item->row()];
            allowedEntries.append(entry);

            if (remember) {
                allowEntry(entry, siteHost, formUrl, entryParameters.realm);
            }
        }

        // Remembered non-selected entries must be denied
        if (remember) {
            auto nonSelectedEntries = accessControlDialog.getEntries(SelectionType::NonSelected);
            for (auto& item : nonSelectedEntries) {
                auto entry = entriesToConfirm[item->row()];
                denyEntry(entry, siteHost, formUrl, entryParameters.realm);
            }
        }
    }

    // Handle disabled entries (returned Accept/Reject status does not matter)
    auto disabledEntries = accessControlDialog.getEntries(SelectionType::Disabled);
    for (auto& item : disabledEntries) {
        auto entry = entriesToConfirm[item->row()];
        denyEntry(entry, siteHost, formUrl, entryParameters.realm);
    }

    // Re-hide the application if it wasn't visible before
    hideWindow();
    m_dialogActive = false;

    return allowedEntries;
}

void BrowserService::showPasswordGenerator(const KeyPairMessage& keyPairMessage)
{
    if (!m_passwordGenerator) {
        m_passwordGenerator = PasswordGeneratorWidget::popupGenerator();

        connect(m_passwordGenerator.data(),
                &PasswordGeneratorWidget::closed,
                m_passwordGenerator.data(),
                [this, keyPairMessage] {
                    if (!m_passwordGenerator->isPasswordGenerated()) {
                        auto errorMessage = browserMessageBuilder()->getErrorReply(
                            "generate-password", ERROR_KEEPASS_ACTION_CANCELLED_OR_DENIED);
                        m_browserHost->sendClientMessage(keyPairMessage.socket, errorMessage);
                    }

                    QTimer::singleShot(50, this, [&] { hideWindow(); });
                });

        connect(m_passwordGenerator.data(),
                &PasswordGeneratorWidget::appliedPassword,
                m_passwordGenerator.data(),
                [this, keyPairMessage](const QString& password) {
                    const Parameters params{{"password", password}};
                    m_browserHost->sendClientMessage(keyPairMessage.socket,
                                                     browserMessageBuilder()->buildResponse("generate-password",
                                                                                            keyPairMessage.nonce,
                                                                                            params,
                                                                                            keyPairMessage.publicKey,
                                                                                            keyPairMessage.secretKey));
                });
    }

    raiseWindow();
    m_passwordGenerator->show();
    m_passwordGenerator->raise();
    m_passwordGenerator->activateWindow();
}

bool BrowserService::isPasswordGeneratorRequested() const
{
    return m_passwordGenerator && m_passwordGenerator->isVisible();
}

QString BrowserService::storeKey(const QString& key)
{
    auto db = getDatabase();
    if (!db) {
        return {};
    }

    bool contains;
    auto dialogResult = MessageBox::Cancel;
    QString id;

    do {
        QInputDialog keyDialog(m_currentDatabaseWidget);
        connect(m_currentDatabaseWidget, SIGNAL(databaseLockRequested()), &keyDialog, SLOT(reject()));
        keyDialog.setWindowTitle(tr("KeePassXC - New key association request"));
        keyDialog.setLabelText(tr("You have received an association request for the following database:\n%1\n\n"
                                  "Give the connection a unique name or ID, for example:\nchrome-laptop.")
                                   .arg(db->metadata()->name().toHtmlEscaped()));
        keyDialog.setOkButtonText(tr("Save and allow access"));
        keyDialog.setWindowFlags(keyDialog.windowFlags() | Qt::WindowStaysOnTopHint);
        raiseWindow();
        keyDialog.show();
        keyDialog.activateWindow();
        keyDialog.raise();
        auto ok = keyDialog.exec();

        id = keyDialog.textValue();

        if (ok != QDialog::Accepted || id.isEmpty() || !isDatabaseOpened()) {
            hideWindow();
            return {};
        }

        contains = db->metadata()->customData()->contains(CustomData::BrowserKeyPrefix + id);
        if (contains) {
            dialogResult = MessageBox::warning(m_currentDatabaseWidget,
                                               tr("KeePassXC - Overwrite existing key?"),
                                               tr("A shared encryption key with the name \"%1\" "
                                                  "already exists.\nDo you want to overwrite it?")
                                                   .arg(id),
                                               MessageBox::Overwrite | MessageBox::Cancel,
                                               MessageBox::Cancel);
        }
    } while (contains && dialogResult == MessageBox::Cancel);

    hideWindow();
    db->metadata()->customData()->set(CustomData::BrowserKeyPrefix + id, key);
    db->metadata()->customData()->set(QString("%1_%2").arg(CustomData::Created, id),
                                      QLocale::system().toString(Clock::currentDateTime(), QLocale::ShortFormat));
    return id;
}

QString BrowserService::getKey(const QString& id)
{
    auto db = getDatabase();
    if (!db) {
        return {};
    }

    return db->metadata()->customData()->value(CustomData::BrowserKeyPrefix + id);
}

#ifdef WITH_XC_BROWSER_PASSKEYS
// Passkey registration
QJsonObject BrowserService::showPasskeysRegisterPrompt(const QJsonObject& publicKeyOptions,
                                                       const QString& origin,
                                                       const QString& groupName,
                                                       const StringPairList& keyList)
{
    auto db = selectedDatabase();
    if (!db) {
        return getPasskeyError(ERROR_KEEPASS_DATABASE_NOT_OPENED);
    }

    QJsonObject credentialCreationOptions;
    const auto pkOptionsResult =
        browserPasskeysClient()->getCredentialCreationOptions(publicKeyOptions, origin, &credentialCreationOptions);
    if (pkOptionsResult > 0 || credentialCreationOptions.isEmpty()) {
        return getPasskeyError(pkOptionsResult);
    }

    const auto excludeCredentials = credentialCreationOptions["excludeCredentials"].toArray();
    const auto rpId = credentialCreationOptions["rp"].toObject()["id"].toString();
    const auto timeout = publicKeyOptions["timeout"].toInt();
    const auto username = credentialCreationOptions["user"].toObject()["name"].toString();
    const auto user = credentialCreationOptions["user"].toObject();
    const auto userId = user["id"].toString();

    // Parse excludeCredentialDescriptorList
    if (!excludeCredentials.isEmpty() && isPasskeyCredentialExcluded(excludeCredentials, rpId, keyList)) {
        return getPasskeyError(ERROR_PASSKEYS_CREDENTIAL_IS_EXCLUDED);
    }

    const auto existingEntries = getPasskeyEntriesWithUserHandle(rpId, userId, keyList);

    raiseWindow();
    BrowserPasskeysConfirmationDialog confirmDialog(m_currentDatabaseWidget);
    confirmDialog.registerCredential(username, rpId, existingEntries, timeout);

    auto dialogResult = confirmDialog.exec();
    if (dialogResult == QDialog::Accepted) {
        const auto publicKeyCredentials =
            browserPasskeys()->buildRegisterPublicKeyCredential(credentialCreationOptions);
        if (publicKeyCredentials.credentialId.isEmpty() || publicKeyCredentials.key.isEmpty()
            || publicKeyCredentials.response.isEmpty()) {
            return getPasskeyError(ERROR_PASSKEYS_UNKNOWN_ERROR);
        }

        const auto rpName = publicKeyOptions["rp"]["name"].toString();
        if (confirmDialog.isPasskeyUpdated()) {
            // If no entry is selected, show the import dialog for manual entry selection
            auto selectedEntry = confirmDialog.getSelectedEntry();
            if (!selectedEntry) {
                PasskeyImporter passkeyImporter(m_currentDatabaseWidget);
                const auto result = passkeyImporter.showImportDialog(db,
                                                                     nullptr,
                                                                     origin,
                                                                     rpId,
                                                                     username,
                                                                     publicKeyCredentials.credentialId,
                                                                     userId,
                                                                     publicKeyCredentials.key,
                                                                     tr("KeePassXC - Passkey credentials"),
                                                                     tr("Register a new passkey to this entry:"),
                                                                     tr("Register"));
                if (!result) {
                    return getPasskeyError(ERROR_PASSKEYS_REQUEST_CANCELED);
                }
            } else {
                addPasskeyToEntry(selectedEntry,
                                  rpId,
                                  rpName,
                                  username,
                                  publicKeyCredentials.credentialId,
                                  userId,
                                  publicKeyCredentials.key);
            }
        } else {
            // Handle new/existing group
            const auto createResponse =
                createNewGroup(groupName.isEmpty() ? PASSKEYS_DEFAULT_GROUP_NAME : groupName, true);
            const auto group = db->rootGroup()->findGroupByUuid(Tools::hexToUuid(createResponse["uuid"].toString()));

            addPasskeyToGroup(db,
                              group,
                              origin,
                              rpId,
                              rpName,
                              username,
                              publicKeyCredentials.credentialId,
                              userId,
                              publicKeyCredentials.key);
        }

        hideWindow();
        return publicKeyCredentials.response;
    }

    hideWindow();
    return getPasskeyError(ERROR_PASSKEYS_REQUEST_CANCELED);
}

// Passkey authentication
QJsonObject BrowserService::showPasskeysAuthenticationPrompt(const QJsonObject& publicKeyOptions,
                                                             const QString& origin,
                                                             const StringPairList& keyList)
{
    auto db = getDatabase();
    if (!db) {
        return getPasskeyError(ERROR_KEEPASS_DATABASE_NOT_OPENED);
    }

    QJsonObject assertionOptions;
    const auto assertionResult =
        browserPasskeysClient()->getAssertionOptions(publicKeyOptions, origin, &assertionOptions);
    if (assertionResult > 0 || assertionOptions.isEmpty()) {
        return getPasskeyError(assertionResult);
    }

    // Get allowed entries from RP ID
    const auto rpId = assertionOptions["rpId"].toString();
    const auto entries = getPasskeyAllowedEntries(assertionOptions, rpId, keyList);
    if (entries.isEmpty()) {
        return getPasskeyError(ERROR_KEEPASS_NO_LOGINS_FOUND);
    }

    const auto timeout = publicKeyOptions["timeout"].toInt();

    raiseWindow();
    BrowserPasskeysConfirmationDialog confirmDialog(m_currentDatabaseWidget);
    confirmDialog.authenticateCredential(entries, rpId, timeout);
    auto dialogResult = confirmDialog.exec();
    if (dialogResult == QDialog::Accepted) {
        hideWindow();
        const auto selectedEntry = confirmDialog.getSelectedEntry();
        if (!selectedEntry) {
            return getPasskeyError(ERROR_PASSKEYS_UNKNOWN_ERROR);
        }

        const auto privateKeyPem = selectedEntry->attributes()->value(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_PEM);
        const auto credentialId = passkeyUtils()->getCredentialIdFromEntry(selectedEntry);
        const auto userHandle = selectedEntry->attributes()->value(EntryAttributes::KPEX_PASSKEY_USER_HANDLE);

        auto publicKeyCredential =
            browserPasskeys()->buildGetPublicKeyCredential(assertionOptions, credentialId, userHandle, privateKeyPem);
        if (publicKeyCredential.isEmpty()) {
            return getPasskeyError(ERROR_PASSKEYS_UNKNOWN_ERROR);
        }

        return publicKeyCredential;
    }

    hideWindow();
    return getPasskeyError(ERROR_PASSKEYS_REQUEST_CANCELED);
}

void BrowserService::addPasskeyToGroup(const QSharedPointer<Database>& db,
                                       Group* group,
                                       const QString& url,
                                       const QString& rpId,
                                       const QString& rpName,
                                       const QString& username,
                                       const QString& credentialId,
                                       const QString& userHandle,
                                       const QString& privateKey)
{
    // If no group provided, use the default browser group of the selected database
    if (!group) {
        if (!db) {
            return;
        }
        group = getDefaultEntryGroup(db);
    }

    auto* entry = new Entry();
    entry->setUuid(QUuid::createUuid());
    entry->setGroup(group);
    entry->setTitle(tr("%1 (Passkey)").arg(rpName));
    entry->setUsername(username);
    entry->setUrl(url);
    entry->setIcon(KEEPASSXCBROWSER_PASSKEY_ICON);

    addPasskeyToEntry(entry, rpId, rpName, username, credentialId, userHandle, privateKey);

    // Remove blank entry history
    entry->removeHistoryItems(entry->historyItems());
}

void BrowserService::addPasskeyToEntry(Entry* entry,
                                       const QString& rpId,
                                       const QString& rpName,
                                       const QString& username,
                                       const QString& credentialId,
                                       const QString& userHandle,
                                       const QString& privateKey)
{
    // Reserved for future use
    Q_UNUSED(rpName)

    Q_ASSERT(entry);
    if (!entry) {
        return;
    }

    // Ask confirmation if entry already contains a Passkey
    if (entry->hasPasskey()) {
        if (MessageBox::question(m_currentDatabaseWidget,
                                 tr("KeePassXC - Update passkey"),
                                 tr("Entry already has a passkey.\nDo you want to overwrite the passkey in %1 - %2?")
                                     .arg(entry->title(), passkeyUtils()->getUsernameFromEntry(entry)),
                                 MessageBox::Overwrite | MessageBox::Cancel,
                                 MessageBox::Cancel)
            != MessageBox::Overwrite) {
            return;
        }
    }

    entry->beginUpdate();

    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_USERNAME, username);
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_CREDENTIAL_ID, credentialId, true);
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_PEM, privateKey, true);
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_RELYING_PARTY, rpId);
    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_USER_HANDLE, userHandle, true);
    entry->addTag(tr("Passkey"));

    entry->endUpdate();
}
#endif

void BrowserService::addEntry(const EntryParameters& entryParameters,
                              const QString& group,
                              const QString& groupUuid,
                              const bool downloadFavicon,
                              const QSharedPointer<Database>& selectedDb)
{
    // TODO: select database based on this key id
    auto db = selectedDb ? selectedDb : selectedDatabase();
    if (!db) {
        return;
    }

    auto* entry = new Entry();
    entry->setUuid(QUuid::createUuid());
    entry->setTitle(entryParameters.title.isEmpty() ? QUrl(entryParameters.siteUrl).host() : entryParameters.title);
    entry->setUrl(entryParameters.siteUrl);
    entry->setIcon(KEEPASSXCBROWSER_DEFAULT_ICON);
    entry->setUsername(entryParameters.login);
    entry->setPassword(entryParameters.password);

    // Select a group for the entry
    if (!group.isEmpty()) {
        if (db->rootGroup()) {
            auto selectedGroup = db->rootGroup()->findGroupByUuid(Tools::hexToUuid(groupUuid));
            if (selectedGroup) {
                entry->setGroup(selectedGroup);
            } else {
                entry->setGroup(getDefaultEntryGroup(db));
            }
        }
    } else {
        entry->setGroup(getDefaultEntryGroup(db));
    }

    const QString host = QUrl(entryParameters.siteUrl).host();
    const QString submitHost = QUrl(entryParameters.formUrl).host();
    BrowserEntryConfig config;
    config.allow(host);

    if (!submitHost.isEmpty()) {
        config.allow(submitHost);
    }
    if (!entryParameters.realm.isEmpty()) {
        config.setRealm(entryParameters.realm);
    }
    config.save(entry);

    if (downloadFavicon && m_currentDatabaseWidget) {
        m_currentDatabaseWidget->downloadFaviconInBackground(entry);
    }
}

bool BrowserService::updateEntry(const EntryParameters& entryParameters, const QString& uuid)
{
    // TODO: select database based on this key id
    auto db = selectedDatabase();
    if (!db) {
        return false;
    }

    auto entry = db->rootGroup()->findEntryByUuid(Tools::hexToUuid(uuid));
    if (!entry) {
        // If entry is not found for update, add a new one to the selected database
        addEntry(entryParameters, "", "", false, db);
        return true;
    }

    // Check if the entry password is a reference. If so, update the original entry instead
    while (entry->attributes()->isReference(EntryAttributes::PasswordKey)) {
        const QUuid referenceUuid = entry->attributes()->referenceUuid(EntryAttributes::PasswordKey);
        if (!referenceUuid.isNull()) {
            entry = db->rootGroup()->findEntryByUuid(referenceUuid);
            if (!entry) {
                return false;
            }
        }
    }

    auto username = entry->username();
    if (username.isEmpty()) {
        return false;
    }

    bool result = false;
    if (username.compare(entryParameters.login, Qt::CaseSensitive) != 0
        || entry->password().compare(entryParameters.password, Qt::CaseSensitive) != 0) {
        MessageBox::Button dialogResult = MessageBox::No;
        if (!browserSettings()->alwaysAllowUpdate()) {
            raiseWindow();
            dialogResult = MessageBox::question(m_currentDatabaseWidget,
                                                tr("KeePassXC - Update Entry"),
                                                tr("Do you want to update the information in %1 - %2?")
                                                    .arg(QUrl(entryParameters.siteUrl).host(), username),
                                                MessageBox::Save | MessageBox::Cancel,
                                                MessageBox::Cancel);
        }

        if (browserSettings()->alwaysAllowUpdate() || dialogResult == MessageBox::Save) {
            entry->beginUpdate();
            if (!entry->attributes()->isReference(EntryAttributes::UserNameKey)) {
                entry->setUsername(entryParameters.login);
            }
            entry->setPassword(entryParameters.password);
            entry->endUpdate();
            result = true;
        }

        hideWindow();
    }

    return result;
}

bool BrowserService::deleteEntry(const QString& uuid)
{
    auto db = selectedDatabase();
    if (!db) {
        return false;
    }

    auto* entry = db->rootGroup()->findEntryByUuid(Tools::hexToUuid(uuid));
    if (!entry) {
        return false;
    }

    auto dialogResult = MessageBox::warning(m_currentDatabaseWidget,
                                            tr("KeePassXC - Delete entry"),
                                            tr("A request for deleting entry \"%1\" has been received.\n"
                                               "Do you want to delete the entry?\n")
                                                .arg(entry->title()),
                                            MessageBox::Yes | MessageBox::No);
    if (dialogResult != MessageBox::Yes) {
        return false;
    }

    db->recycleEntry(entry);
    return true;
}

void BrowserService::removePluginData(Entry* entry) const
{
    if (entry) {
        entry->beginUpdate();
        entry->customData()->remove(BrowserService::KEEPASSXCBROWSER_NAME);
        entry->endUpdate();
    }
}

QList<Entry*> BrowserService::searchEntries(const QSharedPointer<Database>& db,
                                            const QString& siteUrl,
                                            const QString& formUrl,
                                            const QStringList& keys,
                                            bool passkey)
{
    QList<Entry*> entries;
    auto* rootGroup = db->rootGroup();
    if (!rootGroup) {
        return entries;
    }

    for (const auto& group : rootGroup->groupsRecursive(true)) {
        if (group->isRecycled()
            || group->resolveCustomDataTriState(BrowserService::OPTION_HIDE_ENTRY) == Group::Enable) {
            continue;
        }

        // If a key restriction is specified and not contained in the keys list then skip this group.
        auto restrictKey = group->resolveCustomDataString(BrowserService::OPTION_RESTRICT_KEY);
        if (!restrictKey.isEmpty() && !keys.contains(restrictKey)) {
            continue;
        }

        const auto omitWwwSubdomain =
            group->resolveCustomDataTriState(BrowserService::OPTION_OMIT_WWW) == Group::Enable;

        for (auto* entry : group->entries()) {
            if (entry->isRecycled()
                || (entry->customData()->contains(BrowserService::OPTION_HIDE_ENTRY)
                    && entry->customData()->value(BrowserService::OPTION_HIDE_ENTRY) == TRUE_STR)) {
                continue;
            }

            if (!passkey && !shouldIncludeEntry(entry, siteUrl, formUrl, omitWwwSubdomain)) {
                continue;
            }

#ifdef WITH_XC_BROWSER_PASSKEYS
            // With Passkeys, check for the Relying Party instead of URL
            if (passkey && entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_RELYING_PARTY) != siteUrl) {
                continue;
            }
#endif

            // Additional URL check may have already inserted the entry to the list
            if (!entries.contains(entry)) {
                entries.append(entry);
            }
        }
    }

    return entries;
}

QList<Entry*> BrowserService::searchEntries(const QString& siteUrl,
                                            const QString& formUrl,
                                            const StringPairList& keyList,
                                            bool passkey)
{
    // Check if database is connected with KeePassXC-Browser. If so, return browser key (otherwise empty)
    auto databaseConnected = [&](const QSharedPointer<Database>& db) {
        for (const StringPair& keyPair : keyList) {
            QString key = db->metadata()->customData()->value(CustomData::BrowserKeyPrefix + keyPair.first);
            if (!key.isEmpty() && keyPair.second == key) {
                return keyPair.first;
            }
        }
        return QString();
    };

    // Get the list of databases to search
    QList<QSharedPointer<Database>> databases;
    QStringList keys;
    if (browserSettings()->searchInAllDatabases()) {
        for (auto dbWidget : getMainWindow()->getOpenDatabases()) {
            auto db = dbWidget->database();
            auto key = databaseConnected(dbWidget->database());
            if (db && !key.isEmpty()) {
                databases << db;
                keys << key;
            }
        }
    } else {
        const auto& db = getDatabase();
        auto key = databaseConnected(db);
        if (!key.isEmpty()) {
            databases << db;
            keys << key;
        }
    }

    // Search entries matching the hostname
    QString hostname = QUrl(siteUrl).host();
    QList<Entry*> entries;
    do {
        for (const auto& db : databases) {
            entries << searchEntries(db, siteUrl, formUrl, keys, passkey);
        }
    } while (entries.isEmpty() && removeFirstDomain(hostname));

    return entries;
}

QString BrowserService::decodeCustomDataRestrictKey(const QString& key)
{
    return key.isEmpty() ? tr("Disable") : key;
}

void BrowserService::requestGlobalAutoType(const QString& search)
{
    emit osUtils->globalShortcutTriggered("autotype", search);
}

QList<Entry*> BrowserService::sortEntries(QList<Entry*>& entries, const QString& siteUrl, const QString& formUrl)
{
    // Build map of prioritized entries
    QMultiMap<int, Entry*> priorities;
    for (auto* entry : entries) {
        priorities.insert(sortPriority(entry->getAllUrls(), siteUrl, formUrl), entry);
    }

    auto keys = priorities.uniqueKeys();
    std::sort(keys.begin(), keys.end(), [](int l, int r) { return l > r; });

    QList<Entry*> results;
    for (auto key : keys) {
        results << priorities.values(key);

        if (browserSettings()->bestMatchOnly() && !results.isEmpty()) {
            // Early out once we find the highest batch of matches
            break;
        }
    }

    return results;
}

void BrowserService::allowEntry(Entry* entry, const QString& siteHost, const QString& formUrl, const QString& realm)
{
    BrowserEntryConfig config;
    config.load(entry);
    config.allow(siteHost);

    if (!formUrl.isEmpty() && siteHost != formUrl) {
        config.allow(formUrl);
    }

    if (!realm.isEmpty()) {
        config.setRealm(realm);
    }

    config.save(entry);
}

void BrowserService::denyEntry(Entry* entry, const QString& siteHost, const QString& formUrl, const QString& realm)
{
    BrowserEntryConfig config;
    config.load(entry);
    config.deny(siteHost);

    if (!formUrl.isEmpty() && siteHost != formUrl) {
        config.deny(formUrl);
    }

    if (!realm.isEmpty()) {
        config.setRealm(realm);
    }

    config.save(entry);
}

QJsonObject BrowserService::prepareEntry(const Entry* entry)
{
    QJsonObject res;
    res["login"] = entry->resolveMultiplePlaceholders(entry->username());
    res["password"] = entry->resolveMultiplePlaceholders(entry->password());
    res["name"] = entry->resolveMultiplePlaceholders(entry->title());
    res["uuid"] = entry->resolveMultiplePlaceholders(entry->uuidToHex());
    res["group"] = entry->resolveMultiplePlaceholders(entry->group()->name());

    if (entry->hasTotp()) {
        res["totp"] = entry->totp();
    }

    if (entry->isExpired()) {
        res["expired"] = TRUE_STR;
    }

    auto skipAutoSubmitGroup = entry->group()->resolveCustomDataTriState(BrowserService::OPTION_SKIP_AUTO_SUBMIT);
    if (skipAutoSubmitGroup == Group::Inherit) {
        if (entry->customData()->contains(BrowserService::OPTION_SKIP_AUTO_SUBMIT)) {
            res["skipAutoSubmit"] = entry->customData()->value(BrowserService::OPTION_SKIP_AUTO_SUBMIT);
        }
    } else {
        res["skipAutoSubmit"] = skipAutoSubmitGroup == Group::Enable ? TRUE_STR : FALSE_STR;
    }

    if (browserSettings()->supportKphFields()) {
        const EntryAttributes* attr = entry->attributes();
        QJsonArray stringFields;
        for (const auto& key : attr->keys()) {
            if (key.startsWith("KPH: ")) {
                QJsonObject sField;
                sField[key] = entry->resolveMultiplePlaceholders(attr->value(key));
                stringFields.append(sField);
            }
        }
        res["stringFields"] = stringFields;
    }
    return res;
}

BrowserService::Access
BrowserService::checkAccess(const Entry* entry, const QString& siteHost, const QString& formHost, const QString& realm)
{
    if (entry->isExpired() && !browserSettings()->allowExpiredCredentials()) {
        return Denied;
    }

    BrowserEntryConfig config;
    if (!config.load(entry)) {
        return Unknown;
    }
    if ((config.isAllowed(siteHost)) && (formHost.isEmpty() || config.isAllowed(formHost))) {
        return Allowed;
    }
    if ((config.isDenied(siteHost)) || (!formHost.isEmpty() && config.isDenied(formHost))) {
        return Denied;
    }
    if (!realm.isEmpty() && config.realm() != realm) {
        return Denied;
    }
    return Unknown;
}

Group* BrowserService::getDefaultEntryGroup(const QSharedPointer<Database>& selectedDb)
{
    auto db = selectedDb ? selectedDb : getDatabase();
    if (!db) {
        return nullptr;
    }

    auto* rootGroup = db->rootGroup();
    if (!rootGroup) {
        return nullptr;
    }

    for (auto* g : rootGroup->groupsRecursive(true)) {
        if (g->name() == KEEPASSXCBROWSER_GROUP_NAME && !g->isRecycled()) {
            return db->rootGroup()->findGroupByUuid(g->uuid());
        }
    }

    auto* group = new Group();
    group->setUuid(QUuid::createUuid());
    group->setName(KEEPASSXCBROWSER_GROUP_NAME);
    group->setIcon(KEEPASSXCBROWSER_DEFAULT_ICON);
    group->setParent(rootGroup);
    return group;
}

// Returns the maximum sort priority given a set of match urls and the
// extension provided site and form url.
int BrowserService::sortPriority(const QStringList& urls, const QString& siteUrl, const QString& formUrl)
{
    QList<int> priorityList;
    // NOTE: QUrl::matches is utterly broken in Qt < 5.11, so we work around that
    // by removing parts of the url that we don't match and direct matching others
    const auto stdOpts = QUrl::RemoveFragment | QUrl::RemoveUserInfo;
    const auto adjustedSiteUrl = QUrl(siteUrl).adjusted(stdOpts);
    const auto adjustedFormUrl = QUrl(formUrl).adjusted(stdOpts);

    auto getPriority = [&](const QString& givenUrl) {
        auto url = QUrl::fromUserInput(givenUrl).adjusted(stdOpts);

        // Default to https scheme if undefined
        if (url.scheme().isEmpty() || !givenUrl.contains("://")) {
            url.setScheme("https");
        }

        // Add the empty path to the URL if it's missing.
        // URL's from the extension always have a path set, entry URL's can be without.
        if (url.path().isEmpty() && !url.hasFragment() && !url.hasQuery()) {
            url.setPath("/");
        }

        // Reject invalid urls and hosts, except 'localhost', and scheme mismatch
        if (!url.isValid() || (!url.host().contains(".") && url.host() != "localhost")
            || url.scheme() != adjustedSiteUrl.scheme()) {
            return 0;
        }

        // Exact match with site url or form url
        if (url.matches(adjustedSiteUrl, QUrl::None) || url.matches(adjustedFormUrl, QUrl::None)) {
            return 100;
        }

        // Exact match without the query string
        if (url.matches(adjustedSiteUrl, QUrl::RemoveQuery) || url.matches(adjustedFormUrl, QUrl::RemoveQuery)) {
            return 90;
        }

        // Parent directory match
        if (url.isParentOf(adjustedSiteUrl) || url.isParentOf(adjustedFormUrl)) {
            return 85;
        }

        // Match without path (ie, FQDN match), form url prioritizes lower than site url
        if (url.host() == adjustedSiteUrl.host()) {
            return 80;
        }
        if (url.host() == adjustedFormUrl.host()) {
            return 70;
        }

        // Site/form url ends with given url (subdomain mismatch)
        if (adjustedSiteUrl.host().endsWith(url.host())) {
            return 60;
        }
        if (adjustedFormUrl.host().endsWith(url.host())) {
            return 50;
        }

        // No valid match found
        return 0;
    };

    for (const auto& entryUrl : urls) {
        priorityList << getPriority(entryUrl);
    }

    return *std::max_element(priorityList.begin(), priorityList.end());
}

bool BrowserService::removeFirstDomain(QString& hostname)
{
    int pos = hostname.indexOf(".");
    if (pos < 0) {
        return false;
    }

    // Don't remove the second-level domain if it's the only one
    if (hostname.count(".") > 1) {
        hostname = hostname.mid(pos + 1);
        return !hostname.isEmpty();
    }

    // Nothing removed
    return false;
}

/* Test if a search URL matches a custom entry. If the URL has the schema "keepassxc", some special checks will be made.
 * Otherwise, this simply delegates to handleURL(). */
bool BrowserService::shouldIncludeEntry(Entry* entry,
                                        const QString& url,
                                        const QString& submitUrl,
                                        const bool omitWwwSubdomain)
{
    // Use this special scheme to find entries by UUID
    if (url.startsWith("keepassxc://by-uuid/")) {
        return url.endsWith("by-uuid/" + entry->uuidToHex());
    } else if (url.startsWith("keepassxc://by-path/")) {
        return url.endsWith("by-path/" + entry->path());
    }

    const auto allEntryUrls = entry->getAllUrls();
    for (const auto& entryUrl : allEntryUrls) {
        if (handleURL(entryUrl, url, submitUrl, omitWwwSubdomain)) {
            return true;
        }
    }

    return false;
}

#ifdef WITH_XC_BROWSER_PASSKEYS
// Returns all Passkey entries for the current Relying Party
QList<Entry*> BrowserService::getPasskeyEntries(const QString& rpId, const StringPairList& keyList)
{
    QList<Entry*> entries;
    for (const auto& entry : searchEntries(rpId, "", keyList, true)) {
        if (entry->hasPasskey() && entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_RELYING_PARTY) == rpId) {
            entries << entry;
        }
    }

    return entries;
}

// Returns all Passkey entries for the current Relying Party and identical user handle
QList<Entry*> BrowserService::getPasskeyEntriesWithUserHandle(const QString& rpId,
                                                              const QString& userId,
                                                              const StringPairList& keyList)
{
    QList<Entry*> entries;
    for (const auto& entry : searchEntries(rpId, "", keyList, true)) {
        if (entry->hasPasskey() && entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_RELYING_PARTY) == rpId
            && entry->attributes()->value(EntryAttributes::KPEX_PASSKEY_USER_HANDLE) == userId) {
            entries << entry;
        }
    }

    return entries;
}

// Get all entries for the site that are allowed by the server
QList<Entry*> BrowserService::getPasskeyAllowedEntries(const QJsonObject& assertionOptions,
                                                       const QString& rpId,
                                                       const StringPairList& keyList)
{
    QList<Entry*> entries;
    const auto allowedCredentials = passkeyUtils()->getAllowedCredentialsFromAssertionOptions(assertionOptions);
    if (!assertionOptions["allowCredentials"].toArray().isEmpty() && allowedCredentials.isEmpty()) {
        return {};
    }

    for (const auto& entry : getPasskeyEntries(rpId, keyList)) {
        // If allowedCredentials.isEmpty() check if entry contains an extra attribute for user handle.
        // If that is found, the entry should be allowed.
        // See: https://w3c.github.io/webauthn/#dom-authenticatorassertionresponse-userhandle
        if (allowedCredentials.contains(passkeyUtils()->getCredentialIdFromEntry(entry))
            || (allowedCredentials.isEmpty()
                && entry->attributes()->hasKey(EntryAttributes::KPEX_PASSKEY_USER_HANDLE))) {
            entries << entry;
        }
    }

    return entries;
}

// Checks if the same user ID already exists for the current RP ID
bool BrowserService::isPasskeyCredentialExcluded(const QJsonArray& excludeCredentials,
                                                 const QString& rpId,
                                                 const StringPairList& keyList)
{
    QStringList allIds;
    for (const auto& cred : excludeCredentials) {
        allIds << cred["id"].toString();
    }

    const auto passkeyEntries = getPasskeyEntries(rpId, keyList);
    return std::any_of(passkeyEntries.begin(), passkeyEntries.end(), [&](const auto& entry) {
        return allIds.contains(passkeyUtils()->getCredentialIdFromEntry(entry));
    });
}

QJsonObject BrowserService::getPasskeyError(int errorCode) const
{
    return QJsonObject({{"errorCode", errorCode}});
}
#endif

bool BrowserService::handleURL(const QString& entryUrl,
                               const QString& siteUrl,
                               const QString& formUrl,
                               const bool omitWwwSubdomain)
{
    if (entryUrl.isEmpty()) {
        return false;
    }

    QUrl entryQUrl;
    if (entryUrl.contains("://")) {
        entryQUrl = entryUrl;
    } else {
        entryQUrl = QUrl::fromUserInput(entryUrl);

        if (browserSettings()->matchUrlScheme()) {
            entryQUrl.setScheme("https");
        }
    }

    // Remove WWW subdomain from matching if group setting is enabled
    if (omitWwwSubdomain && entryQUrl.host().startsWith("www.")) {
        entryQUrl.setHost(entryQUrl.host().remove("www."));
    }

    // Make a direct compare if a local file is used
    if (siteUrl.startsWith("file://")) {
        return entryUrl == formUrl;
    }

    // URL host validation fails
    if (entryQUrl.host().isEmpty()) {
        return false;
    }

    // Match port, if used
    QUrl siteQUrl(siteUrl);
    if (entryQUrl.port() > 0 && entryQUrl.port() != siteQUrl.port()) {
        return false;
    }

    // Match scheme
    if (browserSettings()->matchUrlScheme() && !entryQUrl.scheme().isEmpty()
        && entryQUrl.scheme().compare(siteQUrl.scheme()) != 0) {
        return false;
    }

    // Check for illegal characters
    QRegularExpression re("[<>\\^`{|}]");
    if (re.match(entryUrl).hasMatch()) {
        return false;
    }

    // Match the base domain
    if (urlTools()->getBaseDomainFromUrl(siteQUrl.host()) != urlTools()->getBaseDomainFromUrl(entryQUrl.host())) {
        return false;
    }

    // Match the subdomains with the limited wildcard
    if (siteQUrl.host().endsWith(entryQUrl.host())) {
        return true;
    }

    return false;
}

QSharedPointer<Database> BrowserService::getDatabase(const QUuid& rootGroupUuid)
{
    if (!rootGroupUuid.isNull()) {
        const auto openDatabases = getOpenDatabases();
        for (const auto& db : openDatabases) {
            if (db->rootGroup()->uuid() == rootGroupUuid) {
                return db;
            }
        }
    }

    if (m_currentDatabaseWidget) {
        return m_currentDatabaseWidget->database();
    }
    return {};
}

QList<QSharedPointer<Database>> BrowserService::getOpenDatabases()
{
    QList<QSharedPointer<Database>> databaseList;
    for (auto dbWidget : getMainWindow()->getOpenDatabases()) {
        if (!dbWidget->isLocked()) {
            databaseList << dbWidget->database();
        }
    }
    return databaseList;
}

QSharedPointer<Database> BrowserService::selectedDatabase()
{
    QList<DatabaseWidget*> databaseWidgets;
    for (auto dbWidget : getMainWindow()->getOpenDatabases()) {
        // Add only open databases
        if (!dbWidget->isLocked()) {
            databaseWidgets << dbWidget;
        }
    }

    BrowserEntrySaveDialog browserEntrySaveDialog(m_currentDatabaseWidget);
    int openDatabaseCount = browserEntrySaveDialog.setItems(databaseWidgets, m_currentDatabaseWidget);
    if (openDatabaseCount > 1) {
        int res = browserEntrySaveDialog.exec();
        if (res == QDialog::Accepted) {
            const auto selectedDatabase = browserEntrySaveDialog.getSelected();
            if (selectedDatabase.length() > 0) {
                int index = selectedDatabase[0]->data(Qt::UserRole).toInt();
                return databaseWidgets[index]->database();
            }
        } else {
            return {};
        }
    }

    // Return current database
    return getDatabase();
}

void BrowserService::hideWindow() const
{
    if (m_prevWindowState == WindowState::Minimized) {
        getMainWindow()->showMinimized();
    } else {
#ifdef Q_OS_MACOS
        if (m_prevWindowState == WindowState::Hidden) {
            macUtils()->hideOwnWindow();
        } else {
            macUtils()->raiseLastActiveWindow();
        }
#else
        if (m_prevWindowState == WindowState::Hidden) {
            getMainWindow()->hideWindow();
        } else {
            getMainWindow()->lower();
        }
#endif
    }
}

void BrowserService::raiseWindow(const bool force)
{
    m_prevWindowState = WindowState::Normal;
    if (getMainWindow()->isMinimized()) {
        m_prevWindowState = WindowState::Minimized;
    }
#ifdef Q_OS_MACOS
    Q_UNUSED(force)

    if (macUtils()->isHidden()) {
        m_prevWindowState = WindowState::Hidden;
    }
    macUtils()->raiseOwnWindow();
    Tools::wait(500);
#else
    if (getMainWindow()->isHidden()) {
        m_prevWindowState = WindowState::Hidden;
    }

    if (force) {
        getMainWindow()->bringToFront();
    }
#endif
}

void BrowserService::updateWindowState()
{
    m_prevWindowState = WindowState::Normal;
    if (getMainWindow()->isMinimized()) {
        m_prevWindowState = WindowState::Minimized;
    }
#ifdef Q_OS_MACOS
    if (macUtils()->isHidden()) {
        m_prevWindowState = WindowState::Hidden;
    }
#else
    if (getMainWindow()->isHidden()) {
        m_prevWindowState = WindowState::Hidden;
    }
#endif
}

void BrowserService::databaseLocked(DatabaseWidget* dbWidget)
{
    if (dbWidget) {
        QJsonObject msg;
        msg["action"] = QString("database-locked");
        m_browserHost->broadcastClientMessage(msg);
    }
}

void BrowserService::databaseUnlocked(DatabaseWidget* dbWidget)
{
    if (dbWidget) {
        if (m_bringToFrontRequested) {
            m_bringToFrontRequested = false;
            hideWindow();
        }

        QJsonObject msg;
        msg["action"] = QString("database-unlocked");
        m_browserHost->broadcastClientMessage(msg);
    }
}

void BrowserService::activeDatabaseChanged(DatabaseWidget* dbWidget)
{
    if (dbWidget) {
        if (dbWidget->isLocked()) {
            databaseLocked(dbWidget);
        } else {
            databaseUnlocked(dbWidget);
        }
    }

    m_currentDatabaseWidget = dbWidget;
}

void BrowserService::handleDatabaseUnlockDialogFinished(bool accepted, DatabaseWidget* dbWidget)
{
    // User canceled the database open dialog
    if (dbWidget && !accepted && m_bringToFrontRequested) {
        m_bringToFrontRequested = false;
        hideWindow();
    }
}

void BrowserService::processClientMessage(QLocalSocket* socket, const QJsonObject& message)
{
    auto clientID = message["clientID"].toString();
    if (clientID.isEmpty()) {
        return;
    }

    // Create a new client action if we haven't seen this id yet
    if (!m_browserClients.contains(clientID)) {
        m_browserClients.insert(clientID, QSharedPointer<BrowserAction>::create());
    }

    auto& action = m_browserClients.value(clientID);
    auto response = action->processClientMessage(socket, message);
    m_browserHost->sendClientMessage(socket, response);
}
