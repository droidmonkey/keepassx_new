/*
 *  Copyright (C) 2023 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_SIGNALMULTIPLEXER_H
#define KEEPASSX_SIGNALMULTIPLEXER_H

#include <QList>
#include <QObject>
#include <QPointer>

class SignalMultiplexer
{
public:
    SignalMultiplexer();
    ~SignalMultiplexer();
    QObject* currentObject() const;
    void setCurrentObject(QObject* object);

    void connect(QObject* sender, const char* signal, const char* slot);
    void disconnect(QObject* sender, const char* signal, const char* slot);

    void connect(const char* signal, QObject* receiver, const char* slot);
    void disconnect(const char* signal, QObject* receiver, const char* slot);

private:
    struct Connection
    {
        QPointer<QObject> sender;
        QPointer<QObject> receiver;
        const char* signal;
        const char* slot;
    };

    void connect(const Connection& con);
    void disconnect(const Connection& con);

    QPointer<QObject> m_currentObject;
    QList<Connection> m_connections;

    Q_DISABLE_COPY(SignalMultiplexer)
};

#endif // KEEPASSX_SIGNALMULTIPLEXER_H
