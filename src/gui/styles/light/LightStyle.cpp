/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
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

#include "LightStyle.h"

#include <QDialog>
#include <QFile>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>

#ifdef Q_OS_MACOS
#include "gui/osutils/OSUtils.h"
#endif

LightStyle::LightStyle()
    : BaseStyle()
{
#ifdef Q_OS_MACOS
    m_drawNativeMacOsToolBar = !osUtils->isDarkMode();
#endif
}

QPalette LightStyle::standardPalette() const
{
    auto palette = BaseStyle::standardPalette();
    palette.setColor(QPalette::Active, QPalette::Window, QRgb(0xF7F7F7));
    palette.setColor(QPalette::Inactive, QPalette::Window, QRgb(0xFCFCFC));
    palette.setColor(QPalette::Disabled, QPalette::Window, QRgb(0xEDEDED));

    palette.setColor(QPalette::Active, QPalette::WindowText, QRgb(0x1D1D20));
    palette.setColor(QPalette::Inactive, QPalette::WindowText, QRgb(0x252528));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QRgb(0x8C8C92));

    palette.setColor(QPalette::Active, QPalette::Text, QRgb(0x1D1D20));
    palette.setColor(QPalette::Inactive, QPalette::Text, QRgb(0x252528));
    palette.setColor(QPalette::Disabled, QPalette::Text, QRgb(0x8C8C92));

    palette.setColor(QPalette::Active, QPalette::PlaceholderText, QRgb(0x71727D));
    palette.setColor(QPalette::Inactive, QPalette::PlaceholderText, QRgb(0x878893));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QRgb(0xA3A4AC));

    palette.setColor(QPalette::Active, QPalette::BrightText, QRgb(0xF3F3F4));
    palette.setColor(QPalette::Inactive, QPalette::BrightText, QRgb(0xEAEAEB));
    palette.setColor(QPalette::Disabled, QPalette::BrightText, QRgb(0xE4E5E7));

    palette.setColor(QPalette::Active, QPalette::Base, QRgb(0xF9F9F9));
    palette.setColor(QPalette::Inactive, QPalette::Base, QRgb(0xFCFCFC));
    palette.setColor(QPalette::Disabled, QPalette::Base, QRgb(0xEFEFF2));

    palette.setColor(QPalette::Active, QPalette::AlternateBase, QRgb(0xECF3E8));
    palette.setColor(QPalette::Inactive, QPalette::AlternateBase, QRgb(0xF1F6EE));
    palette.setColor(QPalette::Disabled, QPalette::AlternateBase, QRgb(0xE1E9DD));

    palette.setColor(QPalette::All, QPalette::ToolTipBase, QRgb(0x4D7F1A));
    palette.setColor(QPalette::All, QPalette::ToolTipText, QRgb(0xF9F9F9));

    palette.setColor(QPalette::Active, QPalette::Button, QRgb(0xD4D5DD));
    palette.setColor(QPalette::Inactive, QPalette::Button, QRgb(0xDCDCE0));
    palette.setColor(QPalette::Disabled, QPalette::Button, QRgb(0xE5E5E6));

    palette.setColor(QPalette::Active, QPalette::ButtonText, QRgb(0x181A18));
    palette.setColor(QPalette::Inactive, QPalette::ButtonText, QRgb(0x454A54));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QRgb(0x97979B));

    palette.setColor(QPalette::Active, QPalette::Highlight, QRgb(0x507F1F));
    palette.setColor(QPalette::Inactive, QPalette::Highlight, QRgb(0xA6BE8E));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QRgb(0xC3D5B4));

    palette.setColor(QPalette::Active, QPalette::HighlightedText, QRgb(0xFFFFFF));
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, QRgb(0x252528));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QRgb(0x8C8C92));

    palette.setColor(QPalette::All, QPalette::Light, QRgb(0xF9F9F9));
    palette.setColor(QPalette::All, QPalette::Midlight, QRgb(0xE9E9EB));
    palette.setColor(QPalette::All, QPalette::Mid, QRgb(0xC9C9CF));
    palette.setColor(QPalette::All, QPalette::Dark, QRgb(0xBBBBC2));
    palette.setColor(QPalette::All, QPalette::Shadow, QRgb(0x6C6D79));

    palette.setColor(QPalette::All, QPalette::Link, QRgb(0x4B7B19));
    palette.setColor(QPalette::Disabled, QPalette::Link, QRgb(0x4F6935));
    palette.setColor(QPalette::All, QPalette::LinkVisited, QRgb(0x507826));
    palette.setColor(QPalette::Disabled, QPalette::LinkVisited, QRgb(0x506935));

    return palette;
}

QString LightStyle::getAppStyleSheet() const
{
    QFile extStylesheetFile(QStringLiteral(":/styles/light/lightstyle.qss"));
    if (extStylesheetFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return extStylesheetFile.readAll();
    }
    qWarning("Failed to load light theme stylesheet.");
    return {};
}

void LightStyle::polish(QWidget* widget)
{
    if (qobject_cast<QMainWindow*>(widget) || qobject_cast<QDialog*>(widget) || qobject_cast<QMenuBar*>(widget)
        || qobject_cast<QToolBar*>(widget)) {
        auto palette = widget->palette();
#if defined(Q_OS_MACOS)
        if (osUtils->isDarkMode()) {
            palette.setColor(QPalette::Active, QPalette::Window, QRgb(0xD4D4D4));
            palette.setColor(QPalette::Inactive, QPalette::Window, QRgb(0xF5F5F5));
            palette.setColor(QPalette::Disabled, QPalette::Window, QRgb(0xF5F5F5));
        }
#elif defined(Q_OS_WIN)
        palette.setColor(QPalette::All, QPalette::Window, QRgb(0xFFFFFF));
#else
        palette.setColor(QPalette::Active, QPalette::Window, QRgb(0xEFF0F1));
        palette.setColor(QPalette::Inactive, QPalette::Window, QRgb(0xEFF0F1));
        palette.setColor(QPalette::Disabled, QPalette::Window, QRgb(0xE1E2E4));
#endif

        widget->setPalette(palette);
    }
}
