/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2017 Lennart Glauer <mail@lennart-glauer.de>
 *  Copyright (C) 2020 Giuseppe D'Angelo <dangelog@gmail.com>.
 *  Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com,
 *  author Giuseppe D'Angelo <giuseppe.dangelo@kdab.com>
 *  Copyright (C) 2021 The Qt Company Ltd.
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
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

#include "Tools.h"

#include "config-keepassx.h"
#include "git-info.h"

#include "core/Clock.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFileInfo>
#include <QIODevice>
#include <QLocale>
#include <QMetaProperty>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QUuid>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h> // for Sleep()
#endif

namespace Tools
{
    QString debugInfo()
    {
        QString debugInfo = "KeePassXC - ";
        debugInfo.append(QObject::tr("Version %1").arg(KEEPASSXC_VERSION).append("\n"));
#ifndef KEEPASSXC_BUILD_TYPE_RELEASE
        debugInfo.append(QObject::tr("Build Type: %1").arg(KEEPASSXC_BUILD_TYPE).append("\n"));
#endif

        QString commitHash;
        if (!QString(GIT_HEAD).isEmpty()) {
            commitHash = GIT_HEAD;
        }
        if (!commitHash.isEmpty()) {
            debugInfo.append(QObject::tr("Revision: %1").arg(commitHash.left(7)).append("\n"));
        }

#ifdef KEEPASSXC_DIST
        debugInfo.append(QObject::tr("Distribution: %1").arg(KEEPASSXC_DIST_TYPE).append("\n"));
#endif

        // Qt related debugging information.
        debugInfo.append("\n");
        debugInfo.append("Qt ").append(QString::fromLocal8Bit(qVersion())).append("\n");
#ifdef QT_NO_DEBUG
        debugInfo.append(QObject::tr("Debugging mode is disabled.").append("\n"));
#else
        debugInfo.append(QObject::tr("Debugging mode is enabled.").append("\n"));
#endif
        debugInfo.append("\n");

        debugInfo.append(QObject::tr("Operating system: %1\nCPU architecture: %2\nKernel: %3 %4")
                             .arg(QSysInfo::prettyProductName(),
                                  QSysInfo::currentCpuArchitecture(),
                                  QSysInfo::kernelType(),
                                  QSysInfo::kernelVersion()));

        debugInfo.append("\n\n");

        QString extensions;
#ifdef WITH_XC_AUTOTYPE
        extensions += "\n- " + QObject::tr("Auto-Type");
#endif
#ifdef WITH_XC_BROWSER
        extensions += "\n- " + QObject::tr("Browser Integration");
#endif
#ifdef WITH_XC_BROWSER_PASSKEYS
        extensions += "\n- " + QObject::tr("Passkeys");
#endif
#ifdef WITH_XC_SSHAGENT
        extensions += "\n- " + QObject::tr("SSH Agent");
#endif
#ifdef WITH_XC_KEESHARE
        extensions += "\n- " + QObject::tr("KeeShare");
#endif
#ifdef WITH_XC_YUBIKEY
        extensions += "\n- " + QObject::tr("YubiKey");
#endif
#if defined(Q_OS_MACOS) || defined(Q_CC_MSVC)
        extensions += "\n- " + QObject::tr("Quick Unlock");
#endif
#ifdef WITH_XC_FDOSECRETS
        extensions += "\n- " + QObject::tr("Secret Service Integration");
#endif

        if (extensions.isEmpty()) {
            extensions = " " + QObject::tr("None");
        }

        debugInfo.append(QObject::tr("Enabled extensions:").append(extensions).append("\n"));
        return debugInfo;
    }

    QString humanReadableFileSize(qint64 bytes, quint32 precision)
    {
        constexpr auto kibibyte = 1024;
        double size = bytes;

        QStringList units = QStringList() << "B" << "KiB" << "MiB" << "GiB";
        int i = 0;
        int maxI = units.size() - 1;

        while ((size >= kibibyte) && (i < maxI)) {
            size /= kibibyte;
            i++;
        }

        // do not display decimals for smallest unit bytes identified by index i==0
        const quint32 displayPrecision = (i == 0 ? 0 : precision);
        return QString("%1 %2").arg(QLocale().toString(size, 'f', displayPrecision), units.at(i));
    }

    QString humanReadableTimeDifference(qint64 seconds)
    {
        constexpr double secondsInHour = 3600;
        constexpr double secondsInDay = secondsInHour * 24;
        constexpr double secondsInWeek = secondsInDay * 7;
        constexpr double secondsInMonth = secondsInDay * 30; // Approximation
        constexpr double secondsInYear = secondsInDay * 365;

        seconds = abs(seconds);

        if (seconds >= secondsInYear) {
            auto years = std::floor(seconds / secondsInYear);
            return QObject::tr("over %1 year(s)", nullptr, years).arg(years);
        } else if (seconds >= secondsInMonth) {
            auto months = std::round(seconds / secondsInMonth);
            return QObject::tr("about %1 month(s)", nullptr, months).arg(months);
        } else if (seconds >= secondsInWeek) {
            auto weeks = std::round(seconds / secondsInWeek);
            return QObject::tr("%1 week(s)", nullptr, weeks).arg(weeks);
        } else if (seconds >= secondsInDay) {
            auto days = std::floor(seconds / secondsInDay);
            return QObject::tr("%1 day(s)", nullptr, days).arg(days);
        } else if (seconds >= secondsInHour) {
            auto hours = std::floor(seconds / secondsInHour);
            return QObject::tr("%1 hour(s)", nullptr, hours).arg(hours);
        }

        auto minutes = std::floor(seconds / 60);
        return QObject::tr("%1 minute(s)", nullptr, minutes).arg(minutes);
    }

    bool readFromDevice(QIODevice* device, QByteArray& data, int size)
    {
        QByteArray buffer;
        buffer.resize(size);

        qint64 readResult = device->read(buffer.data(), size);
        if (readResult == -1) {
            return false;
        } else {
            buffer.resize(readResult);
            data = buffer;
            return true;
        }
    }

    bool readAllFromDevice(QIODevice* device, QByteArray& data)
    {
        QByteArray result;
        qint64 readBytes = 0;
        qint64 readResult;
        do {
            result.resize(result.size() + 16384);
            readResult = device->read(result.data() + readBytes, result.size() - readBytes);
            if (readResult > 0) {
                readBytes += readResult;
            }
        } while (readResult > 0);

        if (readResult == -1) {
            return false;
        } else {
            result.resize(static_cast<int>(readBytes));
            data = result;
            return true;
        }
    }

    bool isHex(const QByteArray& ba)
    {
        for (const uchar c : ba) {
            if (!std::isxdigit(c)) {
                return false;
            }
        }

        return true;
    }

    bool isBase64(const QByteArray& ba)
    {
        constexpr auto pattern = R"(^(?:[a-z0-9+/]{4})*(?:[a-z0-9+/]{3}=|[a-z0-9+/]{2}==)?$)";
        QRegExp regexp(pattern, Qt::CaseInsensitive, QRegExp::RegExp2);

        QString base64 = QString::fromLatin1(ba.constData(), ba.size());

        return regexp.exactMatch(base64);
    }

    bool isAsciiString(const QString& str)
    {
        constexpr auto pattern = R"(^[\x00-\x7F]+$)";
        QRegularExpression regexp(pattern, QRegularExpression::CaseInsensitiveOption);
        return regexp.match(str).hasMatch();
    }

    void sleep(int ms)
    {
        Q_ASSERT(ms >= 0);

        if (ms == 0) {
            return;
        }

#ifdef Q_OS_WIN
        Sleep(uint(ms));
#else
        timespec ts;
        ts.tv_sec = ms / 1000;
        ts.tv_nsec = (ms % 1000) * 1000 * 1000;
        nanosleep(&ts, nullptr);
#endif
    }

    void wait(int ms)
    {
        Q_ASSERT(ms >= 0);

        if (ms == 0) {
            return;
        }

        QElapsedTimer timer;
        timer.start();

        if (ms <= 50) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, ms);
            sleep(qMax(ms - static_cast<int>(timer.elapsed()), 0));
        } else {
            int timeLeft;
            do {
                timeLeft = ms - timer.elapsed();
                if (timeLeft > 0) {
                    QCoreApplication::processEvents(QEventLoop::AllEvents, timeLeft);
                    sleep(10);
                }
            } while (!timer.hasExpired(ms));
        }
    }

    /****************************************************************************
     *
     * Copyright (C) 2020 Giuseppe D'Angelo <dangelog@gmail.com>.
     * Copyright (C) 2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com,
     * author Giuseppe D'Angelo <giuseppe.dangelo@kdab.com>
     * Copyright (C) 2021 The Qt Company Ltd. Contact: https://www.qt.io/licensing/
     *
     * This function is part of the QtCore module of the Qt Toolkit. And subject to the
     * following licenses.
     *
     * GNU General Public License Usage
     * Alternatively, this function may be used under the terms of the GNU
     * General Public License version 2.0 or (at your option) the GNU General
     * Public license version 3 or any later version approved by the KDE Free
     * Qt Foundation. The licenses are as published by the Free Software
     * Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
     * included in the packaging of this file. Please review the following
     * information to ensure the GNU General Public License requirements will
     * be met: https://www.gnu.org/licenses/gpl-2.0.html and
     * https://www.gnu.org/licenses/gpl-3.0.html.
     */
    QString escapeRegex(const QString& str)
    {
        QString result;
        const auto count = str.size();
        result.reserve(count * 2);

        // everything but [a-zA-Z0-9_] gets escaped,
        // cf. perldoc -f quotemeta
        for (int i = 0; i < count; ++i) {
            const QChar current = str.at(i);

            if (current == QChar::Null) {
                // unlike Perl, a literal NUL must be escaped with
                // "\\0" (backslash + 0) and not "\\\0" (backslash + NUL),
                // because pcre16_compile uses a NUL-terminated string
                result.append(u'\\');
                result.append(u'0');
            } else if ((current < u'a' || current > u'z') && (current < u'A' || current > u'Z')
                       && (current < u'0' || current > u'9') && current != u'_') {
                result.append(u'\\');
                result.append(current);
                if (current.isHighSurrogate() && i < (count - 1)) {
                    result.append(str.at(++i));
                }
            } else {
                result.append(current);
            }
        }

        result.squeeze();
        return result;
    }

    QRegularExpression convertToRegex(const QString& string, int opts)
    {
        QString pattern = string;

        // Wildcard support (*, ?, |)
        if (opts & RegexConvertOpts::WILDCARD_ALL || opts & RegexConvertOpts::ESCAPE_REGEX) {
            pattern = escapeRegex(pattern);

            if (opts & RegexConvertOpts::WILDCARD_UNLIMITED_MATCH) {
                pattern.replace("\\*", ".*");
            }
            if (opts & RegexConvertOpts::WILDCARD_SINGLE_MATCH) {
                pattern.replace("\\?", ".");
            }
            if (opts & RegexConvertOpts::WILDCARD_LOGICAL_OR) {
                pattern.replace("\\|", "|");
            }
        }

        if (opts & RegexConvertOpts::EXACT_MATCH) {
            // Exact modifier
            pattern = "^(?:" + pattern + ")$";
        }

        auto regex = QRegularExpression(pattern);
        if ((opts & RegexConvertOpts::CASE_SENSITIVE) != RegexConvertOpts::CASE_SENSITIVE) {
            regex.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        }

        return regex;
    }

    QString uuidToHex(const QUuid& uuid)
    {
        return QString::fromLatin1(uuid.toRfc4122().toHex());
    }

    QUuid hexToUuid(const QString& uuid)
    {
        return QUuid::fromRfc4122(QByteArray::fromHex(uuid.toLatin1()));
    }

    bool isValidUuid(const QString& uuidStr)
    {
        if (uuidStr.isEmpty() || uuidStr.length() != 32 || !isHex(uuidStr.toLatin1())) {
            return false;
        }

        const auto uuid = hexToUuid(uuidStr);
        if (uuid.isNull()) {
            return false;
        }

        return true;
    }

    QString envSubstitute(const QString& filepath, QProcessEnvironment environment)
    {
        QString subbed = filepath;

#if defined(Q_OS_WIN)
        QRegularExpression varRe("\\%([A-Za-z][A-Za-z0-9_]*)\\%");
        QString homeEnv = "USERPROFILE";
#else
        QRegularExpression varRe("\\$([A-Za-z][A-Za-z0-9_]*)");
        QString homeEnv = "HOME";
#endif

        if (subbed.startsWith("~/") || subbed.startsWith("~\\"))
            subbed.replace(0, 1, environment.value(homeEnv));

        QRegularExpressionMatch match;

        do {
            match = varRe.match(subbed);
            if (match.hasMatch()) {
                subbed.replace(match.capturedStart(), match.capturedLength(), environment.value(match.captured(1)));
            }
        } while (match.hasMatch());

        return subbed;
    }

    QString cleanFilename(QString filename)
    {
        // Remove forward slash from title on all platforms
        filename.replace("/", "_");
        // Remove invalid characters
        filename.remove(QRegularExpression("[:*?\"<>|]"));

        return filename.trimmed();
    }

    QVariantMap qo2qvm(const QObject* object, const QStringList& ignoredProperties)
    {
        QVariantMap result;
        const QMetaObject* metaobject = object->metaObject();
        int count = metaobject->propertyCount();
        for (int i = 0; i < count; ++i) {
            QMetaProperty metaproperty = metaobject->property(i);
            const char* name = metaproperty.name();

            if (ignoredProperties.contains(QLatin1String(name)) || (!metaproperty.isReadable())) {
                continue;
            }

            QVariant value = object->property(name);
            result[QLatin1String(name)] = value;
        }
        return result;
    }

    QString substituteBackupFilePath(QString pattern, const QString& databasePath)
    {
        if (databasePath.isEmpty()) {
            return {};
        }

        const QString baseName = QFileInfo{databasePath}.completeBaseName();

        pattern.replace(QStringLiteral("{DB_FILENAME}"), baseName);

        const QDateTime now = Clock::currentDateTime();

        const QRegularExpression re(R"(\{TIME(?::([^\\{}]*))?\})");
        auto match = re.match(pattern);
        while (match.hasMatch()) {
            // Extract time format specifier, or use default value if absent
            QString formatSpecifier = "dd_MM_yyyy_hh-mm-ss";
            if (!match.captured(1).isEmpty()) {
                formatSpecifier = match.captured(1);
            }
            const auto replacement = now.toString(formatSpecifier);
            pattern.replace(match.capturedStart(), match.capturedLength(), replacement);
            match = re.match(pattern);
        }

        // Replace escaped braces
        pattern.replace(QStringLiteral("\\{"), QStringLiteral("{"));
        pattern.replace(QStringLiteral("\\}"), QStringLiteral("}"));

        return pattern;
    }

    MimeType toMimeType(const QString& mimeName)
    {
        static QStringList textFormats = {
            "text/",
            "application/json",
            "application/xml",
            "application/soap+xml",
            "application/x-yaml",
            "application/protobuf",
        };

        static QStringList imageFormats = {"image/"};
        static QStringList pdfFormats = {"application/pdf"};

        static auto isCompatible = [](const QString& format, const QStringList& list) {
            return std::any_of(
                list.cbegin(), list.cend(), [&format](const auto& item) { return format.startsWith(item); });
        };

        if (isCompatible(mimeName, imageFormats)) {
            return MimeType::Image;
        }

        if (isCompatible(mimeName, textFormats)) {
            return MimeType::PlainText;
        }

        if (isCompatible(mimeName, pdfFormats)) {
            return MimeType::Pdf;
        }

        return MimeType::Unknown;
    }
} // namespace Tools
