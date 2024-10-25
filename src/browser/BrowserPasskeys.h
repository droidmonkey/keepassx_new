/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef BROWSERPASSKEYS_H
#define BROWSERPASSKEYS_H

#include "BrowserCbor.h"
#include <QJsonObject>
#include <QObject>

#include <botan/asn1_obj.h>
#include <botan/bigint.h>

#define ID_BYTES 32
#define HASH_BYTES 32
#define RSA_BITS 2048
#define RSA_EXPONENT 65537

enum AuthDataOffsets : int
{
    RPIDHASH = 0,
    FLAGS = 32,
    SIGNATURE_COUNTER = 33,
    AAGUID = 37,
    CREDENTIAL_LENGTH = 53,
    CREDENTIAL_ID = 55
};

enum AuthenticatorFlags
{
    UP = 0,
    UV = 2,
    BE = 3,
    BS = 4,
    AT = 6,
    ED = 7
};

struct PublicKeyCredential
{
    QString credentialId;
    QJsonObject response;
    QByteArray key;
};

struct AttestationKeyPair
{
    QByteArray cborEncodedPublicKey;
    QByteArray privateKeyPem;
};

// Predefined variables used for testing the class
struct TestingVariables
{
    QString credentialId;
    QString first;
    QString second;
};

class BrowserPasskeys : public QObject
{
    Q_OBJECT

public:
    explicit BrowserPasskeys() = default;
    ~BrowserPasskeys() = default;
    static BrowserPasskeys* instance();

    PublicKeyCredential buildRegisterPublicKeyCredential(const QJsonObject& credentialCreationOptions,
                                                         const TestingVariables& predefinedVariables = {});
    QJsonObject buildGetPublicKeyCredential(const QJsonObject& assertionOptions,
                                            const QString& credentialId,
                                            const QString& userHandle,
                                            const QString& privateKeyPem);

    static const QString AAGUID;

    static const QString ATTACHMENT_CROSS_PLATFORM;
    static const QString ATTACHMENT_PLATFORM;
    static const QString AUTHENTICATOR_TRANSPORT_INTERNAL;
    static const QString AUTHENTICATOR_TRANSPORT_NFC;
    static const QString AUTHENTICATOR_TRANSPORT_USB;
    static const bool SUPPORT_RESIDENT_KEYS;
    static const bool SUPPORT_USER_VERIFICATION;

    static const QString PUBLIC_KEY;
    static const QString REQUIREMENT_DISCOURAGED;
    static const QString REQUIREMENT_PREFERRED;
    static const QString REQUIREMENT_REQUIRED;

    static const QString PASSKEYS_ATTESTATION_DIRECT;
    static const QString PASSKEYS_ATTESTATION_NONE;

private:
    QByteArray buildAttestationObject(const QJsonObject& credentialCreationOptions,
                                      const QString& extensions,
                                      const QString& credentialId,
                                      const QByteArray& cborEncodedPublicKey,
                                      const TestingVariables& predefinedVariables = {});
    QByteArray buildAuthenticatorData(const QString& rpId, const QString& extensions);
    AttestationKeyPair buildCredentialPrivateKey(int alg,
                                                 const QString& predefinedFirst = QString(),
                                                 const QString& predefinedSecond = QString());
    QByteArray
    buildSignature(const QByteArray& authenticatorData, const QByteArray& clientData, const QString& privateKeyPem);
    QJsonObject parseAuthData(const QByteArray& authData) const;
    QJsonObject parseFlags(const QByteArray& flags) const;
    char setFlagsFromJson(const QJsonObject& flags) const;
    WebAuthnAlgorithms getAlgorithmFromPublicKey(const QJsonObject& credentialCreationOptions) const;
    QByteArray bigIntToQByteArray(Botan::BigInt& bigInt) const;

    Q_DISABLE_COPY(BrowserPasskeys);

    friend class TestPasskeys;

private:
    BrowserCbor m_browserCbor;
};

static inline BrowserPasskeys* browserPasskeys()
{
    return BrowserPasskeys::instance();
}

#endif // BROWSERPASSKEYS_H
