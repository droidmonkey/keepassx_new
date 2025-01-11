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

#include "OpVaultReader.h"

#include "core/Entry.h"
#include "core/Totp.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocale>
#include <QUrlQuery>

namespace
{
    QDateTime resolveDate(const QString& kind, const QJsonValue& value)
    {
        QDateTime date;
        if (kind == "monthYear") {
            // 1Password programmers are sadistic...
            auto dateValue = QString::number(value.toInt());
            date = QDateTime::fromString(dateValue, "yyyyMM");
            date.setTimeSpec(Qt::UTC);
        } else if (value.isString()) {
            date = QDateTime::fromSecsSinceEpoch(value.toString().toUInt(), Qt::UTC);
        } else {
            date = QDateTime::fromSecsSinceEpoch(value.toInt(), Qt::UTC);
        }
        return date;
    }
} // namespace

void OpVaultReader::fillFromSection(Entry* entry, const QJsonObject& section)
{
    const auto uuid = entry->uuid();
    auto sectionTitle = section["title"].toString();

    if (!section.contains("fields")) {
        auto sectionName = section["name"].toString();
        if (!(sectionName.toLower() == "linked items" && sectionTitle.toLower() == "related items")) {
            qWarning() << R"(Skipping "fields"-less Section in UUID ")" << uuid << "\": <<" << section << ">>";
        }
        return;
    } else if (!section["fields"].isArray()) {
        qWarning() << R"(Skipping non-Array "fields" in UUID ")" << uuid << "\"\n";
        return;
    }

    QJsonArray sectionFields = section["fields"].toArray();
    for (const QJsonValue sectionField : sectionFields) {
        if (!sectionField.isObject()) {
            qWarning() << R"(Skipping non-Object "fields" in UUID ")" << uuid << "\": << " << sectionField << ">>";
            continue;
        }
        fillFromSectionField(entry, sectionTitle, sectionField.toObject());
    }
}

void OpVaultReader::fillFromSectionField(Entry* entry, const QString& sectionName, const QJsonObject& field)
{
    if (!field.contains("v")) {
        // for our purposes, we don't care if there isn't a value in the field
        return;
    }

    // Ignore "a" and "inputTraits" fields, they don't apply to KPXC

    auto attrName = resolveAttributeName(sectionName, field["n"].toString(), field["t"].toString());
    auto attrValue = field.value("v").toString();
    auto kind = field["k"].toString();

    if (attrName.startsWith("TOTP_")) {
        if (entry->hasTotp()) {
            // Store multiple TOTP definitions as additional otp attributes
            int i = 0;
            QString name("otp");
            auto attributes = entry->attributes()->keys();
            while (attributes.contains(name)) {
                name = QString("otp_%1").arg(++i);
            }
            entry->attributes()->set(name, attrValue, true);
        } else if (attrValue.startsWith("otpauth://")) {
            QUrlQuery query(attrValue);
            // at least as of 1Password 7, they don't append the digits= and period= which totp.cpp requires
            if (!query.hasQueryItem("digits")) {
                query.addQueryItem("digits", QString("%1").arg(Totp::DEFAULT_DIGITS));
            }
            if (!query.hasQueryItem("period")) {
                query.addQueryItem("period", QString("%1").arg(Totp::DEFAULT_STEP));
            }
            attrValue = query.toString(QUrl::FullyEncoded);
            entry->setTotp(Totp::parseSettings(attrValue));
        } else {
            entry->setTotp(Totp::parseSettings({}, attrValue));
        }

    } else if (attrName.startsWith("expir", Qt::CaseInsensitive)) {
        QDateTime expiry = resolveDate(kind, field.value("v"));
        if (expiry.isValid()) {
            entry->setExpiryTime(expiry);
            entry->setExpires(true);
        } else {
            qWarning() << QString("[%1] Invalid expiration date found: %2").arg(entry->title(), attrValue);
        }
    } else {
        if (kind == "date" || kind == "monthYear") {
            QDateTime date = resolveDate(kind, field.value("v"));
            if (date.isValid()) {
                entry->attributes()->set(attrName, QLocale::system().toString(date, QLocale::ShortFormat));
            } else {
                qWarning()
                    << QString("[%1] Invalid date attribute found: %2 = %3").arg(entry->title(), attrName, attrValue);
            }
        } else if (kind == "address") {
            // Expand address into multiple attributes
            auto addrFields = field.value("v").toObject().toVariantMap();
            for (auto& part : addrFields.keys()) {
                entry->attributes()->set(attrName + QString("_%1").arg(part), addrFields.value(part).toString());
            }
        } else {
            if (entry->attributes()->hasKey(attrName)) {
                // Append a random string to the attribute name to avoid collisions
                attrName += QString("_%1").arg(QUuid::createUuid().toString().mid(1, 5));
            }
            entry->attributes()->set(attrName, attrValue, (kind == "password" || kind == "concealed"));
        }
    }
}

QString OpVaultReader::resolveAttributeName(const QString& section, const QString& name, const QString& text)
{
    // Special case for TOTP
    if (name.startsWith("TOTP_")) {
        return name;
    }

    auto lowName = name.toLower();
    auto lowText = text.toLower();
    if (section.isEmpty() || name.startsWith("address")) {
        // Empty section implies these are core attributes
        // try to find username, password, url
        if (lowName == "password" || lowText == "password") {
            return EntryAttributes::PasswordKey;
        } else if (lowName == "username" || lowText == "username") {
            return EntryAttributes::UserNameKey;
        } else if (lowName == "url" || lowText == "url" || lowName == "hostname" || lowText == "server"
                   || lowName == "website") {
            return EntryAttributes::URLKey;
        }
        return text;
    }

    return QString("%1_%2").arg(section, text);
}
