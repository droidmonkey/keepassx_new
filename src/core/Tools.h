/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_TOOLS_H
#define KEEPASSX_TOOLS_H

#include "core/Global.h"

#include <QDateTime>
#include <QList>
#include <QProcessEnvironment>
#include <QSet>

class QIODevice;
class QRegularExpression;

namespace Tools
{
    QString debugInfo();
    QString humanReadableFileSize(qint64 bytes, quint32 precision = 2);
    QString humanReadableTimeDifference(qint64 seconds);
    bool readFromDevice(QIODevice* device, QByteArray& data, int size = 16384);
    bool readAllFromDevice(QIODevice* device, QByteArray& data);
    bool isHex(const QByteArray& ba);
    bool isBase64(const QByteArray& ba);
    bool isAsciiString(const QString& str);
    void sleep(int ms);
    void wait(int ms);
    QString uuidToHex(const QUuid& uuid);
    QUuid hexToUuid(const QString& uuid);
    bool isValidUuid(const QString& uuidStr);
    QString envSubstitute(const QString& filepath,
                          QProcessEnvironment environment = QProcessEnvironment::systemEnvironment());
    QString cleanFilename(QString filename);

    template <class T> QSet<T> asSet(const QList<T>& a)
    {
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
        return QSet<T>(a.begin(), a.end());
#else
        return QSet<T>::fromList(a);
#endif
    }

    /**
     * Escapes all characters in regex such that they do not receive any special treatment when used
     * in a regular expression. Essentially, this function escapes any characters not in a-zA-Z0-9.
     * @param regex The unescaped regular expression string.
     * @return An escaped string safe to use in a regular expression.
     */
    QString escapeRegex(const QString& regex);

    enum RegexConvertOpts
    {
        DEFAULT = 0,
        WILDCARD_UNLIMITED_MATCH = 1,
        WILDCARD_SINGLE_MATCH = 1 << 2,
        WILDCARD_LOGICAL_OR = 1 << 3,
        WILDCARD_ALL = WILDCARD_UNLIMITED_MATCH | WILDCARD_SINGLE_MATCH | WILDCARD_LOGICAL_OR,
        EXACT_MATCH = 1 << 4,
        CASE_SENSITIVE = 1 << 5,
        ESCAPE_REGEX = 1 << 6,
    };

    /**
     * Converts input string to a regular expression according to the options specified in opts.
     * Note that, unless ESCAPE_REGEX is set, convertToRegex assumes a proper regular expression as input.
     * @param string The input string. Assumed to be a proper regular expression unless ESCAPE_REGEX is set.
     * @param opts Tools::RegexConvertOpts options the regex will be converted with.
     * @return The regular expression built from string and opts.
     */
    QRegularExpression convertToRegex(const QString& string, int opts = RegexConvertOpts::DEFAULT);

    template <typename RandomAccessIterator, typename T>
    RandomAccessIterator binaryFind(RandomAccessIterator begin, RandomAccessIterator end, const T& value)
    {
        RandomAccessIterator it = std::lower_bound(begin, end, value);

        if ((it == end) || (value < *it)) {
            return end;
        } else {
            return it;
        }
    }

    // Checks if all values are found inside the list. Returns a list of values not found.
    template <typename T> QList<T> getMissingValuesFromList(const QList<T>& list, const QList<T>& required)
    {
        QList<T> missingValues;
        for (const auto& r : required) {
            if (!list.contains(r)) {
                missingValues << r;
            }
        }

        return missingValues;
    }

    QVariantMap qo2qvm(const QObject* object, const QStringList& ignoredProperties = {"objectName"});

    QString substituteBackupFilePath(QString pattern, const QString& databasePath);

    enum class MimeType : uint8_t
    {
        Image,
        PlainText,
        Pdf,
        Unknown
    };

    MimeType toMimeType(const QString& mimeName);
} // namespace Tools

#endif // KEEPASSX_TOOLS_H
