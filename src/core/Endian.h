/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_ENDIAN_H
#define KEEPASSX_ENDIAN_H

#include <QIODevice>
#include <QtEndian>

namespace Endian
{

    template <typename SizedQInt> SizedQInt bytesToSizedInt(const QByteArray& ba, QSysInfo::Endian byteOrder)
    {
        Q_ASSERT(ba.size() == sizeof(SizedQInt));

        if (byteOrder == QSysInfo::LittleEndian) {
            return qFromLittleEndian<SizedQInt>(ba.constData());
        }
        return qFromBigEndian<SizedQInt>(ba.constData());
    }

    template <typename SizedQInt> SizedQInt readSizedInt(QIODevice* device, QSysInfo::Endian byteOrder, bool* ok)
    {
        QByteArray ba = device->read(sizeof(SizedQInt));

        if (ba.size() != sizeof(SizedQInt)) {
            *ok = false;
            return 0;
        }
        *ok = true;
        return bytesToSizedInt<SizedQInt>(ba, byteOrder);
    }

    template <typename SizedQInt> QByteArray sizedIntToBytes(SizedQInt num, QSysInfo::Endian byteOrder)
    {
        QByteArray ba;
        ba.resize(sizeof(SizedQInt));

        if (byteOrder == QSysInfo::LittleEndian) {
            qToLittleEndian<SizedQInt>(num, ba.data());
        } else {
            qToBigEndian<SizedQInt>(num, ba.data());
        }

        return ba;
    }

    template <typename SizedQInt> bool writeSizedInt(SizedQInt num, QIODevice* device, QSysInfo::Endian byteOrder)
    {
        QByteArray ba = sizedIntToBytes<SizedQInt>(num, byteOrder);
        qint64 bytesWritten = device->write(ba);
        return (bytesWritten == ba.size());
    }

} // namespace Endian

#endif // KEEPASSX_ENDIAN_H
