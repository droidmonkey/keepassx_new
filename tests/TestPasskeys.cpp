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

#include "TestPasskeys.h"
#include "browser/BrowserCbor.h"
#include "browser/BrowserMessageBuilder.h"
#include "browser/BrowserPasskeysClient.h"
#include "browser/BrowserService.h"
#include "browser/PasskeyUtils.h"
#include "core/Database.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "crypto/Crypto.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>
#include <botan/sodium.h>

using namespace Botan::Sodium;

QTEST_GUILESS_MAIN(TestPasskeys)

// Register request
// clang-format off
const QString PublicKeyCredentialOptions = R"(
    {
        "attestation": "none",
        "authenticatorSelection": {
            "residentKey": "preferred",
            "requireResidentKey": false,
            "userVerification": "required"
        },
        "challenge": "lVeHzVxWsr8MQxMkZF0ti6FXhdgMljqKzgA-q_zk2Mnii3eJ47VF97sqUoYktVC85WAZ1uIASm-a_lDFZwsLfw",
        "pubKeyCredParams": [
            {
                "type": "public-key",
                "alg": -7
            },
            {
                "type": "public-key",
                "alg": -257
            }
        ],
        "rp": {
            "name": "webauthn.io",
            "id": "webauthn.io"
        },
        "timeout": 60000,
        "excludeCredentials": [],
        "user": {
            "displayName": "Test User",
            "id": "VkdWemRDQlZjMlZ5",
            "name": "Test User"
        }
    }
)";

// Register response
const QString PublicKeyCredential = R"(
    {
        "authenticatorAttachment": "platform",
        "id": "yrzFJ5lwcpTwYMOdXSmxF5b5cYQlqBMzbbU_d-oFLO8",
        "rawId": "cabcc52799707294f060c39d5d29b11796f9718425a813336db53f77ea052cef",
        "response": {
            "attestationObject": "o2NmbXRkbm9uZWdhdHRTdG10oGhhdXRoRGF0YVikdKbqkhPJnC90siSSsyDPQCYqlMGpUKA5fyklC2CEHvBFAAAAAP2xQbJdhEQ-ijVGmMIFpQIAIMq8xSeZcHKU8GDDnV0psReW-XGEJagTM221P3fqBSzvpQECAyYgASFYIAbsrzRbYpFhbRlZA6ZQKsoxxJWoaeXwh-XUuDLNCIXdIlgg4u5_6Q8O6R0Hg0oDCdtCJLEL0yX_GDLhU5m3HUIE54M",
            "clientDataJSON": "eyJ0eXBlIjoid2ViYXV0aG4uY3JlYXRlIiwiY2hhbGxlbmdlIjoibFZlSHpWeFdzcjhNUXhNa1pGMHRpNkZYaGRnTWxqcUt6Z0EtcV96azJNbmlpM2VKNDdWRjk3c3FVb1lrdFZDODVXQVoxdUlBU20tYV9sREZad3NMZnciLCJvcmlnaW4iOiJodHRwczovL3dlYmF1dGhuLmlvIiwiY3Jvc3NPcmlnaW4iOmZhbHNlfQ"
        },
        "type": "public-key"
    }
)";

// Get request
const QString PublicKeyCredentialRequestOptions = R"(
    {
        "allowCredentials": [
            {
                "id": "yrzFJ5lwcpTwYMOdXSmxF5b5cYQlqBMzbbU_d-oFLO8",
                "transports": ["internal"],
                "type": "public-key"
            }
        ],
        "challenge": "9z36vTfQTL95Lf7WnZgyte7ohGeF-XRiLxkL-LuGU1zopRmMIUA1LVwzGpyIm1fOBn1QnRa0QH27ADAaJGHysQ",
        "rpId": "webauthn.io",
        "timeout": 60000,
        "userVerification": "required"
    }
)";

const QJsonArray validPubKeyCredParams = {
    QJsonObject({
       {"type", "public-key"},
       {"alg", -7}
    }),
    QJsonObject({
        {"type", "public-key"},
        {"alg", -257}
    }),
};

// clang-format on

void TestPasskeys::initTestCase()
{
    QVERIFY(Crypto::init());
}

void TestPasskeys::init()
{
}

void TestPasskeys::testBase64WithHexStrings()
{
    const size_t bufSize = 64;
    unsigned char buf[bufSize] = {31,  141, 30,  29,  142, 73,  5,   239, 242, 84,  187, 202, 40,  54,  15,  223,
                                  201, 0,   108, 109, 209, 104, 207, 239, 160, 89,  208, 117, 134, 66,  42,  12,
                                  31,  66,  163, 248, 221, 88,  241, 164, 6,   55,  182, 97,  186, 243, 162, 162,
                                  81,  220, 55,  60,  93,  207, 170, 222, 56,  234, 227, 45,  115, 175, 138, 182};

    auto base64FromArray = browserMessageBuilder()->getBase64FromArray(reinterpret_cast<const char*>(buf), bufSize);
    QCOMPARE(base64FromArray,
             QString("H40eHY5JBe_yVLvKKDYP38kAbG3RaM_voFnQdYZCKgwfQqP43VjxpAY3tmG686KiUdw3PF3Pqt446uMtc6-Ktg"));

    auto arrayFromBase64 = browserMessageBuilder()->getArrayFromBase64(base64FromArray);
    QCOMPARE(arrayFromBase64.size(), bufSize);

    for (size_t i = 0; i < bufSize; i++) {
        QCOMPARE(static_cast<unsigned char>(arrayFromBase64.at(i)), buf[i]);
    }

    auto randomDataBase64 = browserMessageBuilder()->getRandomBytesAsBase64(24);
    QCOMPARE(randomDataBase64.isEmpty(), false);
}

void TestPasskeys::testDecodeResponseData()
{
    const auto publicKeyCredential = browserMessageBuilder()->getJsonObject(PublicKeyCredential.toUtf8());
    auto response = publicKeyCredential["response"].toObject();
    auto clientDataJson = response["clientDataJSON"].toString();
    auto attestationObject = response["attestationObject"].toString();

    QVERIFY(!clientDataJson.isEmpty());
    QVERIFY(!attestationObject.isEmpty());

    // Parse clientDataJSON
    auto clientDataByteArray = browserMessageBuilder()->getArrayFromBase64(clientDataJson);
    auto clientDataJsonObject = browserMessageBuilder()->getJsonObject(clientDataByteArray);
    QCOMPARE(clientDataJsonObject["challenge"],
             QString("lVeHzVxWsr8MQxMkZF0ti6FXhdgMljqKzgA-q_zk2Mnii3eJ47VF97sqUoYktVC85WAZ1uIASm-a_lDFZwsLfw"));
    QCOMPARE(clientDataJsonObject["origin"], QString("https://webauthn.io"));
    QCOMPARE(clientDataJsonObject["type"], QString("webauthn.create"));

    // Parse attestationObject (CBOR decoding needed)
    BrowserCbor browserCbor;
    auto attestationByteArray = browserMessageBuilder()->getArrayFromBase64(attestationObject);
    auto attestationJsonObject = browserCbor.getJsonFromCborData(attestationByteArray);

    // Parse authData
    auto authDataJsonObject = attestationJsonObject["authData"].toString();
    auto authDataArray = browserMessageBuilder()->getArrayFromBase64(authDataJsonObject);
    QVERIFY(authDataArray.size() >= 37);

    auto authData = browserPasskeys()->parseAuthData(authDataArray);
    auto credentialData = authData["credentialData"].toObject();
    auto flags = authData["flags"].toObject();
    auto publicKey = credentialData["publicKey"].toObject();

    // The attestationObject should include the same ID after decoding with the response root
    QCOMPARE(credentialData["credentialId"].toString(), publicKeyCredential["id"].toString());
    QCOMPARE(credentialData["aaguid"].toString(), QString("_bFBsl2ERD6KNUaYwgWlAg"));
    QCOMPARE(authData["rpIdHash"].toString(), QString("dKbqkhPJnC90siSSsyDPQCYqlMGpUKA5fyklC2CEHvA"));
    QCOMPARE(flags["AT"], true);
    QCOMPARE(flags["UP"], true);
    QCOMPARE(publicKey["1"], 2);
    QCOMPARE(publicKey["3"], -7);
    QCOMPARE(publicKey["-1"], 1);
    QCOMPARE(publicKey["-2"], QString("BuyvNFtikWFtGVkDplAqyjHElahp5fCH5dS4Ms0Ihd0"));
    QCOMPARE(publicKey["-3"], QString("4u5_6Q8O6R0Hg0oDCdtCJLEL0yX_GDLhU5m3HUIE54M"));
}

void TestPasskeys::testLoadingECPrivateKeyFromPem()
{
#if BOTAN_VERSION_CODE < BOTAN_VERSION_CODE_FOR(2, 14, 0)
    QSKIP("ECDSA Signature is broken on Botan < 2.14.0");
#endif
    const auto publicKeyCredentialRequestOptions =
        browserMessageBuilder()->getJsonObject(PublicKeyCredentialRequestOptions.toUtf8());
    const auto privateKeyPem = QString("-----BEGIN PRIVATE KEY-----"
                                       "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg5DX2R6I37nMSZqCp"
                                       "XfHlE3UeitkGGE03FqGsdfxIBoOhRANCAAQG7K80W2KRYW0ZWQOmUCrKMcSVqGnl"
                                       "8Ifl1LgyzQiF3eLuf+kPDukdB4NKAwnbQiSxC9Ml/xgy4VOZtx1CBOeD"
                                       "-----END PRIVATE KEY-----");

    const auto authenticatorData =
        browserMessageBuilder()->getArrayFromBase64("dKbqkhPJnC90siSSsyDPQCYqlMGpUKA5fyklC2CEHvAFAAAAAA");
    const auto clientData = browserMessageBuilder()->getArrayFromBase64(
        "eyJ0eXBlIjoid2ViYXV0aG4uZ2V0IiwiY2hhbGxlbmdlIjoiOXozNnZUZlFUTDk1TGY3V25aZ3l0ZTdvaEdlRi1YUmlMeGtMLUx1R1Uxem9wUm"
        "1NSVVBMUxWd3pHcHlJbTFmT0JuMVFuUmEwUUgyN0FEQWFKR0h5c1EiLCJvcmlnaW4iOiJodHRwczovL3dlYmF1dGhuLmlvIiwiY3Jvc3NPcmln"
        "aW4iOmZhbHNlfQ");

    const auto signature = browserPasskeys()->buildSignature(authenticatorData, clientData, privateKeyPem);
    QCOMPARE(
        browserMessageBuilder()->getBase64FromArray(signature),
        QString("MEYCIQCpbDaYJ4b2ofqWBxfRNbH3XCpsyao7Iui5lVuJRU9HIQIhAPl5moNZgJu5zmurkKK_P900Ct6wd3ahVIqCEqTeeRdE"));
}

void TestPasskeys::testLoadingRSAPrivateKeyFromPem()
{
    const auto privateKeyPem = QString("-----BEGIN PRIVATE KEY-----"
                                       "MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQC5OHjBHQaRfxxX\n4WHRmqq7e7JgT"
                                       "FRs1bd4dIOFAOZnhNE3vAg2IF5VurmeB+9ye9xh7frw8ubrL0cv\nsBWiJfN5CY3SYGRLbGTtBC0fZ6"
                                       "OhhhjwvVM1GW6nVeRU66atzuo4NBfYXJWIYECd\npRBU4+xsDL4vJnn1mj05+v/Tqp6Uo1HrEPx9+Dc"
                                       "oYJD+cw7+OQ83XeGmjD+Dtm5z\nNIyYdweaafVR4PEUlB3CYZuOq9xcpxay3ps2MuYT1zGoiQqk6fla"
                                       "d+0tBWGY8Lwp\nCVulXCv7ljNJ4gxgQtOqWX8j2hC0hBxeqNYDYbrkECid3TsMTEMcV5uaVJXULg4t"
                                       "\nn6UItA11AgMBAAECggEAC3B0WBxHuieIfllOOOC4H9/7S7fDH2f7+W2cFtQ6pqo9\nCq0WBmkYMmw"
                                       "Xx9hpHoq4TnhhHyL9WzPzuKYD0Vx4gvacV/ckkppFScnQKJ2hF+99\nLax1DbU+UImSknfDDFPYbYld"
                                       "b1CD2rpJG1i6X2fRQ6NuK+F7jE05mqcIyE+ZajK+\nIpx8XFmE+tI1EEWsn3CzxMLiTQfXyFt/drM9i"
                                       "GYfcDjYY+q5vzGU3Kxj68gjc96A\nOra79DGOmwX+4zIwo5sSzI3noHnhWPLsaRtE5jWu21Qkb+1BvB"
                                       "jPmbQfN274OQfy\n8/BNNR/NZM1mJm/8x4Mt+h5d946XlIo0AkyYZXY/UQKBgQDYI3G3BCwaYk6MDMe"
                                       "T\nIamRZo25phPtr3if47dhT2xFWJplIt35sW+6KjD6c1Qpb2aGOUh7JPmb57H54OgJ\nmojkS5tv9Y"
                                       "EQZFfgCCZoeuqBx+ArqtJdkXOiNEFS0dpt44I+eO3Do5pnwKRemH+Y\ncqJ/eMH96UMzYDO7WNsyOyo"
                                       "5UQKBgQDbYU0KbGbTrMEV4T9Q41rZ2TnWzs5moqn2\nCRtB8LOdKAZUG7FRsw5KgC1CvFn3Xuk+qphY"
                                       "GUQeJvv7FjxMRUv4BktNpXju6eUj\n3tWHzI2QOkHaeq/XibwbNomfkdyTjtLX2+v8DBHcZnCSlukxc"
                                       "JISyPqZ6CnTjXGE\nEGB+itBI5QKBgQCA+gWttOusguVkZWvivL+3aH9CPXy+5WsR3o1boE13xDu+Bm"
                                       "R3\n0A5gBTVc/t1GLJf9mMlL0vCwvD5UYoWU1YbC1OtYkCQIaBiYM8TXrCGseF2pMTJ/\na4CZVp10k"
                                       "o3J7W2XYgpgKIzHRQnQ+SeLDT0y3BjHMB9N1SaJsah7/RphQQKBgQCr\nL+4yKAzFOJUjQbVqpT8Lp5"
                                       "qeqJofNOdzef+vIOjHxafKkiF4I0UPlZ276cY6ZfGU\nWQKwHGcvMDSI5fz/d0OksySn3mvT4uhPaV8"
                                       "urMv6s7sXhY0Zn/0NLy2NOwDolBar\nIo2vDKwTVEyb1u75CWKzDemfl66ryj++Uhk6JZAKkQKBgQCc"
                                       "NYVe7m648DzD0nu9\n3lgetBTaAS1zZmMs8Cinj44v0ksfqxrRBzBZcO9kCQqiJZ7uCAaVYcQ+PwkY+"
                                       "05C\n+w1+KvdGcKM+8TQYTQM3s2B9IyKExRS/dbQf9F7stJL+k5vbt6OUerwfmbNI9R3t\ngDZ4DEfo"
                                       "pPivs9dnequ9wfaPOw=="
                                       "-----END PRIVATE KEY-----");

    const auto authenticatorData =
        browserMessageBuilder()->getArrayFromBase64("dKbqkhPJnC90siSSsyDPQCYqlMGpUKA5fyklC2CEHvAFAAAAAA");
    const auto clientData = browserMessageBuilder()->getArrayFromBase64(
        "eyJ0eXBlIjoid2ViYXV0aG4uZ2V0IiwiY2hhbGxlbmdlIjoiOXozNnZUZlFUTDk1TGY3V25aZ3l0ZTdvaEdlRi1YUmlMeGtMLUx1R1Uxem9wUm"
        "1NSVVBMUxWd3pHcHlJbTFmT0JuMVFuUmEwUUgyN0FEQWFKR0h5c1EiLCJvcmlnaW4iOiJodHRwczovL3dlYmF1dGhuLmlvIiwiY3Jvc3NPcmln"
        "aW4iOmZhbHNlfQ");

    const auto signature = browserPasskeys()->buildSignature(authenticatorData, clientData, privateKeyPem);
    QCOMPARE(
        browserMessageBuilder()->getBase64FromArray(signature),
        QString("MOGw6KrerCgPf2mPig7FOTFIUDXYAU1v2uZj89_NgQTg2UddWnAB3JId3pa4zXghj8CkjjadVOI_LvweJGCEpmPQnRby71yFXnja6j"
                "Y3woX2b2klG2fB2alGZHHrVg6yVEmnAii4kYSdmoWxI7SmzLftoZfCJNFPFHujx2Pbr-6dIB02sZhtncetT0cpyWobtj9r7C5dIGfm"
                "J5n-LccP-F9gXGqtbN605VrIkC2WNztjdk3dAt5FGM_dlIwSe-vP1dKfIuNqAEbgr2IVZAUFn_ZfzUo-XbXTysksuz9JZfEopJBiUi"
                "9tjQDNvrYQFqB6wDPqkZAomkbRCohUb3TzCg"));
}

void TestPasskeys::testCreatingAttestationObjectWithEC()
{
    // Predefined values for a desired outcome
    const auto id = QString("yrzFJ5lwcpTwYMOdXSmxF5b5cYQlqBMzbbU_d-oFLO8");
    const auto predefinedFirst = QString("BuyvNFtikWFtGVkDplAqyjHElahp5fCH5dS4Ms0Ihd0");
    const auto predefinedSecond = QString("4u5_6Q8O6R0Hg0oDCdtCJLEL0yX_GDLhU5m3HUIE54M");

    const auto publicKeyCredentialOptions = browserMessageBuilder()->getJsonObject(PublicKeyCredentialOptions.toUtf8());
    QJsonObject credentialCreationOptions;
    browserPasskeysClient()->getCredentialCreationOptions(
        publicKeyCredentialOptions, QString("https://webauthn.io"), &credentialCreationOptions);

    auto rpIdHash = browserMessageBuilder()->getSha256HashAsBase64(QString("webauthn.io"));
    QCOMPARE(rpIdHash, QString("dKbqkhPJnC90siSSsyDPQCYqlMGpUKA5fyklC2CEHvA"));

    TestingVariables testingVariables = {id, predefinedFirst, predefinedSecond};
    const auto alg = browserPasskeys()->getAlgorithmFromPublicKey(credentialCreationOptions);
    const auto credentialPrivateKey =
        browserPasskeys()->buildCredentialPrivateKey(alg, predefinedFirst, predefinedSecond);
    auto result = browserPasskeys()->buildAttestationObject(
        credentialCreationOptions, "", id, credentialPrivateKey.cborEncodedPublicKey, testingVariables);
    QCOMPARE(result,
             QByteArray::fromHex(
                 "a363666d74646e6f6e656761747453746d74a068617574684461746158a474a6ea9213c99c2f74b22492b320cf40262a94c1a"
                 "950a0397f29250b60841ef04500000000fdb141b25d84443e8a354698c205a5020020cabcc52799707294f060c39d5d29b117"
                 "96f9718425a813336db53f77ea052cefa501020326200121582006ecaf345b6291616d195903a6502aca31c495a869e5f087e"
                 "5d4b832cd0885dd225820e2ee7fe90f0ee91d07834a0309db4224b10bd325ff1832e15399b71d4204e783"));

    // Double check that the result can be decoded
    BrowserCbor browserCbor;
    auto attestationJsonObject = browserCbor.getJsonFromCborData(result);

    // Parse authData
    auto authDataJsonObject = attestationJsonObject["authData"].toString();
    auto authDataArray = browserMessageBuilder()->getArrayFromBase64(authDataJsonObject);
    QVERIFY(authDataArray.size() >= 37);

    auto authData = browserPasskeys()->parseAuthData(authDataArray);
    auto credentialData = authData["credentialData"].toObject();
    auto flags = authData["flags"].toObject();
    auto publicKey = credentialData["publicKey"].toObject();

    // The attestationObject should include the same ID after decoding with the response root
    QCOMPARE(credentialData["credentialId"].toString(), QString("yrzFJ5lwcpTwYMOdXSmxF5b5cYQlqBMzbbU_d-oFLO8"));
    QCOMPARE(authData["rpIdHash"].toString(), QString("dKbqkhPJnC90siSSsyDPQCYqlMGpUKA5fyklC2CEHvA"));
    QCOMPARE(flags["AT"], true);
    QCOMPARE(flags["UP"], true);
    QCOMPARE(publicKey["1"], WebAuthnCoseKeyType::EC2);
    QCOMPARE(publicKey["3"], WebAuthnAlgorithms::ES256);
    QCOMPARE(publicKey["-1"], 1);
    QCOMPARE(publicKey["-2"], predefinedFirst);
    QCOMPARE(publicKey["-3"], predefinedSecond);
}

void TestPasskeys::testCreatingAttestationObjectWithRSA()
{
    // Predefined values for a desired outcome
    const auto id = QString("yrzFJ5lwcpTwYMOdXSmxF5b5cYQlqBMzbbU_d-oFLO8");
    const auto predefinedModulus = QString("vUhOZnyn8yn7U-nuHlsXZ6WDWLuYvevWWnwtoHxDEQq27vlp7yAfeVvAPkcvhxRcwoCEUespoa5"
                                           "5IDbkpp2Ypd6b15KbB4C-_4gM4r2FK9gfXghLPAXsMhstYv4keNFb4ghdlY5oUU3JCqUSMyOpmd"
                                           "HeX-RikLL0wgGv_tLT2DaDiWeyQCAtiDblr6COuTAU2kTpLc3Bn35geV9Iqw4iT8DwBQ-f8vjnI"
                                           "EDANXKUiRPojfy1q7WwEl-zMv6Ke2jFHxf68u82BSy3u9DOQaa24FAHoCm8Yd0n5IazMyoxyttl"
                                           "tRt8un8myVOGxcXMiR9_kQb9pu1RRLQMQLd-icE1Qw");
    const auto predefinedExponent = QString("AQAB");

    // Force algorithm to RSA
    QJsonArray pubKeyCredParams;
    pubKeyCredParams.append(QJsonObject({{"type", "public-key"}, {"alg", -257}}));

    const auto publicKeyCredentialOptions = browserMessageBuilder()->getJsonObject(PublicKeyCredentialOptions.toUtf8());
    QJsonObject credentialCreationOptions;
    browserPasskeysClient()->getCredentialCreationOptions(
        publicKeyCredentialOptions, QString("https://webauthn.io"), &credentialCreationOptions);
    credentialCreationOptions["credTypesAndPubKeyAlgs"] = pubKeyCredParams;

    auto rpIdHash = browserMessageBuilder()->getSha256HashAsBase64(QString("webauthn.io"));
    QCOMPARE(rpIdHash, QString("dKbqkhPJnC90siSSsyDPQCYqlMGpUKA5fyklC2CEHvA"));

    TestingVariables testingVariables = {id, predefinedModulus, predefinedExponent};
    const auto alg = browserPasskeys()->getAlgorithmFromPublicKey(credentialCreationOptions);
    auto credentialPrivateKey =
        browserPasskeys()->buildCredentialPrivateKey(alg, predefinedModulus, predefinedExponent);
    auto result = browserPasskeys()->buildAttestationObject(
        credentialCreationOptions, "", id, credentialPrivateKey.cborEncodedPublicKey, testingVariables);

    // Double check that the result can be decoded
    BrowserCbor browserCbor;
    auto attestationJsonObject = browserCbor.getJsonFromCborData(result);

    // Parse authData
    auto authDataJsonObject = attestationJsonObject["authData"].toString();
    auto authDataArray = browserMessageBuilder()->getArrayFromBase64(authDataJsonObject);
    QVERIFY(authDataArray.size() >= 37);

    auto authData = browserPasskeys()->parseAuthData(authDataArray);
    auto credentialData = authData["credentialData"].toObject();
    auto flags = authData["flags"].toObject();
    auto publicKey = credentialData["publicKey"].toObject();

    // The attestationObject should include the same ID after decoding with the response root
    QCOMPARE(credentialData["credentialId"].toString(), QString("yrzFJ5lwcpTwYMOdXSmxF5b5cYQlqBMzbbU_d-oFLO8"));
    QCOMPARE(authData["rpIdHash"].toString(), QString("dKbqkhPJnC90siSSsyDPQCYqlMGpUKA5fyklC2CEHvA"));
    QCOMPARE(flags["AT"], true);
    QCOMPARE(flags["UP"], true);
    QCOMPARE(publicKey["1"], WebAuthnCoseKeyType::RSA);
    QCOMPARE(publicKey["3"], WebAuthnAlgorithms::RS256);
    QCOMPARE(publicKey["-1"], predefinedModulus);
    QCOMPARE(publicKey["-2"], predefinedExponent);
}

void TestPasskeys::testRegister()
{
    // Predefined values for a desired outcome
    const auto predefinedId = QString("yrzFJ5lwcpTwYMOdXSmxF5b5cYQlqBMzbbU_d-oFLO8");
    const auto predefinedX = QString("BuyvNFtikWFtGVkDplAqyjHElahp5fCH5dS4Ms0Ihd0");
    const auto predefinedY = QString("4u5_6Q8O6R0Hg0oDCdtCJLEL0yX_GDLhU5m3HUIE54M");
    const auto origin = QString("https://webauthn.io");
    const auto testDataPublicKey = browserMessageBuilder()->getJsonObject(PublicKeyCredential.toUtf8());
    const auto testDataResponse = testDataPublicKey["response"];
    const auto publicKeyCredentialOptions = browserMessageBuilder()->getJsonObject(PublicKeyCredentialOptions.toUtf8());

    QJsonObject credentialCreationOptions;
    const auto creationResult = browserPasskeysClient()->getCredentialCreationOptions(
        publicKeyCredentialOptions, origin, &credentialCreationOptions);
    QVERIFY(creationResult == 0);

    TestingVariables testingVariables = {predefinedId, predefinedX, predefinedY};
    auto result = browserPasskeys()->buildRegisterPublicKeyCredential(credentialCreationOptions, testingVariables);
    auto publicKeyCredential = result.response;
    QCOMPARE(publicKeyCredential["type"], QString("public-key"));
    QCOMPARE(publicKeyCredential["authenticatorAttachment"], QString("platform"));
    QCOMPARE(publicKeyCredential["id"], QString("yrzFJ5lwcpTwYMOdXSmxF5b5cYQlqBMzbbU_d-oFLO8"));

    auto response = publicKeyCredential["response"].toObject();
    auto attestationObject = response["attestationObject"].toString();
    auto clientDataJson = response["clientDataJSON"].toString();
    QCOMPARE(attestationObject, testDataResponse["attestationObject"].toString());

    // Parse clientDataJSON
    auto clientDataByteArray = browserMessageBuilder()->getArrayFromBase64(clientDataJson);
    auto clientDataJsonObject = browserMessageBuilder()->getJsonObject(clientDataByteArray);
    QCOMPARE(clientDataJsonObject["challenge"],
             QString("lVeHzVxWsr8MQxMkZF0ti6FXhdgMljqKzgA-q_zk2Mnii3eJ47VF97sqUoYktVC85WAZ1uIASm-a_lDFZwsLfw"));
    QCOMPARE(clientDataJsonObject["origin"], origin);
    QCOMPARE(clientDataJsonObject["type"], QString("webauthn.create"));
}

void TestPasskeys::testGet()
{
#if BOTAN_VERSION_CODE < BOTAN_VERSION_CODE_FOR(2, 14, 0)
    QSKIP("ECDSA Signature is broken on Botan < 2.14.0");
#endif
    const auto privateKeyPem = QString("-----BEGIN PRIVATE KEY-----"
                                       "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQg5DX2R6I37nMSZqCp"
                                       "XfHlE3UeitkGGE03FqGsdfxIBoOhRANCAAQG7K80W2KRYW0ZWQOmUCrKMcSVqGnl"
                                       "8Ifl1LgyzQiF3eLuf+kPDukdB4NKAwnbQiSxC9Ml/xgy4VOZtx1CBOeD"
                                       "-----END PRIVATE KEY-----");
    const auto origin = QString("https://webauthn.io");
    const auto id = QString("yrzFJ5lwcpTwYMOdXSmxF5b5cYQlqBMzbbU_d-oFLO8");
    const auto publicKeyCredentialRequestOptions =
        browserMessageBuilder()->getJsonObject(PublicKeyCredentialRequestOptions.toUtf8());

    QJsonObject assertionOptions;
    const auto assertionResult =
        browserPasskeysClient()->getAssertionOptions(publicKeyCredentialRequestOptions, origin, &assertionOptions);
    QVERIFY(assertionResult == 0);

    auto publicKeyCredential = browserPasskeys()->buildGetPublicKeyCredential(assertionOptions, id, {}, privateKeyPem);
    QVERIFY(!publicKeyCredential.isEmpty());
    QCOMPARE(publicKeyCredential["id"].toString(), id);

    auto response = publicKeyCredential["response"].toObject();
    QCOMPARE(response["authenticatorData"].toString(), QString("dKbqkhPJnC90siSSsyDPQCYqlMGpUKA5fyklC2CEHvAFAAAAAA"));
    QCOMPARE(response["clientDataJSON"].toString(),
             QString("eyJjaGFsbGVuZ2UiOiI5ejM2dlRmUVRMOTVMZjdXblpneXRlN29oR2VGLVhSaUx4a0wtTHVHVTF6b3BSbU1JVUExTFZ3ekdwe"
                     "UltMWZPQm4xUW5SYTBRSDI3QURBYUpHSHlzUSIsImNyb3NzT3JpZ2luIjpmYWxzZSwib3JpZ2luIjoiaHR0cHM6Ly93ZWJhdX"
                     "Robi5pbyIsInR5cGUiOiJ3ZWJhdXRobi5nZXQifQ"));
    QCOMPARE(
        response["signature"].toString(),
        QString("MEUCIHFv0lOOGGloi_XoH5s3QDSs__8yAp9ZTMEjNiacMpOxAiEA04LAfO6TE7j12XNxd3zHQpn4kZN82jQFPntPiPBSD5c"));

    auto clientDataJson = response["clientDataJSON"].toString();
    auto clientDataByteArray = browserMessageBuilder()->getArrayFromBase64(clientDataJson);
    auto clientDataJsonObject = browserMessageBuilder()->getJsonObject(clientDataByteArray);
    QCOMPARE(clientDataJsonObject["challenge"].toString(), publicKeyCredentialRequestOptions["challenge"].toString());
}

void TestPasskeys::testExtensions()
{
    auto extensions = QJsonObject({{"credProps", true}, {"uvm", true}});
    auto result = passkeyUtils()->buildExtensionData(extensions);

    BrowserCbor cbor;
    auto extensionJson = cbor.getJsonFromCborData(result.extensionData);
    auto uvmArray = result.extensionObject["uvm"].toArray();
    QCOMPARE(result.extensionObject["credProps"].toObject()["rk"].toBool(), true);
    QCOMPARE(uvmArray.size(), 1);
    QCOMPARE(uvmArray.first().toArray().size(), 3);

    auto partial = QJsonObject({{"props", true}, {"uvm", true}});
    auto faulty = QJsonObject({{"uvx", true}});
    auto partialData = passkeyUtils()->buildExtensionData(partial);
    auto faultyData = passkeyUtils()->buildExtensionData(faulty);

    auto partialJson = cbor.getJsonFromCborData(partialData.extensionData);
    QCOMPARE(partialJson["uvm"].toArray().size(), 1);

    auto faultyJson = cbor.getJsonFromCborData(faultyData.extensionData);
    QCOMPARE(faultyJson.size(), 0);
}

void TestPasskeys::testParseFlags()
{
    auto registerResult = browserPasskeys()->parseFlags("\x45");
    QCOMPARE(registerResult["ED"], false);
    QCOMPARE(registerResult["AT"], true);
    QCOMPARE(registerResult["BS"], false);
    QCOMPARE(registerResult["BE"], false);
    QCOMPARE(registerResult["UV"], true);
    QCOMPARE(registerResult["UP"], true);

    auto getResult = browserPasskeys()->parseFlags("\x05"); // Only UP and UV
    QCOMPARE(getResult["ED"], false);
    QCOMPARE(getResult["AT"], false);
    QCOMPARE(getResult["BS"], false);
    QCOMPARE(getResult["BE"], false);
    QCOMPARE(getResult["UV"], true);
    QCOMPARE(getResult["UP"], true);
}

void TestPasskeys::testSetFlags()
{
    auto registerJson =
        QJsonObject({{"ED", false}, {"AT", true}, {"BS", false}, {"BE", false}, {"UV", true}, {"UP", true}});
    auto registerResult = browserPasskeys()->setFlagsFromJson(registerJson);
    QCOMPARE(registerResult, 0x45);

    auto getJson =
        QJsonObject({{"ED", false}, {"AT", false}, {"BS", false}, {"BE", false}, {"UV", true}, {"UP", true}});
    auto getResult = browserPasskeys()->setFlagsFromJson(getJson);
    QCOMPARE(getResult, 0x05);

    // With "discouraged", so UV is false
    auto discouragedJson =
        QJsonObject({{"ED", false}, {"AT", false}, {"BS", false}, {"BE", false}, {"UV", false}, {"UP", true}});
    auto discouragedResult = browserPasskeys()->setFlagsFromJson(discouragedJson);
    QCOMPARE(discouragedResult, 0x01);
}

void TestPasskeys::testEntry()
{
    Database db;
    auto* root = db.rootGroup();
    root->setUuid(QUuid::createUuid());

    auto* group1 = new Group();
    group1->setUuid(QUuid::createUuid());
    group1->setParent(root);

    auto* entry = new Entry();
    entry->setGroup(root);

    browserService()->addPasskeyToEntry(entry,
                                        QString("example.com"),
                                        QString("example.com"),
                                        QString("username"),
                                        QString("userId"),
                                        QString("userHandle"),
                                        QString("privateKey"));

    QVERIFY(entry->hasPasskey());
}

void TestPasskeys::testIsDomain()
{
    QVERIFY(passkeyUtils()->isDomain("test.example.com"));
    QVERIFY(passkeyUtils()->isDomain("example.com"));

    QVERIFY(!passkeyUtils()->isDomain("exa[mple.org"));
    QVERIFY(!passkeyUtils()->isDomain("example.com."));
    QVERIFY(!passkeyUtils()->isDomain("127.0.0.1"));
    QVERIFY(!passkeyUtils()->isDomain("127.0.0.1."));
}

// List from https://html.spec.whatwg.org/multipage/browsers.html#is-a-registrable-domain-suffix-of-or-is-equal-to
void TestPasskeys::testRegistrableDomainSuffix()
{
    QVERIFY(passkeyUtils()->isRegistrableDomainSuffix(QString("example.com"), QString("example.com")));
    QVERIFY(!passkeyUtils()->isRegistrableDomainSuffix(QString("example.com"), QString("example.com.")));
    QVERIFY(!passkeyUtils()->isRegistrableDomainSuffix(QString("example.com."), QString("example.com")));
    QVERIFY(passkeyUtils()->isRegistrableDomainSuffix(QString("example.com"), QString("www.example.com")));
    QVERIFY(!passkeyUtils()->isRegistrableDomainSuffix(QString("com"), QString("example.com")));
    QVERIFY(passkeyUtils()->isRegistrableDomainSuffix(QString("example"), QString("example")));
    QVERIFY(
        !passkeyUtils()->isRegistrableDomainSuffix(QString("s3.amazonaws.com"), QString("example.s3.amazonaws.com")));
    QVERIFY(!passkeyUtils()->isRegistrableDomainSuffix(QString("example.compute.amazonaws.com"),
                                                       QString("www.example.compute.amazonaws.com")));
    QVERIFY(!passkeyUtils()->isRegistrableDomainSuffix(QString("amazonaws.com"),
                                                       QString("www.example.compute.amazonaws.com")));
    QVERIFY(passkeyUtils()->isRegistrableDomainSuffix(QString("amazonaws.com"), QString("test.amazonaws.com")));
}

void TestPasskeys::testRpIdValidation()
{
    QString result;
    auto allowedIdentical = passkeyUtils()->validateRpId(QString("example.com"), QString("example.com"), &result);
    QCOMPARE(result, QString("example.com"));
    QVERIFY(allowedIdentical == PASSKEYS_SUCCESS);

    result.clear();
    auto allowedSubdomain = passkeyUtils()->validateRpId(QString("example.com"), QString("www.example.com"), &result);
    QCOMPARE(result, QString("example.com"));
    QVERIFY(allowedSubdomain == PASSKEYS_SUCCESS);

    result.clear();
    QJsonValue emptyValue;
    auto emptyRpId = passkeyUtils()->validateRpId(emptyValue, QString("example.com"), &result);
    QCOMPARE(result, QString("example.com"));
    QVERIFY(emptyRpId == PASSKEYS_SUCCESS);

    result.clear();
    auto ipRpId = passkeyUtils()->validateRpId(QString("127.0.0.1"), QString("example.com"), &result);
    QCOMPARE(result, QString(""));
    QVERIFY(ipRpId == ERROR_PASSKEYS_DOMAIN_RPID_MISMATCH);

    result.clear();
    auto emptyOrigin = passkeyUtils()->validateRpId(QString("example.com"), QString(""), &result);
    QVERIFY(result.isEmpty());
    QCOMPARE(emptyOrigin, ERROR_PASSKEYS_ORIGIN_NOT_ALLOWED);

    result.clear();
    auto ipOrigin = passkeyUtils()->validateRpId(QString("example.com"), QString("127.0.0.1"), &result);
    QVERIFY(result.isEmpty());
    QCOMPARE(ipOrigin, ERROR_PASSKEYS_DOMAIN_RPID_MISMATCH);

    result.clear();
    auto invalidRpId = passkeyUtils()->validateRpId(QString(".com"), QString("example.com"), &result);
    QVERIFY(result.isEmpty());
    QCOMPARE(invalidRpId, ERROR_PASSKEYS_DOMAIN_RPID_MISMATCH);

    result.clear();
    auto malformedOrigin = passkeyUtils()->validateRpId(QString("example.com."), QString("example.com."), &result);
    QVERIFY(result.isEmpty());
    QCOMPARE(malformedOrigin, ERROR_PASSKEYS_DOMAIN_RPID_MISMATCH);

    result.clear();
    auto malformed = passkeyUtils()->validateRpId(QString("...com."), QString("example...com"), &result);
    QVERIFY(result.isEmpty());
    QCOMPARE(malformed, ERROR_PASSKEYS_DOMAIN_RPID_MISMATCH);

    result.clear();
    auto differentDomain = passkeyUtils()->validateRpId(QString("another.com"), QString("example.com"), &result);
    QVERIFY(result.isEmpty());
    QCOMPARE(differentDomain, ERROR_PASSKEYS_DOMAIN_RPID_MISMATCH);
}

void TestPasskeys::testParseAttestation()
{
    QVERIFY(passkeyUtils()->parseAttestation(QString("")) == QString("none"));
    QVERIFY(passkeyUtils()->parseAttestation(QString("direct")) == QString("direct"));
    QVERIFY(passkeyUtils()->parseAttestation(QString("none")) == QString("none"));
    QVERIFY(passkeyUtils()->parseAttestation(QString("indirect")) == QString("none"));
    QVERIFY(passkeyUtils()->parseAttestation(QString("invalidvalue")) == QString("none"));
}

void TestPasskeys::testParseCredentialTypes()
{
    const QJsonArray invalidPubKeyCredParams = {
        QJsonObject({{"type", "private-key"}, {"alg", -7}}),
        QJsonObject({{"type", "private-key"}, {"alg", -257}}),
    };

    const QJsonArray partiallyInvalidPubKeyCredParams = {
        QJsonObject({{"type", "private-key"}, {"alg", -7}}),
        QJsonObject({{"type", "public-key"}, {"alg", -257}}),
    };

    auto validResponse = passkeyUtils()->parseCredentialTypes(validPubKeyCredParams);
    QVERIFY(validResponse == validPubKeyCredParams);

    auto invalidResponse = passkeyUtils()->parseCredentialTypes(invalidPubKeyCredParams);
    QVERIFY(invalidResponse.isEmpty());

    auto partiallyInvalidResponse = passkeyUtils()->parseCredentialTypes(partiallyInvalidPubKeyCredParams);
    QVERIFY(partiallyInvalidResponse != validPubKeyCredParams);
    QVERIFY(partiallyInvalidResponse.size() == 1);
    QVERIFY(partiallyInvalidResponse.first()["type"].toString() == QString("public-key"));
    QVERIFY(partiallyInvalidResponse.first()["alg"].toInt() == -257);

    auto emptyResponse = passkeyUtils()->parseCredentialTypes({});
    QVERIFY(emptyResponse == validPubKeyCredParams);

    const auto publicKeyOptions = browserMessageBuilder()->getJsonObject(PublicKeyCredentialOptions.toUtf8());
    auto responseFromPublicKey = passkeyUtils()->parseCredentialTypes(publicKeyOptions["pubKeyCredParams"].toArray());
    QVERIFY(responseFromPublicKey == validPubKeyCredParams);
}

void TestPasskeys::testIsAuthenticatorSelectionValid()
{
    QVERIFY(passkeyUtils()->isAuthenticatorSelectionValid({}));
    QVERIFY(passkeyUtils()->isAuthenticatorSelectionValid(QJsonObject({{"authenticatorAttachment", "platform"}})));
    QVERIFY(
        passkeyUtils()->isAuthenticatorSelectionValid(QJsonObject({{"authenticatorAttachment", "cross-platform"}})));
    QVERIFY(!passkeyUtils()->isAuthenticatorSelectionValid(QJsonObject({{"authenticatorAttachment", "something"}})));
}

void TestPasskeys::testIsResidentKeyRequired()
{
    QVERIFY(passkeyUtils()->isResidentKeyRequired(QJsonObject({{"residentKey", "required"}})));
    QVERIFY(passkeyUtils()->isResidentKeyRequired(QJsonObject({{"residentKey", "preferred"}})));
    QVERIFY(!passkeyUtils()->isResidentKeyRequired(QJsonObject({{"residentKey", "discouraged"}})));
    QVERIFY(passkeyUtils()->isResidentKeyRequired(QJsonObject({{"requireResidentKey", true}})));
}

void TestPasskeys::testIsUserVerificationRequired()
{
    QVERIFY(passkeyUtils()->isUserVerificationRequired(QJsonObject({{"userVerification", "required"}})));
    QVERIFY(passkeyUtils()->isUserVerificationRequired(QJsonObject({{"userVerification", "preferred"}})));
    QVERIFY(!passkeyUtils()->isUserVerificationRequired(QJsonObject({{"userVerification", "discouraged"}})));
}

void TestPasskeys::testAllowLocalhostWithPasskeys()
{
    QVERIFY(passkeyUtils()->isOriginAllowedWithLocalhost(false, "https://example.com"));
    QVERIFY(!passkeyUtils()->isOriginAllowedWithLocalhost(false, "http://example.com"));
    QVERIFY(passkeyUtils()->isOriginAllowedWithLocalhost(true, "https://example.com"));
    QVERIFY(!passkeyUtils()->isOriginAllowedWithLocalhost(true, "http://example.com"));
    QVERIFY(!passkeyUtils()->isOriginAllowedWithLocalhost(false, "http://localhost"));
    QVERIFY(passkeyUtils()->isOriginAllowedWithLocalhost(true, "http://localhost"));
    QVERIFY(!passkeyUtils()->isOriginAllowedWithLocalhost(true, "http://localhosting"));
    QVERIFY(passkeyUtils()->isOriginAllowedWithLocalhost(true, "http://test.localhost"));
    QVERIFY(!passkeyUtils()->isOriginAllowedWithLocalhost(false, "http://test.localhost"));
    QVERIFY(!passkeyUtils()->isOriginAllowedWithLocalhost(true, "http://localhost.example.com"));
}
