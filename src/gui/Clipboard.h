/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSX_CLIPBOARD_H
#define KEEPASSX_CLIPBOARD_H

#include <QElapsedTimer>
#include <QObject>
#ifdef Q_OS_MACOS
#include <QPointer>
#include <QUtiMimeConverter>
#endif

class QTimer;

class Clipboard : public QObject
{
    Q_OBJECT

public:
    void setText(const QString& text, bool clear = true);
    int secondsToClear();

    static Clipboard* instance();

public slots:
    void clearCopiedText();

signals:
    void updateCountdown(int percentage, QString message);

private slots:
    void countdownTick();

private:
    explicit Clipboard(QObject* parent = nullptr);

    static Clipboard* m_instance;

    void sendCountdownStatus();

    QTimer* m_timer;
    int m_secondsToClear = 0;

#ifdef Q_OS_MACOS
    // This object lives for the whole program lifetime and we cannot delete it on exit,
    // so ignore leak warnings. See https://bugreports.qt.io/browse/QTBUG-54832
    static QPointer<QUtiMimeConverter> m_pasteboard;
#endif
    QString m_lastCopied;
};

inline Clipboard* clipboard()
{
    return Clipboard::instance();
}

#endif // KEEPASSX_CLIPBOARD_H
