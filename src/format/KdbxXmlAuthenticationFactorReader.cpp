/*
 * Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or (at your option)
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "KdbxXmlAuthenticationFactorReader.h"
#include "format/multifactor/FidoAuthenticationFactor.h"
#include "format/multifactor/PasswordAuthenticationFactor.h"
#include <QDebug>

/**
 * Read XML contents from a file into a new database.
 *
 * @param authenticationFactorXml A blob of XML describing authentication factors
 * @return pointer to authentication factor information
 */
QSharedPointer<AuthenticationFactorInfo>
KdbxXmlAuthenticationFactorReader::readAuthenticationFactors(Database* db, const QString& authenticationFactorXml)
{
    m_error = false;
    m_errorStr.clear();

    m_xml.clear();

    qDebug() << tr("Read authentication factor XML: %1").arg(authenticationFactorXml);

    auto result = QSharedPointer<AuthenticationFactorInfo>::create();

    m_xml.addData(authenticationFactorXml);

    if (m_xml.hasError()) {
        raiseError(tr("XML parsing failure on authentication factors: %1").arg(m_xml.error()));
        return result;
    }

    bool factorInfoParsed = false;

    if (m_xml.readNextStartElement() && m_xml.name() == "FactorInfo") {
        factorInfoParsed = parseFactorInfo(result);
    }

    if (!factorInfoParsed) {
        if (!m_error) {
            raiseError(tr("Failed to parse authentication factor info"));
        }
        return result;
    }

    if (db != nullptr) {
        db->setAuthenticationFactorInfo(result);
    }

    return result;
}

bool KdbxXmlAuthenticationFactorReader::hasError() const
{
    return m_error;
}

QString KdbxXmlAuthenticationFactorReader::errorString() const
{
    return m_errorStr;
}

void KdbxXmlAuthenticationFactorReader::raiseError(const QString& errorMessage)
{
    m_error = true;
    m_errorStr = errorMessage;
}

bool KdbxXmlAuthenticationFactorReader::parseFactorInfo(const QSharedPointer<AuthenticationFactorInfo>& info)
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "FactorInfo");

    bool compatVersionFound = false;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "CompatVersion") {
            const auto compatVersion = m_xml.readElementText();
            qDebug() << tr("Read authentication factor compat version: %1").arg(compatVersion);
            if (compatVersion != "1") {
                raiseError(tr("Incompatible authentication factor version"));
                return false;
            }
            compatVersionFound = true;
            continue;
        }

        if (m_xml.name() == "Comprehensive") {
            const auto comprehensive = m_xml.readElementText();
            if (comprehensive == "true") {
                qDebug() << tr("Secondary authentication factors are comprehensive");
                info.data()->setComprehensive(true);
            } else {
                raiseError(tr("Comprehensive set to unknown value %1").arg(comprehensive));
                return false;
            }
            continue;
        }

        if (m_xml.name() == "Group") {
            parseFactorGroup(info);

            if (m_error) {
                return false;
            }

            continue;
        }

        raiseError(
            tr("Unknown element type while processing authentication factor info: %1").arg(m_xml.name().toString()));
        return false;
    }

    return compatVersionFound;
}

bool KdbxXmlAuthenticationFactorReader::parseFactorGroup(const QSharedPointer<AuthenticationFactorInfo>& info)
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Group");

    auto group = QSharedPointer<AuthenticationFactorGroup>::create();

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "ValidationIn") {
            QByteArray value = QByteArray::fromBase64(m_xml.readElementText().toLatin1());
            if (value.isEmpty()) {
                raiseError(tr("Unable to decode validation input for authentication factor"));
                return false;
            }
            group->setValidationIn(value);
            continue;
        }
        if (m_xml.name() == "ValidationOut") {
            QByteArray value = QByteArray::fromBase64(m_xml.readElementText().toLatin1());
            if (value.isEmpty()) {
                raiseError(tr("Unable to decode validation output for authentication factor"));
                return false;
            }
            group->setValidationOut(value);
            continue;
        }
        if (m_xml.name() == "ValidationType") {
            const auto& text = m_xml.readElementText();

            AuthenticationFactorGroupValidationType validationType = AuthenticationFactorGroupValidationType::NONE;

            if (text == "HMAC-SHA512") {
                validationType = AuthenticationFactorGroupValidationType::HMAC_SHA512;
            }

            if (validationType == AuthenticationFactorGroupValidationType::NONE) {
                qWarning() << tr("Unknown authentication validation type %1").arg(text);
            }

            group->setValidationType(validationType);
            continue;
        }
        if (m_xml.name() == "Challenge") {
            QByteArray value = QByteArray::fromBase64(m_xml.readElementText().toLatin1());
            if (value.isEmpty()) {
                raiseError(tr("Unable to decode challenge for authentication factor"));
                return false;
            }
            group->setChallenge(value);
            continue;
        }
        if (m_xml.name() == "Factor") {
            parseFactor(group.data());

            if (m_error) {
                return false;
            }

            continue;
        }

        raiseError(
            tr("Unknown element type while processing authentication factor group: %1").arg(m_xml.name().toString()));
        return group;
    }

    if (group->getFactors().isEmpty()) {
        raiseError(tr("Authentication factor group is empty!"));
        return false;
    }

    bool foundCompatibleFactor = false;
    for (auto& factor : group->getFactors()) {
        if (factor->getFactorType() != FACTOR_TYPE_NULL && factor->getKeyType() != AuthenticationFactorKeyType::NONE) {
            foundCompatibleFactor = true;
            break;
        }
    }

    if (!foundCompatibleFactor) {
        raiseError(tr("An authentication factor group contains only unsupported factors"));
        return false;
    }

    info->addGroup(group);

    return true;
}

/**
 * Parses the <Factor> XML element from a header-stored FactorInfo block.
 *
 * @param group The group to which the factor belongs
 * @return true if parse successful; false on error
 */
bool KdbxXmlAuthenticationFactorReader::parseFactor(AuthenticationFactorGroup* group)
{
    Q_ASSERT(m_xml.isStartElement() && m_xml.name() == "Factor");

    auto factor = QSharedPointer<AuthenticationFactor>::create();

    bool foundFactorType = false;
    bool foundWrappedKey = false;
    bool foundKeyType = false;

    while (!m_xml.hasError() && m_xml.readNextStartElement()) {
        if (m_xml.name() == "Name") {
            const auto factorName = m_xml.readElementText();
            factor->setName(factorName);
            continue;
        }
        if (m_xml.name() == "TypeUUID") {
            // Lowercase to not care about how the UUID is formatted as much
            const auto& text = m_xml.readElementText().toLower();

            if (text == FACTOR_TYPE_PASSWORD_SHA256) {
                qDebug() << tr("Factor is a SHA256-hashed password");

                factor = QSharedPointer<PasswordAuthenticationFactor>::create(factor);
            } else if (text == FACTOR_TYPE_FIDO_ES256) {
                qDebug() << tr("Factor is a FIDO credential with type ES256");

                factor = QSharedPointer<FidoAuthenticationFactor>::create(factor);
            } else {
                qWarning() << tr("Unrecognized factor UUID %1").arg(text);
            }

            foundFactorType = true;
            continue;
        }
        if (m_xml.name() == "KeyType") {
            const auto& text = m_xml.readElementText();
            AuthenticationFactorKeyType type = AuthenticationFactorKeyType::NONE;

            if (text == "AES-CBC") {
                type = AuthenticationFactorKeyType::AES_CBC;
            }

            if (type == AuthenticationFactorKeyType::NONE) {
                qWarning() << tr("Unrecognized factor key type %1").arg(text);
            }

            // Note: unknown types get AuthenticationFactorKeyType::NONE - in other words, unusable
            factor->setKeyType(type);

            foundKeyType = true;
            continue;
        }
        if (m_xml.name() == "KeySalt") {
            QByteArray value = QByteArray::fromBase64(m_xml.readElementText().toLatin1());
            if (value.isEmpty()) {
                raiseError(tr("Unable to decode key salt for authentication factor"));
                return false;
            }
            factor->setKeySalt(value);
            continue;
        }
        if (m_xml.name() == "WrappedKey") {
            QByteArray value = QByteArray::fromBase64(m_xml.readElementText().toLatin1());

            if (value.isEmpty()) {
                raiseError(tr("Unable to decode wrapped key for authentication factor"));
                return false;
            }

            factor->setWrappedKey(value);
            foundWrappedKey = true;
            continue;
        }
        if (m_xml.name() == "CredentialID") {
            // This block should move to a FIDO-factor-type-specified code location eventually, but right now
            // since this is the only thing that isn't generic to all factors, it's here
            auto factorType = factor->getFactorType();
            if (factorType != FACTOR_TYPE_FIDO_ES256) {
                raiseError(tr("Encountered a CredentialID element on factor of non-FIDO type %1").arg(factorType));
                return false;
            }

            QByteArray value = QByteArray::fromBase64(m_xml.readElementText().toLatin1());
            if (value.isEmpty()) {
                raiseError(tr("Unable to decode FIDO credential ID for authentication factor"));
                return false;
            }

            factor.dynamicCast<FidoAuthenticationFactor>()->setCredentialID(value);

            continue;
        }

        raiseError(
            tr("Unknown element type while processing generic authentication factor: %1").arg(m_xml.name().toString()));
        return false;
    }

    if (!foundFactorType || !foundWrappedKey || !foundKeyType) {
        // Missing a required field (or several...)
        raiseError(tr("Factor %1 is missing required fields").arg(factor->getName()));
        return false;
    }

    group->addFactor(factor);

    return true;
}
