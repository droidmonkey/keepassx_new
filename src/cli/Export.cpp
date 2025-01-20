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

#include "Export.h"

#include "TextStream.h"
#include "Utils.h"
#include "core/Global.h"
#include "format/CsvExporter.h"
#include "format/HtmlExporter.h"

#include <QCommandLineParser>

const QCommandLineOption Export::FormatOption = QCommandLineOption(
    QStringList() << "f" << "format",
    QObject::tr("Format to use when exporting. Available choices are 'xml', 'csv' or 'html'. Defaults to 'xml'."),
    QStringLiteral("xml|csv|html"));

Export::Export()
{
    name = QStringLiteral("export");
    options.append(Export::FormatOption);
    description = QObject::tr("Exports the content of a database to standard output in the specified format.");
}

int Export::executeWithDatabase(QSharedPointer<Database> database, QSharedPointer<QCommandLineParser> parser)
{
    TextStream out(Utils::STDOUT.device());
    auto& err = Utils::STDERR;

    QString format = parser->value(Export::FormatOption);
    if (format.isEmpty() || format.startsWith(QStringLiteral("xml"), Qt::CaseInsensitive)) {
        QByteArray xmlData;
        QString errorMessage;
        if (!database->extract(xmlData, &errorMessage)) {
            err << QObject::tr("Unable to export database to XML: %1").arg(errorMessage) << Qt::endl;
            return EXIT_FAILURE;
        }
        out.write(xmlData.constData());
    } else if (format.startsWith(QStringLiteral("csv"), Qt::CaseInsensitive)) {
        CsvExporter csvExporter;
        out << csvExporter.exportDatabase(database);
    } else if (format.startsWith(QStringLiteral("html"), Qt::CaseInsensitive)) {
        HtmlExporter htmlExporter;
        out << htmlExporter.exportDatabase(database);
    } else {
        err << QObject::tr("Unsupported format %1").arg(format) << Qt::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
