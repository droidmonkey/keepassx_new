/*
 *  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSX_YUBIKEY_INTERFACE_PCSC_H
#define KEEPASSX_YUBIKEY_INTERFACE_PCSC_H

#include "YubiKeyInterface.h"

#include <winscard.h>

#define CLA_ISO 0x00
#define INS_SELECT 0xA4
#define INS_GET_RESPONSE 0xC0
#define SEL_APP_AID 0x04
#define INS_API_REQ 0x01
#define INS_STATUS 0x03
#define CMD_GET_SERIAL 0x10
#define CMD_HMAC_1 0x30
#define CMD_HMAC_2 0x38
#define SW_OK_HIGH 0x90
#define SW_OK_LOW 0x00
#define SW_PRECOND_HIGH 0x69
#define SW_PRECOND_LOW 0x85
#define SW_NOTFOUND_HIGH 0x6A
#define SW_NOTFOUND_LOW 0x82
#define SW_UNSUP_HIGH 0x6D
#define SW_MORE_DATA_HIGH 0x61

typedef QPair<SCARDHANDLE, QByteArray> SCardAID;

/**
 * Singleton class to manage the PCSC interface to hardware key(s)
 */
class YubiKeyInterfacePCSC : public YubiKeyInterface
{
    Q_OBJECT

public:
    static YubiKeyInterfacePCSC* instance();

    YubiKey::KeyMap findValidKeys(int& connectedKeys) override;

    YubiKey::ChallengeResult
    challenge(YubiKeySlot slot, const QByteArray& challenge, Botan::secure_vector<char>& response) override;
    bool testChallenge(YubiKeySlot slot, bool* wouldBlock) override;

private:
    explicit YubiKeyInterfacePCSC();
    ~YubiKeyInterfacePCSC();

    static YubiKeyInterfacePCSC* m_instance;

    YubiKey::ChallengeResult performChallenge(void* key,
                                              int slot,
                                              bool mayBlock,
                                              const QByteArray& challenge,
                                              Botan::secure_vector<char>& response) override;
    bool performTestChallenge(void* key, int slot, bool* wouldBlock) override;

    SCARDCONTEXT m_sc_context{};

    // This list contains all the AID (application identifier) codes for the Yubikey HMAC-SHA1 applet
    //  and also for compatible third-party ones. They will be tried one by one.
    const QList<QByteArray> m_aid_codes = {
        QByteArrayLiteral("\xA0\x00\x00\x05\x27\x20\x01"), // Yubico Yubikey
        QByteArrayLiteral("\xA0\x00\x00\x05\x27\x21\x01"), // Yubico Yubikey OATH AID / Nitrokey 3 Secrets App
        QByteArrayLiteral("\xA0\x00\x00\x06\x17\x00\x07\x53\x4E\xAF\x01") // Fidesmo development
    };

    // This map provides display names for the various hardware-specific ATR (answer to reset) codes
    //  of the Yubikeys (and other compatible tokens)
    const QHash<QByteArray, QString> m_atr_names = {
        // Yubico Yubikeys
        {QByteArrayLiteral("\x3B\x8C\x80\x01\x59\x75\x62\x69\x6B\x65\x79\x4E\x45\x4F\x72\x33\x58"), "YubiKey NEO"},
        {QByteArrayLiteral("\x3B\x8C\x80\x01\x59\x75\x62\x69\x6B\x65\x79\x4E\x45\x4F\x72\xFF\x94"), "YubiKey NEO"},
        {QByteArrayLiteral("\x3B\x8D\x80\x01\x80\x73\xC0\x21\xC0\x57\x59\x75\x62\x69\x4B\x65\x79\xF9"), "YubiKey 5"},
        {QByteArrayLiteral("\x3B\x8D\x80\x01\x80\x73\xC0\x21\xC0\x57\x59\x75\x62\x69\x4B\x65\xFF\x7F"), "YubiKey 5"},
        {QByteArrayLiteral("\x3B\xF8\x13\x00\x00\x81\x31\xFE\x15\x59\x75\x62\x69\x6B\x65\x79\x34\xD4"),
         "YubiKey 4 - OTP+CCID"},
        {QByteArrayLiteral("\x3B\xF9\x18\x00\xFF\x81\x31\xFE\x45\x50\x56\x5F\x4A\x33\x41\x30\x34\x30\x40"),
         "YubiKey NEO - OTP+U2F+CCID (PKI)"},
        {QByteArrayLiteral("\x3B\xFA\x13\x00\x00\x81\x31\xFE\x15\x59\x75\x62\x69\x6B\x65\x79\x4E\x45\x4F\xA6"),
         "YubiKey NEO"},
        {QByteArrayLiteral("\x3B\xFC\x13\x00\x00\x81\x31\xFE\x15\x59\x75\x62\x69\x6B\x65\x79\x4E\x45\x4F\x72\x33\xE1"),
         "YubiKey NEO (PKI)"},
        {QByteArrayLiteral("\x3B\xFC\x13\x00\x00\x81\x31\xFE\x45\x59\x75\x62\x69\x6B\x65\x79\x4E\x45\x4F\x72\x33\xB1"),
         "YubiKey NEO"},
        {QByteArrayLiteral(
             "\x3B\xFD\x13\x00\x00\x81\x31\xFE\x15\x80\x73\xC0\x21\xC0\x57\x59\x75\x62\x69\x4B\x65\x79\x40"),
         "YubiKey 5 (PKI)"},
        {QByteArrayLiteral(
             "\x3B\xFD\x13\x00\x00\x81\x31\xFE\x45\x41\x37\x30\x30\x36\x43\x47\x20\x32\x34\x32\x52\x31\xD6"),
         "YubiKey NEO (token)"},
        // Other tokens implementing the Yubikey challenge-response protocol
        {QByteArrayLiteral("\x3B\x80\x80\x01\x01"), "Fidesmo Card 2.0"},
        {QByteArrayLiteral("\x3B\x8A\x80\x01\x00\x31\xC1\x73\xC8\x40\x00\x00\x90\x00\x90"), "VivoKey Apex"},
        {QByteArrayLiteral("\x3B\x8D\x80\x01\x00\x31\xC1\x73\xC8\x40\x00\x52\xA5\x10\x00\x90\x00\x70"),
         "Dangerous Things FlexSecure"},
        {QByteArrayLiteral("\x3b\x8f\x01\x80\x5d\x4e\x69\x74\x72\x6f\x6b\x65\x79\x00\x00\x00\x00\x00\x6a"),
         "Nitrokey 3"},
    };
};

#endif // KEEPASSX_YUBIKEY_INTERFACE_PCSC_H
