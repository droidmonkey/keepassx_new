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

#ifndef KEEPASSXC_SSHAGENT_H
#define KEEPASSXC_SSHAGENT_H

#include <QHash>

#include "OpenSSHKey.h"

class KeeAgentSettings;
class Database;

class SSHAgent : public QObject
{
    Q_OBJECT

public:
    ~SSHAgent() override = default;
    static SSHAgent* instance();

    bool isEnabled() const;
    void setEnabled(bool enabled);
    QString socketPath(bool allowOverride = true) const;
    QString securityKeyProvider(bool allowOverride = true) const;
    QString authSockOverride() const;
    QString securityKeyProviderOverride() const;
    void setAuthSockOverride(QString& authSockOverride);
    void setSecurityKeyProviderOverride(QString& securityKeyProviderOverride);
#ifdef Q_OS_WIN
    bool useOpenSSH() const;
    bool usePageant() const;
    void setUseOpenSSH(bool useOpenSSH);
    void setUsePageant(bool usePageant);
#endif

    const QString errorString() const;
    bool isAgentRunning() const;
    bool addIdentity(OpenSSHKey& key, const KeeAgentSettings& settings, const QUuid& databaseUuid);
    bool listIdentities(QList<QSharedPointer<OpenSSHKey>>& list);
    bool checkIdentity(const OpenSSHKey& key, bool& loaded);
    bool removeIdentity(OpenSSHKey& key);
    void removeAllIdentities();
    bool clearAllAgentIdentities();
    void setAutoRemoveOnLock(const OpenSSHKey& key, bool autoRemove);

signals:
    void error(const QString& message);
    void enabledChanged(bool enabled);

public slots:
    void databaseLocked(const QSharedPointer<Database>& db);
    void databaseUnlocked(const QSharedPointer<Database>& db);

private:
    const quint8 SSH_AGENT_FAILURE = 5;
    const quint8 SSH_AGENT_SUCCESS = 6;
    const quint8 SSH_AGENTC_REQUEST_IDENTITIES = 11;
    const quint8 SSH_AGENT_IDENTITIES_ANSWER = 12;
    const quint8 SSH_AGENTC_ADD_IDENTITY = 17;
    const quint8 SSH_AGENTC_REMOVE_IDENTITY = 18;
    const quint8 SSH_AGENTC_ADD_ID_CONSTRAINED = 25;
    const quint8 SSH_AGENTC_REMOVE_ALL_RSA_IDENTITIES = 9;
    const quint8 SSH2_AGENTC_REMOVE_ALL_IDENTITIES = 19;

    const quint8 SSH_AGENT_CONSTRAIN_LIFETIME = 1;
    const quint8 SSH_AGENT_CONSTRAIN_CONFIRM = 2;
    const quint8 SSH_AGENT_CONSTRAIN_EXTENSION = 255;

    bool sendMessage(const QByteArray& in, QByteArray& out);
    bool sendMessageOpenSSH(const QByteArray& in, QByteArray& out);
#ifdef Q_OS_WIN
    bool sendMessagePageant(const QByteArray& in, QByteArray& out);

    const quint32 AGENT_MAX_MSGLEN = 8192;
    const quint32 AGENT_COPYDATA_ID = 0x804e50ba;
#endif

    QHash<OpenSSHKey, QPair<QUuid, bool>> m_addedKeys;
    QString m_error;
};

static inline SSHAgent* sshAgent()
{
    return SSHAgent::instance();
}

#endif // KEEPASSXC_SSHAGENT_H
