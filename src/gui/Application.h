/*
 *  Copyright (C) 2012 Tobias Tangemann
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_APPLICATION_H
#define KEEPASSX_APPLICATION_H

#include <QApplication>
#include <QString>
#include <QtNetwork/qlocalserver.h>

#if defined(Q_OS_WIN) || (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))

class OSEventFilter;
#endif
class QLockFile;
class QSocketNotifier;

constexpr int RESTART_EXITCODE = -1;

class Application : public QApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    static void bootstrap(const QString& uiLanguage = "system");
    static void applyFontSize();

    void applyTheme();

    bool event(QEvent* event) override;
    bool isAlreadyRunning() const;
    bool isDarkTheme() const;

    bool sendFileNamesToRunningInstance(const QStringList& fileNames);
    bool sendLockToInstance();

    void restart();

signals:
    void openFile(const QString& filename);
    void anotherInstanceStarted();
    void applicationActivated();
    void quitSignalReceived();

private slots:
#if defined(Q_OS_UNIX)
    void quitBySignal();
#endif
    void processIncomingConnection();
    void socketReadyRead();

private:
#if defined(Q_OS_UNIX)
    /**
     * Register Unix signals such as SIGINT and SIGTERM for clean shutdown.
     */
    void registerUnixSignals();
    QSocketNotifier* m_unixSignalNotifier;
    static void handleUnixSignal(int sig);
    static int unixSignalSocket[2];
#endif
    bool m_alreadyRunning;
    bool m_darkTheme = false;
    QLockFile* m_lockFile;
    QLocalServer m_lockServer;
    QString m_socketName;
};

#define kpxcApp qobject_cast<Application*>(Application::instance())

#endif // KEEPASSX_APPLICATION_H
