/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#ifndef SCREENLOCKLISTENERWIN_H
#define SCREENLOCKLISTENERWIN_H

#include <QAbstractNativeEventFilter>
#include <QWidget>

#include "gui/osutils/ScreenLockListenerPrivate.h"

class ScreenLockListenerWin : public ScreenLockListenerPrivate, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit ScreenLockListenerWin(QWidget* parent);
    ~ScreenLockListenerWin();
    virtual bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

private:
    void* m_powerNotificationHandle;
};

#endif // SCREENLOCKLISTENERWIN_H
