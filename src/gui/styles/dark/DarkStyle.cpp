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

#include "DarkStyle.h"

#include <QDialog>
#include <QFile>
#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QToolBar>

#ifdef Q_OS_MACOS
#include "gui/osutils/OSUtils.h"
#endif

DarkStyle::DarkStyle()
    : BaseStyle()
{
#ifdef Q_OS_MACOS
    m_drawNativeMacOsToolBar = osUtils->isDarkMode();
#endif
}

QPalette DarkStyle::standardPalette() const
{
    auto palette = BaseStyle::standardPalette();
    palette.setColor(QPalette::Active, QPalette::Window, QRgb(0x3B3B3D));
    palette.setColor(QPalette::Inactive, QPalette::Window, QRgb(0x404042));
    palette.setColor(QPalette::Disabled, QPalette::Window, QRgb(0x424242));

    palette.setColor(QPalette::Active, QPalette::WindowText, QRgb(0xCACBCE));
    palette.setColor(QPalette::Inactive, QPalette::WindowText, QRgb(0xC8C8C6));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QRgb(0x707070));

    palette.setColor(QPalette::Active, QPalette::Text, QRgb(0xCACBCE));
    palette.setColor(QPalette::Inactive, QPalette::Text, QRgb(0xC8C8C6));
    palette.setColor(QPalette::Disabled, QPalette::Text, QRgb(0x707070));

    palette.setColor(QPalette::Active, QPalette::PlaceholderText, QRgb(0x7D7D82));
    palette.setColor(QPalette::Inactive, QPalette::PlaceholderText, QRgb(0x87888C));
    palette.setColor(QPalette::Disabled, QPalette::PlaceholderText, QRgb(0x737373));

    palette.setColor(QPalette::Active, QPalette::BrightText, QRgb(0x252627));
    palette.setColor(QPalette::Inactive, QPalette::BrightText, QRgb(0x2D2D2F));
    palette.setColor(QPalette::Disabled, QPalette::BrightText, QRgb(0x333333));

    palette.setColor(QPalette::Active, QPalette::Base, QRgb(0x27272A));
    palette.setColor(QPalette::Inactive, QPalette::Base, QRgb(0x2A2A2D));
    palette.setColor(QPalette::Disabled, QPalette::Base, QRgb(0x343437));

    palette.setColor(QPalette::Active, QPalette::AlternateBase, QRgb(0x2C2C30));
    palette.setColor(QPalette::Inactive, QPalette::AlternateBase, QRgb(0x2B2B2F));
    palette.setColor(QPalette::Disabled, QPalette::AlternateBase, QRgb(0x36363A));

    palette.setColor(QPalette::All, QPalette::ToolTipBase, QRgb(0x2D532D));
    palette.setColor(QPalette::All, QPalette::ToolTipText, QRgb(0xBFBFBF));

    palette.setColor(QPalette::Active, QPalette::Button, QRgb(0x28282B));
    palette.setColor(QPalette::Inactive, QPalette::Button, QRgb(0x28282B));
    palette.setColor(QPalette::Disabled, QPalette::Button, QRgb(0x2B2A2A));

    palette.setColor(QPalette::Active, QPalette::ButtonText, QRgb(0xB9B9BE));
    palette.setColor(QPalette::Inactive, QPalette::ButtonText, QRgb(0x9E9FA5));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QRgb(0x73747E));

    palette.setColor(QPalette::Active, QPalette::Highlight, QRgb(0x2D532D));
    palette.setColor(QPalette::Inactive, QPalette::Highlight, QRgb(0x354637));
    palette.setColor(QPalette::Disabled, QPalette::Highlight, QRgb(0x293D29));

    palette.setColor(QPalette::Active, QPalette::HighlightedText, QRgb(0xCCCCCC));
    palette.setColor(QPalette::Inactive, QPalette::HighlightedText, QRgb(0xCECECE));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QRgb(0x707070));

    palette.setColor(QPalette::All, QPalette::Light, QRgb(0x414145));
    palette.setColor(QPalette::All, QPalette::Midlight, QRgb(0x39393C));
    palette.setColor(QPalette::All, QPalette::Mid, QRgb(0x2F2F32));
    palette.setColor(QPalette::All, QPalette::Dark, QRgb(0x202022));
    palette.setColor(QPalette::All, QPalette::Shadow, QRgb(0x19191A));

    palette.setColor(QPalette::All, QPalette::Link, QRgb(0x68B668));
    palette.setColor(QPalette::Disabled, QPalette::Link, QRgb(0x74A474));
    palette.setColor(QPalette::All, QPalette::LinkVisited, QRgb(0x75B875));
    palette.setColor(QPalette::Disabled, QPalette::LinkVisited, QRgb(0x77A677));

    return palette;
}

QString DarkStyle::getAppStyleSheet() const
{
    QFile extStylesheetFile(QStringLiteral(":/styles/dark/darkstyle.qss"));
    if (extStylesheetFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return extStylesheetFile.readAll();
    }
    qWarning("Failed to load dark theme stylesheet.");
    return {};
}

void DarkStyle::polish(QWidget* widget)
{
    if (qobject_cast<QMainWindow*>(widget) || qobject_cast<QDialog*>(widget) || qobject_cast<QMenuBar*>(widget)
        || qobject_cast<QToolBar*>(widget)) {
        auto palette = widget->palette();
#if defined(Q_OS_MACOS)
        if (!osUtils->isDarkMode()) {
            palette.setColor(QPalette::Active, QPalette::Window, QRgb(0x2A2A2A));
            palette.setColor(QPalette::Inactive, QPalette::Window, QRgb(0x2D2D2D));
            palette.setColor(QPalette::Disabled, QPalette::Window, QRgb(0x2D2D2D));
        }
#elif defined(Q_OS_WIN)
        palette.setColor(QPalette::All, QPalette::Window, QRgb(0x2F2F30));
#else
        palette.setColor(QPalette::Active, QPalette::Window, QRgb(0x2F2F30));
        palette.setColor(QPalette::Inactive, QPalette::Window, QRgb(0x313133));
        palette.setColor(QPalette::Disabled, QPalette::Window, QRgb(0x3A3A3B));
#endif

        widget->setPalette(palette);
    }
}
