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

#ifndef KEEPASSXC_TEXTSTREAM_H
#define KEEPASSXC_TEXTSTREAM_H

#include <QIODevice>
#include <QTextStream>

/**
 * QTextStream with codec fixes for the Windows command line.
 *
 * QTextStream uses the system default locale, but this breaks in various
 * situations: (1) It does not work on the native Windows shell (cmd.exe, Powershell),
 * since the default Windows locale is Windows-1252, but the shell uses Windows-850.
 * (2) It also breaks on *nix systems where the locale is Latin1 or C, which
 * is the case for most CI systems or build servers.
 *
 * We allow overriding the detected codec by setting the ENCODING_OVERRIDE
 * environment variable, but otherwise prefer Windows-850 on Windows and UTF-8
 * on any other system, even if LANG is set to something else.
 */
class TextStream : public QTextStream
{
public:
    TextStream();
    explicit TextStream(QIODevice* device);
    explicit TextStream(FILE* fileHandle, QIODevice::OpenMode openMode = QIODevice::ReadWrite);
    explicit TextStream(QString* string, QIODevice::OpenMode openMode = QIODevice::ReadWrite);
    explicit TextStream(QByteArray* array, QIODevice::OpenMode openMode = QIODevice::ReadWrite);
    explicit TextStream(const QByteArray& array, QIODevice::OpenMode openMode = QIODevice::ReadOnly);

    void write(const char* str);

private:
    void detectCodec();
};

#endif // KEEPASSXC_TEXTSTREAM_H
