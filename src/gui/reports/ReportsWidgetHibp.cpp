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

#include "ReportsWidgetHibp.h"
#include "ui_ReportsWidgetHibp.h"

#include "config-keepassx.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "gui/GuiTools.h"
#include "gui/Icons.h"

#include <QMenu>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

#include <algorithm>

namespace
{
    class ReportSortProxyModel : public QSortFilterProxyModel
    {
    public:
        ReportSortProxyModel(QObject* parent)
            : QSortFilterProxyModel(parent){};
        ~ReportSortProxyModel() override = default;

    protected:
        bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
        {
            // Sort count column by user data
            if (left.column() == 2) {
                return sourceModel()->data(left, Qt::UserRole).toInt()
                       < sourceModel()->data(right, Qt::UserRole).toInt();
            }
            // Otherwise use default sorting
            return QSortFilterProxyModel::lessThan(left, right);
        }
    };
} // namespace

ReportsWidgetHibp::ReportsWidgetHibp(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ReportsWidgetHibp())
    , m_referencesModel(new QStandardItemModel(this))
    , m_modelProxy(new ReportSortProxyModel(this))
{
    m_ui->setupUi(this);

    m_modelProxy->setSourceModel(m_referencesModel.data());
    m_modelProxy->setSortLocaleAware(true);
    m_ui->hibpTableView->setModel(m_modelProxy.data());
    m_ui->hibpTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_ui->hibpTableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(m_ui->hibpTableView, SIGNAL(doubleClicked(QModelIndex)), SLOT(emitEntryActivated(QModelIndex)));
    connect(m_ui->hibpTableView, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customMenuRequested(QPoint)));
    connect(m_ui->showKnownBadCheckBox, SIGNAL(stateChanged(int)), this, SLOT(makeHibpTable()));
#ifdef WITH_XC_NETWORKING
    connect(&m_downloader, SIGNAL(hibpResult(QString, int)), SLOT(addHibpResult(QString, int)));
    connect(&m_downloader, SIGNAL(fetchFailed(QString)), SLOT(fetchFailed(QString)));

    connect(m_ui->validationButton, &QPushButton::pressed, [this] { startValidation(); });
#endif

    new QShortcut(Qt::Key_Delete, this, SLOT(deleteSelectedEntries()));
}

ReportsWidgetHibp::~ReportsWidgetHibp() = default;

void ReportsWidgetHibp::loadSettings(QSharedPointer<Database> db)
{
    // Re-initialize
    m_db = std::move(db);
    m_referencesModel->clear();
    m_pwndPasswords.clear();
    m_error.clear();
    m_rowToEntry.clear();
    m_editedEntry = nullptr;
#ifdef WITH_XC_NETWORKING
    m_ui->stackedWidget->setCurrentIndex(0);
    m_ui->validationButton->setEnabled(true);
    m_ui->progressBar->hide();
#else
    // Compiled without networking, can't do anything
    m_ui->stackedWidget->setCurrentIndex(2);
#endif
}

/*
 * Fill the table will all entries that have passwords that we've
 * found to have been pwned.
 */
void ReportsWidgetHibp::makeHibpTable()
{
    // Reset the table
    m_referencesModel->clear();
    m_rowToEntry.clear();

    // If there were no findings, display a motivational message
    if (m_pwndPasswords.isEmpty() && m_error.isEmpty()) {
        m_referencesModel->setHorizontalHeaderLabels(QStringList() << tr("Congratulations, no exposed passwords!"));
        m_ui->stackedWidget->setCurrentIndex(1);
        return;
    }

    // Standard header labels for found issues
    m_referencesModel->setHorizontalHeaderLabels(QStringList() << tr("Title") << tr("Path") << tr("Password exposed…"));

    // Search database for passwords that we've found so far
    QList<QPair<Entry*, int>> items;
    for (auto entry : m_db->rootGroup()->entriesRecursive()) {
        if (!entry->isRecycled()) {
            const auto found = m_pwndPasswords.find(entry->password());
            if (found != m_pwndPasswords.end()) {
                items.append({entry, found.value()});
            }
        }
    }

    // Sort descending by the number the password has been exposed
    std::sort(items.begin(), items.end(), [](QPair<Entry*, int>& lhs, QPair<Entry*, int>& rhs) {
        return lhs.second > rhs.second;
    });

    // Display entries that are marked as "known bad"?
    const auto showExcluded = m_ui->showKnownBadCheckBox->isChecked();

    // The colors for table cells
    const auto red = QBrush("red");

    // Build the table
    bool anyExcluded = false;
    for (const auto& item : items) {
        const auto entry = item.first;
        const auto group = entry->group();
        const auto count = item.second;
        auto title = entry->title();

        // Hide entry if excluded unless explicitly requested
        if (entry->excludeFromReports()) {
            anyExcluded = true;
            if (!showExcluded) {
                continue;
            }

            title.append(tr(" (Excluded)"));
        }

        auto row = QList<QStandardItem*>();
        row << new QStandardItem(Icons::entryIconPixmap(entry), title)
            << new QStandardItem(Icons::groupIconPixmap(group), group->hierarchy().join("/"))
            << new QStandardItem(countToText(count));

        if (entry->excludeFromReports()) {
            row[1]->setToolTip(tr("This entry is being excluded from reports"));
        }

        row[2]->setForeground(red);
        row[2]->setData(count, Qt::UserRole);
        m_referencesModel->appendRow(row);

        // Store entry pointer per table row (used in double click handler)
        m_rowToEntry.append(entry);
    }

    // If there was an error, append the error message to the table
    if (!m_error.isEmpty()) {
        auto row = QList<QStandardItem*>();
        row << new QStandardItem(m_error);
        m_referencesModel->appendRow(row);
        row[0]->setForeground(QBrush(QColor("red")));
    }

    // If we're done and everything is good, display a motivational message
#ifdef WITH_XC_NETWORKING
    if (m_downloader.passwordsRemaining() == 0 && m_pwndPasswords.isEmpty() && m_error.isEmpty()) {
        m_referencesModel->clear();
        m_referencesModel->setHorizontalHeaderLabels(QStringList() << tr("Congratulations, no exposed passwords!"));
    }
#endif

    // Show the "show known bad entries" checkbox if there's any known
    // bad entry in the database.
    if (anyExcluded) {
        m_ui->showKnownBadCheckBox->show();
    } else {
        m_ui->showKnownBadCheckBox->hide();
    }

    m_ui->hibpTableView->resizeColumnsToContents();
    m_ui->hibpTableView->sortByColumn(2, Qt::DescendingOrder);

    m_ui->stackedWidget->setCurrentIndex(1);
}

/*
 * Invoked when the downloader has finished checking one password.
 */
void ReportsWidgetHibp::addHibpResult(const QString& password, int count)
{
    // Add the password to the list of our findings if it has been pwned
    if (count > 0) {
        m_pwndPasswords[password] = count;
    }

#ifdef WITH_XC_NETWORKING
    // Update the progress bar
    int remaining = m_downloader.passwordsRemaining();
    if (remaining > 0) {
        m_ui->progressBar->setValue(m_ui->progressBar->maximum() - remaining);
    } else {
        // Finished, remove the progress bar and build the table
        m_ui->progressBar->hide();
        makeHibpTable();
    }
#endif
}

/*
 * Invoked when a query to the HIBP server fails.
 *
 * Displays the table with the current findings.
 */
void ReportsWidgetHibp::fetchFailed(const QString& error)
{
    m_error = error;
    m_ui->progressBar->hide();
    makeHibpTable();
}

/*
 * Add passwords to the downloader and start the actual online validation.
 */
void ReportsWidgetHibp::startValidation()
{
#ifdef WITH_XC_NETWORKING
    // Collect all passwords in the database (unless recycled, and
    // unless empty, and unless marked as "known bad") and submit them
    // to the downloader.
    for (const auto* entry : m_db->rootGroup()->entriesRecursive()) {
        if (!entry->isRecycled() && !entry->password().isEmpty()) {
            m_downloader.add(entry->password());
        }
    }

    // Short circuit if we didn't actually add any passwords
    if (m_downloader.passwordsToValidate() == 0) {
        makeHibpTable();
        return;
    }

    // Store the number of passwords we need to check for the progress bar
    m_ui->progressBar->show();
    m_ui->progressBar->setMaximum(m_downloader.passwordsToValidate());
    m_ui->validationButton->setEnabled(false);

    m_downloader.validate();
#endif
}

/*
 * Convert the number of times a password has been pwned into
 * a display text for the third table column.
 */
QString ReportsWidgetHibp::countToText(int count)
{
    if (count == 1) {
        return tr("once", "Password exposure amount");
    } else if (count <= 10) {
        return tr("up to 10 times", "Password exposure amount");
    } else if (count <= 100) {
        return tr("up to 100 times", "Password exposure amount");
    } else if (count <= 1000) {
        return tr("up to 1000 times", "Password exposure amount");
    } else if (count <= 10000) {
        return tr("up to 10,000 times", "Password exposure amount");
    } else if (count <= 100000) {
        return tr("up to 100,000 times", "Password exposure amount");
    } else if (count <= 1000000) {
        return tr("up to a million times", "Password exposure amount");
    }

    return tr("millions of times", "Password exposure amount");
}

/*
 * Double-click handler
 */
void ReportsWidgetHibp::emitEntryActivated(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    // Find which database entry was double-clicked
    auto mappedIndex = m_modelProxy->mapToSource(index);
    const auto entry = m_rowToEntry[mappedIndex.row()];
    if (entry) {
        // Found it, invoke entry editor
        m_editedEntry = entry;
        m_editedPassword = entry->password();
        m_editedExcluded = entry->excludeFromReports();
        emit entryActivated(const_cast<Entry*>(entry));
    }
}

/*
 * Invoked after "OK" was clicked in the entry editor.
 * Re-validates the edited entry's new password.
 */
void ReportsWidgetHibp::refreshAfterEdit()
{
    // Sanity check
    if (!m_editedEntry) {
        return;
    }

    // No need to re-validate if there was no change that affects
    // the HIBP result (i. e., change to the password or to the
    // "known bad" flag)
    if (m_editedEntry->password() == m_editedPassword && m_editedEntry->excludeFromReports() == m_editedExcluded) {
        // Don't go through HIBP but still rebuild the table, the user might
        // have edited the entry title.
        makeHibpTable();
        return;
    }

    // Remove the previous password from the list of findings
    m_pwndPasswords.remove(m_editedPassword);

    // Validate the new password against HIBP
#ifdef WITH_XC_NETWORKING
    m_downloader.add(m_editedEntry->password());
    m_downloader.validate();
#endif

    m_editedEntry = nullptr;
}

void ReportsWidgetHibp::customMenuRequested(QPoint pos)
{
    auto selected = m_ui->hibpTableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }

    // Create the context menu
    const auto menu = new QMenu(this);

    // Create the "edit entry" menu item if 1 row is selected
    if (selected.size() == 1) {
        const auto edit = new QAction(icons()->icon("entry-edit"), tr("Edit Entry…"), this);
        menu->addAction(edit);
        connect(edit, &QAction::triggered, edit, [this, selected] {
            auto row = m_modelProxy->mapToSource(selected[0]).row();
            auto entry = m_rowToEntry[row];
            emit entryActivated(entry);
        });
    }

    // Create the "Expire entry" menu item
    const auto expEntry = new QAction(icons()->icon("entry-expire"), tr("Expire Entry(s)…", "", selected.size()), this);
    menu->addAction(expEntry);
    connect(expEntry, &QAction::triggered, this, &ReportsWidgetHibp::expireSelectedEntries);

    // Create the "delete entry" menu item
    const auto delEntry = new QAction(icons()->icon("entry-delete"), tr("Delete Entry(s)…", "", selected.size()), this);
    menu->addAction(delEntry);
    connect(delEntry, &QAction::triggered, this, &ReportsWidgetHibp::deleteSelectedEntries);

    // Create the "exclude from reports" menu item
    const auto exclude = new QAction(icons()->icon("reports-exclude"), tr("Exclude from reports"), this);

    bool isExcluded = false;
    for (auto index : selected) {
        auto row = m_modelProxy->mapToSource(index).row();
        auto entry = m_rowToEntry[row];
        if (entry && entry->excludeFromReports()) {
            // If at least one entry is excluded switch to inclusion
            isExcluded = true;
            break;
        }
    }
    exclude->setCheckable(true);
    exclude->setChecked(isExcluded);

    menu->addAction(exclude);
    connect(exclude, &QAction::toggled, exclude, [this, selected](bool state) {
        for (auto index : selected) {
            auto row = m_modelProxy->mapToSource(index).row();
            auto entry = m_rowToEntry[row];
            if (entry) {
                entry->setExcludeFromReports(state);
            }
        }
        makeHibpTable();
    });

    // Show the context menu
    menu->popup(m_ui->hibpTableView->viewport()->mapToGlobal(pos));
}

QList<Entry*> ReportsWidgetHibp::getSelectedEntries()
{
    QList<Entry*> selectedEntries;
    for (auto index : m_ui->hibpTableView->selectionModel()->selectedRows()) {
        auto row = m_modelProxy->mapToSource(index).row();
        auto entry = m_rowToEntry[row];
        if (entry) {
            selectedEntries << entry;
        }
    }
    return selectedEntries;
}

void ReportsWidgetHibp::expireSelectedEntries()
{
    for (auto entry : getSelectedEntries()) {
        entry->expireNow();
    }

    makeHibpTable();
}

void ReportsWidgetHibp::deleteSelectedEntries()
{
    QList<Entry*> selectedEntries = getSelectedEntries();
    bool permanent = !m_db->metadata()->recycleBinEnabled();
    if (GuiTools::confirmDeleteEntries(this, selectedEntries, permanent)) {
        GuiTools::deleteEntriesResolveReferences(this, selectedEntries, permanent);
    }

    makeHibpTable();
}

void ReportsWidgetHibp::saveSettings()
{
    // nothing to do - the tab is passive
}
