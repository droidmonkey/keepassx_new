/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#ifndef PROTONPASS_READER_H
#define PROTONPASS_READER_H

#include <QSharedPointer>

class Database;

/*!
 * Imports a Proton Pass vault in JSON format: https://proton.me/support/pass-export
 */
class ProtonPassReader
{
public:
    explicit ProtonPassReader() = default;
    ~ProtonPassReader() = default;

    QSharedPointer<Database> convert(const QString& path);

    bool hasError();
    QString errorString();

private:
    QString m_error;
};

#endif // PROTONPASS_READER_H
