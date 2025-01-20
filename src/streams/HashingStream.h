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

#ifndef KEEPASSX_HASHINGSTREAM_H
#define KEEPASSX_HASHINGSTREAM_H

#include <QCryptographicHash>

#include "streams/LayeredStream.h"

class HashingStream : public LayeredStream
{
    Q_OBJECT

public:
    explicit HashingStream(QIODevice* baseDevice);
    HashingStream(QIODevice* baseDevice, QCryptographicHash::Algorithm hashAlgo, qint64 sizeToHash);
    ~HashingStream() override;

    bool reset() override;

    QByteArray hashingResult();

protected:
    void init();

    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

protected:
    QCryptographicHash m_hash;
    bool m_hashFinalized;
    qint64 m_sizeToHash;
    qint64 m_sizeHashed;
    qint64 m_sizeStreamed;
};

#endif // KEEPASSX_HASHINGSTREAM_H