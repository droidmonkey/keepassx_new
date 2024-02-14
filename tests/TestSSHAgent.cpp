/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
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

#include "TestSSHAgent.h"
#include "config-keepassx-tests.h"
#include "core/Config.h"
#include "crypto/Crypto.h"
#include "sshagent/KeeAgentSettings.h"
#include "sshagent/OpenSSHKeyGen.h"
#include "sshagent/SSHAgent.h"

#include <QTest>
#include <QVersionNumber>

QTEST_GUILESS_MAIN(TestSSHAgent)

static const QList<KeeAgentSettings::KeySpec> githubKeys = {
    {
        .key = "ssh-rsa "
               "AAAAB3NzaC1yc2EAAAADAQABAAABgQCj7ndNxQowgcQnjshcLrqPEiiphnt+"
               "VTTvDP6mHBL9j1aNUkY4Ue1gvwnGLVlOhGeYrnZaMgRK6+PKCUXaDbC7qtbW8gIkhL7aGCsOr/C56SJMy/"
               "BCZfxd1nWzAOxSDPgVsmerOBYfNqltV9/"
               "hWCqBywINIR+5dIg6JTJ72pcEpEjcYgXkE2YEFXV1JHnsKgbLWNlhScqb2UmyRkQyytRLtL+38TGxkxCflmO+"
               "5Z8CSSNY7GidjMIZ7Q4zMjA2n1nGrlTDkzwDCsw+"
               "wqFPGQA179cnfGWOWRVruj16z6XyvxvjJwbz0wQZ75XK5tKSb7FNyeIEs4TT4jk+S4dhPeAUC5y+"
               "bDYirYgM4GC7uEnztnZyaVWQ7B381AK4Qdrwt51ZqExKbQpTUNn+EjqoTwvqNj4kqx5QUCI0ThS/"
               "YkOxJCXmPUWZbhjpCg56i+2aB6CmK2JGhn57K5mj0MNdBXA4/WnwH6XoPWJzK5Nyu2zB3nAZp+S5hpQs+p1vN1/wsjk=",
        .isCertificateAuthority = false,
    },
    {
        .key = "ecdsa-sha2-nistp256 "
               "AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBEmKSENjQEezOmxkZMy7opKgwFB9nkt5YRrYMjNuG5N"
               "87uRgg6CLrbo5wAdT/y6v0mKV0U2w0WZ2YB/++Tpockg=",
        .isCertificateAuthority = false,
    },
    {
        .key = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIOMqqnkVzrm0SdG6UOoqKLsabgH5C9okWi0dh2l9GKJl",
        .isCertificateAuthority = false,
    }};

static const QList<KeeAgentSettings::KeySpec> gitlabKeys = {
    {
        .key = "ssh-rsa "
               "AAAAB3NzaC1yc2EAAAADAQABAAABAQCsj2bNKTBSpIYDEGk9KxsGh3mySTRgMtXL583qmBpzeQ+jqCMRgBqB98u3z++"
               "J1sKlXHWfM9dyhSevkMwSbhoR8XIq/U0tCNyokEi/"
               "ueaBMCvbcTHhO7FcwzY92WK4Yt0aGROY5qX2UKSeOvuP4D6TPqKF1onrSzH9bx9XUf2lEdWT/ia1NEKjunUqu1xOB/"
               "StKDHMoX4/OKyIzuS0q/"
               "T1zOATthvasJFoPrAjkohTyaDUz2LN5JoH839hViyEG82yB+MjcFV5MU3N1l1QL3cVUCh93xSaua1N85qivl+"
               "siMkPGbO5xR/En4iEY6K2XPASUEMaieWVNTRCtJ4S8H+9",
        .isCertificateAuthority = false,
    },
    {
        .key = "ecdsa-sha2-nistp256 "
               "AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBFSMqzJeV9rUzU4kWitGjeR4PWSa29SPqJ1fVkhtj3H"
               "w9xjLVXVYrU9QlYWrOLXBpQ6KWjbjTDTdDkoohFzgbEY=",
        .isCertificateAuthority = false,
    },
    {
        .key = "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIAfuCHKVTjquxvt6CM6tdG4SLp1Btn/nOeHHE5UOzRdf",
        .isCertificateAuthority = false,
    }};

void TestSSHAgent::initTestCase()
{
    QVERIFY(Crypto::init());
    Config::createTempFileInstance();

    m_agentSocketFile.setAutoRemove(true);
    QVERIFY(m_agentSocketFile.open());

    m_agentSocketFileName = m_agentSocketFile.fileName();
    QVERIFY(!m_agentSocketFileName.isEmpty());

    // let ssh-agent re-create it as a socket
    QVERIFY(m_agentSocketFile.remove());

    QStringList arguments;
    arguments << "-D" << "-a" << m_agentSocketFileName;

    QElapsedTimer timer;
    timer.start();

    qDebug() << "ssh-agent starting with arguments" << arguments;
    m_agentProcess.setProcessChannelMode(QProcess::ForwardedChannels);
    m_agentProcess.start("ssh-agent", arguments);
    m_agentProcess.closeWriteChannel();

    if (!m_agentProcess.waitForStarted()) {
        QSKIP("ssh-agent could not be started");
    }

    qDebug() << "ssh-agent started as pid" << m_agentProcess.processId();

    // we need to wait for the agent to open the socket before going into real tests
    QFileInfo socketFileInfo(m_agentSocketFileName);
    while (!timer.hasExpired(2000)) {
        if (socketFileInfo.exists()) {
            break;
        }
        QTest::qWait(10);
    }

    QVERIFY(socketFileInfo.exists());
    qDebug() << "ssh-agent initialized in" << timer.elapsed() << "ms";

    // initialize test key
    const QString keyString = QString("-----BEGIN OPENSSH PRIVATE KEY-----\n"
                                      "b3BlbnNzaC1rZXktdjEAAAAABG5vbmUAAAAEbm9uZQAAAAAAAAABAAAAMwAAAAtzc2gtZW\n"
                                      "QyNTUxOQAAACDdlO5F2kF2WzedrBAHBi9wBHeISzXZ0IuIqrp0EzeazAAAAKjgCfj94An4\n"
                                      "/QAAAAtzc2gtZWQyNTUxOQAAACDdlO5F2kF2WzedrBAHBi9wBHeISzXZ0IuIqrp0EzeazA\n"
                                      "AAAEBe1iilZFho8ZGAliiSj5URvFtGrgvmnEKdiLZow5hOR92U7kXaQXZbN52sEAcGL3AE\n"
                                      "d4hLNdnQi4iqunQTN5rMAAAAH29wZW5zc2hrZXktdGVzdC1wYXJzZUBrZWVwYXNzeGMBAg\n"
                                      "MEBQY=\n"
                                      "-----END OPENSSH PRIVATE KEY-----\n");

    const QByteArray keyData = keyString.toLatin1();

    QVERIFY(m_key.parsePKCS1PEM(keyData));
}

void TestSSHAgent::testConfiguration()
{
    SSHAgent agent;

    // default config must not enable agent
    QVERIFY(!agent.isEnabled());

    agent.setEnabled(true);
    QVERIFY(agent.isEnabled());

    // this will either be an empty string or the real ssh-agent socket path, doesn't matter
    QString defaultSocketPath = agent.socketPath(false);

    // overridden path must match default before setting an override
    QCOMPARE(agent.socketPath(true), defaultSocketPath);

    agent.setAuthSockOverride(m_agentSocketFileName);

    // overridden path must match what we set
    QCOMPARE(agent.socketPath(true), m_agentSocketFileName);

    // non-overridden path must match the default
    QCOMPARE(agent.socketPath(false), defaultSocketPath);
}

void TestSSHAgent::testKeeAgentSettings()
{
    KeeAgentSettings settings;
    KeeAgentSettings settings2;

    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings == settings2);

    QVERIFY(!settings.allowUseOfSshKey());
    settings.setAllowUseOfSshKey(true);
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.allowUseOfSshKey());
    QVERIFY(settings == settings2);

    QVERIFY(!settings.addAtDatabaseOpen());
    settings.setAddAtDatabaseOpen(true);
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.addAtDatabaseOpen());
    QVERIFY(settings == settings2);

    QVERIFY(!settings.removeAtDatabaseClose());
    settings.setRemoveAtDatabaseClose(true);
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.removeAtDatabaseClose());
    QVERIFY(settings == settings2);

    QVERIFY(!settings.useConfirmConstraintWhenAdding());
    settings.setUseConfirmConstraintWhenAdding(true);
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.useConfirmConstraintWhenAdding());
    QVERIFY(settings == settings2);

    QVERIFY(!settings.useLifetimeConstraintWhenAdding());
    settings.setUseLifetimeConstraintWhenAdding(true);
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.useLifetimeConstraintWhenAdding());
    QVERIFY(settings == settings2);

    QVERIFY(settings.lifetimeConstraintDuration() == 600);
    settings.setLifetimeConstraintDuration(120);
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.lifetimeConstraintDuration());
    QVERIFY(settings == settings2);

    QVERIFY(settings.fileName().isEmpty());
    settings.setFileName("dummy.pkey");
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.fileName() == "dummy.pkey");
    QVERIFY(settings == settings2);

    QVERIFY(settings.selectedType() == "file");
    settings.setSelectedType(QStringLiteral("attachment"));
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.selectedType() == "attachment");
    QVERIFY(settings == settings2);

    QVERIFY(settings.attachmentName().isEmpty());
    settings.setAttachmentName("dummy.pkey");
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.attachmentName() == "dummy.pkey");
    QVERIFY(settings == settings2);

    QVERIFY(!settings.saveAttachmentToTempFile());
    settings.setSaveAttachmentToTempFile(true);
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.saveAttachmentToTempFile());
    QVERIFY(settings == settings2);

    QVERIFY(!settings.useDestinationConstraintsWhenAdding());
    settings.setUseDestinationConstraintsWhenAdding(true);
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.useDestinationConstraintsWhenAdding());
    QVERIFY(settings == settings2);

    QList<KeeAgentSettings::DestinationConstraint> destinationConstraints;

    // ssh-add -h github.com <keyfile>
    destinationConstraints.append({
        .fromHost = "",
        .fromHostKeys = {},
        .toUser = "",
        .toHost = "github.com",
        .toHostKeys = githubKeys,
    });
    QVERIFY(settings.destinationConstraints().isEmpty());
    settings.setDestinationConstraints(destinationConstraints);
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.destinationConstraints() == destinationConstraints);
    QVERIFY(settings == settings2);

    // ssh-add -h github.com -h "github.com>git@gitlab.com" <keyfile>
    destinationConstraints.append({
        .fromHost = "github.com",
        .fromHostKeys = githubKeys,
        .toUser = "git",
        .toHost = "gitlab.com",
        .toHostKeys = gitlabKeys,
    });
    settings.setDestinationConstraints(destinationConstraints);
    QVERIFY(settings2.fromXml(settings.toXml()));
    QVERIFY(settings2.destinationConstraints() == destinationConstraints);
    QVERIFY(settings == settings2);
}

void TestSSHAgent::testIdentity()
{
    SSHAgent agent;
    agent.setEnabled(true);
    agent.setAuthSockOverride(m_agentSocketFileName);

    QVERIFY(agent.isAgentRunning());

    KeeAgentSettings settings;
    bool keyInAgent;

    // test adding a key works
    QVERIFY(agent.addIdentity(m_key, settings, m_uuid));
    QVERIFY(agent.checkIdentity(m_key, keyInAgent) && keyInAgent);

    // test non-conflicting key ownership doesn't throw an error
    QVERIFY(agent.addIdentity(m_key, settings, m_uuid));

    // test conflicting key ownership throws an error
    QUuid secondUuid("{11111111-1111-1111-1111-111111111111}");
    QVERIFY(!agent.addIdentity(m_key, settings, secondUuid));

    // test removing a key works
    QVERIFY(agent.removeIdentity(m_key));
    QVERIFY(agent.checkIdentity(m_key, keyInAgent) && !keyInAgent);
}

void TestSSHAgent::testRemoveOnClose()
{
    SSHAgent agent;
    agent.setEnabled(true);
    agent.setAuthSockOverride(m_agentSocketFileName);

    QVERIFY(agent.isAgentRunning());

    KeeAgentSettings settings;
    bool keyInAgent;

    settings.setRemoveAtDatabaseClose(true);
    QVERIFY(agent.addIdentity(m_key, settings, m_uuid));
    QVERIFY(agent.checkIdentity(m_key, keyInAgent) && keyInAgent);
    agent.setEnabled(false);
    QVERIFY(agent.checkIdentity(m_key, keyInAgent) && !keyInAgent);
}

void TestSSHAgent::testLifetimeConstraint()
{
    SSHAgent agent;
    agent.setEnabled(true);
    agent.setAuthSockOverride(m_agentSocketFileName);

    QVERIFY(agent.isAgentRunning());

    KeeAgentSettings settings;
    bool keyInAgent;

    settings.setUseLifetimeConstraintWhenAdding(true);
    settings.setLifetimeConstraintDuration(2); // two seconds

    // identity should be in agent immediately after adding
    QVERIFY(agent.addIdentity(m_key, settings, m_uuid));
    QVERIFY(agent.checkIdentity(m_key, keyInAgent) && keyInAgent);

    QElapsedTimer timer;
    timer.start();

    // wait for the identity to time out
    while (!timer.hasExpired(5000)) {
        QVERIFY(agent.checkIdentity(m_key, keyInAgent));

        if (!keyInAgent) {
            break;
        }

        QTest::qWait(100);
    }

    QVERIFY(!keyInAgent);
}

void TestSSHAgent::testConfirmConstraint()
{
    SSHAgent agent;
    agent.setEnabled(true);
    agent.setAuthSockOverride(m_agentSocketFileName);

    QVERIFY(agent.isAgentRunning());

    KeeAgentSettings settings;
    bool keyInAgent;

    settings.setUseConfirmConstraintWhenAdding(true);

    QVERIFY(agent.addIdentity(m_key, settings, m_uuid));

    // we can't test confirmation itself is working but we can test the agent accepts the key
    QVERIFY(agent.checkIdentity(m_key, keyInAgent) && keyInAgent);

    QVERIFY(agent.removeIdentity(m_key));
    QVERIFY(agent.checkIdentity(m_key, keyInAgent) && !keyInAgent);
}

void TestSSHAgent::testDestinationConstraints()
{
    // ssh-agent does not support destination constraints before OpenSSH
    // version 8.9. Therefore we want to skip this test on older versions.
    // Unfortunately ssh-agent does not give us any way to retrieve its version
    // number neither via protocol nor on the command line. Therefore we use
    // the version number of the SSH client and assume it to be the same.
    QProcess ssh;
    ssh.setReadChannel(QProcess::StandardError);
    ssh.start("ssh", QStringList() << "-V");
    ssh.waitForFinished();
    auto ssh_version = QString::fromUtf8(ssh.readLine());
    ssh_version.remove(QRegExp("^OpenSSH_"));
    if (QVersionNumber ::fromString(ssh_version) < QVersionNumber(8, 9)) {
        QSKIP("Test requires ssh-agent >= 8.9");
    }

    SSHAgent agent;
    agent.setEnabled(true);
    agent.setAuthSockOverride(m_agentSocketFileName);

    QVERIFY(agent.isAgentRunning());

    KeeAgentSettings settings;
    bool keyInAgent;

    // ssh-add -h github.com -h "github.com>git@gitlab.com" <keyfile>
    settings.setUseDestinationConstraintsWhenAdding(true);
    settings.setDestinationConstraints({{
                                            .fromHost = "",
                                            .fromHostKeys = {},
                                            .toUser = "",
                                            .toHost = "github.com",
                                            .toHostKeys = githubKeys,
                                        },
                                        {
                                            .fromHost = "github.com",
                                            .fromHostKeys = githubKeys,
                                            .toUser = "git",
                                            .toHost = "gitlab.com",
                                            .toHostKeys = gitlabKeys,
                                        }});

    QVERIFY(agent.addIdentity(m_key, settings, m_uuid));

    // we can't test destination constraints itself is working but we can test the agent accepts the key
    QVERIFY(agent.checkIdentity(m_key, keyInAgent) && keyInAgent);

    QVERIFY(agent.removeIdentity(m_key));
    QVERIFY(agent.checkIdentity(m_key, keyInAgent) && !keyInAgent);
}

void TestSSHAgent::testToOpenSSHKey()
{
    KeeAgentSettings settings;
    settings.setSelectedType("file");
    settings.setFileName(QString("%1/id_rsa-encrypted-asn1").arg(QString(KEEPASSX_TEST_DATA_DIR)));

    OpenSSHKey key;
    settings.toOpenSSHKey("username", "correctpassphrase", QString(), nullptr, key, false);

    QVERIFY(!key.publicKey().isEmpty());
}

void TestSSHAgent::testKeyGenRSA()
{
    SSHAgent agent;
    agent.setEnabled(true);
    agent.setAuthSockOverride(m_agentSocketFileName);

    QVERIFY(agent.isAgentRunning());

    OpenSSHKey key;
    KeeAgentSettings settings;
    bool keyInAgent;

    QVERIFY(OpenSSHKeyGen::generateRSA(key, 2048));

    QVERIFY(agent.addIdentity(key, settings, m_uuid));
    QVERIFY(agent.checkIdentity(key, keyInAgent) && keyInAgent);
    QVERIFY(agent.removeIdentity(key));
    QVERIFY(agent.checkIdentity(key, keyInAgent) && !keyInAgent);
}

void TestSSHAgent::testKeyGenECDSA()
{
    SSHAgent agent;
    agent.setEnabled(true);
    agent.setAuthSockOverride(m_agentSocketFileName);

    QVERIFY(agent.isAgentRunning());

    OpenSSHKey key;
    KeeAgentSettings settings;
    bool keyInAgent;

    QVERIFY(OpenSSHKeyGen::generateECDSA(key, 256));

    QVERIFY(agent.addIdentity(key, settings, m_uuid));
    QVERIFY(agent.checkIdentity(key, keyInAgent) && keyInAgent);
    QVERIFY(agent.removeIdentity(key));
    QVERIFY(agent.checkIdentity(key, keyInAgent) && !keyInAgent);
}

void TestSSHAgent::testKeyGenEd25519()
{
    SSHAgent agent;
    agent.setEnabled(true);
    agent.setAuthSockOverride(m_agentSocketFileName);

    QVERIFY(agent.isAgentRunning());

    OpenSSHKey key;
    KeeAgentSettings settings;
    bool keyInAgent;

    QVERIFY(OpenSSHKeyGen::generateEd25519(key));

    QVERIFY(agent.addIdentity(key, settings, m_uuid));
    QVERIFY(agent.checkIdentity(key, keyInAgent) && keyInAgent);
    QVERIFY(agent.removeIdentity(key));
    QVERIFY(agent.checkIdentity(key, keyInAgent) && !keyInAgent);
}

void TestSSHAgent::cleanupTestCase()
{
    if (m_agentProcess.state() != QProcess::NotRunning) {
        qDebug() << "Killing ssh-agent pid" << m_agentProcess.processId();
        m_agentProcess.terminate();
        m_agentProcess.waitForFinished();
    }

    m_agentSocketFile.remove();
}
