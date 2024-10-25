/*
 *  Copyright (C) 2017 Toni Spets <toni.spets@iki.fi>
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

#include "SSHAgent.h"

#include "core/Config.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "sshagent/BinaryStream.h"
#include "sshagent/KeeAgentSettings.h"

#include <QFileInfo>
#include <QLocalSocket>
#include <QThread>

#ifdef Q_OS_WIN
#include <QtEndian>
#include <windows.h>
#endif

Q_GLOBAL_STATIC(SSHAgent, s_sshAgent);

SSHAgent* SSHAgent::instance()
{
    return s_sshAgent;
}

bool SSHAgent::isEnabled() const
{
    return config()->get(Config::SSHAgent_Enabled).toBool();
}

void SSHAgent::setEnabled(bool enabled)
{
    if (isEnabled() && !enabled) {
        removeAllIdentities();
    }

    config()->set(Config::SSHAgent_Enabled, enabled);

    emit enabledChanged(enabled);
}

QString SSHAgent::authSockOverride() const
{
    return config()->get(Config::SSHAgent_AuthSockOverride).toString();
}

QString SSHAgent::securityKeyProviderOverride() const
{
    return config()->get(Config::SSHAgent_SecurityKeyProviderOverride).toString();
}

void SSHAgent::setAuthSockOverride(QString& authSockOverride)
{
    config()->set(Config::SSHAgent_AuthSockOverride, authSockOverride);
}

void SSHAgent::setSecurityKeyProviderOverride(QString& securityKeyProviderOverride)
{
    config()->set(Config::SSHAgent_SecurityKeyProviderOverride, securityKeyProviderOverride);
}

#ifdef Q_OS_WIN
bool SSHAgent::useOpenSSH() const
{
    return config()->get(Config::SSHAgent_UseOpenSSH).toBool();
}

bool SSHAgent::usePageant() const
{
    return config()->get(Config::SSHAgent_UsePageant).toBool();
}

void SSHAgent::setUseOpenSSH(bool useOpenSSH)
{
    config()->set(Config::SSHAgent_UseOpenSSH, useOpenSSH);
}

void SSHAgent::setUsePageant(bool usePageant)
{
    config()->set(Config::SSHAgent_UsePageant, usePageant);
}
#endif

QString SSHAgent::socketPath(bool allowOverride) const
{
    QString socketPath;

#ifndef Q_OS_WIN
    if (allowOverride) {
        socketPath = authSockOverride();
    }

    // if the overridden path is empty (no override set), default to environment
    if (socketPath.isEmpty()) {
        socketPath = QProcessEnvironment::systemEnvironment().value("SSH_AUTH_SOCK");
    }
#else
    Q_UNUSED(allowOverride)
    socketPath = "\\\\.\\pipe\\openssh-ssh-agent";
#endif

    return socketPath;
}

QString SSHAgent::securityKeyProvider(bool allowOverride) const
{
    QString skProvider;

    if (allowOverride) {
        skProvider = securityKeyProviderOverride();
    }

    if (skProvider.isEmpty()) {
        skProvider = QProcessEnvironment::systemEnvironment().value("SSH_SK_PROVIDER", "internal");
    }

    return skProvider;
}

const QString SSHAgent::errorString() const
{
    return m_error;
}

bool SSHAgent::isAgentRunning() const
{
#ifndef Q_OS_WIN
    QFileInfo socketFileInfo(socketPath());
    return !socketFileInfo.path().isEmpty() && socketFileInfo.exists();
#else
    if (usePageant() && useOpenSSH()) {
        return (FindWindowA("Pageant", "Pageant") != nullptr) && WaitNamedPipe(socketPath().toLatin1().data(), 100);
    } else if (useOpenSSH()) {
        return WaitNamedPipe(socketPath().toLatin1().data(), 100);
    } else if (usePageant()) {
        return (FindWindowA("Pageant", "Pageant") != nullptr);
    } else {
        return false;
    }
#endif
}

bool SSHAgent::sendMessage(const QByteArray& in, QByteArray& out)
{
#ifdef Q_OS_WIN
    if (usePageant() && !sendMessagePageant(in, out)) {
        return false;
    }
    if (useOpenSSH() && !sendMessageOpenSSH(in, out)) {
        return false;
    }
    return true;
#else
    return sendMessageOpenSSH(in, out);
#endif
}

bool SSHAgent::sendMessageOpenSSH(const QByteArray& in, QByteArray& out)
{
    QLocalSocket socket;
    BinaryStream stream(&socket);

    socket.connectToServer(socketPath());
    if (!socket.waitForConnected(500)) {
        m_error = tr("Agent connection failed.");
        return false;
    }

    stream.writeString(in);
    stream.flush();

    if (!stream.readString(out)) {
        m_error = tr("Agent protocol error.");
        return false;
    }

    socket.close();
    return true;
}

#ifdef Q_OS_WIN
bool SSHAgent::sendMessagePageant(const QByteArray& in, QByteArray& out)
{
    HWND hWnd = FindWindowA("Pageant", "Pageant");

    if (!hWnd) {
        m_error = tr("Agent connection failed.");
        return false;
    }

    if (static_cast<quint32>(in.length()) > AGENT_MAX_MSGLEN - 4) {
        m_error = tr("Agent connection failed.");
        return false;
    }

    auto threadId = reinterpret_cast<qlonglong>(QThread::currentThreadId());
    QByteArray mapName = (QString("SSHAgentRequest%1").arg(threadId, 8, 16, QChar('0'))).toLatin1();

    HANDLE handle = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, AGENT_MAX_MSGLEN, mapName.data());

    if (!handle) {
        m_error = tr("Agent connection failed.");
        return false;
    }

    LPVOID ptr = MapViewOfFile(handle, FILE_MAP_WRITE, 0, 0, 0);

    if (!ptr) {
        CloseHandle(handle);
        m_error = tr("Agent connection failed.");
        return false;
    }

    quint32* requestLength = reinterpret_cast<quint32*>(ptr);
    void* requestData = reinterpret_cast<void*>(reinterpret_cast<char*>(ptr) + 4);

    *requestLength = qToBigEndian<quint32>(in.length());
    memcpy(requestData, in.data(), in.length());

    COPYDATASTRUCT data;
    data.dwData = AGENT_COPYDATA_ID;
    data.cbData = mapName.length() + 1;
    data.lpData = reinterpret_cast<LPVOID>(mapName.data());

    LRESULT res = SendMessageA(hWnd, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&data));

    if (res) {
        quint32 responseLength = qFromBigEndian<quint32>(*requestLength);
        if (responseLength <= AGENT_MAX_MSGLEN) {
            out.resize(responseLength);
            memcpy(out.data(), requestData, responseLength);
        } else {
            m_error = tr("Agent protocol error.");
        }
    } else {
        m_error = tr("Agent protocol error.");
    }

    UnmapViewOfFile(ptr);
    CloseHandle(handle);

    return (res > 0);
}
#endif

/**
 * Add the identity to the SSH agent.
 *
 * @param key identity / key to add
 * @param settings constraints (lifetime, confirm), remove-on-lock
 * @param databaseUuid database that owns the key for remove-on-lock
 * @return true on success
 */
bool SSHAgent::addIdentity(OpenSSHKey& key, const KeeAgentSettings& settings, const QUuid& databaseUuid)
{
    if (!isAgentRunning()) {
        m_error = tr("No agent running, cannot add identity.");
        return false;
    }

    if (m_addedKeys.contains(key) && m_addedKeys[key].first != databaseUuid) {
        m_error = tr("Key identity ownership conflict. Refusing to add.");
        return false;
    }

    QByteArray requestData;
    BinaryStream request(&requestData);
    bool isSecurityKey = key.type().startsWith("sk-");

    request.write(
        (settings.useLifetimeConstraintWhenAdding() || settings.useConfirmConstraintWhenAdding() || isSecurityKey)
            ? SSH_AGENTC_ADD_ID_CONSTRAINED
            : SSH_AGENTC_ADD_IDENTITY);
    key.writePrivate(request);

    if (settings.useLifetimeConstraintWhenAdding()) {
        request.write(SSH_AGENT_CONSTRAIN_LIFETIME);
        request.write(static_cast<quint32>(settings.lifetimeConstraintDuration()));
    }

    if (settings.useConfirmConstraintWhenAdding()) {
        request.write(SSH_AGENT_CONSTRAIN_CONFIRM);
    }

    if (isSecurityKey) {
        request.write(SSH_AGENT_CONSTRAIN_EXTENSION);
        request.writeString(QString("sk-provider@openssh.com"));
        request.writeString(securityKeyProvider());
    }

    QByteArray responseData;
    if (!sendMessage(requestData, responseData)) {
        return false;
    }

    if (responseData.length() < 1 || static_cast<quint8>(responseData[0]) != SSH_AGENT_SUCCESS) {
        m_error =
            tr("Agent refused this identity. Possible reasons include:") + "\n" + tr("The key has already been added.");

        if (settings.useLifetimeConstraintWhenAdding()) {
            m_error += "\n" + tr("Restricted lifetime is not supported by the agent (check options).");
        }

        if (settings.useConfirmConstraintWhenAdding()) {
            m_error += "\n" + tr("A confirmation request is not supported by the agent (check options).");
        }

        if (isSecurityKey) {
            m_error +=
                "\n" + tr("Security keys are not supported by the agent or the security key provider is unavailable.");
        }

        return false;
    }

    OpenSSHKey keyCopy = key;
    keyCopy.clearPrivate();
    m_addedKeys[keyCopy] = qMakePair(databaseUuid, settings.removeAtDatabaseClose());
    return true;
}

/**
 * Remove an identity from the SSH agent.
 *
 * @param key identity to remove
 * @return true on success
 */
bool SSHAgent::removeIdentity(OpenSSHKey& key)
{
    if (!isAgentRunning()) {
        m_error = tr("No agent running, cannot remove identity.");
        return false;
    }

    QByteArray requestData;
    BinaryStream request(&requestData);

    QByteArray keyData;
    BinaryStream keyStream(&keyData);
    key.writePublic(keyStream);

    request.write(SSH_AGENTC_REMOVE_IDENTITY);
    request.writeString(keyData);

    QByteArray responseData;
    return sendMessage(requestData, responseData);
}

/**
 * Remove all identities from the SSH agent.
 *
 * Since the agent might be forwarded, old or non-OpenSSH, when asked
 * to remove all keys, attempt to remove both protocol v.1 and v.2
 * keys.
 *
 * @return true on success
 */
bool SSHAgent::clearAllAgentIdentities()
{
    if (!isAgentRunning()) {
        m_error = tr("No agent running, cannot remove identity.");
        return false;
    }

    bool ret = true;
    QByteArray requestData;
    QByteArray responseData;
    BinaryStream request(&requestData);

    // Same request order as OpenBSD ssh-add: useful?
    request.write(SSH2_AGENTC_REMOVE_ALL_IDENTITIES);

    if (!sendMessage(requestData, responseData)) {
        m_error = tr("Failed to remove all SSH identities from agent.");
        ret = false;
    }

    request.flush();
    responseData.clear();

    // Same request order as OpenBSD ssh-add: useful?
    request.write(SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES);

    // ignore error-code for ssh1
    sendMessage(requestData, responseData);

    m_error = tr("All SSH identities removed from agent.");
    return ret;
}

/**
 * Get a list of identities from the SSH agent.
 *
 * @param list list of keys to append
 * @return true on success
 */
bool SSHAgent::listIdentities(QList<QSharedPointer<OpenSSHKey>>& list)
{
    if (!isAgentRunning()) {
        m_error = tr("No agent running, cannot list identities.");
        return false;
    }

    QByteArray requestData;
    BinaryStream request(&requestData);

    request.write(SSH_AGENTC_REQUEST_IDENTITIES);

    QByteArray responseData;
    if (!sendMessage(requestData, responseData)) {
        return false;
    }

    BinaryStream response(&responseData);

    quint8 responseType;
    if (!response.read(responseType) || responseType != SSH_AGENT_IDENTITIES_ANSWER) {
        m_error = tr("Agent protocol error.");
        return false;
    }

    quint32 nKeys;
    if (!response.read(nKeys)) {
        m_error = tr("Agent protocol error.");
        return false;
    }

    for (quint32 i = 0; i < nKeys; i++) {
        QByteArray publicData;
        QString comment;

        if (!response.readString(publicData)) {
            m_error = tr("Agent protocol error.");
            return false;
        }

        if (!response.readString(comment)) {
            m_error = tr("Agent protocol error.");
            return false;
        }

        OpenSSHKey* key = new OpenSSHKey();
        key->setComment(comment);

        list.append(QSharedPointer<OpenSSHKey>(key));

        BinaryStream publicDataStream(&publicData);
        if (!key->readPublic(publicDataStream)) {
            m_error = key->errorString();
            return false;
        }
    }

    return true;
}

/**
 * Check if this identity is loaded in the SSH Agent.
 *
 * @param key identity to remove
 * @param loaded is the key loaded
 * @return true on success
 */
bool SSHAgent::checkIdentity(const OpenSSHKey& key, bool& loaded)
{
    QList<QSharedPointer<OpenSSHKey>> list;

    if (!listIdentities(list)) {
        return false;
    }

    loaded = false;

    for (const auto& it : list) {
        if (*it == key) {
            loaded = true;
            break;
        }
    }

    return true;
}

/**
 * Remove all identities known to this instance
 */
void SSHAgent::removeAllIdentities()
{
    auto it = m_addedKeys.begin();
    while (it != m_addedKeys.end()) {
        // Remove key if requested to remove on lock
        if (it.value().second) {
            OpenSSHKey key = it.key();
            removeIdentity(key);
        }
        it = m_addedKeys.erase(it);
    }
}

/**
 * Change "remove identity on lock" setting for a key already added to the agent.
 * Will to nothing if the key has not been added to the agent.
 *
 * @param key key to change setting for
 * @param autoRemove whether to remove the key from the agent when database is locked
 */
void SSHAgent::setAutoRemoveOnLock(const OpenSSHKey& key, bool autoRemove)
{
    if (m_addedKeys.contains(key)) {
        m_addedKeys[key].second = autoRemove;
    }
}

void SSHAgent::databaseLocked(const QSharedPointer<Database>& db)
{
    if (!db) {
        return;
    }

    auto it = m_addedKeys.begin();
    while (it != m_addedKeys.end()) {
        if (it.value().first != db->uuid()) {
            ++it;
            continue;
        }
        OpenSSHKey key = it.key();
        if (it.value().second) {
            if (!removeIdentity(key)) {
                emit error(m_error);
            }
        }
        it = m_addedKeys.erase(it);
    }
}

void SSHAgent::databaseUnlocked(const QSharedPointer<Database>& db)
{
    if (!db || !isEnabled()) {
        return;
    }

    for (auto entry : db->rootGroup()->entriesRecursive()) {
        if (entry->isRecycled()) {
            continue;
        }

        KeeAgentSettings settings;

        if (!settings.fromEntry(entry)) {
            continue;
        }

        if (!settings.allowUseOfSshKey() || !settings.addAtDatabaseOpen()) {
            continue;
        }

        OpenSSHKey key;

        if (!settings.toOpenSSHKey(entry, key, true)) {
            continue;
        }

        // Add key to agent; ignore errors if we have previously added the key
        bool known_key = m_addedKeys.contains(key);
        if (!addIdentity(key, settings, db->uuid()) && !known_key) {
            emit error(m_error);
        }
    }
}
