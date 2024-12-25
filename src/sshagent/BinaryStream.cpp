/*
 *  Copyright (C) 2017 Toni Spets <toni.spets@iki.fi>
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

#include "BinaryStream.h"
#include <QtEndian>

BinaryStream::BinaryStream(QIODevice* device)
    : QObject(device)
    , m_device(device)
{
}

BinaryStream::BinaryStream(QByteArray* ba, QObject* parent)
    : QObject(parent)
{
    m_buffer.reset(new QBuffer(ba));
    m_buffer->open(QIODevice::ReadWrite);

    m_device = m_buffer.data();
}

BinaryStream::~BinaryStream()
{
}

const QString BinaryStream::errorString() const
{
    return m_error;
}

QIODevice* BinaryStream::device() const
{
    return m_device;
}

void BinaryStream::setTimeout(int timeout)
{
    m_timeout = timeout;
}

bool BinaryStream::read(char* ptr, qint64 size)
{
    qint64 pos = 0;

    while (pos < size) {
        if (m_device->bytesAvailable() == 0) {
            if (!m_device->waitForReadyRead(m_timeout)) {
                m_error = m_device->errorString();
                return false;
            }
        }

        qint64 nread = m_device->read(ptr + pos, size - pos);

        if (nread == -1) {
            m_error = m_device->errorString();
            return false;
        }

        pos += nread;
    }

    return true;
}

bool BinaryStream::read(QByteArray& ba)
{
    return read(ba.data(), ba.length());
}

bool BinaryStream::read(quint32& i)
{
    if (read(reinterpret_cast<char*>(&i), sizeof(i))) {
        i = qFromBigEndian<quint32>(i);
        return true;
    }

    return false;
}

bool BinaryStream::read(quint16& i)
{
    if (read(reinterpret_cast<char*>(&i), sizeof(i))) {
        i = qFromBigEndian<quint16>(i);
        return true;
    }

    return false;
}

bool BinaryStream::read(quint8& i)
{
    return read(reinterpret_cast<char*>(&i), sizeof(i));
}

bool BinaryStream::readString(QByteArray& ba)
{
    quint32 length;

    if (!read(length)) {
        return false;
    }

    ba.resize(length);

    if (!read(ba.data(), ba.length())) {
        return false;
    }

    return true;
}

bool BinaryStream::readString(QString& str)
{
    QByteArray ba;

    if (!readString(ba)) {
        return false;
    }

    str = QString::fromLatin1(ba);
    return true;
}

bool BinaryStream::write(const char* ptr, qint64 size)
{
    if (m_device->write(ptr, size) < 0) {
        m_error = m_device->errorString();
        return false;
    }

    return true;
}

bool BinaryStream::flush()
{
    if (!m_device->waitForBytesWritten(m_timeout)) {
        m_error = m_device->errorString();
        return false;
    }

    return true;
}

bool BinaryStream::write(const QByteArray& ba)
{
    return write(ba.data(), ba.length());
}

bool BinaryStream::write(quint32 i)
{
    i = qToBigEndian<quint32>(i);
    return write(reinterpret_cast<char*>(&i), sizeof(i));
}

bool BinaryStream::write(quint16 i)
{
    i = qToBigEndian<quint16>(i);
    return write(reinterpret_cast<char*>(&i), sizeof(i));
}

bool BinaryStream::write(quint8 i)
{
    return write(reinterpret_cast<char*>(&i), sizeof(i));
}

bool BinaryStream::writeString(const QByteArray& ba)
{
    if (!write(static_cast<quint32>(ba.length()))) {
        return false;
    }

    if (!write(ba)) {
        return false;
    }

    return true;
}

bool BinaryStream::writeString(const QString& s)
{
    return writeString(s.toLatin1());
}
