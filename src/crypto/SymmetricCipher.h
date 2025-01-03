/*
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

#ifndef KEEPASSX_SYMMETRICCIPHER_H
#define KEEPASSX_SYMMETRICCIPHER_H

#include <QByteArray>
#include <QSharedPointer>
#include <QString>

namespace Botan
{
    class Cipher_Mode;
}

class SymmetricCipher
{
public:
    enum Mode
    {
        Aes128_CBC,
        Aes256_CBC,
        Aes128_CTR,
        Aes256_CTR,
        Twofish_CBC,
        ChaCha20,
        Salsa20,
        Aes256_GCM,
        Aes256_CBC_UNPADDED,
        InvalidMode = -1,
    };

    enum Direction
    {
        Decrypt,
        Encrypt
    };

    explicit SymmetricCipher() = default;
    ~SymmetricCipher() = default;

    bool isInitialized() const;
    Q_REQUIRED_RESULT bool init(Mode mode, Direction direction, const QByteArray& key, const QByteArray& iv);
    Q_REQUIRED_RESULT bool process(char* data, int len);
    Q_REQUIRED_RESULT bool process(QByteArray& data);
    Q_REQUIRED_RESULT bool finish(QByteArray& data);

    static bool aesKdf(const QByteArray& key, int rounds, QByteArray& data);

    void reset();
    Mode mode();

    QString errorString() const;

    static Mode cipherUuidToMode(const QUuid& uuid);
    static Mode stringToMode(const QString& cipher);
    static int defaultIvSize(Mode mode);
    static int keySize(Mode mode);
    static int blockSize(Mode mode);
    static int ivSize(Mode mode);

private:
    static QString modeToString(const Mode mode);

    QString m_error;
    Mode m_mode;
    QSharedPointer<Botan::Cipher_Mode> m_cipher;

    Q_DISABLE_COPY(SymmetricCipher)
};

#endif // KEEPASSX_SYMMETRICCIPHER_H
