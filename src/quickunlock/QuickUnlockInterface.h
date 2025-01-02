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

#ifndef KEEPASSXC_QUICKUNLOCKINTERFACE_H
#define KEEPASSXC_QUICKUNLOCKINTERFACE_H

#include <QSharedPointer>
#include <QUuid>

class QuickUnlockInterface
{
    Q_DISABLE_COPY(QuickUnlockInterface)

public:
    QuickUnlockInterface() = default;
    virtual ~QuickUnlockInterface() = default;

    virtual bool isAvailable() const = 0;
    virtual QString errorString() const = 0;

    virtual bool setKey(const QUuid& dbUuid, const QByteArray& key) = 0;
    virtual bool getKey(const QUuid& dbUuid, QByteArray& key) = 0;
    virtual bool hasKey(const QUuid& dbUuid) const = 0;

    virtual bool canRemember() const = 0;

    virtual void reset(const QUuid& dbUuid) = 0;
    virtual void reset() = 0;
};

class QuickUnlockManager final
{
    Q_DISABLE_COPY(QuickUnlockManager)

public:
    QuickUnlockManager();
    ~QuickUnlockManager();

    QSharedPointer<QuickUnlockInterface> interface() const;
    bool isNativeAvailable() const;
    bool isRememberAvailable() const;

private:
    QSharedPointer<QuickUnlockInterface> m_nativeInterface;
    QSharedPointer<QuickUnlockInterface> m_fallbackInterface;
};

QuickUnlockManager* getQuickUnlock();

#endif // KEEPASSXC_QUICKUNLOCKINTERFACE_H
