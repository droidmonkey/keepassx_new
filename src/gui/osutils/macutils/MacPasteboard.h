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

#ifndef KEEPASSXC_MACPASTEBOARD_H
#define KEEPASSXC_MACPASTEBOARD_H

#include <QObject>
#include <QStringConverter>
#include <QUtiMimeConverter>
#include <QVariant>

class MacPasteboard : public QObject, public QUtiMimeConverter
{
public:
    explicit MacPasteboard()
        : QUtiMimeConverter()
    {
    }

    bool canConvert(const QString& mime, const QString& uti) const;
    QString mimeForUti(const QString& uti) const override;
    QString utiForMime(const QString& mime) const override;
    QVariant convertToMime(const QString& mime, const QList<QByteArray>& data, const QString& uti) const override;
    QList<QByteArray> convertFromMime(const QString& mime, const QVariant& data, const QString& uti) const override;
};

#endif // KEEPASSXC_MACPASTEBOARD_H
