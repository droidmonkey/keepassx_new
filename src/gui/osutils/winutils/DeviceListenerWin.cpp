/*
 * Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or (at your option)
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "DeviceListenerWin.h"

#include <QCoreApplication>

#include <windows.h>
#include <winuser.h>

#include <dbt.h>

DeviceListenerWin::DeviceListenerWin(QWidget* parent)
    : QObject(parent)
{
    // Event listeners need a valid window reference
    Q_ASSERT(parent);
    QCoreApplication::instance()->installNativeEventFilter(this);
}

DeviceListenerWin::~DeviceListenerWin()
{
    deregisterHotplugCallback();
}

void DeviceListenerWin::registerHotplugCallback(bool arrived,
                                                bool left,
                                                int vendorId,
                                                int productId,
                                                const QUuid* deviceClass)
{
    Q_ASSERT(deviceClass);

    if (m_deviceNotifyHandle) {
        deregisterHotplugCallback();
    }

    QString regex = R"(^\\{2}\?\\[A-Z]+#)";
    if (vendorId > 0) {
        regex += QString("VID_%1&").arg(vendorId, 0, 16).toUpper();
        if (productId > 0) {
            regex += QString("PID_%1&").arg(productId, 0, 16).toUpper();
        }
    }
    regex += QString(".*$"); // Qt won't match otherwise
    m_deviceIdMatch = QRegularExpression(regex);

    DEV_BROADCAST_DEVICEINTERFACE_W notificationFilter{
        sizeof(DEV_BROADCAST_DEVICEINTERFACE_W), DBT_DEVTYP_DEVICEINTERFACE, 0u, *deviceClass, {0x00}};
    auto w = reinterpret_cast<HWND>(qobject_cast<QWidget*>(parent())->winId());
    m_deviceNotifyHandle = RegisterDeviceNotificationW(w, &notificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!m_deviceNotifyHandle) {
        qWarning("Failed to register device notification handle.");
        return;
    }
    m_handleArrival = arrived;
    m_handleRemoval = left;
}

void DeviceListenerWin::deregisterHotplugCallback()
{
    if (m_deviceNotifyHandle) {
        UnregisterDeviceNotification(m_deviceNotifyHandle);
        m_deviceNotifyHandle = nullptr;
        m_handleArrival = false;
        m_handleRemoval = false;
    }
}

bool DeviceListenerWin::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    Q_UNUSED(result);
    if (eventType != "windows_generic_MSG") {
        return false;
    }

    const auto* m = static_cast<MSG*>(message);
    if (m->message != WM_DEVICECHANGE) {
        return false;
    }
    if ((m_handleArrival && m->wParam == DBT_DEVICEARRIVAL)
        || (m_handleRemoval && m->wParam == DBT_DEVICEREMOVECOMPLETE)) {
        const auto pBrHdr = reinterpret_cast<PDEV_BROADCAST_HDR>(m->lParam);
        const auto pDevIface = reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE_W>(pBrHdr);
        const auto name = QString::fromWCharArray(pDevIface->dbcc_name, pDevIface->dbcc_size);
        if (m_deviceIdMatch.match(name).hasMatch()) {
            emit devicePlugged(m->wParam == DBT_DEVICEARRIVAL, nullptr, pDevIface);
            return true;
        }
    }

    return false;
}
