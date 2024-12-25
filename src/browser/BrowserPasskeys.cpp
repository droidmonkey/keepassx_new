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

#include "BrowserPasskeys.h"
#include "BrowserMessageBuilder.h"
#include "BrowserService.h"
#include "PasskeyUtils.h"
#include "config-keepassx.h"
#include "crypto/Random.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

#include <botan/data_src.h>
#include <botan/ec_group.h>
#include <botan/ecdsa.h>
#include <botan/ed25519.h>
#include <botan/pkcs8.h>
#include <botan/pubkey.h>
#include <botan/rsa.h>
#include <botan/sodium.h>

#include <bitset>

Q_GLOBAL_STATIC(BrowserPasskeys, s_browserPasskeys);

// KeePassXC AAGUID: fdb141b2-5d84-443e-8a35-4698c205a502
const QString BrowserPasskeys::AAGUID = QStringLiteral("fdb141b25d84443e8a354698c205a502");

// Authenticator capabilities
const QString BrowserPasskeys::ATTACHMENT_CROSS_PLATFORM = QStringLiteral("cross-platform");
const QString BrowserPasskeys::ATTACHMENT_PLATFORM = QStringLiteral("platform");
const QString BrowserPasskeys::AUTHENTICATOR_TRANSPORT_INTERNAL = QStringLiteral("internal");
const QString BrowserPasskeys::AUTHENTICATOR_TRANSPORT_NFC = QStringLiteral("nfc");
const QString BrowserPasskeys::AUTHENTICATOR_TRANSPORT_USB = QStringLiteral("usb");
const bool BrowserPasskeys::SUPPORT_RESIDENT_KEYS = true;
const bool BrowserPasskeys::SUPPORT_USER_VERIFICATION = true;

const QString BrowserPasskeys::PUBLIC_KEY = QStringLiteral("public-key");
const QString BrowserPasskeys::REQUIREMENT_DISCOURAGED = QStringLiteral("discouraged");
const QString BrowserPasskeys::REQUIREMENT_PREFERRED = QStringLiteral("preferred");
const QString BrowserPasskeys::REQUIREMENT_REQUIRED = QStringLiteral("required");

const QString BrowserPasskeys::PASSKEYS_ATTESTATION_DIRECT = QStringLiteral("direct");
const QString BrowserPasskeys::PASSKEYS_ATTESTATION_NONE = QStringLiteral("none");

BrowserPasskeys* BrowserPasskeys::instance()
{
    return s_browserPasskeys;
}

PublicKeyCredential BrowserPasskeys::buildRegisterPublicKeyCredential(const QJsonObject& credentialCreationOptions,
                                                                      const TestingVariables& testingVariables)
{
    if (!passkeyUtils()->checkCredentialCreationOptions(credentialCreationOptions)) {
        return {};
    }

    const auto authenticatorAttachment = credentialCreationOptions["authenticatorAttachment"];
    const auto clientDataJson = credentialCreationOptions["clientDataJSON"].toObject();
    const auto extensions = credentialCreationOptions["extensions"].toString();
    const auto credentialId = testingVariables.credentialId.isEmpty()
                                  ? browserMessageBuilder()->getRandomBytesAsBase64(ID_BYTES)
                                  : testingVariables.credentialId;

    // Credential private key
    const auto alg = getAlgorithmFromPublicKey(credentialCreationOptions);
    const auto privateKey = buildCredentialPrivateKey(alg, testingVariables.first, testingVariables.second);
    if (privateKey.cborEncodedPublicKey.isEmpty() && privateKey.privateKeyPem.isEmpty()) {
        // Key creation failed
        return {};
    }

    // Attestation
    const auto attestationObject = buildAttestationObject(
        credentialCreationOptions, extensions, credentialId, privateKey.cborEncodedPublicKey, testingVariables);
    if (attestationObject.isEmpty()) {
        return {};
    }

    // Authenticator data
    const auto authenticatorData = buildAuthenticatorData(credentialCreationOptions["rp"]["id"].toString(), extensions);

    // Response
    QJsonObject responseObject;
    responseObject["attestationObject"] = browserMessageBuilder()->getBase64FromArray(attestationObject);
    responseObject["clientDataJSON"] = browserMessageBuilder()->getBase64FromJson(clientDataJson);
    responseObject["clientExtensionResults"] = credentialCreationOptions["clientExtensionResults"];

    // Additions for extension side functions
    responseObject["authenticatorData"] = browserMessageBuilder()->getBase64FromArray(authenticatorData);
    responseObject["publicKeyAlgorithm"] = alg;

    // PublicKeyCredential
    QJsonObject publicKeyCredential;
    publicKeyCredential["authenticatorAttachment"] = authenticatorAttachment;
    publicKeyCredential["id"] = credentialId;
    publicKeyCredential["response"] = responseObject;
    publicKeyCredential["type"] = PUBLIC_KEY;

    PublicKeyCredential result;
    result.credentialId = credentialId;
    result.key = privateKey.privateKeyPem;
    result.response = publicKeyCredential;
    return result;
}

QJsonObject BrowserPasskeys::buildGetPublicKeyCredential(const QJsonObject& assertionOptions,
                                                         const QString& credentialId,
                                                         const QString& userHandle,
                                                         const QString& privateKeyPem)
{
    if (!passkeyUtils()->checkCredentialAssertionOptions(assertionOptions)) {
        return {};
    }

    const auto authenticatorData =
        buildAuthenticatorData(assertionOptions["rpId"].toString(), assertionOptions["extensions"].toString());
    const auto clientDataJson = assertionOptions["clientDataJson"].toObject();
    const auto clientDataArray = QJsonDocument(clientDataJson).toJson(QJsonDocument::Compact);

    const auto signature = buildSignature(authenticatorData, clientDataArray, privateKeyPem);
    if (signature.isEmpty()) {
        return {};
    }

    QJsonObject responseObject;
    responseObject["authenticatorData"] = browserMessageBuilder()->getBase64FromArray(authenticatorData);
    responseObject["clientDataJSON"] = browserMessageBuilder()->getBase64FromJson(clientDataJson);
    responseObject["clientExtensionResults"] = assertionOptions["clientExtensionResults"];
    responseObject["signature"] = browserMessageBuilder()->getBase64FromArray(signature);
    responseObject["userHandle"] = userHandle;

    QJsonObject publicKeyCredential;
    publicKeyCredential["authenticatorAttachment"] = BrowserPasskeys::ATTACHMENT_PLATFORM;
    publicKeyCredential["id"] = credentialId;
    publicKeyCredential["response"] = responseObject;
    publicKeyCredential["type"] = PUBLIC_KEY;

    return publicKeyCredential;
}

// https://w3c.github.io/webauthn/#attestation-object
QByteArray BrowserPasskeys::buildAttestationObject(const QJsonObject& credentialCreationOptions,
                                                   const QString& extensions,
                                                   const QString& credentialId,
                                                   const QByteArray& cborEncodedPublicKey,
                                                   const TestingVariables& testingVariables)
{
    QByteArray result;

    // Create SHA256 hash from rpId
    const auto rpIdHash = browserMessageBuilder()->getSha256Hash(credentialCreationOptions["rp"]["id"].toString());
    result.append(rpIdHash);

    // Use default flags
    const auto flags = setFlagsFromJson(QJsonObject(
        {{"ED", !extensions.isEmpty()}, {"AT", true}, {"BS", false}, {"BE", false}, {"UV", true}, {"UP", true}}));
    result.append(flags);

    // Signature counter (not supported, always 0
    const char counter[4] = {0x00, 0x00, 0x00, 0x00};
    result.append(QByteArray::fromRawData(counter, 4));

    // AAGUID
    result.append(browserMessageBuilder()->getArrayFromHexString(AAGUID));

    // Credential length
    const char credentialLength[2] = {0x00, ID_BYTES};
    result.append(QByteArray::fromRawData(credentialLength, 2));

    // Credential Id
    result.append(QByteArray::fromBase64(
        testingVariables.credentialId.isEmpty() ? credentialId.toUtf8() : testingVariables.credentialId.toUtf8(),
        QByteArray::Base64UrlEncoding));

    // Credential public key
    result.append(cborEncodedPublicKey);

    // Add extension data if available
    if (!extensions.isEmpty()) {
        result.append(browserMessageBuilder()->getArrayFromBase64(extensions));
    }

    // The final result should be CBOR encoded
    return m_browserCbor.cborEncodeAttestation(result);
}

// Build a short version of the attestation object for webauthn.get
QByteArray BrowserPasskeys::buildAuthenticatorData(const QString& rpId, const QString& extensions)
{
    QByteArray result;

    const auto rpIdHash = browserMessageBuilder()->getSha256Hash(rpId);
    result.append(rpIdHash);

    const auto flags = setFlagsFromJson(QJsonObject(
        {{"ED", !extensions.isEmpty()}, {"AT", false}, {"BS", false}, {"BE", false}, {"UV", true}, {"UP", true}}));
    result.append(flags);

    // Signature counter (not supported, always 0
    const char counter[4] = {0x00, 0x00, 0x00, 0x00};
    result.append(QByteArray::fromRawData(counter, 4));

    if (!extensions.isEmpty()) {
        result.append(browserMessageBuilder()->getArrayFromBase64(extensions));
    }

    return result;
}

// See: https://w3c.github.io/webauthn/#sctn-encoded-credPubKey-examples
AttestationKeyPair
BrowserPasskeys::buildCredentialPrivateKey(int alg, const QString& predefinedFirst, const QString& predefinedSecond)
{
    // Only support -7, P256 (EC), -8 (EdDSA) and -257 (RSA) for now
    if (alg != WebAuthnAlgorithms::ES256 && alg != WebAuthnAlgorithms::RS256 && alg != WebAuthnAlgorithms::EDDSA) {
        return {};
    }

    QByteArray firstPart;
    QByteArray secondPart;
    QByteArray pem;

    if (!predefinedFirst.isEmpty() && !predefinedSecond.isEmpty()) {
        firstPart = browserMessageBuilder()->getArrayFromBase64(predefinedFirst);
        secondPart = browserMessageBuilder()->getArrayFromBase64(predefinedSecond);
    } else {
        if (alg == WebAuthnAlgorithms::ES256) {
            try {
                Botan::ECDSA_PrivateKey privateKey(*randomGen()->getRng(), Botan::EC_Group("secp256r1"));
                const auto& publicPoint = privateKey.public_point();
                auto x = publicPoint.get_affine_x();
                auto y = publicPoint.get_affine_y();
                firstPart = bigIntToQByteArray(x);
                secondPart = bigIntToQByteArray(y);

                auto privateKeyPem = Botan::PKCS8::PEM_encode(privateKey);
                pem = QByteArray::fromStdString(privateKeyPem);
            } catch (std::exception& e) {
                qWarning("BrowserWebAuthn::buildCredentialPrivateKey: Could not create EC2 private key: %s", e.what());
                return {};
            }
        } else if (alg == WebAuthnAlgorithms::RS256) {
            try {
                Botan::RSA_PrivateKey privateKey(*randomGen()->getRng(), RSA_BITS, RSA_EXPONENT);
                auto modulus = privateKey.get_n();
                auto exponent = privateKey.get_e();
                firstPart = bigIntToQByteArray(modulus);
                secondPart = bigIntToQByteArray(exponent);

                auto privateKeyPem = Botan::PKCS8::PEM_encode(privateKey);
                pem = QByteArray::fromStdString(privateKeyPem);
            } catch (std::exception& e) {
                qWarning("BrowserWebAuthn::buildCredentialPrivateKey: Could not create RSA private key: %s", e.what());
                return {};
            }
        } else if (alg == WebAuthnAlgorithms::EDDSA) {
            try {
                Botan::Ed25519_PrivateKey key(*randomGen()->getRng());
                auto publicKey = key.get_public_key();
#ifdef WITH_XC_BOTAN3
                auto privateKey = key.raw_private_key_bits();
#else
                auto privateKey = key.get_private_key();
#endif
                firstPart = browserMessageBuilder()->getQByteArray(publicKey.data(), publicKey.size());
                secondPart = browserMessageBuilder()->getQByteArray(privateKey.data(), privateKey.size());

                auto privateKeyPem = Botan::PKCS8::PEM_encode(key);
                pem = QByteArray::fromStdString(privateKeyPem);
            } catch (std::exception& e) {
                qWarning("BrowserWebAuthn::buildCredentialPrivateKey: Could not create EdDSA private key: %s",
                         e.what());
                return {};
            }
        }
    }

    auto result = m_browserCbor.cborEncodePublicKey(alg, firstPart, secondPart);
    if (result.isEmpty()) {
        return {};
    }

    AttestationKeyPair attestationKeyPair;
    attestationKeyPair.cborEncodedPublicKey = result;
    attestationKeyPair.privateKeyPem = pem;
    return attestationKeyPair;
}

QByteArray BrowserPasskeys::buildSignature(const QByteArray& authenticatorData,
                                           const QByteArray& clientData,
                                           const QString& privateKeyPem)
{
    const auto clientDataHash = browserMessageBuilder()->getSha256Hash(clientData);
    const auto attToBeSigned = authenticatorData + clientDataHash;

    try {
        const auto privateKeyArray = privateKeyPem.toUtf8();
        Botan::DataSource_Memory dataSource(reinterpret_cast<const uint8_t*>(privateKeyArray.constData()),
                                            privateKeyArray.size());

        const auto key = Botan::PKCS8::load_key(dataSource).release();
        const auto privateKeyBytes = key->private_key_bits();
        const auto algName = key->algo_name();
        const auto algId = key->algorithm_identifier();

        std::vector<uint8_t> rawSignature;
        if (algName == "ECDSA") {
            Botan::ECDSA_PrivateKey privateKey(algId, privateKeyBytes);
#ifdef WITH_XC_BOTAN3
            Botan::PK_Signer signer(
                privateKey, *randomGen()->getRng(), "EMSA1(SHA-256)", Botan::Signature_Format::DerSequence);
#else
            Botan::PK_Signer signer(privateKey, *randomGen()->getRng(), "EMSA1(SHA-256)", Botan::DER_SEQUENCE);
#endif

            signer.update(reinterpret_cast<const uint8_t*>(attToBeSigned.constData()), attToBeSigned.size());
            rawSignature = signer.signature(*randomGen()->getRng());
        } else if (algName == "RSA") {
            Botan::RSA_PrivateKey privateKey(algId, privateKeyBytes);
            Botan::PK_Signer signer(privateKey, *randomGen()->getRng(), "EMSA3(SHA-256)");

            signer.update(reinterpret_cast<const uint8_t*>(attToBeSigned.constData()), attToBeSigned.size());
            rawSignature = signer.signature(*randomGen()->getRng());
        } else if (algName == "Ed25519") {
            Botan::Ed25519_PrivateKey privateKey(algId, privateKeyBytes);
            // "Pure" here means signing message directly. SHA-512 is only used with pre-hashed Ed25519 (Ed25519ph).
            Botan::PK_Signer signer(privateKey, *randomGen()->getRng(), "Pure");

            signer.update(reinterpret_cast<const uint8_t*>(attToBeSigned.constData()), attToBeSigned.size());
            rawSignature = signer.signature(*randomGen()->getRng());
        } else {
            qWarning("BrowserWebAuthn::buildSignature: Algorithm not supported");
            return {};
        }

        auto signature = QByteArray(reinterpret_cast<char*>(rawSignature.data()), rawSignature.size());
        return signature;
    } catch (std::exception& e) {
        qWarning("BrowserWebAuthn::buildSignature: Could not sign key: %s", e.what());
        return {};
    }
}

// Parse authentication data byte array to JSON
// See: https://www.w3.org/TR/webauthn/images/fido-attestation-structures.svg
// And: https://w3c.github.io/webauthn/#attested-credential-data
QJsonObject BrowserPasskeys::parseAuthData(const QByteArray& authData) const
{
    auto rpIdHash = authData.mid(AuthDataOffsets::RPIDHASH, HASH_BYTES);
    auto flags = authData.mid(AuthDataOffsets::FLAGS, 1);
    auto counter = authData.mid(AuthDataOffsets::SIGNATURE_COUNTER, 4);
    auto aaGuid = authData.mid(AuthDataOffsets::AAGUID, 16);
    auto credentialLength = authData.mid(AuthDataOffsets::CREDENTIAL_LENGTH, 2);
    auto credLen = qFromBigEndian<quint16>(credentialLength.data());
    auto credentialId = authData.mid(AuthDataOffsets::CREDENTIAL_ID, credLen);
    auto publicKey = authData.mid(AuthDataOffsets::CREDENTIAL_ID + credLen);

    QJsonObject credentialDataJson({{"aaguid", browserMessageBuilder()->getBase64FromArray(aaGuid)},
                                    {"credentialId", browserMessageBuilder()->getBase64FromArray(credentialId)},
                                    {"publicKey", m_browserCbor.getJsonFromCborData(publicKey)}});

    QJsonObject result({{"credentialData", credentialDataJson},
                        {"flags", parseFlags(flags)},
                        {"rpIdHash", browserMessageBuilder()->getBase64FromArray(rpIdHash)},
                        {"signatureCounter", QJsonValue(qFromBigEndian<int>(counter))}});

    return result;
}

// See: https://w3c.github.io/webauthn/#table-authData
QJsonObject BrowserPasskeys::parseFlags(const QByteArray& flags) const
{
    if (flags.isEmpty()) {
        return {};
    }

    auto flagsByte = static_cast<uint8_t>(flags[0]);
    std::bitset<8> flagBits(flagsByte);

    return QJsonObject({{"ED", flagBits.test(AuthenticatorFlags::ED)},
                        {"AT", flagBits.test(AuthenticatorFlags::AT)},
                        {"BS", flagBits.test(AuthenticatorFlags::BS)},
                        {"BE", flagBits.test(AuthenticatorFlags::BE)},
                        {"UV", flagBits.test(AuthenticatorFlags::UV)},
                        {"UP", flagBits.test(AuthenticatorFlags::UP)}});
}

// https://w3c.github.io/webauthn/#table-authData
// ED - Extension Data, AT - Attested Credential, BS - Reserved
// BE - Reserved , UV - User Verified, UP - User Present
char BrowserPasskeys::setFlagsFromJson(const QJsonObject& flags) const
{
    if (flags.isEmpty()) {
        return 0;
    }

    char flagBits = 0x00;
    auto setFlag = [&](const char* key, unsigned char bit) {
        if (flags[key].toBool()) {
            flagBits |= 1 << bit;
        }
    };

    setFlag("ED", AuthenticatorFlags::ED);
    setFlag("AT", AuthenticatorFlags::AT);
    setFlag("BS", AuthenticatorFlags::BS);
    setFlag("BE", AuthenticatorFlags::BE);
    setFlag("UV", AuthenticatorFlags::UV);
    setFlag("UP", AuthenticatorFlags::UP);

    return flagBits;
}

// Returns the first supported algorithm from the pubKeyCredParams list (only support ES256, RS256 and EdDSA for now)
WebAuthnAlgorithms BrowserPasskeys::getAlgorithmFromPublicKey(const QJsonObject& credentialCreationOptions) const
{
    const auto pubKeyCredParams = credentialCreationOptions["credTypesAndPubKeyAlgs"].toArray();
    if (!pubKeyCredParams.isEmpty()) {
        const auto alg = pubKeyCredParams.first()["alg"].toInt();
        if (alg == WebAuthnAlgorithms::ES256 || alg == WebAuthnAlgorithms::RS256 || alg == WebAuthnAlgorithms::EDDSA) {
            return static_cast<WebAuthnAlgorithms>(alg);
        }
    }

    return WebAuthnAlgorithms::ES256;
}

QByteArray BrowserPasskeys::bigIntToQByteArray(Botan::BigInt& bigInt) const
{
    auto hexString = QString(bigInt.to_hex_string().c_str());

    // Botan might add a leading "0x" to the hex string depending on the version. Remove it.
    if (hexString.startsWith(("0x"))) {
        hexString.remove(0, 2);
    }

    return browserMessageBuilder()->getArrayFromHexString(hexString);
}
