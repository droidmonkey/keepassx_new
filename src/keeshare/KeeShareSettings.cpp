/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
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

#include "KeeShareSettings.h"

#include "core/CustomData.h"
#include "core/Database.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "crypto/Random.h"
#include "gui/DatabaseIcons.h"

#include <QDataStream>
#include <QTextCodec>
#include <QXmlStreamWriter>

#include <botan/data_src.h>
#include <botan/pkcs8.h>
#include <botan/rsa.h>

namespace KeeShareSettings
{
    namespace
    {
        QString xmlSerialize(std::function<void(QXmlStreamWriter& writer)> specific)
        {
            QString buffer;
            QXmlStreamWriter writer(&buffer);

            writer.setCodec(QTextCodec::codecForName("UTF-8"));
            writer.writeStartDocument();
            writer.writeStartElement("KeeShare");
            specific(writer);
            writer.writeEndElement();
            writer.writeEndDocument();
            return buffer;
        }

        void xmlDeserialize(const QString& raw, std::function<void(QXmlStreamReader& reader)> specific)
        {
            QXmlStreamReader reader(raw);
            if (!reader.readNextStartElement() || reader.qualifiedName() != "KeeShare") {
                return;
            }
            specific(reader);
        }
    } // namespace

    void Certificate::serialize(QXmlStreamWriter& writer, const Certificate& certificate)
    {
        if (certificate.isNull()) {
            return;
        }
        auto berKey = Botan::PKCS8::BER_encode(*certificate.key);
        auto baKey = QByteArray::fromRawData(reinterpret_cast<const char*>(berKey.data()), berKey.size());

        writer.writeStartElement("Signer");
        writer.writeCharacters(certificate.signer);
        writer.writeEndElement();
        writer.writeStartElement("Key");
        writer.writeCharacters(baKey.toBase64());
        writer.writeEndElement();
    }

    bool Certificate::operator==(const Certificate& other) const
    {
        if (isNull() || other.isNull()) {
            return isNull() == other.isNull();
        }
        return key->private_key_bits() == other.key->private_key_bits() && signer == other.signer;
    }

    bool Certificate::operator!=(const Certificate& other) const
    {
        return !operator==(other);
    }

    bool Certificate::isNull() const
    {
        return !key || signer.isEmpty();
    }

    QString Certificate::fingerprint() const
    {
        if (isNull()) {
            return {};
        }
        return QString::fromStdString(key->fingerprint_public());
    }

    Certificate Certificate::deserialize(QXmlStreamReader& reader)
    {
        Certificate certificate;
        while (!reader.error() && reader.readNextStartElement()) {
            if (reader.name() == "Signer") {
                certificate.signer = reader.readElementText();
            } else if (reader.name() == "Key") {
                auto rawKey = QByteArray::fromBase64(reader.readElementText().toLatin1());
                if (!rawKey.isEmpty()) {
                    try {
                        Botan::DataSource_Memory dataSource(reinterpret_cast<const uint8_t*>(rawKey.constData()),
                                                            rawKey.size());
                        certificate.key.reset(Botan::PKCS8::load_key(dataSource).release());
                    } catch (std::exception& e) {
                        qWarning("KeeShare: Failed to deserialize key data: %s", e.what());
                    }
                }
            }
        }
        return certificate;
    }

    bool Key::operator==(const Key& other) const
    {
        if (isNull() || other.isNull()) {
            return isNull() == other.isNull();
        }
        return key->private_key_bits() == other.key->private_key_bits();
    }

    bool Key::operator!=(const Key& other) const
    {
        return !operator==(other);
    }

    bool Key::isNull() const
    {
        return !key;
    }

    void Key::serialize(QXmlStreamWriter& writer, const Key& key)
    {
        if (key.isNull()) {
            return;
        }
        auto berKey = Botan::PKCS8::BER_encode(*key.key);
        auto baKey = QByteArray::fromRawData(reinterpret_cast<const char*>(berKey.data()), berKey.size());
        writer.writeCharacters(baKey.toBase64());
    }

    Key Key::deserialize(QXmlStreamReader& reader)
    {
        Key key;
        auto rawKey = QByteArray::fromBase64(reader.readElementText().toLatin1());
        if (!rawKey.isEmpty()) {
            try {
                Botan::DataSource_Memory dataSource(reinterpret_cast<const uint8_t*>(rawKey.constData()),
                                                    rawKey.size());
                key.key.reset(Botan::PKCS8::load_key(dataSource).release());
            } catch (std::exception& e) {
                qWarning("KeeShare: Failed to deserialize key data: %s", e.what());
            }
        }
        return key;
    }

    Own Own::generate()
    {
        Own own;
        own.key.key.reset(new Botan::RSA_PrivateKey(*randomGen()->getRng(), 2048));
        auto name = qgetenv("USER");
        if (name.isEmpty()) {
            name = qgetenv("USERNAME");
        }
        own.certificate.signer = name;
        own.certificate.key = own.key.key;
        return own;
    }

    QString Active::serialize(const Active& active)
    {
        return xmlSerialize([&](QXmlStreamWriter& writer) {
            writer.writeStartElement("Active");
            if (active.in) {
                writer.writeEmptyElement("Import");
            }
            if (active.out) {
                writer.writeEmptyElement("Export");
            }
            writer.writeEndElement();
        });
    }

    Active Active::deserialize(const QString& raw)
    {
        Active active;
        xmlDeserialize(raw, [&](QXmlStreamReader& reader) {
            while (!reader.error() && reader.readNextStartElement()) {
                if (reader.name() == "Active") {
                    while (reader.readNextStartElement()) {
                        if (reader.name() == "Import") {
                            active.in = true;
                            reader.skipCurrentElement();
                        } else if (reader.name() == "Export") {
                            active.out = true;
                            reader.skipCurrentElement();
                        } else {
                            break;
                        }
                    }
                } else {
                    qWarning("Unknown KeeShareSettings element %s", qPrintable(reader.name().toString()));
                    reader.skipCurrentElement();
                }
            }
        });
        return active;
    }

    bool Own::operator==(const Own& other) const
    {
        return key == other.key && certificate == other.certificate;
    }

    bool Own::operator!=(const Own& other) const
    {
        return !operator==(other);
    }

    QString Own::serialize(const Own& own)
    {
        return xmlSerialize([&](QXmlStreamWriter& writer) {
            writer.writeStartElement("PrivateKey");
            Key::serialize(writer, own.key);
            writer.writeEndElement();
            writer.writeStartElement("PublicKey");
            Certificate::serialize(writer, own.certificate);
            writer.writeEndElement();
        });
    }

    Own Own::deserialize(const QString& raw)
    {
        Own own;
        xmlDeserialize(raw, [&](QXmlStreamReader& reader) {
            while (!reader.error() && reader.readNextStartElement()) {
                if (reader.name() == "PrivateKey") {
                    own.key = Key::deserialize(reader);
                } else if (reader.name() == "PublicKey") {
                    own.certificate = Certificate::deserialize(reader);
                } else {
                    qWarning("Unknown KeeShareSettings element %s", qPrintable(reader.name().toString()));
                    reader.skipCurrentElement();
                }
            }
        });
        return own;
    }

    Reference::Reference()
        : type(Inactive)
        , uuid(QUuid::createUuid())
    {
    }

    bool Reference::isNull() const
    {
        return type == Inactive && path.isEmpty() && password.isEmpty();
    }

    bool Reference::isValid() const
    {
        return type != Inactive && !path.isEmpty();
    }

    bool Reference::isExporting() const
    {
        return (type & ExportTo) != 0 && !path.isEmpty();
    }

    bool Reference::isImporting() const
    {
        return (type & ImportFrom) != 0 && !path.isEmpty();
    }

    bool Reference::operator<(const Reference& other) const
    {
        if (type != other.type) {
            return type < other.type;
        }
        return path < other.path;
    }

    bool Reference::operator==(const Reference& other) const
    {
        return path == other.path && uuid == other.uuid && password == other.password && type == other.type;
    }

    QString Reference::serialize(const Reference& reference)
    {
        return xmlSerialize([&](QXmlStreamWriter& writer) {
            writer.writeStartElement("Type");
            if ((reference.type & ImportFrom) == ImportFrom) {
                writer.writeEmptyElement("Import");
            }
            if ((reference.type & ExportTo) == ExportTo) {
                writer.writeEmptyElement("Export");
            }
            writer.writeEndElement();
            writer.writeStartElement("Group");
            writer.writeCharacters(reference.uuid.toRfc4122().toBase64());
            writer.writeEndElement();
            writer.writeStartElement("Path");
            writer.writeCharacters(reference.path.toUtf8().toBase64());
            writer.writeEndElement();
            writer.writeStartElement("Password");
            writer.writeCharacters(reference.password.toUtf8().toBase64());
            writer.writeEndElement();
            writer.writeStartElement("Recurse");
            writer.writeCharacters(reference.recurse ? "True" : "False");
            writer.writeEndElement();
        });
    }

    Reference Reference::deserialize(const QString& raw)
    {
        Reference reference;
        xmlDeserialize(raw, [&](QXmlStreamReader& reader) {
            while (!reader.error() && reader.readNextStartElement()) {
                if (reader.name() == "Type") {
                    while (reader.readNextStartElement()) {
                        if (reader.name() == "Import") {
                            reference.type |= ImportFrom;
                            reader.skipCurrentElement();
                        } else if (reader.name() == "Export") {
                            reference.type |= ExportTo;
                            reader.skipCurrentElement();
                        } else {
                            break;
                        }
                    }
                } else if (reader.name() == "Group") {
                    reference.uuid = QUuid::fromRfc4122(QByteArray::fromBase64(reader.readElementText().toLatin1()));
                } else if (reader.name() == "Path") {
                    reference.path = QString::fromUtf8(QByteArray::fromBase64(reader.readElementText().toLatin1()));
                } else if (reader.name() == "Password") {
                    reference.password = QString::fromUtf8(QByteArray::fromBase64(reader.readElementText().toLatin1()));
                } else if (reader.name() == "Recurse") {
                    reference.recurse = reader.readElementText().compare("True") == 0;
                } else {
                    qWarning("Unknown Reference element %s", qPrintable(reader.name().toString()));
                    reader.skipCurrentElement();
                }
            }
        });
        return reference;
    }

    QString Sign::serialize(const Sign& sign)
    {
        if (sign.certificate.isNull()) {
            return {};
        }

        // Extract RSA key data to serialize an ssh-rsa public key.
        // ssh-rsa keys are currently not built into Botan
        const auto rsaKey = static_cast<Botan::RSA_PrivateKey*>(sign.certificate.key.data());

        std::vector<uint8_t> rsaE(rsaKey->get_e().bytes());
        rsaKey->get_e().binary_encode(rsaE.data());

        std::vector<uint8_t> rsaN(rsaKey->get_n().bytes());
        rsaKey->get_n().binary_encode(rsaN.data());

        QByteArray rsaKeySerialized;
        QDataStream stream(&rsaKeySerialized, QIODevice::WriteOnly);
        stream.writeBytes("ssh-rsa", 7);
        stream.writeBytes(reinterpret_cast<const char*>(rsaE.data()), rsaE.size());
        stream.writeBytes(reinterpret_cast<const char*>(rsaN.data()), rsaN.size());

        return xmlSerialize([&](QXmlStreamWriter& writer) {
            writer.writeStartElement("Signature");
            writer.writeCharacters(sign.signature);
            writer.writeEndElement();
            writer.writeStartElement("Certificate");
            writer.writeStartElement("Signer");
            writer.writeCharacters(sign.certificate.signer);
            writer.writeEndElement();
            writer.writeStartElement("Key");
            writer.writeCharacters(rsaKeySerialized.toBase64());
            writer.writeEndElement();
            writer.writeEndElement();
        });
    }
} // namespace KeeShareSettings
