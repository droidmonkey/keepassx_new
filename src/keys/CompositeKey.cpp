/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#include "CompositeKey.h"

#include "crypto/CryptoHash.h"
#include "crypto/kdf/Kdf.h"
#include "format/KeePass2.h"
#include "keys/ChallengeResponseKey.h"
#include "keys/FileKey.h"
#include "keys/PasswordKey.h"

#include <QDataStream>
#include <QDebug>
#include <QIODevice>

QUuid CompositeKey::UUID("76a7ae25-a542-4add-9849-7c06be945b94");

CompositeKey::CompositeKey()
    : Key(UUID)
{
}

CompositeKey::~CompositeKey()
{
    clear();
}

void CompositeKey::clear()
{
    m_keys.clear();
    m_challengeResponseKeys.clear();
}

bool CompositeKey::isEmpty() const
{
    return m_keys.isEmpty() && m_challengeResponseKeys.isEmpty();
}

/**
 * Get raw key hash as bytes.
 *
 * The key hash does not contain contributions by challenge-response components for
 * backwards compatibility with KeePassXC's pre-KDBX4 challenge-response
 * implementation. To include challenge-response in the raw key,
 * use \link CompositeKey::rawKey(const QByteArray*, bool*) instead.
 *
 * @return key hash
 */
QByteArray CompositeKey::rawKey() const
{
    return rawKey(nullptr);
}

void CompositeKey::setRawKey(const QByteArray& data)
{
    deserialize(data);
}

/**
 * Get raw key hash as bytes.
 *
 * Challenge-response key components will use the provided <tt>transformSeed</tt>
 * as a challenge to acquire their key contribution.
 *
 * @param transformSeed transform seed to challenge or nullptr to exclude challenge-response components
 * @param ok true if challenges were successful and all key components could be added to the composite key
 * @return key hash
 */
QByteArray CompositeKey::rawKey(const QByteArray* transformSeed, bool* ok, QString* error) const
{
    CryptoHash cryptoHash(CryptoHash::Sha256);

    for (auto const& key : m_keys) {
        cryptoHash.addData(key->rawKey());
    }

    if (ok) {
        *ok = true;
    }

    if (transformSeed) {
        QByteArray challengeResult;
        bool challengeOk = challenge(*transformSeed, challengeResult, error);
        if (ok) {
            *ok = challengeOk;
        }
        cryptoHash.addData(challengeResult);
    }

    return cryptoHash.result();
}

/**
 * Transform this composite key.
 *
 * If using AES-KDF as transform function, the transformed key will not include
 * any challenge-response components. Only static key components will be hashed
 * for backwards-compatibility with KeePassXC's KDBX3 implementation, which added
 * challenge response key components after key transformation.
 * KDBX4+ KDFs transform the whole key including challenge-response components.
 *
 * @param kdf key derivation function
 * @param result transformed key hash
 * @return true on success
 */
bool CompositeKey::transform(const Kdf& kdf, QByteArray& result, QString* error) const
{
    if (kdf.uuid() == KeePass2::KDF_AES_KDBX3) {
        // legacy KDBX3 AES-KDF, challenge response is added later to the hash
        return kdf.transform(rawKey(), result);
    }

    QByteArray seed = kdf.seed();
    Q_ASSERT(!seed.isEmpty());
    bool ok = false;
    return kdf.transform(rawKey(&seed, &ok, error), result) && ok;
}

bool CompositeKey::challenge(const QByteArray& seed, QByteArray& result, QString* error) const
{
    // if no challenge response was requested, return nothing to
    // maintain backwards compatibility with regular databases.
    if (m_challengeResponseKeys.length() == 0) {
        result.clear();
        return true;
    }

    CryptoHash cryptoHash(CryptoHash::Sha256);

    for (const auto& key : m_challengeResponseKeys) {
        // if the device isn't present or fails, return an error
        if (!key->challenge(seed)) {
            if (error) {
                *error = key->error();
            }
            qWarning() << "Failed to issue challenge: " << key->error();
            return false;
        }
        cryptoHash.addData(key->rawKey());
    }

    result = cryptoHash.result();
    return true;
}

/**
 * Add a \link Key to this composite key.
 * Keys will be hashed in the order they were initially added.
 *
 * @param key the key
 */
void CompositeKey::addKey(const QSharedPointer<Key>& key)
{
    m_keys.append(key);
}

/**
 * Get the \link Key with the specified ID.
 *
 * @param keyId the ID of the key to get.
 */
QSharedPointer<Key> CompositeKey::getKey(const QUuid keyId) const
{
    for (const QSharedPointer<Key>& key : m_keys) {
        if (key->uuid() == keyId) {
            return key;
        }
    }
    return {};
}

/**
 * Get the \link ChallengeResponseKey with the specified ID.
 *
 * @param keyId the ID of the key to get.
 */
QSharedPointer<ChallengeResponseKey> CompositeKey::getChallengeResponseKey(const QUuid keyId) const
{
    for (const QSharedPointer<ChallengeResponseKey>& key : m_challengeResponseKeys) {
        if (key->uuid() == keyId) {
            return key;
        }
    }
    return {};
}

/**
 * @return list of Keys which are part of this CompositeKey
 */
const QList<QSharedPointer<Key>>& CompositeKey::keys() const
{
    return m_keys;
}

/**
 * Add a \link ChallengeResponseKey to this composite key.
 * ChallengeResponseKeys will be hashed in the order they were initially added,
 * but will always come after normal \link Keys.
 *
 * @param key the key
 */
void CompositeKey::addChallengeResponseKey(const QSharedPointer<ChallengeResponseKey>& key)
{
    m_challengeResponseKeys.append(key);
}

/**
 * @return list of ChallengeResponseKeys which are part of this CompositeKey
 */
const QList<QSharedPointer<ChallengeResponseKey>>& CompositeKey::challengeResponseKeys() const
{
    return m_challengeResponseKeys;
}

QByteArray CompositeKey::serialize() const
{
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    // Write Composite Key UUID then each sub-key UUID and data
    stream << uuid().toRfc4122();
    for (auto const& key : m_keys) {
        stream << key->uuid().toRfc4122() << key->serialize();
    }
    for (auto const& key : m_challengeResponseKeys) {
        stream << key->uuid().toRfc4122() << key->serialize();
    }
    return data;
}

void CompositeKey::deserialize(const QByteArray& data)
{
    QByteArray uuidData;
    QByteArray keyData;

    QDataStream stream(data);
    // Verify this is a valid composite key data stream
    stream >> uuidData;
    if (uuid().toRfc4122() != uuidData) {
        return;
    }

    // Clear existing keys
    m_keys.clear();
    m_challengeResponseKeys.clear();

    while (!stream.atEnd()) {
        // Read the UUID first to construct the key
        stream >> uuidData;
        auto uuid = QUuid::fromRfc4122(uuidData);

        if (uuid == ChallengeResponseKey::UUID) {
            stream >> keyData;
            auto key = QSharedPointer<ChallengeResponseKey>::create();
            key->deserialize(keyData);
            m_challengeResponseKeys.append(key);
        } else if (uuid == PasswordKey::UUID) {
            stream >> keyData;
            auto key = QSharedPointer<PasswordKey>::create();
            key->deserialize(keyData);
            m_keys << key;
        } else if (uuid == FileKey::UUID) {
            stream >> keyData;
            auto key = QSharedPointer<FileKey>::create();
            key->deserialize(keyData);
            m_keys << key;
        } else {
            // Unsupported key type, discard key data
            stream >> keyData;
        }
    }
}
