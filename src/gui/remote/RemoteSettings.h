/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_REMOTESETTINGS_H
#define KEEPASSXC_REMOTESETTINGS_H

#include <QHash>
#include <QObject>
#include <QSharedPointer>

class Database;

struct RemoteParams
{
    QString name;
    QString downloadCommand;
    QString downloadInput;
    int downloadTimeoutMsec;
    QString uploadCommand;
    QString uploadInput;
    int uploadTimeoutMsec;
};
Q_DECLARE_METATYPE(RemoteParams)

class RemoteSettings : public QObject
{
    Q_OBJECT
public:
    explicit RemoteSettings(const QSharedPointer<Database>& db, QObject* parent = nullptr);
    ~RemoteSettings() override;

    void setDatabase(const QSharedPointer<Database>& db);

    void addRemoteParams(RemoteParams* params);
    void removeRemoteParams(const QString& name);
    RemoteParams* getRemoteParams(const QString& name) const;
    QList<RemoteParams*> getAllRemoteParams() const;

    void loadSettings();
    void saveSettings() const;

private:
    void fromConfig(const QString& data);
    QString toConfig() const;

    QHash<QString, RemoteParams*> m_remoteParams;
    QSharedPointer<Database> m_db;
};

#endif // KEEPASSXC_REMOTESETTINGS_H
