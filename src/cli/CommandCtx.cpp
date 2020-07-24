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


#include "CommandCtx.h"

#include "Add.h"
#include "AddGroup.h"
#include "Analyze.h"
#include "Clip.h"
#include "Close.h"
#include "Create.h"
#include "Diceware.h"
#include "Edit.h"
#include "Estimate.h"
#include "Exit.h"
#include "Export.h"
#include "Generate.h"
#include "Help.h"
#include "Import.h"
#include "Info.h"
#include "List.h"
#include "Locate.h"
#include "Merge.h"
#include "Move.h"
#include "Open.h"
#include "Remove.h"
#include "RemoveGroup.h"
#include "Show.h"


template<class Cmd>
void regCmd(QHash<QString, QSharedPointer<Command>>& map)
{
    map.insert(CommandTraits<Cmd>::Name, QSharedPointer<Cmd>::create());
}

void CommandCtx::cmdInit()
{
#define REG_CMD(CMD_T) regCmd<CMD_T>(m_commands)

    REG_CMD(Add);
    REG_CMD(Analyze);
    REG_CMD(Clip);
    REG_CMD(Close);
    REG_CMD(Create);
    REG_CMD(Info);
    REG_CMD(Diceware);
    REG_CMD(Edit);
    REG_CMD(Estimate);
    REG_CMD(Generate);
    REG_CMD(Help);
    REG_CMD(Locate);
    REG_CMD(List);
    REG_CMD(Merge);
    REG_CMD(AddGroup);
    REG_CMD(Move);
    REG_CMD(Open);
    REG_CMD(Remove);
    REG_CMD(RemoveGroup);
    REG_CMD(Show);
    // TODO_vanda interactive
    // TODO_vanda alias 'quit'
    REG_CMD(Exit);
    // TODO_vanda non-interactive
    REG_CMD(Export);
    REG_CMD(Import);

#undef REG_CMD
}

int CommandCtx::parseArgs(QCommandLineParser& parser, const QStringList& args)
{
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsPositionalArguments);

    QString description("KeePassXC command line interface.");
    description = description.append(QObject::tr("\n\nAvailable commands:\n"));
    for (const auto& command : m_commands)
        description = description.append(command->getDescriptionLine());
    parser.setApplicationDescription(description);
    parser.addPositionalArgument("command", QObject::tr("Name of the command to execute."));
    const QCommandLineOption debugInfo("debug-info", QObject::tr("Displays debugging information."));
    parser.addOption(debugInfo);
    const QCommandLineOption help = parser.addHelpOption();
    const QCommandLineOption version = parser.addVersionOption();

    if (!parser.parse(args)) {
        logError(parser.errorText());
        return EXIT_FAILURE;
    }

    if (parser.isSet(version)) {
        m_runmode = Runmode::Version;
        return EXIT_SUCCESS;
    }
    if (parser.isSet(debugInfo)) {
        m_runmode = Runmode::DebugInfo;
        return EXIT_SUCCESS;
    }
    if (parser.isSet(help)) {
        m_runmode = Runmode::Help;
        return EXIT_SUCCESS;
    }

    if (parser.positionalArguments().empty()) {
        logError(QObject::tr("Argument 'command' missing.\n").append(parser.helpText()));
        return EXIT_FAILURE;
    }
    m_runmode = Runmode::SingleCmd;
    return EXIT_SUCCESS;
}
