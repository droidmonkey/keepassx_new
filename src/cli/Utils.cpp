/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include "Utils.h"

#include "core/Database.h"
#include "core/Entry.h"
#include "core/EntryAttributes.h"
#include "core/Global.h"
#include "keys/FileKey.h"
#ifdef WITH_XC_YUBIKEY
#include "keys/ChallengeResponseKey.h"
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <QFileInfo>
#include <QProcess>

namespace Utils
{
    QTextStream STDOUT;
    QTextStream STDERR;
    QTextStream STDIN;
    QTextStream DEVNULL;

#ifdef Q_OS_WIN
    UINT origCodePage;
    UINT origOutputCodePage;
#endif

    void setDefaultTextStreams()
    {
        auto fd = new QFile();
        fd->open(stdout, QIODevice::WriteOnly);
        STDOUT.setDevice(fd);

        fd = new QFile();
        fd->open(stderr, QIODevice::WriteOnly);
        STDERR.setDevice(fd);

        fd = new QFile();
        fd->open(stdin, QIODevice::ReadOnly);
        STDIN.setDevice(fd);

        fd = new QFile();
#ifdef Q_OS_WIN
        fd->open(fopen("nul", "w"), QIODevice::WriteOnly);
#else
        fd->open(fopen("/dev/null", "w"), QIODevice::WriteOnly);
#endif
        DEVNULL.setDevice(fd);

#ifdef Q_OS_WIN
        origCodePage = GetConsoleCP();
        origOutputCodePage = GetConsoleOutputCP();

        // On Windows, we ask via keepassxc-cli.exe.manifest to use UTF-8,
        // but the console code-page isn't automatically changed to match.
        SetConsoleCP(GetACP());
        SetConsoleOutputCP(GetACP());
#endif
    }

    void resetTextStreams()
    {
#ifdef Q_OS_WIN
        SetConsoleCP(origCodePage);
        SetConsoleOutputCP(origOutputCodePage);
#endif
    }

    void setStdinEcho(bool enable = true)
    {
#ifdef Q_OS_WIN
        HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
        DWORD mode;
        GetConsoleMode(hIn, &mode);

        if (enable) {
            mode |= ENABLE_ECHO_INPUT;
        } else {
            mode &= ~ENABLE_ECHO_INPUT;
        }

        SetConsoleMode(hIn, mode);
#else
        struct termios t;
        tcgetattr(STDIN_FILENO, &t);

        if (enable) {
            t.c_lflag |= ECHO;
        } else {
            t.c_lflag &= ~ECHO;
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &t);
#endif
    }

    QSharedPointer<Database> unlockDatabase(const QString& databaseFilename,
                                            bool isPasswordProtected,
                                            const QString& keyFilename,
                                            const QString& yubiKeySlot,
                                            bool quiet)
    {
        auto& err = quiet ? DEVNULL : STDERR;
        auto compositeKey = QSharedPointer<CompositeKey>::create();

        QFileInfo dbFileInfo(databaseFilename);
        if (dbFileInfo.canonicalFilePath().isEmpty()) {
            err << QObject::tr("Failed to open database file %1: not found").arg(databaseFilename) << Qt::endl;
            return {};
        }

        if (!dbFileInfo.isFile()) {
            err << QObject::tr("Failed to open database file %1: not a plain file").arg(databaseFilename) << Qt::endl;
            return {};
        }

        if (!dbFileInfo.isReadable()) {
            err << QObject::tr("Failed to open database file %1: not readable").arg(databaseFilename) << Qt::endl;
            return {};
        }

        if (isPasswordProtected) {
            err << QObject::tr("Enter password to unlock %1: ").arg(databaseFilename) << Qt::flush;
            QString line = Utils::getPassword(quiet);
            auto passwordKey = QSharedPointer<PasswordKey>::create();
            passwordKey->setPassword(line);
            compositeKey->addKey(passwordKey);
        }

        if (!keyFilename.isEmpty()) {
            auto fileKey = QSharedPointer<FileKey>::create();
            QString errorMessage;
            // LCOV_EXCL_START
            if (!fileKey->load(keyFilename, &errorMessage)) {
                err << QObject::tr("Failed to load key file %1: %2").arg(keyFilename, errorMessage) << Qt::endl;
                return {};
            }

            if (fileKey->type() != FileKey::KeePass2XMLv2 && fileKey->type() != FileKey::Hashed) {
                err << QObject::tr("WARNING: You are using an old key file format which KeePassXC may\n"
                                   "stop supporting in the future.\n\n"
                                   "Please consider generating a new key file.")
                    << Qt::endl;
            }
            // LCOV_EXCL_STOP

            compositeKey->addKey(fileKey);
        }

#ifdef WITH_XC_YUBIKEY
        if (!yubiKeySlot.isEmpty()) {
            unsigned int serial = 0;
            int slot;

            bool ok = false;
            auto parts = yubiKeySlot.split(":");
            slot = parts[0].toInt(&ok);

            if (!ok || (slot != 1 && slot != 2)) {
                err << QObject::tr("Invalid YubiKey slot %1").arg(parts[0]) << Qt::endl;
                return {};
            }

            if (parts.size() > 1) {
                serial = parts[1].toUInt(&ok, 10);
                if (!ok) {
                    err << QObject::tr("Invalid YubiKey serial %1").arg(parts[1]) << Qt::endl;
                    return {};
                }
            }

            QObject::connect(YubiKey::instance(), &YubiKey::userInteractionRequest, [&] {
                err << QObject::tr("Please present or touch your YubiKey to continue.") << "\n\n" << Qt::flush;
            });

            auto key = QSharedPointer<ChallengeResponseKey>(new ChallengeResponseKey({serial, slot}));
            compositeKey->addChallengeResponseKey(key);

            YubiKey::instance()->findValidKeys();
        }
#else
        Q_UNUSED(yubiKeySlot);
#endif // WITH_XC_YUBIKEY

        auto db = QSharedPointer<Database>::create();
        QString error;
        if (db->open(databaseFilename, compositeKey, &error)) {
            return db;
        } else {
            err << error << Qt::endl;
            return {};
        }
    }

    /**
     * Read a user password from STDIN or return a password previously
     * set by \link setNextPassword().
     *
     * @return the password
     */
    QString getPassword(bool quiet)
    {
#ifdef __AFL_COMPILER
        // Fuzz test build takes password from environment variable to
        // allow non-interactive operation
        const auto env = getenv("KEYPASSXC_AFL_PASSWORD");
        return env ? env : "";
#else
        auto& in = STDIN;
        auto& out = quiet ? DEVNULL : STDERR;

        setStdinEcho(false);
        QString line = in.readLine();
        setStdinEcho(true);
        out << Qt::endl;

        return line;
#endif // __AFL_COMPILER
    }

    /**
     * Read optional password from stdin.
     *
     * @return Pointer to the PasswordKey or null if passwordkey is skipped
     *         by user
     */
    QSharedPointer<PasswordKey> getConfirmedPassword()
    {
        auto& err = STDERR;
        auto& in = STDIN;

        QSharedPointer<PasswordKey> passwordKey;

        err << QObject::tr("Enter password to encrypt database (optional): ");
        err.flush();
        auto password = Utils::getPassword();

        if (password.isEmpty()) {
            err << QObject::tr("Do you want to create a database with an empty password? [y/N]: ");
            err.flush();
            auto ans = in.readLine();
            if (ans.toLower().startsWith("y")) {
                passwordKey = QSharedPointer<PasswordKey>::create("");
            }
            err << Qt::endl;
        } else {
            err << QObject::tr("Repeat password: ");
            err.flush();
            auto repeat = Utils::getPassword();

            if (password == repeat) {
                passwordKey = QSharedPointer<PasswordKey>::create(password);
            } else {
                err << QObject::tr("Error: Passwords do not match.") << Qt::endl;
            }
        }

        return passwordKey;
    }

    /**
     * A valid and running event loop is needed to use the global QClipboard,
     * so we need to use this from the CLI.
     */
    int clipText(const QString& text)
    {
        auto& err = STDERR;

        // List of programs and their arguments
        QList<QPair<QString, QString>> clipPrograms;

#ifdef Q_OS_UNIX
        if (QProcessEnvironment::systemEnvironment().contains("WAYLAND_DISPLAY")) {
            clipPrograms << qMakePair(QStringLiteral("wl-copy"), QStringLiteral("-t text/plain"));
        } else {
            clipPrograms << qMakePair(QStringLiteral("xclip"), QStringLiteral("-selection clipboard -i"));
        }
#endif

#ifdef Q_OS_MACOS
        clipPrograms << qMakePair(QStringLiteral("pbcopy"), QStringLiteral(""));
#endif

#ifdef Q_OS_WIN
        clipPrograms << qMakePair(QStringLiteral("clip"), QStringLiteral(""));
#endif

        if (clipPrograms.isEmpty()) {
            err << QObject::tr("No program defined for clipboard manipulation");
            err.flush();
            return EXIT_FAILURE;
        }

        QStringList failedProgramNames;

        for (const auto& prog : clipPrograms) {
            QScopedPointer<QProcess> clipProcess(new QProcess(nullptr));

            // Skip empty parts, otherwise the program may clip the empty string
            QStringList progArgs = prog.second.split(" ", Qt::SkipEmptyParts);

            clipProcess->start(prog.first, progArgs);
            clipProcess->waitForStarted();

            if (clipProcess->state() != QProcess::Running) {
                failedProgramNames.append(prog.first);
                continue;
            }

#ifdef Q_OS_WIN
            // Windows clip command only understands Unicode written as UTF-16
            auto data = QByteArray::fromRawData(reinterpret_cast<const char*>(text.utf16()), text.size() * 2);
            if (clipProcess->write(data) == -1) {
#else
            // Other platforms understand UTF-8
            if (clipProcess->write(text.toUtf8()) == -1) {
#endif
                qWarning("Unable to write to process : %s", qPrintable(clipProcess->errorString()));
            }
            clipProcess->waitForBytesWritten();
            clipProcess->closeWriteChannel();
            clipProcess->waitForFinished();

            if (clipProcess->exitCode() == EXIT_SUCCESS) {
                return EXIT_SUCCESS;
            } else {
                failedProgramNames.append(prog.first);
            }
        }

        // No clipping program worked
        err << QObject::tr("All clipping programs failed. Tried %1\n").arg(failedProgramNames.join(", "));
        err.flush();
        return EXIT_FAILURE;
    }

    /**
     * Splits the given QString into a QString list. For example:
     *
     * "hello world" -> ["hello", "world"]
     * "hello    world" -> ["hello", "world"]
     * "hello\\ world" -> ["hello world"] (i.e. backslash is an escape character
     * "\"hello world\"" -> ["hello world"]
     */
    QStringList splitCommandString(const QString& command)
    {
        QStringList result;

        bool insideQuotes = false;
        QString cur;
        for (int i = 0; i < command.size(); ++i) {
            QChar c = command[i];
            if (c == '\\' && i < command.size() - 1) {
                cur.append(command[i + 1]);
                ++i;
            } else if (!insideQuotes && (c == ' ' || c == '\t')) {
                if (!cur.isEmpty()) {
                    result.append(cur);
                    cur.clear();
                }
            } else if (c == '"' && (insideQuotes || i == 0 || command[i - 1].isSpace())) {
                insideQuotes = !insideQuotes;
            } else {
                cur.append(c);
            }
        }

        if (!cur.isEmpty()) {
            result.append(cur);
        }

        return result;
    }

    QString getTopLevelField(const Entry* entry, const QString& fieldName)
    {
        if (fieldName == UuidFieldName) {
            return entry->uuid().toString();
        }
        if (fieldName == TagsFieldName) {
            return entry->tags();
        }
        return "";
    }

    QStringList findAttributes(const EntryAttributes& attributes, const QString& name)
    {
        QStringList result;
        if (attributes.hasKey(name)) {
            result.append(name);
            return result;
        }

        for (const QString& key : attributes.keys()) {
            if (key.compare(name, Qt::CaseSensitivity::CaseInsensitive) == 0) {
                result.append(key);
            }
        }

        return result;
    }

    /**
     * Load a key file from disk. When the path specified does not exist a
     * new file will be generated. No folders will be generated so the parent
     * folder of the specified file needs to exist
     *
     * If the key file cannot be loaded or created the function will fail.
     *
     * @param path Path to the key file to be loaded
     * @param fileKey Resulting fileKey
     * @return true if the key file was loaded successfully
     */
    bool loadFileKey(const QString& path, QSharedPointer<FileKey>& fileKey)
    {
        auto& err = Utils::STDERR;
        QString error;
        fileKey = QSharedPointer<FileKey>(new FileKey());

        if (!QFileInfo::exists(path)) {
            fileKey->create(path, &error);

            if (!error.isEmpty()) {
                err << QObject::tr("Creating KeyFile %1 failed: %2").arg(path, error) << Qt::endl;
                return false;
            }
        }

        if (!fileKey->load(path, &error)) {
            err << QObject::tr("Loading KeyFile %1 failed: %2").arg(path, error) << Qt::endl;
            return false;
        }

        return true;
    }
} // namespace Utils
