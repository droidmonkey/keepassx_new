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

#include "Polkit.h"

#include "crypto/CryptoHash.h"
#include "crypto/Random.h"
#include "crypto/SymmetricCipher.h"
#include "gui/osutils/nixutils/NixUtils.h"

#include <QDebug>
#include <QFile>
#include <QtDBus>
#include <botan/mem_ops.h>
#include <cerrno>

extern "C" {
#include <keyutils.h>
}

const QString polkit_service = "org.freedesktop.PolicyKit1";
const QString polkit_object = "/org/freedesktop/PolicyKit1/Authority";

namespace
{
    QString getKeyName(const QUuid& dbUuid)
    {
        static const QString keyPrefix = "keepassxc_polkit_keys_";
        return keyPrefix + dbUuid.toString();
    }
} // namespace

Polkit::Polkit()
{
    PolkitSubject::registerMetaType();
    PolkitAuthorizationResults::registerMetaType();

    /* Note we explicitly use our own dbus path here, as the ::systemBus() method could be overridden
       through an environment variable to return an alternative bus path. This bus could have an application
       pretending to be polkit running on it, which could approve every authentication request

       Most Linux distros place the system bus at this exact path, so it is hard-coded.
       For any other distros, this path will need to be patched before compilation.
    */
    QDBusConnection bus =
        QDBusConnection::connectToBus("unix:path=/run/dbus/system_bus_socket", "keepassxc_polkit_dbus");

    m_available = bus.isConnected();
    if (!m_available) {
        qDebug() << "polkit: Failed to connect to system dbus (this may be due to a non-standard dbus path)";
        return;
    }

    m_available = bus.interface()->isServiceRegistered(polkit_service);

    if (!m_available) {
        qDebug() << "polkit: Polkit is not registered on dbus";
        return;
    }

    m_polkit.reset(new org::freedesktop::PolicyKit1::Authority(polkit_service, polkit_object, bus));
}

Polkit::~Polkit()
{
}

void Polkit::reset(const QUuid& dbUuid)
{
    m_encryptedMasterKeys.remove(dbUuid);
}

bool Polkit::isAvailable() const
{
    return m_available;
}

QString Polkit::errorString() const
{
    return m_error;
}

void Polkit::reset()
{
    m_encryptedMasterKeys.clear();
}

bool Polkit::setKey(const QUuid& dbUuid, const QByteArray& key)
{
    reset(dbUuid);

    // Generate a random iv/key pair to encrypt the master password with
    QByteArray randomKey = randomGen()->randomArray(SymmetricCipher::keySize(SymmetricCipher::Aes256_GCM));
    QByteArray randomIV = randomGen()->randomArray(SymmetricCipher::defaultIvSize(SymmetricCipher::Aes256_GCM));
    QByteArray keychainKeyValue = randomKey + randomIV;

    SymmetricCipher aes256Encrypt;
    if (!aes256Encrypt.init(SymmetricCipher::Aes256_GCM, SymmetricCipher::Encrypt, randomKey, randomIV)) {
        m_error = QObject::tr("Failed to init KeePassXC crypto.");
        return false;
    }

    // Encrypt the master password
    QByteArray encryptedMasterKey = key;
    if (!aes256Encrypt.finish(encryptedMasterKey)) {
        m_error = QObject::tr("Failed to encrypt key data.");
        qDebug() << "polkit aes encrypt failed: " << aes256Encrypt.errorString();
        return false;
    }

    // Add the iv/key pair into the linux keyring
    key_serial_t key_serial = add_key("user",
                                      getKeyName(dbUuid).toStdString().c_str(),
                                      keychainKeyValue.constData(),
                                      keychainKeyValue.size(),
                                      KEY_SPEC_PROCESS_KEYRING);
    if (key_serial < 0) {
        m_error = QObject::tr("Failed to store key in Linux Keyring. Quick unlock has not been enabled.");
        qDebug() << "polkit keyring failed to store: " << errno;
        return false;
    }

    // Scrub the keys from ram
    Botan::secure_scrub_memory(randomKey.data(), randomKey.size());
    Botan::secure_scrub_memory(randomIV.data(), randomIV.size());
    Botan::secure_scrub_memory(keychainKeyValue.data(), keychainKeyValue.size());

    // Store encrypted master password and return
    m_encryptedMasterKeys.insert(dbUuid, encryptedMasterKey);
    return true;
}

bool Polkit::getKey(const QUuid& dbUuid, QByteArray& key)
{
    if (!m_polkit || !hasKey(dbUuid)) {
        return false;
    }

    PolkitSubject subject;
    subject.kind = "unix-process";
    subject.details.insert("pid", static_cast<uint>(QCoreApplication::applicationPid()));
    subject.details.insert("start-time", nixUtils()->getProcessStartTime());

    QMap<QString, QString> details;

    auto result = m_polkit->CheckAuthorization(
        subject,
        "org.keepassxc.KeePassXC.unlockDatabase",
        details,
        0x00000001,
        // AllowUserInteraction - wait for user to authenticate
        // https://www.freedesktop.org/software/polkit/docs/0.105/eggdbus-interface-org.freedesktop.PolicyKit1.Authority.html#eggdbus-enum-CheckAuthorizationFlags
        "");

    // A general error occurred
    if (result.isError()) {
        auto msg = result.error().message();
        m_error = QObject::tr("Polkit returned an error: %1").arg(msg);
        qDebug() << "polkit returned an error: " << msg;
        return false;
    }

    PolkitAuthorizationResults authResult = result.value();
    if (authResult.is_authorized) {
        QByteArray encryptedMasterKey = m_encryptedMasterKeys.value(dbUuid);
        key_serial_t keySerial =
            find_key_by_type_and_desc("user", getKeyName(dbUuid).toStdString().c_str(), KEY_SPEC_PROCESS_KEYRING);

        if (keySerial == -1) {
            m_error = QObject::tr("Could not locate key in Linux Keyring.");
            qDebug() << "polkit keyring failed to find: " << errno;
            return false;
        }

        void* keychainBuffer;
        long keychainDataSize = keyctl_read_alloc(keySerial, &keychainBuffer);

        if (keychainDataSize == -1) {
            m_error = QObject::tr("Could not read key in Linux Keyring.");
            qDebug() << "polkit keyring failed to read: " << errno;
            return false;
        }

        QByteArray keychainBytes(static_cast<const char*>(keychainBuffer), keychainDataSize);

        Botan::secure_scrub_memory(keychainBuffer, keychainDataSize);
        free(keychainBuffer);

        QByteArray keychainKey = keychainBytes.left(SymmetricCipher::keySize(SymmetricCipher::Aes256_GCM));
        QByteArray keychainIv = keychainBytes.right(SymmetricCipher::defaultIvSize(SymmetricCipher::Aes256_GCM));

        SymmetricCipher aes256Decrypt;
        if (!aes256Decrypt.init(SymmetricCipher::Aes256_GCM, SymmetricCipher::Decrypt, keychainKey, keychainIv)) {
            m_error = QObject::tr("Failed to init KeePassXC crypto.");
            qDebug() << "polkit aes init failed";
            return false;
        }

        key = encryptedMasterKey;
        if (!aes256Decrypt.finish(key)) {
            key.clear();
            m_error = QObject::tr("Failed to decrypt key data.");
            qDebug() << "polkit aes decrypt failed: " << aes256Decrypt.errorString();
            return false;
        }

        // Scrub the keys from ram
        Botan::secure_scrub_memory(keychainKey.data(), keychainKey.size());
        Botan::secure_scrub_memory(keychainIv.data(), keychainIv.size());
        Botan::secure_scrub_memory(keychainBytes.data(), keychainBytes.size());
        Botan::secure_scrub_memory(encryptedMasterKey.data(), encryptedMasterKey.size());

        return true;
    }

    // Failed to authenticate
    if (authResult.is_challenge) {
        m_error = QObject::tr("No Polkit authentication agent was available.");
    } else {
        m_error = QObject::tr("Polkit authorization failed.");
    }

    return false;
}

bool Polkit::canRemember() const
{
    return false;
}

bool Polkit::hasKey(const QUuid& dbUuid) const
{
    if (!m_encryptedMasterKeys.contains(dbUuid)) {
        return false;
    }

    return find_key_by_type_and_desc("user", getKeyName(dbUuid).toStdString().c_str(), KEY_SPEC_PROCESS_KEYRING) != -1;
}
