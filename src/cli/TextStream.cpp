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

#include "TextStream.h"

#include <QProcessEnvironment>
#include <QStringConverter>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

TextStream::TextStream()
{
    detectCodec();
}

TextStream::TextStream(QIODevice* device)
    : QTextStream(device)
{
    detectCodec();
}

TextStream::TextStream(FILE* fileHandle, QIODevice::OpenMode openMode)
    : QTextStream(fileHandle, openMode)
{
    detectCodec();
}

TextStream::TextStream(QString* string, QIODevice::OpenMode openMode)
    : QTextStream(string, openMode)
{
    detectCodec();
}

TextStream::TextStream(QByteArray* array, QIODevice::OpenMode openMode)
    : QTextStream(array, openMode)
{
    detectCodec();
}

TextStream::TextStream(const QByteArray& array, QIODevice::OpenMode openMode)
    : QTextStream(array, openMode)
{
    detectCodec();
}

void TextStream::write(const char* str)
{
    // Workaround for an issue with QTextStream. Its operator<<(const char *string) will encode the
    // string with a non-UTF-8 encoding. We work around this by wrapping the input string into
    // a QString, thus enforcing UTF-8. More info:
    // https://code.qt.io/cgit/qt/qtbase.git/commit?id=cec8cdba4d1b856e17c8743ba8803349d42dc701
    *this << QString(str);
}

void TextStream::detectCodec()
{
    QString codecName = "UTF-8";
    auto env = QProcessEnvironment::systemEnvironment();

#ifdef Q_OS_WIN
    bool success = false;
#ifdef CP_UTF8
    success = SetConsoleOutputCP(CP_UTF8);
#endif
    if (!success && !env.contains("SHELL")) {
        // Fall back to cp850 if this is Windows without CP_UTF8 and we
        // are running in a native shell (i.e., no Msys or Cygwin).
        codecName = "Windows-850";
    }
#else
    if (env.contains("LANG") && !env.value("LANG").isEmpty() && env.value("LANG") != "C") {
        // Only override codec if LANG is set, otherwise Qt will assume
        // US-ASCII, which is almost always wrong and results in
        // Unicode passwords being displayed as question marks.
        codecName = "System";
    }
#endif

    codecName = env.value("ENCODING_OVERRIDE", codecName);

    QStringConverter::Encoding codec = QStringConverter::Utf8;
    if (codecName.toLatin1().compare(QByteArray("UTF-8"), Qt::CaseInsensitive) == 0
        || codecName.toLatin1().compare(QByteArray("Utf8"), Qt::CaseInsensitive) == 0) {
        codec = QStringConverter::Utf8;
    } else if (codecName.toLatin1().compare(QByteArray("UTF-16"), Qt::CaseInsensitive) == 0
               || codecName.toLatin1().compare(QByteArray("Utf16"), Qt::CaseInsensitive) == 0) {
        codec = QStringConverter::Utf16;
    } else if (codecName.toLatin1().compare(QByteArray("UTF-16BE"), Qt::CaseInsensitive) == 0
               || codecName.toLatin1().compare(QByteArray("Utf16BE"), Qt::CaseInsensitive) == 0) {
        codec = QStringConverter::Utf16BE;
    } else if (codecName.toLatin1().compare(QByteArray("UTF-16LE"), Qt::CaseInsensitive) == 0
               || codecName.toLatin1().compare(QByteArray("Utf16LE"), Qt::CaseInsensitive) == 0) {
        codec = QStringConverter::Utf16LE;
    } else if (codecName.toLatin1().compare(QByteArray("UTF-32"), Qt::CaseInsensitive) == 0
               || codecName.toLatin1().compare(QByteArray("Utf32"), Qt::CaseInsensitive) == 0) {
        codec = QStringConverter::Utf32;
    } else if (codecName.toLatin1().compare(QByteArray("UTF-32BE"), Qt::CaseInsensitive) == 0
               || codecName.toLatin1().compare(QByteArray("Utf32BE"), Qt::CaseInsensitive) == 0) {
        codec = QStringConverter::Utf32BE;
    } else if (codecName.toLatin1().compare(QByteArray("UTF-32LE"), Qt::CaseInsensitive) == 0
               || codecName.toLatin1().compare(QByteArray("Utf32LE"), Qt::CaseInsensitive) == 0) {
        codec = QStringConverter::Utf32LE;
    } else if (codecName.toLatin1().compare(QByteArray("ISO 8859-1"), Qt::CaseInsensitive) == 0
               || codecName.toLatin1().compare(QByteArray("Latin1"), Qt::CaseInsensitive) == 0) {
        codec = QStringConverter::Latin1;
    } else if (codecName.toLatin1().compare(QByteArray("Windows-850"), Qt::CaseInsensitive) == 0
               || codecName.toLatin1().compare(QByteArray("Windows-1252"), Qt::CaseInsensitive) == 0
               || codecName.toLatin1().compare(QByteArray("System"), Qt::CaseInsensitive) == 0) {
        codec = QStringConverter::System;
    } else {
        return;
    }

    setEncoding(codec);
}
