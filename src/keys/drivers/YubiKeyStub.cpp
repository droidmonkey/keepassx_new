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

#include "YubiKey.h"

YubiKey::YubiKey()
{
}

YubiKey* YubiKey::m_instance(Q_NULLPTR);

YubiKey* YubiKey::instance()
{
    if (!m_instance) {
        m_instance = new YubiKey();
    }

    return m_instance;
}

bool YubiKey::isInitialized()
{
    return false;
}

bool YubiKey::findValidKeys()
{
    return false;
}

void YubiKey::findValidKeysAsync()
{
}

YubiKey::KeyMap YubiKey::foundKeys()
{
    return {};
}

int YubiKey::connectedKeys()
{
    return 0;
}

QString YubiKey::errorMessage()
{
    return {};
}

bool YubiKey::testChallenge(YubiKeySlot slot, bool* wouldBlock)
{
    Q_UNUSED(slot);
    Q_UNUSED(wouldBlock);
    return false;
}

YubiKey::ChallengeResult YubiKey::challenge(YubiKeySlot slot, const QByteArray& chal, Botan::secure_vector<char>& resp)
{
    Q_UNUSED(slot);
    Q_UNUSED(chal);
    Q_UNUSED(resp);

    return YubiKey::ChallengeResult::YCR_ERROR;
}
