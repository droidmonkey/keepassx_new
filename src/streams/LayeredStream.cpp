/*
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

#include "LayeredStream.h"

LayeredStream::LayeredStream(QIODevice* baseDevice)
    : QIODevice(baseDevice)
    , m_baseDevice(baseDevice)
{
    connect(baseDevice, SIGNAL(aboutToClose()), SLOT(closeStream()));
}

LayeredStream::~LayeredStream()
{
    QIODevice::close();
}

bool LayeredStream::isSequential() const
{
    return true;
}

bool LayeredStream::open(QIODeviceBase::OpenMode mode)
{
    if (isOpen()) {
        qWarning("LayeredStream::open: Device is already open.");
        return false;
    }

    bool readMode = (mode & QIODeviceBase::ReadOnly);
    bool writeMode = (mode & QIODeviceBase::WriteOnly);

    if (readMode && writeMode) {
        qWarning("LayeredStream::open: Reading and writing at the same time is not supported.");
        return false;
    } else if (!readMode && !writeMode) {
        qWarning("LayeredStream::open: Must be opened in read or write mode.");
        return false;
    } else if ((readMode && !m_baseDevice->isReadable()) || (writeMode && !m_baseDevice->isWritable())) {
        qWarning("LayeredStream::open: Base device is not opened correctly.");
        return false;
    } else {
        if (mode & QIODevice::Append) {
            qWarning("LayeredStream::open: QIODevice::Append is not supported.");
            mode = mode & ~QIODevice::Append;
        }
        if (mode & QIODevice::Truncate) {
            qWarning("LayeredStream::open: QIODevice::Truncate is not supported.");
            mode = mode & ~QIODevice::Truncate;
        }

        mode = mode | QIODevice::Unbuffered;

        return QIODevice::open(mode);
    }
}

qint64 LayeredStream::readData(char* data, qint64 maxSize)
{
    return m_baseDevice->read(data, maxSize);
}

qint64 LayeredStream::writeData(const char* data, qint64 maxSize)
{
    return m_baseDevice->write(data, maxSize);
}

void LayeredStream::closeStream()
{
    close();
}
