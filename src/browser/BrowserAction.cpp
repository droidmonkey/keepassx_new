/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#include "BrowserAction.h"
#include "BrowserMessageBuilder.h"
#include "BrowserPasskeys.h"
#include "BrowserSettings.h"
#include "PasskeyUtils.h"
#include "core/Global.h"
#include "core/Tools.h"

#include <QJsonDocument>
#include <QLocalSocket>

const int BrowserAction::MaxUrlLength = 256;

static const QString BROWSER_REQUEST_ASSOCIATE = QStringLiteral("associate");
static const QString BROWSER_REQUEST_CHANGE_PUBLIC_KEYS = QStringLiteral("change-public-keys");
static const QString BROWSER_REQUEST_CREATE_NEW_GROUP = QStringLiteral("create-new-group");
static const QString BROWSER_REQUEST_DELETE_ENTRY = QStringLiteral("delete-entry");
static const QString BROWSER_REQUEST_GENERATE_PASSWORD = QStringLiteral("generate-password");
static const QString BROWSER_REQUEST_GET_DATABASEHASH = QStringLiteral("get-databasehash");
static const QString BROWSER_REQUEST_GET_DATABASE_GROUPS = QStringLiteral("get-database-groups");
static const QString BROWSER_REQUEST_GET_LOGINS = QStringLiteral("get-logins");
static const QString BROWSER_REQUEST_GET_TOTP = QStringLiteral("get-totp");
static const QString BROWSER_REQUEST_LOCK_DATABASE = QStringLiteral("lock-database");
static const QString BROWSER_REQUEST_PASSKEYS_GET = QStringLiteral("passkeys-get");
static const QString BROWSER_REQUEST_PASSKEYS_REGISTER = QStringLiteral("passkeys-register");
static const QString BROWSER_REQUEST_REQUEST_AUTOTYPE = QStringLiteral("request-autotype");
static const QString BROWSER_REQUEST_SET_LOGIN = QStringLiteral("set-login");
static const QString BROWSER_REQUEST_TEST_ASSOCIATE = QStringLiteral("test-associate");

QJsonObject BrowserAction::processClientMessage(QLocalSocket* socket, const QJsonObject& json)
{
    if (json.isEmpty()) {
        return getErrorReply("", ERROR_KEEPASS_EMPTY_MESSAGE_RECEIVED);
    }

    bool triggerUnlock = false;
    const auto trigger = json.value("triggerUnlock").toString();
    if (!trigger.isEmpty() && trigger.compare(TRUE_STR) == 0) {
        triggerUnlock = true;
    }

    const auto action = json.value("action").toString();
    if (action.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    if (action.compare(BROWSER_REQUEST_CHANGE_PUBLIC_KEYS) != 0 && action.compare(BROWSER_REQUEST_REQUEST_AUTOTYPE) != 0
        && !browserService()->isDatabaseOpened()) {
        if (m_clientPublicKey.isEmpty()) {
            return getErrorReply(action, ERROR_KEEPASS_CLIENT_PUBLIC_KEY_NOT_RECEIVED);
        } else if (!browserService()->openDatabase(triggerUnlock)) {
            return getErrorReply(action, ERROR_KEEPASS_DATABASE_NOT_OPENED);
        }
    }

    return handleAction(socket, json);
}

// Private functions
///////////////////////

QJsonObject BrowserAction::handleAction(QLocalSocket* socket, const QJsonObject& json)
{
    QString action = json.value("action").toString();

    if (action.compare(BROWSER_REQUEST_CHANGE_PUBLIC_KEYS) == 0) {
        return handleChangePublicKeys(json, action);
    } else if (action.compare(BROWSER_REQUEST_GET_DATABASEHASH) == 0) {
        return handleGetDatabaseHash(json, action);
    } else if (action.compare(BROWSER_REQUEST_ASSOCIATE) == 0) {
        return handleAssociate(json, action);
    } else if (action.compare(BROWSER_REQUEST_TEST_ASSOCIATE) == 0) {
        return handleTestAssociate(json, action);
    } else if (action.compare(BROWSER_REQUEST_GET_LOGINS) == 0) {
        return handleGetLogins(json, action);
    } else if (action.compare(BROWSER_REQUEST_GENERATE_PASSWORD) == 0) {
        return handleGeneratePassword(socket, json, action);
    } else if (action.compare(BROWSER_REQUEST_SET_LOGIN) == 0) {
        return handleSetLogin(json, action);
    } else if (action.compare(BROWSER_REQUEST_LOCK_DATABASE) == 0) {
        return handleLockDatabase(json, action);
    } else if (action.compare(BROWSER_REQUEST_GET_DATABASE_GROUPS) == 0) {
        return handleGetDatabaseGroups(json, action);
    } else if (action.compare(BROWSER_REQUEST_CREATE_NEW_GROUP) == 0) {
        return handleCreateNewGroup(json, action);
    } else if (action.compare(BROWSER_REQUEST_GET_TOTP) == 0) {
        return handleGetTotp(json, action);
    } else if (action.compare(BROWSER_REQUEST_DELETE_ENTRY) == 0) {
        return handleDeleteEntry(json, action);
    } else if (action.compare(BROWSER_REQUEST_REQUEST_AUTOTYPE) == 0) {
        return handleGlobalAutoType(json, action);
    } else if (action.compare("get-database-entries", Qt::CaseSensitive) == 0) {
        return handleGetDatabaseEntries(json, action);
    } else if (action.compare(BROWSER_REQUEST_PASSKEYS_GET) == 0) {
        return handlePasskeysGet(json, action);
    } else if (action.compare(BROWSER_REQUEST_PASSKEYS_REGISTER) == 0) {
        return handlePasskeysRegister(json, action);
    }

    // Action was not recognized
    return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
}

QJsonObject BrowserAction::handleChangePublicKeys(const QJsonObject& json, const QString& action)
{
    const auto nonce = json.value("nonce").toString();
    const auto clientPublicKey = json.value("publicKey").toString();

    if (clientPublicKey.isEmpty() || nonce.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CLIENT_PUBLIC_KEY_NOT_RECEIVED);
    }

    m_associated = false;
    auto keyPair = browserMessageBuilder()->getKeyPair();
    if (keyPair.first.isEmpty() || keyPair.second.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_ENCRYPTION_KEY_UNRECOGNIZED);
    }

    m_clientPublicKey = clientPublicKey;
    m_publicKey = keyPair.first;
    m_secretKey = keyPair.second;

    auto response = browserMessageBuilder()->buildMessage(browserMessageBuilder()->incrementNonce(nonce));
    response["action"] = action;
    response["publicKey"] = keyPair.first;

    return response;
}

QJsonObject BrowserAction::handleGetDatabaseHash(const QJsonObject& json, const QString& action)
{
    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    if (browserRequest.hash.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_DATABASE_HASH_NOT_RECEIVED);
    }

    const auto command = browserRequest.getString("action");
    if (!command.isEmpty() && command.compare(BROWSER_REQUEST_GET_DATABASEHASH) == 0) {
        const Parameters params{{"hash", browserRequest.hash}};
        return buildResponse(action, browserRequest.incrementedNonce, params);
    }

    return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
}

QJsonObject BrowserAction::handleAssociate(const QJsonObject& json, const QString& action)
{
    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto key = browserRequest.getString("key");
    if (key.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    if (key.compare(m_clientPublicKey) == 0) {
        // Check for identification key. If it's not found, ensure backwards compatibility and use the current public
        // key
        const auto idKey = browserRequest.getString("idKey");
        const auto id = browserService()->storeKey((idKey.isEmpty() ? key : idKey));
        if (id.isEmpty()) {
            return getErrorReply(action, ERROR_KEEPASS_ACTION_CANCELLED_OR_DENIED);
        }

        m_associated = true;

        const Parameters params{{"hash", browserRequest.hash}, {"id", id}};
        return buildResponse(action, browserRequest.incrementedNonce, params);
    }

    return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
}

QJsonObject BrowserAction::handleTestAssociate(const QJsonObject& json, const QString& action)
{
    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto responseKey = browserRequest.getString("key");
    const auto id = browserRequest.getString("id");
    if (responseKey.isEmpty() || id.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_DATABASE_NOT_OPENED);
    }

    const auto key = browserService()->getKey(id);
    if (key.isEmpty() || key.compare(responseKey) != 0) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    m_associated = true;

    const Parameters params{{"hash", browserRequest.hash}, {"id", id}};
    return buildResponse(action, browserRequest.incrementedNonce, params);
}

QJsonObject BrowserAction::handleGetLogins(const QJsonObject& json, const QString& action)
{
    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto siteUrl = browserRequest.getString("url");
    if (siteUrl.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_NO_URL_PROVIDED);
    }

    const auto id = browserRequest.getString("id");
    const auto formUrl = browserRequest.getString("submitUrl");
    const auto auth = browserRequest.getString("httpAuth");
    const bool httpAuth = auth.compare(TRUE_STR) == 0;
    const auto keyList = getConnectionKeys(browserRequest);

    EntryParameters entryParameters;
    entryParameters.dbid = id;
    entryParameters.hash = browserRequest.hash;
    entryParameters.siteUrl = siteUrl;
    entryParameters.formUrl = formUrl;
    entryParameters.httpAuth = httpAuth;

    bool entriesFound = false;
    const auto entries = browserService()->findEntries(entryParameters, keyList, &entriesFound);
    if (!entriesFound) {
        return getErrorReply(action, ERROR_KEEPASS_NO_LOGINS_FOUND);
    }

    const Parameters params{
        {"count", entries.count()}, {"entries", entries}, {"hash", browserRequest.hash}, {"id", id}};
    return buildResponse(action, browserRequest.incrementedNonce, params);
}

QJsonObject BrowserAction::handleGeneratePassword(QLocalSocket* socket, const QJsonObject& json, const QString& action)
{
    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto requestId = browserRequest.getString("requestID");

    // Do not allow multiple requests from the same client
    if (browserService()->isPasswordGeneratorRequested()) {
        auto errorReply = getErrorReply(action, ERROR_KEEPASS_ACTION_CANCELLED_OR_DENIED);

        // Append requestID to the response if found
        if (!requestId.isEmpty()) {
            errorReply["requestID"] = requestId;
        }

        // Show the existing password generator
        browserService()->showPasswordGenerator({});
        return errorReply;
    }

    KeyPairMessage keyPairMessage{socket, browserRequest.incrementedNonce, m_clientPublicKey, m_secretKey};

    browserService()->showPasswordGenerator(keyPairMessage);
    return {};
}

QJsonObject BrowserAction::handleSetLogin(const QJsonObject& json, const QString& action)
{
    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto url = browserRequest.getString("url");
    if (url.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_NO_URL_PROVIDED);
    }

    const auto id = browserRequest.getString("id");
    const auto login = browserRequest.getString("login");
    const auto password = browserRequest.getString("password");
    const auto submitUrl = browserRequest.getString("submitUrl");
    const auto uuid = browserRequest.getString("uuid");
    const auto group = browserRequest.getString("group");
    const auto groupUuid = browserRequest.getString("groupUuid");
    const auto downloadFavicon = browserRequest.getString("downloadFavicon");
    const QString realm;

    EntryParameters entryParameters;
    entryParameters.dbid = id;
    entryParameters.login = login;
    entryParameters.password = password;
    entryParameters.siteUrl = url;
    entryParameters.formUrl = submitUrl;
    entryParameters.realm = realm;

    bool result = true;
    if (uuid.isEmpty()) {
        auto dlFavicon = !downloadFavicon.isEmpty() && downloadFavicon.compare(TRUE_STR) == 0;
        browserService()->addEntry(entryParameters, group, groupUuid, dlFavicon);
    } else {
        if (!Tools::isValidUuid(uuid)) {
            return getErrorReply(action, ERROR_KEEPASS_NO_VALID_UUID_PROVIDED);
        }

        result = browserService()->updateEntry(entryParameters, uuid);
    }

    const Parameters params{{"count", QJsonValue::Null},
                            {"entries", QJsonValue::Null},
                            {"error", result ? QStringLiteral("success") : QStringLiteral("error")},
                            {"hash", browserRequest.hash}};
    return buildResponse(action, browserRequest.incrementedNonce, params);
}

QJsonObject BrowserAction::handleLockDatabase(const QJsonObject& json, const QString& action)
{
    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    if (browserRequest.hash.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_DATABASE_HASH_NOT_RECEIVED);
    }

    const auto command = browserRequest.getString("action");
    if (!command.isEmpty() && command.compare(BROWSER_REQUEST_LOCK_DATABASE) == 0) {
        browserService()->lockDatabase();
        return buildResponse(action, browserRequest.incrementedNonce);
    }

    return getErrorReply(action, ERROR_KEEPASS_DATABASE_HASH_NOT_RECEIVED);
}

QJsonObject BrowserAction::handleGetDatabaseGroups(const QJsonObject& json, const QString& action)
{
    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto command = browserRequest.getString("action");
    if (command.isEmpty() || command.compare(BROWSER_REQUEST_GET_DATABASE_GROUPS) != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    const auto groups = browserService()->getDatabaseGroups();
    if (groups.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_NO_GROUPS_FOUND);
    }

    const Parameters params{{"groups", groups}};
    return buildResponse(action, browserRequest.incrementedNonce, params);
}

QJsonObject BrowserAction::handleGetDatabaseEntries(const QJsonObject& json, const QString& action)
{
    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto command = browserRequest.getString("action");
    if (command.isEmpty() || command.compare("get-database-entries") != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    if (!browserSettings()->allowGetDatabaseEntriesRequest()) {
        return getErrorReply(action, ERROR_KEEPASS_ACCESS_TO_ALL_ENTRIES_DENIED);
    }

    const QJsonArray entries = browserService()->getDatabaseEntries();
    if (entries.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_NO_GROUPS_FOUND);
    }

    const Parameters params{{"entries", entries}};

    return buildResponse(action, browserRequest.incrementedNonce, params);
}

QJsonObject BrowserAction::handleCreateNewGroup(const QJsonObject& json, const QString& action)
{
    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto command = browserRequest.getString("action");
    if (command.isEmpty() || command.compare(BROWSER_REQUEST_CREATE_NEW_GROUP) != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    const auto group = browserRequest.getString("groupName");
    const auto newGroup = browserService()->createNewGroup(group);
    if (newGroup.isEmpty() || newGroup["name"].toString().isEmpty() || newGroup["uuid"].toString().isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_CREATE_NEW_GROUP);
    }

    const Parameters params{{"name", newGroup["name"]}, {"uuid", newGroup["uuid"]}};
    return buildResponse(action, browserRequest.incrementedNonce, params);
}

QJsonObject BrowserAction::handleGetTotp(const QJsonObject& json, const QString& action)
{
    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto command = browserRequest.getString("action");
    if (command.isEmpty() || command.compare(BROWSER_REQUEST_GET_TOTP) != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    const auto uuid = browserRequest.getString("uuid");
    if (!Tools::isValidUuid(uuid)) {
        return getErrorReply(action, ERROR_KEEPASS_NO_VALID_UUID_PROVIDED);
    }

    const Parameters params{{"totp", browserService()->getCurrentTotp(uuid)}};
    return buildResponse(action, browserRequest.incrementedNonce, params);
}

QJsonObject BrowserAction::handleDeleteEntry(const QJsonObject& json, const QString& action)
{
    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto command = browserRequest.getString("action");
    if (command.isEmpty() || command.compare(BROWSER_REQUEST_DELETE_ENTRY) != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    const auto uuid = browserRequest.getString("uuid");
    if (!Tools::isValidUuid(uuid)) {
        return getErrorReply(action, ERROR_KEEPASS_NO_VALID_UUID_PROVIDED);
    }

    const auto result = browserService()->deleteEntry(uuid);

    const Parameters params{{"success", result ? TRUE_STR : FALSE_STR}};
    return buildResponse(action, browserRequest.incrementedNonce, params);
}

QJsonObject BrowserAction::handleGlobalAutoType(const QJsonObject& json, const QString& action)
{
    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto command = browserRequest.getString("action");
    if (command.isEmpty() || command.compare(BROWSER_REQUEST_REQUEST_AUTOTYPE) != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    const auto topLevelDomain = browserRequest.getString("search");
    if (topLevelDomain.length() > BrowserAction::MaxUrlLength) {
        return getErrorReply(action, ERROR_KEEPASS_NO_URL_PROVIDED);
    }

    browserService()->requestGlobalAutoType(topLevelDomain);

    return buildResponse(action, browserRequest.incrementedNonce);
}

QJsonObject BrowserAction::handlePasskeysGet(const QJsonObject& json, const QString& action)
{
    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto command = browserRequest.getString("action");
    if (command.isEmpty() || command.compare(BROWSER_REQUEST_PASSKEYS_GET) != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    const auto publicKey = browserRequest.getObject("publicKey");
    if (publicKey.isEmpty()) {
        return getErrorReply(action, ERROR_PASSKEYS_EMPTY_PUBLIC_KEY);
    }

    const auto origin = browserRequest.getString("origin");
    if (!passkeyUtils()->isOriginAllowedWithLocalhost(browserSettings()->allowLocalhostWithPasskeys(), origin)) {
        return getErrorReply(action, ERROR_PASSKEYS_INVALID_URL_PROVIDED);
    }

    const auto keyList = getConnectionKeys(browserRequest);
    const auto response = browserService()->showPasskeysAuthenticationPrompt(publicKey, origin, keyList);

    const Parameters params{{"response", response}};
    return buildResponse(action, browserRequest.incrementedNonce, params);
}

QJsonObject BrowserAction::handlePasskeysRegister(const QJsonObject& json, const QString& action)
{
    if (!m_associated) {
        return getErrorReply(action, ERROR_KEEPASS_ASSOCIATION_FAILED);
    }

    const auto browserRequest = decodeRequest(json);
    if (browserRequest.isEmpty()) {
        return getErrorReply(action, ERROR_KEEPASS_CANNOT_DECRYPT_MESSAGE);
    }

    const auto command = browserRequest.getString("action");
    if (command.isEmpty() || command.compare(BROWSER_REQUEST_PASSKEYS_REGISTER) != 0) {
        return getErrorReply(action, ERROR_KEEPASS_INCORRECT_ACTION);
    }

    const auto publicKey = browserRequest.getObject("publicKey");
    if (publicKey.isEmpty()) {
        return getErrorReply(action, ERROR_PASSKEYS_EMPTY_PUBLIC_KEY);
    }

    const auto origin = browserRequest.getString("origin");
    if (!passkeyUtils()->isOriginAllowedWithLocalhost(browserSettings()->allowLocalhostWithPasskeys(), origin)) {
        return getErrorReply(action, ERROR_PASSKEYS_INVALID_URL_PROVIDED);
    }

    const auto groupName = browserRequest.getString("groupName");
    const auto keyList = getConnectionKeys(browserRequest);
    const auto response = browserService()->showPasskeysRegisterPrompt(publicKey, origin, groupName, keyList);

    const Parameters params{{"response", response}};
    return buildResponse(action, browserRequest.incrementedNonce, params);
}

QJsonObject BrowserAction::decryptMessage(const QString& message, const QString& nonce)
{
    return browserMessageBuilder()->decryptMessage(message, nonce, m_clientPublicKey, m_secretKey);
}

QJsonObject BrowserAction::getErrorReply(const QString& action, const int errorCode) const
{
    return browserMessageBuilder()->getErrorReply(action, errorCode);
}

QJsonObject BrowserAction::buildResponse(const QString& action, const QString& nonce, const Parameters& params)
{
    return browserMessageBuilder()->buildResponse(action, nonce, params, m_clientPublicKey, m_secretKey);
}

BrowserRequest BrowserAction::decodeRequest(const QJsonObject& json)
{
    const auto nonce = json.value("nonce").toString();
    const auto encrypted = json.value("message").toString();

    return {browserService()->getDatabaseHash(),
            nonce,
            browserMessageBuilder()->incrementNonce(nonce),
            decryptMessage(encrypted, nonce)};
}

StringPairList BrowserAction::getConnectionKeys(const BrowserRequest& browserRequest)
{
    const auto keys = browserRequest.getArray("keys");

    StringPairList keyList;
    for (const auto val : keys) {
        const auto keyObject = val.toObject();
        keyList.push_back(qMakePair(keyObject.value("id").toString(), keyObject.value("key").toString()));
    }

    return keyList;
}
