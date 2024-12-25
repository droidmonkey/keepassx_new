/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "Clipboard.h"

#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QProcess>
#include <QTimer>

#include "core/Config.h"

Clipboard* Clipboard::m_instance(nullptr);
#ifdef Q_OS_MACOS
QPointer<MacPasteboard> Clipboard::m_pasteboard(nullptr);
#endif

Clipboard::Clipboard(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
#ifdef Q_OS_MACOS
    if (!m_pasteboard) {
        m_pasteboard = new MacPasteboard();
    }
#endif
    connect(m_timer, SIGNAL(timeout()), SLOT(countdownTick()));
    connect(qApp, SIGNAL(aboutToQuit()), SLOT(clearCopiedText()));
}

void Clipboard::setText(const QString& text, bool clear)
{
    auto* clipboard = QApplication::clipboard();
    if (!clipboard) {
        qWarning("Unable to access the clipboard.");
        return;
    }

    auto* mime = new QMimeData;
    mime->setText(text);
#if defined(Q_OS_MACOS)
    mime->setData("application/x-nspasteboard-concealed-type", text.toUtf8());
#elif defined(Q_OS_UNIX)
    mime->setData("x-kde-passwordManagerHint", QByteArrayLiteral("secret"));
#elif defined(Q_OS_WIN)
    mime->setData("ExcludeClipboardContentFromMonitorProcessing", QByteArrayLiteral("1"));
    mime->setData("CanIncludeInClipboardHistory", {4, '\0'});
    mime->setData("CanUploadToCloudClipboard ", {4, '\0'});
#endif

    if (clipboard->supportsSelection()) {
        clipboard->setMimeData(mime, QClipboard::Selection);
    }
    clipboard->setMimeData(mime, QClipboard::Clipboard);

    if (clear) {
        m_lastCopied = text;
        if (config()->get(Config::Security_ClearClipboard).toBool()) {
            int timeout = config()->get(Config::Security_ClearClipboardTimeout).toInt();
            if (timeout > 0) {
                m_secondsToClear = timeout;
                sendCountdownStatus();
                m_timer->start(1000);
            } else {
                clearCopiedText();
            }
        }
    }
}

int Clipboard::secondsToClear()
{
    return m_secondsToClear;
}

void Clipboard::clearCopiedText()
{
    m_timer->stop();
    emit updateCountdown(-1, "");

    auto* clipboard = QApplication::clipboard();
    if (!clipboard) {
        qWarning("Unable to access the clipboard.");
        return;
    }

    if (m_lastCopied == clipboard->text(QClipboard::Clipboard)
        || m_lastCopied == clipboard->text(QClipboard::Selection)) {
        clipboard->clear(QClipboard::Clipboard);
        clipboard->clear(QClipboard::Selection);
#ifdef Q_OS_UNIX
        // Gnome Wayland doesn't let apps modify the clipboard when not in focus, so force clear
        if (QProcessEnvironment::systemEnvironment().contains("WAYLAND_DISPLAY")) {
            QProcess::startDetached("wl-copy", {"-c"});
        }
#endif
    }

    m_lastCopied.clear();
}

void Clipboard::countdownTick()
{
    if (--m_secondsToClear <= 0) {
        clearCopiedText();
    } else {
        sendCountdownStatus();
    }
}

void Clipboard::sendCountdownStatus()
{
    emit updateCountdown(
        100 * m_secondsToClear / config()->get(Config::Security_ClearClipboardTimeout).toInt(),
        QObject::tr("Clearing the clipboard in %1 second(s)…", "", m_secondsToClear).arg(m_secondsToClear));
}

Clipboard* Clipboard::instance()
{
    if (!m_instance) {
        m_instance = new Clipboard(qApp);
    }

    return m_instance;
}
