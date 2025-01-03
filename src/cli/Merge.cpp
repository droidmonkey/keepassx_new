/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
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

#include "Merge.h"

#include "Utils.h"
#include "core/Global.h"
#include "core/Merger.h"

#include <QCommandLineParser>

const QCommandLineOption Merge::SameCredentialsOption =
    QCommandLineOption(QStringList() << "s" << "same-credentials",
                       QObject::tr("Use the same credentials for both database files."));

const QCommandLineOption Merge::KeyFileFromOption =
    QCommandLineOption(QStringList() << "key-file-from",
                       QObject::tr("Key file of the database to merge from."),
                       QObject::tr("path"));

const QCommandLineOption Merge::NoPasswordFromOption =
    QCommandLineOption(QStringList() << "no-password-from",
                       QObject::tr("Deactivate password key for the database to merge from."));

const QCommandLineOption Merge::DryRunOption =
    QCommandLineOption(QStringList() << "d" << "dry-run",
                       QObject::tr("Only print the changes detected by the merge operation."));

const QCommandLineOption Merge::YubiKeyFromOption(QStringList() << "yubikey-from",
                                                  QObject::tr("Yubikey slot for the second database."),
                                                  QObject::tr("slot"));
Merge::Merge()
{
    name = QString("merge");
    description = QObject::tr("Merge two databases.");
    options.append(Merge::SameCredentialsOption);
    options.append(Merge::KeyFileFromOption);
    options.append(Merge::NoPasswordFromOption);
    options.append(Merge::DryRunOption);
    options.append(Merge::YubiKeyFromOption);

    positionalArguments.append({QString("database2"), QObject::tr("Path of the database to merge from."), QString("")});
}

int Merge::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    auto& out = parser->isSet(Command::QuietOption) ? Utils::DEVNULL : Utils::STDOUT;
    auto& err = Utils::STDERR;

    const QStringList args = parser->positionalArguments();

    auto& toDatabasePath = args.at(0);
    auto& fromDatabasePath = args.at(1);

    QSharedPointer<Database> db2;
    if (!parser->isSet(Merge::SameCredentialsOption)) {
        db2 = Utils::unlockDatabase(fromDatabasePath,
                                    !parser->isSet(Merge::NoPasswordFromOption),
                                    parser->value(Merge::KeyFileFromOption),
                                    parser->value(Merge::YubiKeyFromOption),
                                    parser->isSet(Command::QuietOption));
        if (!db2) {
            return EXIT_FAILURE;
        }
    } else {
        db2 = QSharedPointer<Database>::create();
        QString errorMessage;
        if (!db2->open(fromDatabasePath, database->key(), &errorMessage)) {
            err << QObject::tr("Error reading merge file:\n%1").arg(errorMessage);
            return EXIT_FAILURE;
        }
    }

    Merger merger(db2.data(), database.data());
    QStringList changeList = merger.merge();

    for (auto& mergeChange : changeList) {
        out << "\t" << mergeChange << Qt::endl;
    }

    if (!changeList.isEmpty() && !parser->isSet(Merge::DryRunOption)) {
        QString errorMessage;
        if (!database->save(Database::Atomic, {}, &errorMessage)) {
            err << QObject::tr("Unable to save database to file : %1").arg(errorMessage) << Qt::endl;
            return EXIT_FAILURE;
        }
        out << QObject::tr("Successfully merged %1 into %2.").arg(fromDatabasePath, toDatabasePath) << Qt::endl;
    } else {
        out << QObject::tr("Database was not modified by merge operation.") << Qt::endl;
    }

    return EXIT_SUCCESS;
}
