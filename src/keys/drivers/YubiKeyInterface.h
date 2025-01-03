/*
 *  Copyright (C) 2014 Kyle Manna <kyle@kylemanna.com>
 *  Copyright (C) 2017-2021 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSX_YUBIKEY_INTERFACE_H
#define KEEPASSX_YUBIKEY_INTERFACE_H

#include "YubiKey.h"
#include <QMultiMap>

/**
 * Abstract base class to manage the interfaces to hardware key(s)
 */
class YubiKeyInterface : public QObject
{
    Q_OBJECT

public:
    bool isInitialized() const;

    virtual YubiKey::KeyMap findValidKeys(int& connectedKeys) = 0;
    virtual YubiKey::ChallengeResult
    challenge(YubiKeySlot slot, const QByteArray& challenge, Botan::secure_vector<char>& response) = 0;
    virtual bool testChallenge(YubiKeySlot slot, bool* wouldBlock) = 0;

    QString errorMessage();

signals:
    /**
     * Emitted when a detection process completes. Use the `detectedSlots`
     * accessor function to get information on the available slots.
     *
     * @param found - true if a key was found
     */
    void detectComplete(bool found);

    /**
     * Emitted before/after a challenge-response is performed
     */
    void challengeStarted();
    void challengeCompleted();

protected:
    explicit YubiKeyInterface();
    virtual YubiKey::ChallengeResult performChallenge(void* key,
                                                      int slot,
                                                      bool mayBlock,
                                                      const QByteArray& challenge,
                                                      Botan::secure_vector<char>& response) = 0;
    virtual bool performTestChallenge(void* key, int slot, bool* wouldBlock) = 0;

    bool m_initialized = false;
    QString m_error;

    Q_DISABLE_COPY(YubiKeyInterface)
};

#endif // KEEPASSX_YUBIKEY_INTERFACE_H
