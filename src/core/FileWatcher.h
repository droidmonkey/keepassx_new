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

#ifndef KEEPASSXC_FILEWATCHER_H
#define KEEPASSXC_FILEWATCHER_H

#include <QFileSystemWatcher>
#include <QTimer>

class FileWatcher : public QObject
{
    Q_OBJECT

public:
    explicit FileWatcher(QObject* parent = nullptr);
    ~FileWatcher() override;

    void start(const QString& path, int checksumIntervalSeconds = 0, int checksumSizeKibibytes = -1);
    void stop();

    bool hasSameFileChecksum();

signals:
    void fileChanged(const QString& path);

public slots:
    void pause();
    void resume();

private slots:
    void checkFileChanged();

private:
    QByteArray calculateChecksum();
    bool shouldIgnoreChanges();

    QString m_filePath;
    QFileSystemWatcher m_fileWatcher;
    QByteArray m_fileChecksum;
    QTimer m_fileChangeDelayTimer;
    QTimer m_fileIgnoreDelayTimer;
    QTimer m_fileChecksumTimer;
    int m_fileChecksumSizeBytes = -1;
    bool m_ignoreFileChange = false;
    bool m_paused = false;
};

#endif // KEEPASSXC_FILEWATCHER_H
