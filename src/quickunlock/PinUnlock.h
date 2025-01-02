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

#ifndef KEEPASSXC_PINUNLOCK_H
#define KEEPASSXC_PINUNLOCK_H

#include "QuickUnlockInterface.h"

#include <QHash>

class PinUnlock : public QuickUnlockInterface
{
public:
    PinUnlock() = default;

    bool isAvailable() const override;
    QString errorString() const override;

    bool setKey(const QUuid& dbUuid, const QByteArray& key) override;
    bool getKey(const QUuid& dbUuid, QByteArray& key) override;
    bool hasKey(const QUuid& dbUuid) const override;

    bool canRemember() const override;

    void reset(const QUuid& dbUuid) override;
    void reset() override;

private:
    QString m_error;
    QHash<QUuid, QPair<int, QByteArray>> m_encryptedKeys;

    Q_DISABLE_COPY(PinUnlock)
};

#endif // KEEPASSXC_PINUNLOCK_H
