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

#include "OpenSSHKeyGen.h"
#include "BinaryStream.h"
#include "OpenSSHKey.h"
#include "config-keepassx.h"
#include "crypto/Random.h"

#include <botan/bigint.h>
#include <botan/ecdsa.h>
#include <botan/ed25519.h>
#include <botan/rsa.h>

namespace OpenSSHKeyGen
{
    namespace
    {
        void bigIntToStream(const Botan::BigInt& i, BinaryStream& stream, int padding = 0)
        {
            QByteArray ba(i.bytes() + padding, 0);
            i.binary_encode(reinterpret_cast<uint8_t*>(ba.data() + padding), ba.size() - padding);
            stream.writeString(ba);
        }

        void vectorToStream(const std::vector<uint8_t>& v, BinaryStream& stream)
        {
            QByteArray ba(reinterpret_cast<const char*>(v.data()), v.size());
            stream.writeString(ba);
        }

        void vectorToStream(const Botan::secure_vector<uint8_t>& v, BinaryStream& stream)
        {
            QByteArray ba(reinterpret_cast<const char*>(v.data()), v.size());
            stream.writeString(ba);
        }
    } // namespace

    bool generateRSA(OpenSSHKey& key, int bits)
    {
        auto rng = randomGen()->getRng();

        try {
            Botan::RSA_PrivateKey rsaKey(*rng, bits);

            QByteArray publicData;
            BinaryStream publicStream(&publicData);
            // intentionally flipped n e -> e n
            bigIntToStream(rsaKey.get_e(), publicStream);
            bigIntToStream(rsaKey.get_n(), publicStream, 1);

            QByteArray privateData;
            BinaryStream privateStream(&privateData);
            bigIntToStream(rsaKey.get_n(), privateStream, 1);
            bigIntToStream(rsaKey.get_e(), privateStream);
            bigIntToStream(rsaKey.get_d(), privateStream, 1);
            bigIntToStream(rsaKey.get_c(), privateStream, 1);
            bigIntToStream(rsaKey.get_p(), privateStream, 1);
            bigIntToStream(rsaKey.get_q(), privateStream, 1);

            key.setType("ssh-rsa");
            key.setCheck(randomGen()->randomUInt(std::numeric_limits<quint32>::max() - 1) + 1);
            key.setPublicData(publicData);
            key.setPrivateData(privateData);
            key.setComment("id_rsa");
            return true;
        } catch (std::exception& e) {
            return false;
        }
    }

    bool generateECDSA(OpenSSHKey& key, int bits)
    {
        auto rng = randomGen()->getRng();
        QString group = QString("nistp%1").arg(bits);

        try {
            Botan::EC_Group domain(QString("secp%1r1").arg(bits).toStdString());
            Botan::ECDSA_PrivateKey ecdsaKey(*rng, domain);

            QByteArray publicData;
            BinaryStream publicStream(&publicData);
            publicStream.writeString(group);
            vectorToStream(ecdsaKey.public_key_bits(), publicStream);

            QByteArray privateData;
            BinaryStream privateStream(&privateData);
            privateStream.writeString(group);
            vectorToStream(ecdsaKey.public_key_bits(), privateStream);
            bigIntToStream(ecdsaKey.private_value(), privateStream, 1);

            key.setType("ecdsa-sha2-" + group);
            key.setCheck(randomGen()->randomUInt(std::numeric_limits<quint32>::max() - 1) + 1);
            key.setPublicData(publicData);
            key.setPrivateData(privateData);
            key.setComment("id_ecdsa");
            return true;
        } catch (std::exception& e) {
            return false;
        }
    }

    bool generateEd25519(OpenSSHKey& key)
    {
        auto rng = randomGen()->getRng();

        try {
            Botan::Ed25519_PrivateKey ed25519Key(*rng);

            QByteArray publicData;
            BinaryStream publicStream(&publicData);
            vectorToStream(ed25519Key.get_public_key(), publicStream);

            QByteArray privateData;
            BinaryStream privateStream(&privateData);
            vectorToStream(ed25519Key.get_public_key(), privateStream);
#ifdef WITH_BOTAN3
            vectorToStream(ed25519Key.raw_private_key_bits(), privateStream);
#else
            vectorToStream(ed25519Key.get_private_key(), privateStream);
#endif

            key.setType("ssh-ed25519");
            key.setCheck(randomGen()->randomUInt(std::numeric_limits<quint32>::max() - 1) + 1);
            key.setPublicData(publicData);
            key.setPrivateData(privateData);
            key.setComment("id_ed25519");
            return true;
        } catch (std::exception& e) {
            return false;
        }
    }
} // namespace OpenSSHKeyGen
