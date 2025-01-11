/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#include "SymmetricCipher.h"

#include "config-keepassx.h"
#include "format/KeePass2.h"

#include <botan/block_cipher.h>
#include <botan/cipher_mode.h>

bool SymmetricCipher::init(Mode mode, Direction direction, const QByteArray& key, const QByteArray& iv)
{
    m_mode = mode;
    if (mode == InvalidMode) {
        m_error = QObject::tr("SymmetricCipher::init: Invalid cipher mode.");
        return false;
    }

    try {
        auto botanMode = modeToString(mode);
        auto botanDirection =
#ifdef WITH_BOTAN3
            (direction == SymmetricCipher::Encrypt ? Botan::Cipher_Dir::Encryption : Botan::Cipher_Dir::Decryption);
#else
            (direction == SymmetricCipher::Encrypt ? Botan::Cipher_Dir::ENCRYPTION : Botan::Cipher_Dir::DECRYPTION);
#endif

        auto cipher = Botan::Cipher_Mode::create_or_throw(botanMode.toStdString(), botanDirection);
        m_cipher.reset(cipher.release());
        m_cipher->set_key(reinterpret_cast<const uint8_t*>(key.data()), key.size());

        if (!m_cipher->valid_nonce_length(iv.size())) {
            m_mode = InvalidMode;
            m_cipher.reset();
            m_error = QObject::tr("SymmetricCipher::init: Invalid IV size of %1 for %2.").arg(iv.size()).arg(botanMode);
            return false;
        }
        m_cipher->start(reinterpret_cast<const uint8_t*>(iv.data()), iv.size());
    } catch (std::exception& e) {
        m_mode = InvalidMode;
        m_cipher.reset();

        m_error = e.what();
        reset();
        return false;
    }

    return true;
}

bool SymmetricCipher::isInitialized() const
{
    return !m_cipher.isNull();
}

bool SymmetricCipher::process(char* data, int len)
{
    Q_ASSERT(isInitialized());
    if (!isInitialized()) {
        m_error = QObject::tr("Cipher not initialized prior to use.");
        return false;
    }
    if (len == 0) {
        m_error = QObject::tr("Cannot process 0 length data.");
        return false;
    }

    try {
        // Block size is checked by Botan, an exception is thrown if invalid
        m_cipher->process(reinterpret_cast<uint8_t*>(data), len);
        return true;
    } catch (std::exception& e) {
        m_error = e.what();
        return false;
    }
}

bool SymmetricCipher::process(QByteArray& data)
{
    return process(data.data(), data.size());
}

bool SymmetricCipher::finish(QByteArray& data)
{
    Q_ASSERT(isInitialized());
    if (!isInitialized()) {
        m_error = QObject::tr("Cipher not initialized prior to use.");
        return false;
    }

    try {
        // Error checking is done by Botan, an exception is thrown if invalid
        Botan::secure_vector<uint8_t> input(data.begin(), data.end());
        m_cipher->finish(input);
        // Post-finished data may be larger than before due to padding
        data.resize(input.size());
        // Direct copy the finished data back into the QByteArray
        std::copy(input.begin(), input.end(), data.begin());
        return true;
    } catch (std::exception& e) {
        m_error = e.what();
        return false;
    }
}

void SymmetricCipher::reset()
{
    m_error.clear();
    if (isInitialized()) {
        m_cipher.reset();
    }
}

SymmetricCipher::Mode SymmetricCipher::mode()
{
    return m_mode;
}

bool SymmetricCipher::aesKdf(const QByteArray& key, int rounds, QByteArray& data)
{
    try {
        std::unique_ptr<Botan::BlockCipher> cipher(Botan::BlockCipher::create("AES-256"));
        cipher->set_key(reinterpret_cast<const uint8_t*>(key.data()), key.size());

        Botan::secure_vector<uint8_t> out(data.begin(), data.end());
        for (int i = 0; i < rounds; ++i) {
            cipher->encrypt(out);
        }
        std::copy(out.begin(), out.end(), data.begin());
        return true;
    } catch (std::exception& e) {
        qWarning("SymmetricCipher::aesKdf: Could not process: %s", e.what());
        return false;
    }
}

QString SymmetricCipher::errorString() const
{
    return m_error;
}

SymmetricCipher::Mode SymmetricCipher::cipherUuidToMode(const QUuid& uuid)
{
    if (uuid == KeePass2::CIPHER_AES128) {
        return Aes128_CBC;
    } else if (uuid == KeePass2::CIPHER_AES256) {
        return Aes256_CBC;
    } else if (uuid == KeePass2::CIPHER_CHACHA20) {
        return ChaCha20;
    } else if (uuid == KeePass2::CIPHER_TWOFISH) {
        return Twofish_CBC;
    }

    qWarning("SymmetricCipher: Invalid KeePass2 Cipher UUID %s", uuid.toString().toLatin1().data());
    return InvalidMode;
}

SymmetricCipher::Mode SymmetricCipher::stringToMode(const QString& cipher)
{
    auto cs = Qt::CaseInsensitive;
    if (cipher.compare("aes-128-cbc", cs) == 0 || cipher.compare("aes128-cbc", cs) == 0) {
        return Aes128_CBC;
    } else if (cipher.compare("aes-256-cbc", cs) == 0 || cipher.compare("aes256-cbc", cs) == 0) {
        return Aes256_CBC;
    } else if (cipher.compare("aes-128-ctr", cs) == 0 || cipher.compare("aes128-ctr", cs) == 0) {
        return Aes128_CTR;
    } else if (cipher.compare("aes-256-ctr", cs) == 0 || cipher.compare("aes256-ctr", cs) == 0) {
        return Aes256_CTR;
    } else if (cipher.compare("aes-256-gcm", cs) == 0 || cipher.compare("aes256-gcm", cs) == 0) {
        return Aes256_GCM;
    } else if (cipher.startsWith("twofish", cs)) {
        return Twofish_CBC;
    } else if (cipher.startsWith("salsa", cs)) {
        return Salsa20;
    } else if (cipher.startsWith("chacha", cs)) {
        return ChaCha20;
    } else {
        return InvalidMode;
    }
}

QString SymmetricCipher::modeToString(const Mode mode)
{
    switch (mode) {
    case Aes128_CBC:
        return QStringLiteral("AES-128/CBC");
    case Aes256_CBC:
        return QStringLiteral("AES-256/CBC");
    case Aes128_CTR:
        return QStringLiteral("CTR(AES-128)");
    case Aes256_CTR:
        return QStringLiteral("CTR(AES-256)");
    case Aes256_GCM:
        return QStringLiteral("AES-256/GCM");
    case Twofish_CBC:
        return QStringLiteral("Twofish/CBC");
    case Salsa20:
        return QStringLiteral("Salsa20");
    case ChaCha20:
        return QStringLiteral("ChaCha20");
    default:
        Q_ASSERT_X(false, "SymmetricCipher::modeToString", "Invalid Mode Specified");
        return {};
    }
}

int SymmetricCipher::defaultIvSize(Mode mode)
{
    switch (mode) {
    case Aes128_CBC:
    case Aes256_CBC:
    case Aes128_CTR:
    case Aes256_CTR:
    case Aes256_GCM:
    case Twofish_CBC:
        return 16;
    case Salsa20:
    case ChaCha20:
        return 12;
    default:
        return -1;
    }
}

int SymmetricCipher::keySize(Mode mode)
{
    switch (mode) {
    case Aes128_CBC:
    case Aes128_CTR:
        return 16;
    case Aes256_CBC:
    case Aes256_CTR:
    case Aes256_GCM:
    case Twofish_CBC:
    case Salsa20:
    case ChaCha20:
        return 32;
    default:
        return 0;
    }
}

int SymmetricCipher::blockSize(Mode mode)
{
    switch (mode) {
    case Aes128_CBC:
    case Aes256_CBC:
    case Aes256_GCM:
    case Twofish_CBC:
        return 16;
    case Aes128_CTR:
    case Aes256_CTR:
    case Salsa20:
    case ChaCha20:
        return 1;
    default:
        return 0;
    }
}

int SymmetricCipher::ivSize(Mode mode)
{
    switch (mode) {
    case Aes128_CBC:
    case Aes256_CBC:
    case Aes128_CTR:
    case Aes256_CTR:
    case Twofish_CBC:
        return 16;
    case Aes256_GCM:
        return 12;
    case Salsa20:
    case ChaCha20:
        return 8;
    default:
        return 0;
    }
}
