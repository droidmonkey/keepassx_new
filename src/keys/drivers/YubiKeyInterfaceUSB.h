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

#ifndef KEEPASSX_YUBIKEY_INTERFACE_USB_H
#define KEEPASSX_YUBIKEY_INTERFACE_USB_H

#include "thirdparty/ykcore/ykdef.h"

#include "YubiKeyInterface.h"

/**
 * Singleton class to manage the USB interface to hardware key(s)
 */
class YubiKeyInterfaceUSB : public YubiKeyInterface
{
    Q_OBJECT

public:
    static YubiKeyInterfaceUSB* instance();
    static constexpr int YUBICO_USB_VID = YUBICO_VID;
    static constexpr int ONLYKEY_USB_VID = ONLYKEY_VID;

    YubiKey::KeyMap findValidKeys(int& connectedKeys) override;

    YubiKey::ChallengeResult
    challenge(YubiKeySlot slot, const QByteArray& challenge, Botan::secure_vector<char>& response) override;
    bool testChallenge(YubiKeySlot slot, bool* wouldBlock) override;

private:
    explicit YubiKeyInterfaceUSB();
    ~YubiKeyInterfaceUSB();

    static YubiKeyInterfaceUSB* m_instance;

    YubiKey::ChallengeResult performChallenge(void* key,
                                              int slot,
                                              bool mayBlock,
                                              const QByteArray& challenge,
                                              Botan::secure_vector<char>& response) override;
    bool performTestChallenge(void* key, int slot, bool* wouldBlock) override;

    // This map provides display names for the various USB PIDs of the Yubikeys
    const QHash<int, QString> m_pid_names = {{YUBIKEY_PID, "YubiKey %ver"},
                                             {NEO_OTP_PID, "YubiKey NEO - OTP"},
                                             {NEO_OTP_CCID_PID, "YubiKey NEO - OTP+CCID"},
                                             {NEO_CCID_PID, "YubiKey NEO - CCID"},
                                             {NEO_U2F_PID, "YubiKey NEO - FIDO"},
                                             {NEO_OTP_U2F_PID, "YubiKey NEO - OTP+FIDO"},
                                             {NEO_U2F_CCID_PID, "YubiKey NEO - FIDO+CCID"},
                                             {NEO_OTP_U2F_CCID_PID, "YubiKey NEO - OTP+FIDO+CCID"},
                                             {YK4_OTP_PID, "YubiKey %ver - OTP"},
                                             {YK4_U2F_PID, "YubiKey %ver - U2F"},
                                             {YK4_OTP_U2F_PID, "YubiKey %ver - OTP+FIDO"},
                                             {YK4_CCID_PID, "YubiKey %ver - CCID"},
                                             {YK4_OTP_CCID_PID, "YubiKey %ver - OTP+CCID"},
                                             {YK4_U2F_CCID_PID, "YubiKey %ver - FIDO+CCID"},
                                             {YK4_OTP_U2F_CCID_PID, "YubiKey %ver - OTP+FIDO+CCID"},
                                             {PLUS_U2F_OTP_PID, "YubiKey plus - OTP+FIDO"}};
};

#endif // KEEPASSX_YUBIKEY_INTERFACE_USB_H
