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

#ifndef KEEPASSX_LAYEREDSTREAM_H
#define KEEPASSX_LAYEREDSTREAM_H

#include <QIODevice>

class LayeredStream : public QIODevice
{
    Q_OBJECT

public:
    explicit LayeredStream(QIODevice* baseDevice);
    ~LayeredStream() override;

    bool isSequential() const override;
    bool open(QIODeviceBase::OpenMode mode) override;

protected:
    qint64 readData(char* data, qint64 maxSize) override;
    qint64 writeData(const char* data, qint64 maxSize) override;

    QIODevice* const m_baseDevice;

private slots:
    void closeStream();
};

#endif // KEEPASSX_LAYEREDSTREAM_H
