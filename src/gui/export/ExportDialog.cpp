/*
 *  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
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

#include "ExportDialog.h"
#include "ui_ExportDialog.h"

#include "gui/FileDialog.h"
#include "gui/HtmlGuiExporter.h"

ExportDialog::ExportDialog(QSharedPointer<const Database> db, DatabaseTabWidget* parent)
    : QDialog(parent)
    , m_ui(new Ui::ExportDialog())
    , m_db(std::move(db))
{
    m_ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);

    connect(m_ui->buttonBox, SIGNAL(rejected()), SLOT(close()));
    connect(m_ui->buttonBox, SIGNAL(accepted()), SLOT(exportDatabase()));

    m_ui->sortingStrategy->addItem(getStrategyName(BY_NAME_ASC), BY_NAME_ASC);
    m_ui->sortingStrategy->addItem(getStrategyName(BY_NAME_DESC), BY_NAME_DESC);
    m_ui->sortingStrategy->addItem(getStrategyName(BY_DATABASE_ORDER), BY_DATABASE_ORDER);

    m_ui->messageWidget->setCloseButtonVisible(false);
    m_ui->messageWidget->setAutoHideTimeout(-1);
    m_ui->messageWidget->showMessage(tr("You are about to export your database to an unencrypted file.\n"
                                        "This will leave your passwords and sensitive information vulnerable!\n"),
                                     MessageWidget::Warning);
}

ExportDialog::~ExportDialog() = default;

QString ExportDialog::getStrategyName(ExportSortingStrategy strategy)
{
    switch (strategy) {
    case ExportSortingStrategy::BY_DATABASE_ORDER:
        return tr("database order");
    case ExportSortingStrategy::BY_NAME_ASC:
        return tr("name (ascending)");
    case ExportSortingStrategy::BY_NAME_DESC:
        return tr("name (descending)");
    }
    return tr("unknown");
}

void ExportDialog::exportDatabase()
{
    auto sortBy = m_ui->sortingStrategy->currentData().toInt();
    bool ascendingOrder = sortBy == ExportSortingStrategy::BY_NAME_ASC;

    const QString fileName = fileDialog()->getSaveFileName(
        this, tr("Export database to HTML file"), FileDialog::getLastDir("html"), tr("HTML file").append(" (*.html)"));
    if (fileName.isEmpty()) {
        return;
    }

    FileDialog::saveLastDir("html", fileName, true);

    HtmlGuiExporter htmlExporter;
    if (!htmlExporter.exportDatabase(
            fileName, m_db, sortBy != ExportSortingStrategy::BY_DATABASE_ORDER, ascendingOrder)) {
        emit exportFailed(htmlExporter.errorString());
        reject();
    }

    accept();
}
