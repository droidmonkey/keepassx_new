/*
 *  Copyright (C) 2016 Enrico Mariotti <enricomariotti@yahoo.it>
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#include "CsvImportWidget.h"
#include "ui_CsvImportWidget.h"

#include "core/Clock.h"
#include "core/Database.h"
#include "core/Group.h"
#include "core/Totp.h"
#include "format/CsvParser.h"
#include "format/KeePass2Writer.h"
#include "gui/MessageBox.h"
#include "gui/csvImport/CsvParserModel.h"

#include <QStringListModel>

namespace
{
    // Extract group names from nested path and return the last group created
    Group* createGroupStructure(Database* db, const QString& groupPath)
    {
        auto group = db->rootGroup();
        if (!group || groupPath.isEmpty()) {
            return group;
        }

        auto nameList = groupPath.split("/", Qt::SkipEmptyParts);
        // Skip over first group name if root
        if (nameList.first().compare("root", Qt::CaseInsensitive) == 0) {
            nameList.removeFirst();
        }

        for (const auto& name : qAsConst(nameList)) {
            auto child = group->findChildByName(name);
            if (!child) {
                auto newGroup = new Group();
                newGroup->setUuid(QUuid::createUuid());
                newGroup->setName(name);
                newGroup->setParent(group);
                group = newGroup;
            } else {
                group = child;
            }
        }
        return group;
    }
} // namespace

CsvImportWidget::CsvImportWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::CsvImportWidget())
    , m_parserModel(new CsvParserModel(this))
    , m_comboModel(new QStringListModel(this))
{
    m_ui->setupUi(this);

    m_ui->tableViewFields->setSelectionMode(QAbstractItemView::NoSelection);
    m_ui->tableViewFields->setFocusPolicy(Qt::NoFocus);

    m_columnHeader << QObject::tr("Group") << QObject::tr("Title") << QObject::tr("Username") << QObject::tr("Password")
                   << QObject::tr("URL") << QObject::tr("Notes") << QObject::tr("TOTP") << QObject::tr("Icon")
                   << QObject::tr("Last Modified") << QObject::tr("Created");

    m_fieldSeparatorList << "," << ";" << "-" << ":" << "." << "\t";

    m_combos << m_ui->groupCombo << m_ui->titleCombo << m_ui->usernameCombo << m_ui->passwordCombo << m_ui->urlCombo
             << m_ui->notesCombo << m_ui->totpCombo << m_ui->iconCombo << m_ui->lastModifiedCombo << m_ui->createdCombo;

    for (auto combo : m_combos) {
        combo->setModel(m_comboModel);
        connect(combo, SIGNAL(currentIndexChanged(int)), SLOT(comboChanged(int)));
    }

    m_parserModel->setHeaderLabels(m_columnHeader);
    m_ui->tableViewFields->setModel(m_parserModel);

    connect(m_ui->spinBoxSkip, SIGNAL(valueChanged(int)), SLOT(skippedChanged(int)));
    connect(m_ui->comboBoxCodec, SIGNAL(currentIndexChanged(int)), SLOT(parse()));
    connect(m_ui->comboBoxTextQualifier, SIGNAL(currentIndexChanged(int)), SLOT(parse()));
    connect(m_ui->comboBoxComment, SIGNAL(currentIndexChanged(int)), SLOT(parse()));
    connect(m_ui->comboBoxFieldSeparator, SIGNAL(currentIndexChanged(int)), SLOT(parse()));
    connect(m_ui->checkBoxBackslash, SIGNAL(toggled(bool)), SLOT(parse()));
    connect(m_ui->checkBoxFieldNames, SIGNAL(toggled(bool)), SLOT(updatePreview()));
}

void CsvImportWidget::comboChanged(int index)
{
    // this line is the one that actually updates GUI table
    m_parserModel->mapColumns(index - 1, m_combos.indexOf(qobject_cast<QComboBox*>(sender())));
    updateTableview();
}

void CsvImportWidget::skippedChanged(int rows)
{
    m_parserModel->setSkippedRows(rows);
    updateTableview();
}

CsvImportWidget::~CsvImportWidget() = default;

void CsvImportWidget::configParser()
{
    auto parser = m_parserModel->parser();
    parser->setBackslashSyntax(m_ui->checkBoxBackslash->isChecked());
    parser->setComment(m_ui->comboBoxComment->currentText().at(0));
    parser->setTextQualifier(m_ui->comboBoxTextQualifier->currentText().at(0));
    parser->setCodec(m_ui->comboBoxCodec->currentText());
    parser->setFieldSeparator(m_fieldSeparatorList.at(m_ui->comboBoxFieldSeparator->currentIndex()).at(0));
}

void CsvImportWidget::updateTableview()
{
    if (!m_buildingPreview) {
        m_ui->tableViewFields->resizeRowsToContents();
        m_ui->tableViewFields->resizeColumnsToContents();

        for (int c = 0; c < m_ui->tableViewFields->horizontalHeader()->count(); ++c) {
            m_ui->tableViewFields->horizontalHeader()->setSectionResizeMode(c, QHeaderView::Stretch);
        }
    }
}

void CsvImportWidget::updatePreview()
{
    m_buildingPreview = true;

    int minSkip = m_ui->checkBoxFieldNames->isChecked() ? 1 : 0;
    m_ui->labelSizeRowsCols->setText(m_parserModel->getFileInfo());
    m_ui->spinBoxSkip->setRange(minSkip, qMax(minSkip, m_parserModel->rowCount() - 1));
    m_ui->spinBoxSkip->setValue(minSkip);

    QStringList csvColumns(tr("Not Present"));
    auto parser = m_parserModel->parser();
    for (int i = 0; i < parser->getCsvCols(); ++i) {
        if (m_ui->checkBoxFieldNames->isChecked()) {
            auto columnName = parser->getCsvTable().at(0).at(i);
            if (columnName.isEmpty()) {
                csvColumns << QString(tr("Column %1").arg(i));
            } else {
                csvColumns << columnName;
            }
        } else {
            csvColumns << QString(tr("Column %1").arg(i));
        }
    }
    m_comboModel->setStringList(csvColumns);

    // Try to match named columns to the combo boxes
    for (int i = 0; i < m_columnHeader.size(); ++i) {
        if (i >= m_combos.size()) {
            // This should not happen, it is a programming error otherwise
            Q_ASSERT(false);
            break;
        }

        bool found = false;
        for (int j = 0; j < csvColumns.size(); ++j) {
            if (m_columnHeader.at(i).compare(csvColumns.at(j), Qt::CaseInsensitive) == 0) {
                m_combos.at(i)->setCurrentIndex(j);
                found = true;
                break;
            }
        }
        // Named column not found, default to "Not Present"
        if (!found) {
            m_combos.at(i)->setCurrentIndex(0);
        }
    }

    m_buildingPreview = false;
    updateTableview();
}

void CsvImportWidget::load(const QString& filename)
{
    m_filename = filename;
    m_parserModel->setFilename(filename);
    parse();
}

void CsvImportWidget::parse()
{
    configParser();
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents();
    bool good = m_parserModel->parse();
    updatePreview();
    QApplication::restoreOverrideCursor();
    if (!good) {
        emit message(tr("Failed to parse CSV file: %1").arg(formatStatusText()));
    }
}

QSharedPointer<Database> CsvImportWidget::buildDatabase()
{
    // Warn if the title column wasn't specified
    if (m_combos[1]->currentIndex() == 0) {
        auto ans = MessageBox::question(
            this,
            tr("No Title Selected"),
            tr("No title column was selected, entries will be hard to tell apart.\nAre you sure you want to import?"),
            MessageBox::Continue | MessageBox::Cancel);
        if (ans == MessageBox::Cancel) {
            return {};
        }
    }

    auto db = QSharedPointer<Database>::create();
    db->rootGroup()->setNotes(tr("Imported from CSV file: %1").arg(m_filename));

    auto rows = m_parserModel->rowCount() - m_parserModel->skippedRows();
    for (int r = 0; r < rows; ++r) {
        auto group = createGroupStructure(db.data(), m_parserModel->data(m_parserModel->index(r, 0)).toString());
        if (!group) {
            continue;
        }

        // Standard entry fields
        auto entry = new Entry();
        entry->setUuid(QUuid::createUuid());
        entry->setGroup(group);
        entry->setTitle(m_parserModel->data(m_parserModel->index(r, 1)).toString());
        entry->setUsername(m_parserModel->data(m_parserModel->index(r, 2)).toString());
        entry->setPassword(m_parserModel->data(m_parserModel->index(r, 3)).toString());
        entry->setUrl(m_parserModel->data(m_parserModel->index(r, 4)).toString());
        entry->setNotes(m_parserModel->data(m_parserModel->index(r, 5)).toString());

        // TOTP
        auto otpString = m_parserModel->data(m_parserModel->index(r, 6));
        if (otpString.isValid() && !otpString.toString().isEmpty()) {
            auto totp = Totp::parseSettings(otpString.toString());
            if (!totp || totp->key.isEmpty()) {
                // Bare secret, use default TOTP settings
                totp = Totp::parseSettings({}, otpString.toString());
            }
            entry->setTotp(totp);
        }

        // Icon
        bool ok;
        int icon = m_parserModel->data(m_parserModel->index(r, 7)).toInt(&ok);
        if (ok) {
            entry->setIcon(icon);
        }

        // Modified Time
        TimeInfo timeInfo;
        if (m_parserModel->data(m_parserModel->index(r, 8)).isValid()) {
            auto datetime = m_parserModel->data(m_parserModel->index(r, 8)).toString();
            if (datetime.contains(QRegularExpression("^\\d+$"))) {
                auto t = datetime.toLongLong();
                if (t <= INT32_MAX) {
                    t *= 1000;
                }
                auto lastModified = Clock::datetimeUtc(t);
                timeInfo.setLastModificationTime(lastModified);
                timeInfo.setLastAccessTime(lastModified);
            } else {
                auto lastModified = QDateTime::fromString(datetime, Qt::ISODate);
                if (lastModified.isValid()) {
                    timeInfo.setLastModificationTime(lastModified);
                    timeInfo.setLastAccessTime(lastModified);
                }
            }
        }
        // Creation Time
        if (m_parserModel->data(m_parserModel->index(r, 9)).isValid()) {
            auto datetime = m_parserModel->data(m_parserModel->index(r, 9)).toString();
            if (datetime.contains(QRegularExpression("^\\d+$"))) {
                auto t = datetime.toLongLong();
                if (t <= INT32_MAX) {
                    t *= 1000;
                }
                timeInfo.setCreationTime(Clock::datetimeUtc(t));
            } else {
                auto created = QDateTime::fromString(datetime, Qt::ISODate);
                if (created.isValid()) {
                    timeInfo.setCreationTime(created);
                }
            }
        }
        entry->setTimeInfo(timeInfo);
    }

    return db;
}

QString CsvImportWidget::formatStatusText() const
{
    QString text = m_parserModel->parser()->getStatus();
    int items = text.count('\n');
    if (items > 2) {
        return text.section('\n', 0, 1).append("\n").append(tr("[%n more message(s) skipped]", "", items - 2));
    }
    if (items == 1) {
        text.append(QString("\n"));
    }
    return text;
}
