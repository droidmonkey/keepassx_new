/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
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

#include "Icons.h"

#include <QBuffer>
#include <QIconEngine>
#include <QImageReader>
#include <QPaintDevice>
#include <QPainter>

#include "core/Config.h"
#include "core/Database.h"
#include "gui/DatabaseIcons.h"
#include "gui/MainWindow.h"
#include "gui/osutils/OSUtils.h"
#include "keeshare/KeeShare.h"

class AdaptiveIconEngine : public QIconEngine
{
public:
    explicit AdaptiveIconEngine(QIcon baseIcon, QColor overrideColor = {});
    void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override;
    QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override;
    QIconEngine* clone() const override;

private:
    QIcon m_baseIcon;
    QColor m_overrideColor;
};

Icons* Icons::m_instance(nullptr);

Icons::Icons() = default;

QString Icons::applicationIconName()
{
#ifdef KEEPASSXC_DIST_FLATPAK
    return "org.keepassxc.KeePassXC";
#else
    return "keepassxc";
#endif
}

QIcon Icons::applicationIcon()
{
    return icon(applicationIconName(), false);
}

QString Icons::trayIconAppearance() const
{
    auto iconAppearance = config()->get(Config::GUI_TrayIconAppearance).toString();
    if (iconAppearance.isNull()) {
#ifdef Q_OS_MACOS
        iconAppearance = osUtils->isDarkMode() ? "monochrome-light" : "monochrome-dark";
#else
        iconAppearance = "monochrome-light";
#endif
    }
    return iconAppearance;
}

QIcon Icons::trayIcon(bool unlocked)
{
    QString suffix;
    if (!unlocked) {
        suffix = "-locked";
    }

    auto iconAppearance = trayIconAppearance();
    if (!iconAppearance.startsWith("monochrome")) {
        return icon(QString("%1%2").arg(applicationIconName(), suffix), false);
    }

    QIcon i;
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
    if (osUtils->isStatusBarDark()) {
        i = icon(QString("keepassxc-monochrome-light%1").arg(suffix), false);
    } else {
        i = icon(QString("keepassxc-monochrome-dark%1").arg(suffix), false);
    }
#else
    i = icon(QString("%1-%2%3").arg(applicationIconName(), iconAppearance, suffix), false);
#endif
    // Set as mask to allow the operating system to recolour the tray icon. This may look weird
    // if we failed to detect the status bar background colour correctly, but it is certainly
    // better than a barely visible icon and even if we did guess correctly, it allows for better
    // integration should the system's preferred colours not be 100% black or white.
    i.setIsMask(true);
    return i;
}

AdaptiveIconEngine::AdaptiveIconEngine(QIcon baseIcon, QColor overrideColor)
    : QIconEngine()
    , m_baseIcon(std::move(baseIcon))
    , m_overrideColor(overrideColor)
{
}

void AdaptiveIconEngine::paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state)
{
    // Temporary image canvas to ensure that the background is transparent and alpha blending works.
    auto scale = painter->device()->devicePixelRatioF();
    QImage img(rect.size() * scale, QImage::Format_ARGB32_Premultiplied);
    img.fill(0);
    QPainter p(&img);

    m_baseIcon.paint(&p, img.rect(), Qt::AlignCenter, mode, state);

    if (m_overrideColor.isValid()) {
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);
        p.fillRect(img.rect(), m_overrideColor);
    } else if (getMainWindow()) {
        QPalette palette = getMainWindow()->palette();
        p.setCompositionMode(QPainter::CompositionMode_SourceIn);

        if (mode == QIcon::Active) {
            p.fillRect(img.rect(), palette.color(QPalette::Active, QPalette::ButtonText));
        } else if (mode == QIcon::Selected) {
            p.fillRect(img.rect(), palette.color(QPalette::Active, QPalette::HighlightedText));
        } else if (mode == QIcon::Disabled) {
            p.fillRect(img.rect(), palette.color(QPalette::Disabled, QPalette::WindowText));
        } else {
            p.fillRect(img.rect(), palette.color(QPalette::Normal, QPalette::WindowText));
        }
    }

    painter->drawImage(rect, img);
}

QPixmap AdaptiveIconEngine::pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state)
{
    QImage img(size, QImage::Format_ARGB32_Premultiplied);
    img.fill(0);
    QPainter painter(&img);
    paint(&painter, QRect(0, 0, size.width(), size.height()), mode, state);
    return QPixmap::fromImage(img, Qt::ImageConversionFlag::NoFormatConversion);
}

QIconEngine* AdaptiveIconEngine::clone() const
{
    return new AdaptiveIconEngine(m_baseIcon);
}

QIcon Icons::icon(const QString& name, bool recolor, const QColor& overrideColor)
{
#ifdef Q_OS_LINUX
    // Resetting the application theme name before calling QIcon::fromTheme() is required for hacky
    // QPA platform themes such as qt5ct, which randomly mess with the configured icon theme.
    // If we do not reset the theme name here, it will become empty at some point, causing
    // Qt to look for icons at the user-level and global default locations.
    //
    // See issue #4963: https://github.com/keepassxreboot/keepassxc/issues/4963
    // and qt5ct issue #80: https://sourceforge.net/p/qt5ct/tickets/80/
    QIcon::setThemeName("application");
#endif

    QString cacheName =
        QString("%1:%2:%3").arg(recolor ? "1" : "0", overrideColor.isValid() ? overrideColor.name() : "#", name);
    QIcon icon = m_iconCache.value(cacheName);

    if (!icon.isNull() && !overrideColor.isValid()) {
        return icon;
    }

    icon = QIcon::fromTheme(name);
    if (recolor) {
        icon = QIcon(new AdaptiveIconEngine(icon, overrideColor));
        icon.setIsMask(true);
    }

    m_iconCache.insert(cacheName, icon);
    return icon;
}

QIcon Icons::onOffIcon(const QString& name, bool on, bool recolor)
{
    return icon(name + (on ? "-on" : "-off"), recolor);
}

Icons* Icons::instance()
{
    if (!m_instance) {
        m_instance = new Icons();

        Q_INIT_RESOURCE(icons);
        QIcon::setThemeSearchPaths(QStringList{":/icons"} << QIcon::themeSearchPaths());
        QIcon::setThemeName("application");
    }

    return m_instance;
}

QPixmap Icons::customIconPixmap(const Database* db, const QUuid& uuid, IconSize size)
{
    if (!db->metadata()->hasCustomIcon(uuid)) {
        return {};
    }
    // Generate QIcon with pre-baked resolutions
    auto icon = QImage::fromData(db->metadata()->customIcon(uuid).data);
    auto basePixmap = QPixmap::fromImage(icon.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    return QIcon(basePixmap).pixmap(databaseIcons()->iconSize(size));
}

QHash<QUuid, QPixmap> Icons::customIconsPixmaps(const Database* db, IconSize size)
{
    QHash<QUuid, QPixmap> result;

    for (const QUuid& uuid : db->metadata()->customIconsOrder()) {
        result.insert(uuid, Icons::customIconPixmap(db, uuid, size));
    }

    return result;
}

QPixmap Icons::entryIconPixmap(const Entry* entry, IconSize size)
{
    QPixmap icon(size, size);
    if (entry->iconUuid().isNull()) {
        icon = databaseIcons()->icon(entry->iconNumber(), size);
    } else {
        if (entry->database()) {
            icon = Icons::customIconPixmap(entry->database(), entry->iconUuid(), size);
        }
    }

    if (entry->isExpired()) {
        icon = databaseIcons()->applyBadge(icon, DatabaseIcons::Badges::Expired);
    }

    return icon;
}

QPixmap Icons::groupIconPixmap(const Group* group, IconSize size)
{
    QPixmap icon(size, size);
    if (group->iconUuid().isNull()) {
        icon = databaseIcons()->icon(group->iconNumber(), size);
    } else {
        if (group->database()) {
            icon = Icons::customIconPixmap(group->database(), group->iconUuid(), size);
        }
    }

    if (group->isExpired()) {
        icon = databaseIcons()->applyBadge(icon, DatabaseIcons::Badges::Expired);
    } else if (KeeShare::isShared(group)) {
        icon = KeeShare::indicatorBadge(group, icon);
    }

    return icon;
}

QString Icons::imageFormatsFilter()
{
    const QList<QByteArray> formats = QImageReader::supportedImageFormats();
    QStringList formatsStringList;

    for (const QByteArray& format : formats) {
        bool codePointClean = true;
        for (char codePoint : format) {
            if (!QChar(codePoint).isLetterOrNumber()) {
                codePointClean = false;
                break;
            }
        }
        if (codePointClean) {
            formatsStringList.append("*." + QString::fromLatin1(format).toLower());
        }
    }

    return formatsStringList.join(" ");
}

QByteArray Icons::saveToBytes(const QImage& image)
{
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    // TODO: check !icon.save()
    image.save(&buffer, "PNG");
    buffer.close();
    return ba;
}
