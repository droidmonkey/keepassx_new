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

#ifndef KEEPASSXC_BINARYSTREAM_H
#define KEEPASSXC_BINARYSTREAM_H

#include <QBuffer>

class BinaryStream : QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(BinaryStream)
public:
    explicit BinaryStream(QIODevice* device);
    explicit BinaryStream(QByteArray* ba, QObject* parent = nullptr);
    ~BinaryStream() override;

    const QString errorString() const;
    QIODevice* device() const;
    void setTimeout(int timeout);

    bool read(QByteArray& ba);
    bool read(quint32& i);
    bool read(quint16& i);
    bool read(quint8& i);
    bool readString(QByteArray& ba);
    bool readString(QString& s);

    bool write(const QByteArray& ba);
    bool write(quint32 i);
    bool write(quint16 i);
    bool write(quint8 i);
    bool writeString(const QByteArray& ba);
    bool writeString(const QString& s);

    bool flush();

protected:
    bool read(char* ptr, qint64 len);
    bool write(const char* ptr, qint64 len);

private:
    int m_timeout = 3000;
    QString m_error;
    QIODevice* m_device;
    QScopedPointer<QBuffer> m_buffer;
};

#endif // KEEPASSXC_BINARYSTREAM_H
