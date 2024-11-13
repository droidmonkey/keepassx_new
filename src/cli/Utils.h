/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_UTILS_H
#define KEEPASSXC_UTILS_H

#include <QTextStream>

class CompositeKey;
class Database;
class Entry;
class EntryAttributes;
class FileKey;
class PasswordKey;

namespace Utils
{
    extern QTextStream STDOUT;
    extern QTextStream STDERR;
    extern QTextStream STDIN;
    extern QTextStream DEVNULL;

    static const QString UuidFieldName = "Uuid";
    static const QString TagsFieldName = "Tags";
    static const QStringList EntryFieldNames(QStringList() << UuidFieldName << TagsFieldName);

    void setDefaultTextStreams();
    void resetTextStreams();

    void setStdinEcho(bool enable);
    bool loadFileKey(const QString& path, QSharedPointer<FileKey>& fileKey);
    QString getPassword(bool quiet = false);
    QSharedPointer<PasswordKey> getConfirmedPassword();
    int clipText(const QString& text);
    QSharedPointer<Database> unlockDatabase(const QString& databaseFilename,
                                            bool isPasswordProtected = true,
                                            const QString& keyFilename = {},
                                            const QString& yubiKeySlot = {},
                                            bool quiet = false);

    QStringList splitCommandString(const QString& command);

    /**
     * If `attributes` contains an attribute named `name` (case-sensitive),
     * returns a list containing only `name`. Otherwise, returns the list of
     * all attribute names in `attributes` matching the given name
     * (case-insensitive).
     */
    QStringList findAttributes(const EntryAttributes& attributes, const QString& name);
    /**
     * Get the value of a top-level Entry field using its name.
     */
    QString getTopLevelField(const Entry* entry, const QString& fieldName);
}; // namespace Utils

#endif // KEEPASSXC_UTILS_H
