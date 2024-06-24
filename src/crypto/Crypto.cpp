/*
 *  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
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

#include "Crypto.h"

#include "config-keepassx.h"

#include "crypto/CryptoHash.h"
#include "crypto/SymmetricCipher.h"

#include <botan/version.h>

namespace
{
    QString g_cryptoError;

    bool testSha256()
    {
        if (CryptoHash::hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", CryptoHash::Sha256)
            != QByteArray::fromHex("248D6A61D20638B8E5C026930C3E6039A33CE45964FF2167F6ECEDD419DB06C1")) {
            g_cryptoError = "SHA-256 mismatch.";
            return false;
        }

        return true;
    }

    bool testSha512()
    {
        if (CryptoHash::hash("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", CryptoHash::Sha512)
            != QByteArray::fromHex("204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c335"
                                   "96fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445")) {
            g_cryptoError = "SHA-512 mismatch.";
            return false;
        }

        return true;
    }

    bool testAes256Cbc()
    {
        QByteArray key = QByteArray::fromHex("603deb1015ca71be2b73aef0857d7781"
                                             "1f352c073b6108d72d9810a30914dff4");
        QByteArray iv = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f");
        QByteArray plainText = QByteArray::fromHex("6bc1bee22e409f96e93d7e117393172a"
                                                   "ae2d8a571e03ac9c9eb76fac45af8e51");
        QByteArray cipherText = QByteArray::fromHex("f58c4c04d6e5f1ba779eabfb5f7bfbd6"
                                                    "9cfc4e967edb808d679f777bc6702c7d");

        QByteArray data = plainText;
        SymmetricCipher aes256;
        if (!aes256.init(SymmetricCipher::Aes256_CBC, SymmetricCipher::Encrypt, key, iv)) {
            g_cryptoError = aes256.errorString();
            return false;
        }
        if (!aes256.process(data)) {
            g_cryptoError = aes256.errorString();
            return false;
        }
        if (data != cipherText) {
            g_cryptoError = "AES-256 CBC encryption mismatch.";
            return false;
        }

        if (!aes256.init(SymmetricCipher::Aes256_CBC, SymmetricCipher::Decrypt, key, iv)) {
            g_cryptoError = aes256.errorString();
            return false;
        }
        if (!aes256.process(data)) {
            g_cryptoError = aes256.errorString();
            return false;
        }
        if (data != plainText) {
            g_cryptoError = "AES-256 CBC decryption mismatch.";
            return false;
        }

        return true;
    }

    bool testAesKdf()
    {
        QByteArray key = QByteArray::fromHex("000102030405060708090A0B0C0D0E0F"
                                             "101112131415161718191A1B1C1D1E1F");
        QByteArray plainText = QByteArray::fromHex("00112233445566778899AABBCCDDEEFF");
        QByteArray cipherText = QByteArray::fromHex("8EA2B7CA516745BFEAFC49904B496089");

        if (!SymmetricCipher::aesKdf(key, 1, plainText)) {
            g_cryptoError = "AES KDF Failed.";
        }

        if (plainText != cipherText) {
            g_cryptoError = "AES KDF encryption mismatch.";
            return false;
        }

        return true;
    }

    bool testTwofish()
    {
        QByteArray key = QByteArray::fromHex("603deb1015ca71be2b73aef0857d7781"
                                             "1f352c073b6108d72d9810a30914dff4");
        QByteArray iv = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f");
        QByteArray plainText = QByteArray::fromHex("6bc1bee22e409f96e93d7e117393172a"
                                                   "ae2d8a571e03ac9c9eb76fac45af8e51");
        QByteArray cipherText = QByteArray::fromHex("e0227c3cc80f3cb1b2ed847cc6f57d3c"
                                                    "657b1e7960b30fb7c8d62e72ae37c3a0");
        QByteArray data = plainText;
        SymmetricCipher twofish;
        if (!twofish.init(SymmetricCipher::Twofish_CBC, SymmetricCipher::Encrypt, key, iv)) {
            g_cryptoError = twofish.errorString();
            return false;
        }
        if (!twofish.process(data)) {
            g_cryptoError = twofish.errorString();
            return false;
        }
        if (data != cipherText) {
            g_cryptoError = "Twofish encryption mismatch.";
            return false;
        }

        if (!twofish.init(SymmetricCipher::Twofish_CBC, SymmetricCipher::Decrypt, key, iv)) {
            g_cryptoError = twofish.errorString();
            return false;
        }
        if (!twofish.process(data)) {
            g_cryptoError = twofish.errorString();
            return false;
        }
        if (data != plainText) {
            g_cryptoError = "Twofish encryption mismatch.";
            return false;
        }

        return true;
    }

    bool testSalsa20()
    {
        QByteArray salsa20Key = QByteArray::fromHex("F3F4F5F6F7F8F9FAFBFCFDFEFF000102"
                                                    "030405060708090A0B0C0D0E0F101112");
        QByteArray salsa20iv = QByteArray::fromHex("0000000000000000");
        QByteArray salsa20Plain = QByteArray::fromHex("00000000000000000000000000000000");
        QByteArray salsa20Cipher = QByteArray::fromHex("B4C0AFA503BE7FC29A62058166D56F8F");

        QByteArray data = salsa20Plain;
        SymmetricCipher salsa20Stream;
        if (!salsa20Stream.init(SymmetricCipher::Salsa20, SymmetricCipher::Encrypt, salsa20Key, salsa20iv)) {
            g_cryptoError = salsa20Stream.errorString();
            return false;
        }
        if (!salsa20Stream.process(data)) {
            g_cryptoError = salsa20Stream.errorString();
            return false;
        }
        if (data != salsa20Cipher) {
            g_cryptoError = "Salsa20 stream cipher encrypt mismatch.";
            return false;
        }

        if (!salsa20Stream.init(SymmetricCipher::Salsa20, SymmetricCipher::Decrypt, salsa20Key, salsa20iv)) {
            g_cryptoError = salsa20Stream.errorString();
            return false;
        }
        if (!salsa20Stream.process(data)) {
            g_cryptoError = salsa20Stream.errorString();
            return false;
        }
        if (data != salsa20Plain) {
            g_cryptoError = "Salsa20 stream cipher decrypt mismatch.";
            return false;
        }

        return true;
    }

    bool testChaCha20()
    {
        QByteArray chacha20Key = QByteArray::fromHex("00000000000000000000000000000000"
                                                     "00000000000000000000000000000000");
        QByteArray chacha20iv = QByteArray::fromHex("0000000000000000");
        QByteArray chacha20Plain =
            QByteArray::fromHex("0000000000000000000000000000000000000000000000000000000000000000"
                                "0000000000000000000000000000000000000000000000000000000000000000");
        QByteArray chacha20Cipher =
            QByteArray::fromHex("76b8e0ada0f13d90405d6ae55386bd28bdd219b8a08ded1aa836efcc8b770dc7"
                                "da41597c5157488d7724e03fb8d84a376a43b8f41518a11cc387b669b2ee6586");

        QByteArray data = chacha20Plain;
        SymmetricCipher chacha20Stream;
        if (!chacha20Stream.init(SymmetricCipher::ChaCha20, SymmetricCipher::Encrypt, chacha20Key, chacha20iv)) {
            g_cryptoError = chacha20Stream.errorString();
            return false;
        }
        if (!chacha20Stream.process(data)) {
            g_cryptoError = chacha20Stream.errorString();
            return false;
        }
        if (data != chacha20Cipher) {
            g_cryptoError = "ChaCha20 stream cipher encrypt mismatch.";
            return false;
        }

        if (!chacha20Stream.init(SymmetricCipher::ChaCha20, SymmetricCipher::Decrypt, chacha20Key, chacha20iv)) {
            g_cryptoError = chacha20Stream.errorString();
            return false;
        }
        if (!chacha20Stream.process(data)) {
            g_cryptoError = chacha20Stream.errorString();
            return false;
        }
        if (data != chacha20Plain) {
            g_cryptoError = "ChaCha20 stream cipher decrypt mismatch.";
            return false;
        }

        return true;
    }
} // namespace

namespace Crypto
{
    bool init()
    {
#ifdef WITH_BOTAN3
        unsigned int version_major = 3, min_version_minor = 0;
        QString versionString = "3.x";
#else
        unsigned int version_major = 2, min_version_minor = 11;
        QString versionString = "2.11.x";
#endif
        if (Botan::version_major() != version_major || Botan::version_minor() < min_version_minor) {
            g_cryptoError = QObject::tr("Botan library must be at least %1, found %2.%3.%4")
                                .arg(versionString)
                                .arg(Botan::version_major())
                                .arg(Botan::version_minor())
                                .arg(Botan::version_patch());
            return false;
        }

        return testSha256() && testSha512() && testAes256Cbc() && testAesKdf() && testTwofish() && testSalsa20()
               && testChaCha20();
    }

    QString errorString()
    {
        return g_cryptoError;
    }

    QString debugInfo()
    {
        QString debugInfo = QObject::tr("Cryptographic libraries:").append("\n");
        debugInfo.append(QString("- Botan %1.%2.%3\n")
                             .arg(Botan::version_major())
                             .arg(Botan::version_minor())
                             .arg(Botan::version_patch()));
        return debugInfo;
    }
} // namespace Crypto
