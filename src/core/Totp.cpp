/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2017 Weslly Honorato <﻿weslly@protonmail.com>
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

#include "Totp.h"

#include "core/Base32.h"
#include "core/Clock.h"

#include <QMessageAuthenticationCode>
#include <QSharedPointer>
#include <QUrlQuery>
#include <QVariant>
#include <QtEndian>

#include <cmath>

static QList<Totp::Encoder> totpEncoders{
    {"", "", "0123456789", Totp::DEFAULT_DIGITS, Totp::DEFAULT_STEP, false},
    {"steam", Totp::STEAM_SHORTNAME, "23456789BCDFGHJKMNPQRTVWXY", Totp::STEAM_DIGITS, Totp::DEFAULT_STEP, true},
};

static Totp::Algorithm getHashTypeByName(const QString& name)
{
    auto nameUpper = name.toUpper();
    if (nameUpper == "SHA512" || nameUpper == "HMAC-SHA-512") {
        return Totp::Algorithm::Sha512;
    }
    if (nameUpper == "SHA256" || nameUpper == "HMAC-SHA-256") {
        return Totp::Algorithm::Sha256;
    }
    return Totp::Algorithm::Sha1;
}

static QString getNameForHashType(const Totp::Algorithm hashType)
{
    switch (hashType) {
    case Totp::Algorithm::Sha512:
        return "SHA512";
    case Totp::Algorithm::Sha256:
        return "SHA256";
    default:
        return "SHA1";
    }
}

QSharedPointer<Totp::Settings>
Totp::fromKeePass2Totp(const QString& secret, const QString& algorithm, const QString& length, const QString& period)
{
    // Must have at least a secret to continue
    if (secret.isEmpty()) {
        return {};
    }

    // Create default settings
    auto settings = createSettings(secret);

    if (!algorithm.isEmpty()) {
        settings->algorithm = getHashTypeByName(algorithm);
    }
    if (!length.isEmpty()) {
        settings->digits = length.toUInt();
    }
    if (!period.isEmpty()) {
        settings->step = period.toUInt();
    }

    return settings;
}

QSharedPointer<Totp::Settings> Totp::parseSettings(const QString& rawSettings, const QString& key)
{
    // Early out if both strings are empty
    if (rawSettings.isEmpty() && key.isEmpty()) {
        return {};
    }

    // Create default settings
    auto settings = createSettings(key);

    QUrl url(rawSettings);
    if (url.isValid() && url.scheme() == "otpauth") {
        // Default OTP url format
        QUrlQuery query(url);
        settings->format = StorageFormat::OTPURL;
        settings->key = query.queryItemValue("secret");
        if (query.hasQueryItem("digits")) {
            settings->digits = query.queryItemValue("digits").toUInt();
        }
        if (query.hasQueryItem("period")) {
            settings->step = query.queryItemValue("period").toUInt();
        }
        if (query.hasQueryItem("encoder")) {
            settings->encoder = getEncoderByName(query.queryItemValue("encoder"));
        }
        if (query.hasQueryItem("algorithm")) {
            settings->algorithm = getHashTypeByName(query.queryItemValue("algorithm"));
        }
    } else {
        QUrlQuery query(rawSettings);
        if (query.hasQueryItem("key")) {
            // Compatibility with "KeeOtp" plugin
            settings->format = StorageFormat::KEEOTP;
            settings->key = query.queryItemValue("key");
            if (query.hasQueryItem("size")) {
                settings->digits = query.queryItemValue("size").toUInt();
            }
            if (query.hasQueryItem("step")) {
                settings->step = query.queryItemValue("step").toUInt();
            }
            if (query.hasQueryItem("otpHashMode")) {
                settings->algorithm = getHashTypeByName(query.queryItemValue("otpHashMode"));
            }
        } else {
            if (settings->key.isEmpty()) {
                // Legacy format cannot work with an empty key
                return {};
            }

            // Parse semi-colon separated values ([step];[digits|S])
            settings->format = StorageFormat::LEGACY;
            auto vars = rawSettings.split(";");
            if (vars.size() >= 2) {
                if (vars[1] == STEAM_SHORTNAME) {
                    // Explicit steam encoder
                    settings->encoder = steamEncoder();
                    settings->digits = STEAM_DIGITS;
                } else {
                    // Extract step and digits
                    settings->step = vars[0].toUInt();
                    settings->digits = vars[1].toUInt();
                }
            }
        }
    }

    // Bound digits and step
    settings->digits = qBound(1u, settings->digits, 10u);
    settings->step = qBound(1u, settings->step, 86400u);

    return settings;
}

QSharedPointer<Totp::Settings> Totp::createSettings(const QString& key,
                                                    const uint digits,
                                                    const uint step,
                                                    const Totp::StorageFormat format,
                                                    const QString& encoderShortName,
                                                    const Totp::Algorithm algorithm)
{
    return QSharedPointer<Totp::Settings>(
        new Totp::Settings{format, getEncoderByShortName(encoderShortName), algorithm, key, digits, step});
}

QString Totp::writeSettings(const QSharedPointer<Totp::Settings>& settings,
                            const QString& title,
                            const QString& username,
                            bool forceOtp)
{
    if (settings.isNull()) {
        return {};
    }

    // OTP Url output
    if (settings->format == StorageFormat::OTPURL || forceOtp) {
        auto urlstring = QString("otpauth://totp/%1:%2?secret=%3&period=%4&digits=%5&issuer=%1")
                             .arg(title.isEmpty() ? "KeePassXC" : QString(QUrl::toPercentEncoding(title)),
                                  username.isEmpty() ? "none" : QString(QUrl::toPercentEncoding(username)),
                                  QString(QUrl::toPercentEncoding(Base32::sanitizeInput(settings->key.toLatin1()))),
                                  QString::number(settings->step),
                                  QString::number(settings->digits));

        if (!settings->encoder.name.isEmpty()) {
            urlstring.append("&encoder=").append(settings->encoder.name);
        }
        if (settings->algorithm != Totp::DEFAULT_ALGORITHM) {
            urlstring.append("&algorithm=").append(getNameForHashType(settings->algorithm));
        }
        return urlstring;
    } else if (settings->format == StorageFormat::KEEOTP) {
        // KeeOtp output
        auto keyString = QString("key=%1&size=%2&step=%3")
                             .arg(QString(Base32::sanitizeInput(settings->key.toLatin1())))
                             .arg(settings->digits)
                             .arg(settings->step);
        if (settings->algorithm != Totp::DEFAULT_ALGORITHM) {
            keyString.append("&otpHashMode=").append(getNameForHashType(settings->algorithm));
        }
        return keyString;
    } else if (!settings->encoder.shortName.isEmpty()) {
        // Semicolon output [step];[encoder]
        return QString("%1;%2").arg(settings->step).arg(settings->encoder.shortName);
    } else {
        // Semicolon output [step];[digits]
        return QString("%1;%2").arg(settings->step).arg(settings->digits);
    }
}

QString Totp::generateTotp(const QSharedPointer<Totp::Settings>& settings, const quint64 time)
{
    Q_ASSERT(!settings.isNull());
    if (settings.isNull()) {
        return QObject::tr("Invalid Settings", "TOTP");
    }

    const Encoder& encoder = settings->encoder;
    uint step = settings->step;
    uint digits = settings->digits;

    quint64 current;
    if (time == 0) {
        current = qToBigEndian(static_cast<quint64>(Clock::currentSecondsSinceEpoch()) / step);
    } else {
        current = qToBigEndian(time / step);
    }

    const auto secret = Base32::decode(Base32::sanitizeInput(settings->key.toLatin1()));
    if (secret.isEmpty()) {
        return QObject::tr("Invalid Key", "TOTP");
    }

    QCryptographicHash::Algorithm cryptoHash;
    switch (settings->algorithm) {
    case Totp::Algorithm::Sha512:
        cryptoHash = QCryptographicHash::Sha512;
        break;
    case Totp::Algorithm::Sha256:
        cryptoHash = QCryptographicHash::Sha256;
        break;
    default:
        cryptoHash = QCryptographicHash::Sha1;
        break;
    }
    QMessageAuthenticationCode code(cryptoHash);
    code.setKey(secret);
    code.addData(QByteArray(reinterpret_cast<char*>(&current), sizeof(current)));
    QByteArray hmac = code.result();

    int offset = (hmac[hmac.length() - 1] & 0xf);

    // clang-format off
    int binary =
            ((hmac[offset] & 0x7f) << 24)
            | ((hmac[offset + 1] & 0xff) << 16)
            | ((hmac[offset + 2] & 0xff) << 8)
            | (hmac[offset + 3] & 0xff);
    // clang-format on

    int direction = -1;
    int startpos = digits - 1;
    if (encoder.reverse) {
        direction = 1;
        startpos = 0;
    }
    quint32 digitsPower = pow(encoder.alphabet.size(), digits);

    quint64 password = binary % digitsPower;
    QString retval(int(digits), encoder.alphabet[0]);
    for (quint8 pos = startpos; password > 0; pos += direction) {
        retval[pos] = encoder.alphabet[int(password % encoder.alphabet.size())];
        password /= encoder.alphabet.size();
    }
    return retval;
}

QList<QPair<QString, QString>> Totp::supportedEncoders()
{
    QList<QPair<QString, QString>> encoders;
    for (auto& encoder : totpEncoders) {
        encoders << QPair<QString, QString>(encoder.name, encoder.shortName);
    }
    return encoders;
}

QList<QPair<QString, Totp::Algorithm>> Totp::supportedAlgorithms()
{
    QList<QPair<QString, Algorithm>> algorithms;
    algorithms << QPair<QString, Algorithm>(QStringLiteral("SHA-1"), Algorithm::Sha1);
    algorithms << QPair<QString, Algorithm>(QStringLiteral("SHA-256"), Algorithm::Sha256);
    algorithms << QPair<QString, Algorithm>(QStringLiteral("SHA-512"), Algorithm::Sha512);
    return algorithms;
}

bool Totp::hasCustomSettings(const QSharedPointer<Totp::Settings>& settings)
{
    return settings
           && (settings->digits != DEFAULT_DIGITS || settings->step != DEFAULT_STEP
               || settings->algorithm != DEFAULT_ALGORITHM);
}

Totp::Encoder& Totp::defaultEncoder()
{
    // The first encoder is always the default
    Q_ASSERT(!totpEncoders.empty());
    return totpEncoders[0];
}

Totp::Encoder& Totp::steamEncoder()
{
    return getEncoderByShortName("S");
}

Totp::Encoder& Totp::getEncoderByShortName(const QString& shortName)
{
    for (auto& encoder : totpEncoders) {
        if (encoder.shortName == shortName) {
            return encoder;
        }
    }
    return defaultEncoder();
}

Totp::Encoder& Totp::getEncoderByName(const QString& name)
{
    for (auto& encoder : totpEncoders) {
        if (encoder.name == name) {
            return encoder;
        }
    }
    return defaultEncoder();
}
