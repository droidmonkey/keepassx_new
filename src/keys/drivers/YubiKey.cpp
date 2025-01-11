/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2014 Kyle Manna <kyle@kylemanna.com>
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
#include "YubiKeyInterfacePCSC.h"
#include "YubiKeyInterfaceUSB.h"

#include <QMutexLocker>
#include <QSet>
#include <QtConcurrent>

YubiKey::YubiKey()
{
    int num_interfaces = 0;

    if (YubiKeyInterfaceUSB::instance()->isInitialized()) {
        ++num_interfaces;
        connect(YubiKeyInterfaceUSB::instance(), SIGNAL(challengeStarted()), this, SIGNAL(challengeStarted()));
        connect(YubiKeyInterfaceUSB::instance(), SIGNAL(challengeCompleted()), this, SIGNAL(challengeCompleted()));
    } else {
        qDebug("YubiKey: USB interface is not initialized.");
    }

    if (YubiKeyInterfacePCSC::instance()->isInitialized()) {
        ++num_interfaces;
        connect(YubiKeyInterfacePCSC::instance(), SIGNAL(challengeStarted()), this, SIGNAL(challengeStarted()));
        connect(YubiKeyInterfacePCSC::instance(), SIGNAL(challengeCompleted()), this, SIGNAL(challengeCompleted()));
    } else {
        qDebug("YubiKey: PCSC interface is disabled or not initialized.");
    }

    m_initialized = num_interfaces > 0;

    m_interactionTimer.setSingleShot(true);
    m_interactionTimer.setInterval(200);
    connect(&m_interactionTimer, SIGNAL(timeout()), this, SIGNAL(userInteractionRequest()));
    connect(this, &YubiKey::challengeStarted, this, [this] { m_interactionTimer.start(); });
    connect(this, &YubiKey::challengeCompleted, this, [this] { m_interactionTimer.stop(); });
}

YubiKey* YubiKey::m_instance(nullptr);

YubiKey* YubiKey::instance()
{
    if (!m_instance) {
        m_instance = new YubiKey();
    }

    return m_instance;
}

bool YubiKey::isInitialized()
{
    return m_initialized;
}

bool YubiKey::findValidKeys()
{
    QMutexLocker lock(&m_interfaces_detect_mutex);

    m_connectedKeys = 0;
    m_usbKeys = YubiKeyInterfaceUSB::instance()->findValidKeys(m_connectedKeys);
    m_pcscKeys = YubiKeyInterfacePCSC::instance()->findValidKeys(m_connectedKeys);

    return !m_usbKeys.isEmpty() || !m_pcscKeys.isEmpty();
}

void YubiKey::findValidKeysAsync()
{
    auto res = QtConcurrent::run([this] { emit detectComplete(findValidKeys()); });
}

YubiKey::KeyMap YubiKey::foundKeys()
{
    QMutexLocker lock(&m_interfaces_detect_mutex);
    KeyMap foundKeys = m_usbKeys;
    foundKeys.unite(m_pcscKeys);

    return foundKeys;
}

int YubiKey::connectedKeys()
{
    return m_connectedKeys;
}

QString YubiKey::errorMessage()
{
    QMutexLocker lock(&m_interfaces_detect_mutex);

    QString error;
    error.clear();
    if (!m_error.isNull()) {
        error += tr("General: ") + m_error;
    }

    QString usb_error = YubiKeyInterfaceUSB::instance()->errorMessage();
    if (!usb_error.isNull()) {
        if (!error.isNull()) {
            error += " | ";
        }
        error += "USB: " + usb_error;
    }

    QString pcsc_error = YubiKeyInterfacePCSC::instance()->errorMessage();
    if (!pcsc_error.isNull()) {
        if (!error.isNull()) {
            error += " | ";
        }
        error += "PCSC: " + pcsc_error;
    }

    return error;
}

/**
 * Issue a test challenge to the specified slot to determine if challenge
 * response is properly configured.
 *
 * @param slot YubiKey configuration slot
 * @param wouldBlock return if the operation requires user input
 * @return whether the challenge succeeded
 */
bool YubiKey::testChallenge(YubiKeySlot slot, bool* wouldBlock)
{
    QMutexLocker lock(&m_interfaces_detect_mutex);

    if (m_usbKeys.contains(slot)) {
        return YubiKeyInterfaceUSB::instance()->testChallenge(slot, wouldBlock);
    }

    if (m_pcscKeys.contains(slot)) {
        return YubiKeyInterfacePCSC::instance()->testChallenge(slot, wouldBlock);
    }

    return false;
}

/**
 * Issue a challenge to the specified slot
 * This operation could block if the YubiKey requires a touch to trigger.
 *
 * @param slot YubiKey configuration slot
 * @param challenge challenge input to YubiKey
 * @param response response output from YubiKey
 * @return challenge result
 */
YubiKey::ChallengeResult
YubiKey::challenge(YubiKeySlot slot, const QByteArray& challenge, Botan::secure_vector<char>& response)
{
    QMutexLocker lock(&m_interfaces_detect_mutex);

    m_error.clear();

    // Make sure we tried to find available keys
    if (m_usbKeys.isEmpty() && m_pcscKeys.isEmpty()) {
        findValidKeys();
    }

    if (m_usbKeys.contains(slot)) {
        return YubiKeyInterfaceUSB::instance()->challenge(slot, challenge, response);
    }

    if (m_pcscKeys.contains(slot)) {
        return YubiKeyInterfacePCSC::instance()->challenge(slot, challenge, response);
    }

    m_error = tr("Could not find interface for hardware key with serial number %1. Please connect it to continue.")
                  .arg(slot.first);

    return ChallengeResult::YCR_ERROR;
}
