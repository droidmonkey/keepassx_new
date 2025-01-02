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

#include "QuickUnlockInterface.h"
#include "PinUnlock.h"

#include <QObject>

#if defined(Q_OS_MACOS)
#include "TouchID.h"
#elif defined(Q_CC_MSVC)
#include "WindowsHello.h"
#elif defined(Q_OS_LINUX)
#include "Polkit.h"
#endif

QuickUnlockManager* g_quickUnlockManager = nullptr;

QuickUnlockManager* getQuickUnlock()
{
    if (!g_quickUnlockManager) {
        g_quickUnlockManager = new QuickUnlockManager();
    }
    return g_quickUnlockManager;
}

QuickUnlockManager::QuickUnlockManager()
{
    // Create the native interface based on the platform
#if defined(Q_OS_MACOS)
    m_nativeInterface.reset(new TouchID());
#elif defined(Q_CC_MSVC)
    m_nativeInterface.reset(new WindowsHello());
#elif defined(Q_OS_LINUX)
    m_nativeInterface.reset(new Polkit());
#endif
    // Always create the fallback interface
    m_fallbackInterface.reset(new PinUnlock());
}

QuickUnlockManager::~QuickUnlockManager()
{
}

QSharedPointer<QuickUnlockInterface> QuickUnlockManager::interface() const
{
    if (isNativeAvailable()) {
        return m_nativeInterface;
    }
    return m_fallbackInterface;
}

bool QuickUnlockManager::isNativeAvailable() const
{
    return m_nativeInterface && m_nativeInterface->isAvailable();
}

bool QuickUnlockManager::isRememberAvailable() const
{
    if (isNativeAvailable()) {
        return m_nativeInterface->canRemember();
    }
    return m_fallbackInterface->canRemember();
}
