/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSX_TOUCHID_H
#define KEEPASSX_TOUCHID_H

#include "QuickUnlockInterface.h"
#include <QHash>

class TouchID : public QuickUnlockInterface
{
public:
    bool isAvailable() const override;
    QString errorString() const override;

    bool setKey(const QUuid& dbUuid, const QByteArray& passwordKey) override;
    bool getKey(const QUuid& dbUuid, QByteArray& passwordKey) override;
    bool hasKey(const QUuid& dbUuid) const override;

    void reset(const QUuid& dbUuid = "") override;
    void reset() override;

private:
    static bool isWatchAvailable();
    static bool isTouchIdAvailable();
    static bool isPasswordFallbackPossible();
    bool setKey(const QUuid& dbUuid, const QByteArray& passwordKey, const bool ignoreTouchID);

    static void deleteKeyEntry(const QString& accountName);
    static QString databaseKeyName(const QUuid& dbUuid);

    QHash<QUuid, QByteArray> m_encryptedMasterKeys;
};

#endif // KEEPASSX_TOUCHID_H
