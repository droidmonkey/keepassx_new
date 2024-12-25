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

#include <QCommandLineParser>
#include <QFileInfo>

#include "Command.h"
#include "Open.h"
#include "TextStream.h"
#include "Utils.h"
#include "config-keepassx.h"
#include "core/Bootstrap.h"
#include "core/Config.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "crypto/Crypto.h"

#if defined(WITH_ASAN) && defined(WITH_LSAN)
#include <sanitizer/lsan_interface.h>
#endif

#if defined(USE_READLINE)
#include <readline/history.h>
#include <readline/readline.h>
#endif

class LineReader
{
public:
    virtual ~LineReader() = default;
    virtual QString readLine(QString prompt) = 0;
    virtual bool isFinished() = 0;
};

class SimpleLineReader : public LineReader
{
public:
    SimpleLineReader()
        : inStream(stdin, QIODevice::ReadOnly)
        , outStream(stdout, QIODevice::WriteOnly)
        , finished(false)
    {
    }

    QString readLine(QString prompt) override
    {
        outStream << prompt;
        outStream.flush();
        QString result = inStream.readLine();
        if (result.isNull()) {
            finished = true;
        }
        return result;
    }

    bool isFinished() override
    {
        return finished;
    }

private:
    TextStream inStream;
    TextStream outStream;
    bool finished;
};

#if defined(USE_READLINE)
class ReadlineLineReader : public LineReader
{
public:
    ReadlineLineReader()
        : finished(false)
    {
    }

    QString readLine(QString prompt) override
    {
        char* result = readline(prompt.toStdString().c_str());
        if (!result) {
            finished = true;
            return {};
        }
        add_history(result);
        QString qstr(result);
        free(result);
        return qstr;
    }

    bool isFinished() override
    {
        return finished;
    }

private:
    bool finished;
};
#endif

int enterInteractiveMode(const QStringList& arguments)
{
    auto& err = Utils::STDERR;
    // Replace command list with interactive version
    Commands::setupCommands(true);

    Open openCmd;
    QStringList openArgs(arguments);
    openArgs.removeFirst();
    if (openCmd.execute(openArgs) != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    };

    QScopedPointer<LineReader> reader;
#if defined(USE_READLINE)
    reader.reset(new ReadlineLineReader());
#else
    reader.reset(new SimpleLineReader());
#endif

    QSharedPointer<Database> currentDatabase(openCmd.currentDatabase);

    QString command;
    while (true) {
        QString prompt;
        if (currentDatabase) {
            prompt += currentDatabase->metadata()->name();
            if (prompt.isEmpty()) {
                prompt += QFileInfo(currentDatabase->filePath()).fileName();
            }
        }
        prompt += "> ";
        command = reader->readLine(prompt);
        if (reader->isFinished()) {
            break;
        }

        QStringList args = Utils::splitCommandString(command);
        if (args.empty()) {
            continue;
        }

        auto cmd = Commands::getCommand(args[0]);
        if (!cmd) {
            err << QObject::tr("Unknown command %1").arg(args[0]) << Qt::endl;
            continue;
        } else if (cmd->name == "quit" || cmd->name == "exit") {
            break;
        }

        cmd->currentDatabase.swap(currentDatabase);
        cmd->execute(args);
        currentDatabase.swap(cmd->currentDatabase);
    }

    if (currentDatabase) {
        currentDatabase->releaseData();
    }

    return EXIT_SUCCESS;
}

int main(int argc, char** argv)
{
    if (!Crypto::init()) {
        qWarning("Fatal error while testing the cryptographic functions:\n%s", qPrintable(Crypto::errorString()));
        return EXIT_FAILURE;
    }

    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(KEEPASSXC_VERSION);
    // Cleanup code pages after cli exits
    QObject::connect(&app, &QCoreApplication::destroyed, &app, [] { Utils::resetTextStreams(); });

    Bootstrap::bootstrap(config()->get(Config::GUI_Language).toString());
    Utils::setDefaultTextStreams();
    Commands::setupCommands(false);

    auto& out = Utils::STDOUT;
    auto& err = Utils::STDERR;

    QStringList arguments;
    for (int i = 0; i < argc; ++i) {
        arguments << QString(argv[i]);
    }
    QCommandLineParser parser;

    QString description("KeePassXC command line interface.");
    description = description.append(QObject::tr("\n\nAvailable commands:\n"));
    for (auto& command : Commands::getCommands()) {
        description = description.append(command->getDescriptionLine());
    }
    parser.setApplicationDescription(description);

    parser.addPositionalArgument("command", QObject::tr("Name of the command to execute."));

    QCommandLineOption debugInfoOption(QStringList() << "debug-info", QObject::tr("Displays debugging information."));
    parser.addOption(debugInfoOption);
    parser.addHelpOption();
    parser.addVersionOption();
    // TODO : use the setOptionsAfterPositionalArgumentsMode (Qt 5.6) function
    // when available. Until then, options passed to sub-commands won't be
    // recognized by this parser.
    parser.parse(arguments);

    if (parser.positionalArguments().empty()) {
        if (parser.isSet("version")) {
            // Switch to parser.showVersion() when available (QT 5.4).
            out << KEEPASSXC_VERSION << Qt::endl;
            return EXIT_SUCCESS;
        }

        if (parser.isSet(debugInfoOption)) {
            QString debugInfo = Tools::debugInfo().append("\n").append(Crypto::debugInfo());
            out << debugInfo << Qt::endl;
            return EXIT_SUCCESS;
        }
        // showHelp exits the application immediately.
        parser.showHelp();
    }

    QString commandName = parser.positionalArguments().at(0);
    if (commandName == "open") {
        return enterInteractiveMode(arguments);
    }

    auto command = Commands::getCommand(commandName);
    if (!command) {
        err << QObject::tr("Invalid command %1.").arg(commandName) << Qt::endl;
        err << parser.helpText();
        return EXIT_FAILURE;
    }

    // Removing the first argument (keepassxc).
    arguments.removeFirst();
    int exitCode = command->execute(arguments);

    if (command->currentDatabase) {
        command->currentDatabase.reset();
    }

#if defined(WITH_ASAN) && defined(WITH_LSAN)
    // do leak check here to prevent massive tail of end-of-process leak errors from third-party libraries
    __lsan_do_leak_check();
    __lsan_disable();
#endif

    return exitCode;
}
