/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#include "BitwardenReader.h"

#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "core/Totp.h"
#include "crypto/CryptoHash.h"
#include "crypto/SymmetricCipher.h"
#include "crypto/kdf/Argon2Kdf.h"

#include <botan/kdf.h>
#include <botan/pwdhash.h>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QScopedPointer>
#include <QUrl>

namespace
{
    Entry* readItem(const QJsonObject& item, QString& folderId)
    {
        // Create the item map and extract the folder id
        const auto itemMap = item.toVariantMap();
        folderId = itemMap.value("folderId").toString();
        if (folderId.isEmpty()) {
            // Bitwarden organization vaults use collectionId instead of folderId
            auto collectionIds = itemMap.value("collectionIds").toStringList();
            if (!collectionIds.empty()) {
                folderId = collectionIds.first();
            }
        }

        // Create entry and assign basic values
        QScopedPointer<Entry> entry(new Entry());
        entry->setUuid(QUuid::createUuid());
        entry->setTitle(itemMap.value("name").toString());
        entry->setNotes(itemMap.value("notes").toString());

        if (itemMap.value("favorite").toBool()) {
            entry->addTag(QObject::tr("Favorite", "Tag for favorite entries"));
        }

        // Parse login details if present
        if (itemMap.contains("login")) {
            const auto loginMap = itemMap.value("login").toMap();
            entry->setUsername(loginMap.value("username").toString());
            entry->setPassword(loginMap.value("password").toString());
            if (loginMap.contains("totp")) {
                auto totp = loginMap.value("totp").toString();
                if (!totp.startsWith("otpauth://")) {
                    QUrl url(QString("otpauth://totp/%1:%2?secret=%3")
                                 .arg(QString(QUrl::toPercentEncoding(entry->title())),
                                      QString(QUrl::toPercentEncoding(entry->username())),
                                      QString(QUrl::toPercentEncoding(totp))));
                    totp = url.toString(QUrl::FullyEncoded);
                }
                entry->setTotp(Totp::parseSettings(totp));
            }

            // Parse passkey
            if (loginMap.contains("fido2Credentials")) {
                const auto fido2CredentialsMap = loginMap.value("fido2Credentials").toList();
                for (const auto& fido2Credentials : fido2CredentialsMap) {
                    const auto passkey = fido2Credentials.toMap();

                    // Change from UUID to base64 byte array
                    const auto credentialIdValue = passkey.value("credentialId").toString();
                    if (!credentialIdValue.isEmpty()) {
                        const auto credentialUuid = Tools::uuidToHex(credentialIdValue);
                        const auto credentialIdArray = QByteArray::fromHex(credentialUuid.toUtf8());
                        const auto credentialId =
                            credentialIdArray.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
                        entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_CREDENTIAL_ID, credentialId, true);
                    }

                    // Base64 needs to be changed from URL encoding back to normal, and the result as PEM string
                    const auto keyValue = passkey.value("keyValue").toString();
                    if (!keyValue.isEmpty()) {
                        const auto keyValueArray =
                            QByteArray::fromBase64(keyValue.toUtf8(), QByteArray::Base64UrlEncoding);
                        auto privateKey = keyValueArray.toBase64(QByteArray::Base64Encoding);
                        privateKey.insert(0, EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_START.toUtf8());
                        privateKey.append(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_END.toUtf8());
                        entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_PEM, privateKey, true);
                    }

                    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_USERNAME,
                                             passkey.value("userName").toString());
                    entry->attributes()->set(EntryAttributes::KPEX_PASSKEY_RELYING_PARTY,
                                             passkey.value("rpId").toString());
                    entry->attributes()->set(
                        EntryAttributes::KPEX_PASSKEY_USER_HANDLE, passkey.value("userHandle").toString(), true);
                    entry->addTag(QObject::tr("Passkey"));
                }
            }

            // Set the entry url(s)
            int i = 1;
            for (const auto& urlObj : loginMap.value("uris").toList()) {
                const auto url = urlObj.toMap().value("uri").toString();
                if (entry->url().isEmpty()) {
                    // First url encountered is set as the primary url
                    entry->setUrl(url);
                } else {
                    // Subsequent urls
                    entry->attributes()->set(
                        QString("%1_%2").arg(EntryAttributes::AdditionalUrlAttribute, QString::number(i)), url);
                    ++i;
                }
            }
        }

        // Parse identity details if present
        if (itemMap.contains("identity")) {
            const auto idMap = itemMap.value("identity").toMap();

            // Combine name attributes
            auto attrs = QStringList({idMap.value("title").toString(),
                                      idMap.value("firstName").toString(),
                                      idMap.value("middleName").toString(),
                                      idMap.value("lastName").toString()});
            attrs.removeAll("");
            entry->attributes()->set("identity_name", attrs.join(" "));

            // Combine all the address attributes
            attrs = QStringList({idMap.value("address1").toString(),
                                 idMap.value("address2").toString(),
                                 idMap.value("address3").toString()});
            attrs.removeAll("");
            auto address = attrs.join("\n") + "\n" + idMap.value("city").toString() + ", "
                           + idMap.value("state").toString() + " " + idMap.value("postalCode").toString() + "\n"
                           + idMap.value("country").toString();
            entry->attributes()->set("identity_address", address);

            // Add the remaining attributes
            attrs = QStringList({"company", "email", "phone", "ssn", "passportNumber", "licenseNumber"});
            const QStringList sensitive({"ssn", "passportNumber", "licenseNumber"});
            for (const auto& attr : attrs) {
                const auto value = idMap.value(attr).toString();
                if (!value.isEmpty()) {
                    entry->attributes()->set("identity_" + attr, value, sensitive.contains(attr));
                }
            }

            // Set the username or push it into attributes if already set
            const auto username = idMap.value("username").toString();
            if (!username.isEmpty()) {
                if (entry->username().isEmpty()) {
                    entry->setUsername(username);
                } else {
                    entry->attributes()->set("identity_username", username);
                }
            }
        }

        // Parse card details if present
        if (itemMap.contains("card")) {
            const auto cardMap = itemMap.value("card").toMap();
            const QStringList attrs({"cardholderName", "brand", "number", "expMonth", "expYear", "code"});
            const QStringList sensitive({"code"});
            for (const auto& attr : attrs) {
                auto value = cardMap.value(attr).toString();
                if (!value.isEmpty()) {
                    entry->attributes()->set("card_" + attr, value, sensitive.contains(attr));
                }
            }
        }

        // Parse remaining fields
        for (const auto& field : itemMap.value("fields").toList()) {
            // Derive a prefix for attribute names using the title or uuid if missing
            const auto fieldMap = field.toMap();
            auto name = fieldMap.value("name").toString();
            if (entry->attributes()->hasKey(name)) {
                name = QString("%1_%2").arg(name, QUuid::createUuid().toString().mid(1, 5));
            }

            const auto value = fieldMap.value("value").toString();
            const auto type = fieldMap.value("type").toInt();

            entry->attributes()->set(name, value, type == 1);
        }

        // Collapse any accumulated history
        entry->removeHistoryItems(entry->historyItems());

        return entry.take();
    }

    void writeVaultToDatabase(const QJsonObject& vault, QSharedPointer<Database> db)
    {
        auto folderField = QString("folders");
        if (!vault.contains(folderField)) {
            // Handle Bitwarden organization vaults
            folderField = "collections";
        }

        if (!vault.contains(folderField) || !vault.contains("items")) {
            // Early out if the vault is missing critical items
            return;
        }

        // Create groups from folders and store a temporary map of id -> uuid
        QMap<QString, Group*> folderMap;
        for (const auto& folder : vault.value(folderField).toArray()) {
            auto group = new Group();
            group->setUuid(QUuid::createUuid());
            group->setName(folder.toObject().value("name").toString());
            group->setParent(db->rootGroup());

            folderMap.insert(folder.toObject().value("id").toString(), group);
        }

        QString folderId;
        const auto items = vault.value("items").toArray();
        for (const auto& item : items) {
            auto entry = readItem(item.toObject(), folderId);
            if (entry) {
                entry->setGroup(folderMap.value(folderId, db->rootGroup()), false);
            }
        }
    }
} // namespace

bool BitwardenReader::hasError()
{
    return !m_error.isEmpty();
}

QString BitwardenReader::errorString()
{
    return m_error;
}

QSharedPointer<Database> BitwardenReader::convert(const QString& path, const QString& password)
{
    m_error.clear();

    QFileInfo fileinfo(path);
    if (!fileinfo.exists()) {
        m_error = QObject::tr("File does not exist.").arg(path);
        return {};
    }

    // Bitwarden uses a json file format
    QFile file(fileinfo.absoluteFilePath());
    if (!file.open(QFile::ReadOnly)) {
        m_error = QObject::tr("Cannot open file: %1").arg(file.errorString());
        return {};
    }

    QJsonParseError error;
    auto json = QJsonDocument::fromJson(file.readAll(), &error).object();
    if (error.error != QJsonParseError::NoError) {
        m_error =
            QObject::tr("Cannot parse file: %1 at position %2").arg(error.errorString(), QString::number(error.offset));
        return {};
    }

    file.close();

    // Check if this is an encrypted json
    if (json.contains("encrypted") && json.value("encrypted").toBool()) {
        auto buildError = [](const QString& errorString) {
            return QObject::tr("Failed to decrypt json file: %1").arg(errorString);
        };

        if (!json.contains("kdfType") || !json.contains("salt")) {
            m_error = buildError(QObject::tr("Unsupported format, ensure your Bitwarden export is password-protected"));
            return {};
        }

        QByteArray key(32, '\0');
        auto salt = json.value("salt").toString().toUtf8();
        auto kdfType = json.value("kdfType").toInt();

        // Derive the Master Key
        if (kdfType == 0) {
            // PBKDF2
            auto iterations = json.value("kdfIterations").toInt();
            if (iterations <= 0) {
                m_error = buildError(QObject::tr("Invalid KDF iterations, cannot decrypt json file"));
                return {};
            }
            auto pwd_fam = Botan::PasswordHashFamily::create_or_throw("PBKDF2(SHA-256)");
            auto pwd_hash = pwd_fam->from_params(iterations);
            pwd_hash->derive_key(reinterpret_cast<uint8_t*>(key.data()),
                                 key.size(),
                                 password.toUtf8().data(),
                                 password.toUtf8().size(),
                                 reinterpret_cast<uint8_t*>(salt.data()),
                                 salt.size());
        } else if (kdfType == 1) {
            // Argon2
            // Bitwarden hashes the salt prior to use
            CryptoHash saltHash(CryptoHash::Sha256);
            saltHash.addData(salt);
            salt = saltHash.result();

            Argon2Kdf argon2(Argon2Kdf::Type::Argon2id);
            argon2.setSeed(salt);
            argon2.setRounds(json.value("kdfIterations").toInt());
            argon2.setMemory(json.value("kdfMemory").toInt() * 1024);
            argon2.setParallelism(json.value("kdfParallelism").toInt());
            argon2.transform(password.toUtf8(), key);
        } else {
            m_error = buildError(QObject::tr("Only PBKDF and Argon2 are supported, cannot decrypt json file"));
            return {};
        }

        auto hkdf = Botan::KDF::create_or_throw("HKDF-Expand(SHA-256)");

        // Derive the MAC Key
        auto stretched_mac = hkdf->derive_key(32, reinterpret_cast<const uint8_t*>(key.data()), key.size(), "", "mac");
        auto mac = QByteArray(reinterpret_cast<const char*>(stretched_mac.data()), stretched_mac.size());

        // Stretch the Master Key
        auto stretched_key = hkdf->derive_key(32, reinterpret_cast<const uint8_t*>(key.data()), key.size(), "", "enc");
        key = QByteArray(reinterpret_cast<const char*>(stretched_key.data()), stretched_key.size());

        // Validate the encryption key
        auto keyList = json.value("encKeyValidation_DO_NOT_EDIT").toString().split(".");
        if (keyList.size() < 2) {
            m_error = buildError(QObject::tr("Invalid encKeyValidation field"));
            return {};
        }
        auto cipherList = keyList[1].split("|");
        if (cipherList.size() < 3) {
            m_error = buildError(QObject::tr("Invalid cipher list within encKeyValidation field"));
            return {};
        }
        CryptoHash hash(CryptoHash::Sha256, true);
        hash.setKey(mac);
        hash.addData(QByteArray::fromBase64(cipherList[0].toUtf8())); // iv
        hash.addData(QByteArray::fromBase64(cipherList[1].toUtf8())); // ciphertext
        if (hash.result().toBase64() != cipherList[2].toUtf8()) {
            // Calculated MAC doesn't equal the Validation
            m_error = buildError(QObject::tr("Wrong password"));
            return {};
        }

        // Decrypt data field using AES-256-CBC
        keyList = json.value("data").toString().split(".");
        if (keyList.size() < 2) {
            m_error = buildError(QObject::tr("Invalid encrypted data field"));
            return {};
        }
        cipherList = keyList[1].split("|");
        if (cipherList.size() < 2) {
            m_error = buildError(QObject::tr("Invalid cipher list within encrypted data field"));
            return {};
        }
        auto iv = QByteArray::fromBase64(cipherList[0].toUtf8());
        auto data = QByteArray::fromBase64(cipherList[1].toUtf8());

        SymmetricCipher cipher;
        if (!cipher.init(SymmetricCipher::Aes256_CBC, SymmetricCipher::Decrypt, key, iv)) {
            m_error = buildError(QObject::tr("Cannot initialize cipher"));
            return {};
        }
        if (!cipher.finish(data)) {
            m_error = buildError(QObject::tr("Cannot decrypt data"));
            return {};
        }

        json = QJsonDocument::fromJson(data, &error).object();
        if (error.error != QJsonParseError::NoError) {
            m_error = buildError(error.errorString());
            return {};
        }
    }

    auto db = QSharedPointer<Database>::create();
    db->rootGroup()->setName(QObject::tr("Bitwarden Import"));

    writeVaultToDatabase(json, db);

    return db;
}
