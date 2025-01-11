/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#include "OpData01.h"
#include "OpVaultReader.h"

#include "core/Group.h"
#include "core/Tools.h"
#include "crypto/CryptoHash.h"
#include "crypto/SymmetricCipher.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool OpVaultReader::decryptBandEntry(const QJsonObject& bandEntry,
                                     QJsonObject& data,
                                     QByteArray& key,
                                     QByteArray& hmacKey)
{
    if (!bandEntry.contains("d")) {
        qWarning() << "Band entries must contain a \"d\" key: " << bandEntry.keys();
        return false;
    }
    if (!bandEntry.contains("k")) {
        qWarning() << "Band entries must contain a \"k\" key: " << bandEntry.keys();
        return false;
    }

    const QString uuid = bandEntry.value("uuid").toString();

    /*!
     * This is the encrypted item and MAC keys.
     * It is encrypted with the master encryption key and authenticated with the master MAC key.
     *
     * The last 32 bytes comprise the HMAC-SHA256 of the IV and the encrypted data.
     * The MAC is computed with the master MAC key.
     * The data before the MAC is the AES-CBC encrypted item keys using unique random 16-byte IV.
     * \code
     *   uint8_t crypto_key[32];
     *   uint8_t mac_key[32];
     * \endcode
     * \sa https://support.1password.com/opvault-design/#k
     */
    const QString& entKStr = bandEntry["k"].toString();
    QByteArray kBA = QByteArray::fromBase64(entKStr.toUtf8());
    const int wantKsize = 16 + 32 + 32 + 32;
    if (kBA.size() != wantKsize) {
        qCritical("Malformed \"k\" size; expected %d got %" PRIdQPTRDIFF "\n", wantKsize, kBA.size());
        return false;
    }

    QByteArray hmacSig = kBA.mid(kBA.size() - 32, 32);
    const QByteArray& realHmacSig =
        CryptoHash::hmac(kBA.mid(0, kBA.size() - hmacSig.size()), m_masterHmacKey, CryptoHash::Sha256);
    if (realHmacSig != hmacSig) {
        qCritical() << QString(R"(Entry "k" failed its HMAC in UUID "%1", wanted "%2" got "%3")")
                           .arg(uuid)
                           .arg(QString::fromUtf8(hmacSig.toHex()))
                           .arg(QString::fromUtf8(realHmacSig));
        return false;
    }

    QByteArray iv = kBA.mid(0, 16);
    QByteArray keyAndMacKey = kBA.mid(iv.size(), 64);
    SymmetricCipher cipher;
    if (!cipher.init(SymmetricCipher::Aes256_CBC, SymmetricCipher::Decrypt, m_masterKey, iv)) {
        qCritical() << "Unable to init cipher using masterKey in UUID " << uuid;
        return false;
    }
    if (!cipher.process(keyAndMacKey)) {
        qCritical() << "Unable to decipher \"k\"(key+hmac) in UUID " << uuid;
        return false;
    }

    key = keyAndMacKey.mid(0, 32);
    hmacKey = keyAndMacKey.mid(32);

    QString dKeyB64 = bandEntry.value("d").toString();
    OpData01 entD01;
    if (!entD01.decodeBase64(dKeyB64, key, hmacKey)) {
        qCritical() << R"(Unable to decipher "d" in UUID ")" << uuid << "\": " << entD01.errorString();
        return false;
    }

    auto clearText = entD01.getClearText();
    data = QJsonDocument::fromJson(clearText).object();
    return true;
}

Entry* OpVaultReader::processBandEntry(const QJsonObject& bandEntry, const QDir& attachmentDir, Group* rootGroup)
{
    const QString uuid = bandEntry.value("uuid").toString();
    if (!(uuid.size() == 32 || uuid.size() == 36)) {
        qWarning() << QString("Skipping suspicious band UUID <<%1>> with length %2").arg(uuid).arg(uuid.size());
        return nullptr;
    }

    QScopedPointer<Entry> entry(new Entry());

    if (bandEntry.contains("trashed") && bandEntry["trashed"].toBool()) {
        // Send this entry to the recycle bin
        rootGroup->database()->recycleEntry(entry.data());
    } else if (bandEntry.contains("category")) {
        const QJsonValue& categoryValue = bandEntry["category"];
        if (categoryValue.isString()) {
            bool found = false;
            const QString category = categoryValue.toString();
            for (Group* group : rootGroup->children()) {
                const QVariant& groupCode = group->property("code");
                if (category == groupCode.toString()) {
                    entry->setGroup(group);
                    found = true;
                    break;
                }
            }
            if (!found) {
                qWarning() << QString("Unable to place Entry.Category \"%1\" so using the Root instead").arg(category);
                entry->setGroup(rootGroup);
            }
        } else {
            qWarning() << QString(R"(Skipping non-String Category type "%1" in UUID "%2")")
                              .arg(categoryValue.type())
                              .arg(uuid);
            entry->setGroup(rootGroup);
        }
    } else {
        qWarning() << "Using the root group because the entry is category-less: <<\n"
                   << bandEntry << "\n>> in UUID " << uuid;
        entry->setGroup(rootGroup);
    }

    entry->setUpdateTimeinfo(false);
    TimeInfo ti;
    bool timeInfoOk = false;
    if (bandEntry.contains("created")) {
        auto createdTime = static_cast<uint>(bandEntry["created"].toInt());
        ti.setCreationTime(QDateTime::fromSecsSinceEpoch(createdTime, Qt::UTC));
        timeInfoOk = true;
    }
    if (bandEntry.contains("updated")) {
        auto updateTime = static_cast<uint>(bandEntry["updated"].toInt());
        ti.setLastModificationTime(QDateTime::fromSecsSinceEpoch(updateTime, Qt::UTC));
        timeInfoOk = true;
    }
    // "tx" is modified by sync, not by user; maybe a custom attribute?
    if (timeInfoOk) {
        entry->setTimeInfo(ti);
    }
    entry->setUuid(Tools::hexToUuid(uuid));

    if (!fillAttributes(entry.data(), bandEntry)) {
        return nullptr;
    }

    QJsonObject data;
    QByteArray entryKey;
    QByteArray entryHmacKey;

    if (!decryptBandEntry(bandEntry, data, entryKey, entryHmacKey)) {
        return nullptr;
    }

    if (data.contains("notesPlain")) {
        entry->setNotes(data.value("notesPlain").toString());
    }

    // it seems sometimes the password is a top-level field, and not in "fields" themselves
    if (data.contains("password")) {
        entry->setPassword(data.value("password").toString());
    }

    for (const auto fieldValue : data.value("fields").toArray()) {
        if (!fieldValue.isObject()) {
            continue;
        }

        auto field = fieldValue.toObject();
        auto designation = field["designation"].toString();
        auto value = field["value"].toString();
        if (designation == "password") {
            entry->setPassword(value);
        } else if (designation == "username") {
            entry->setUsername(value);
        }
    }

    const QJsonArray& sectionsArray = data["sections"].toArray();
    for (const QJsonValue& sectionValue : sectionsArray) {
        if (!sectionValue.isObject()) {
            qWarning() << R"(Skipping non-Object in "sections" for UUID ")" << uuid << "\" << " << sectionsArray
                       << ">>";
            continue;
        }
        const QJsonObject& section = sectionValue.toObject();

        fillFromSection(entry.data(), section);
    }

    fillAttachments(entry.data(), attachmentDir, entryKey, entryHmacKey);
    return entry.take();
}

bool OpVaultReader::fillAttributes(Entry* entry, const QJsonObject& bandEntry)
{
    const QString overviewStr = bandEntry.value("o").toString();
    OpData01 entOver01;
    if (!entOver01.decodeBase64(overviewStr, m_overviewKey, m_overviewHmacKey)) {
        qCritical() << "Unable to decipher 'o' in UUID \"" << entry->uuid() << "\"\n"
                    << ": " << entOver01.errorString();
        return false;
    }

    auto overviewJsonBytes = entOver01.getClearText();
    auto overviewDoc = QJsonDocument::fromJson(overviewJsonBytes);
    auto overviewJson = overviewDoc.object();

    QString title = overviewJson.value("title").toString();
    entry->setTitle(title);

    QString url = overviewJson["url"].toString();
    entry->setUrl(url);

    int i = 1;
    for (const auto urlV : overviewJson["URLs"].toArray()) {
        const auto& urlObj = urlV.toObject();
        if (urlObj.contains("u")) {
            auto newUrl = urlObj["u"].toString();
            if (newUrl != url) {
                // Add this url if it isn't the base one
                entry->attributes()->set(
                    QString("%1_%2").arg(EntryAttributes::AdditionalUrlAttribute, QString::number(i)), newUrl);
                ++i;
            }
        }
    }

    QStringList tagsList;
    for (const auto tagV : overviewJson["tags"].toArray()) {
        if (tagV.isString()) {
            tagsList << tagV.toString();
        }
    }
    entry->setTags(tagsList.join(','));

    return true;
}
