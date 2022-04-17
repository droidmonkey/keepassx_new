/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSX_COMPOSITEKEY_H
#define KEEPASSX_COMPOSITEKEY_H

#include <QList>
#include <QSharedPointer>

#include "keys/Key.h"

class Kdf;
class ChallengeResponseKey;

class CompositeKey : public Key
{
public:
    static QUuid UUID;

    CompositeKey();
    ~CompositeKey() override;
    void clear();
    bool isEmpty() const;

    QByteArray rawKey() const override;
    void setRawKey(const QByteArray& data) override;

    Q_REQUIRED_RESULT bool transform(const Kdf& kdf, QByteArray& result, QString* error = nullptr) const;
    bool challenge(const QByteArray& seed, QByteArray& result, QString* error = nullptr) const;

    void addKey(const QSharedPointer<Key>& key);
    QSharedPointer<Key> getKey(const QUuid keyType) const;
    QSharedPointer<ChallengeResponseKey> getChallengeResponseKey(const QUuid keyType) const;
    const QList<QSharedPointer<Key>>& keys() const;

    void addChallengeResponseKey(const QSharedPointer<ChallengeResponseKey>& key);
    const QList<QSharedPointer<ChallengeResponseKey>>& challengeResponseKeys() const;

    QByteArray serialize() const override;
    void deserialize(const QByteArray& data) override;

private:
    QByteArray rawKey(const QByteArray* transformSeed, bool* ok = nullptr, QString* error = nullptr) const;

    QList<QSharedPointer<Key>> m_keys;
    QList<QSharedPointer<ChallengeResponseKey>> m_challengeResponseKeys;
};

#endif // KEEPASSX_COMPOSITEKEY_H
