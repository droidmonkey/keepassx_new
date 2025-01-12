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

#include "ReportsWidgetHealthcheck.h"
#include "ui_ReportsWidgetHealthcheck.h"

#include "core/AsyncTask.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/PasswordHealth.h"
#include "gui/GuiTools.h"
#include "gui/Icons.h"
#include "gui/styles/StateColorPalette.h"

#include <QMenu>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

namespace
{
    class Health
    {
    public:
        struct Item
        {
            QPointer<Group> group;
            QPointer<Entry> entry;
            QSharedPointer<PasswordHealth> health;
            bool exclude = false;

            Item(Group* g, Entry* e, QSharedPointer<PasswordHealth> h)
                : group(g)
                , entry(e)
                , health(h)
                , exclude(e->excludeFromReports())
            {
            }

            bool operator<(const Item& rhs) const
            {
                return health->score() < rhs.health->score();
            }
        };

        explicit Health(QSharedPointer<Database>);

        const QList<QSharedPointer<Item>>& items() const
        {
            return m_items;
        }

        bool anyExcludedEntries() const
        {
            return m_anyExcludedEntries;
        }

    private:
        QSharedPointer<Database> m_db;
        HealthChecker m_checker;
        QList<QSharedPointer<Item>> m_items;
        bool m_anyExcludedEntries = false;
    };

    class ReportSortProxyModel : public QSortFilterProxyModel
    {
    public:
        ReportSortProxyModel(QObject* parent)
            : QSortFilterProxyModel(parent){};
        ~ReportSortProxyModel() override = default;

    protected:
        bool lessThan(const QModelIndex& left, const QModelIndex& right) const override
        {
            // Check if the display data is a number, convert and compare if so
            bool ok = false;
            int leftInt = sourceModel()->data(left).toString().toInt(&ok);
            if (ok) {
                return leftInt < sourceModel()->data(right).toString().toInt();
            }
            // Otherwise use default sorting
            return QSortFilterProxyModel::lessThan(left, right);
        }
    };
} // namespace

Health::Health(QSharedPointer<Database> db)
    : m_db(db)
    , m_checker(db)
{
    for (auto group : db->rootGroup()->groupsRecursive(true)) {
        // Skip recycle bin
        if (group->isRecycled()) {
            continue;
        }

        for (auto entry : group->entries()) {
            if (entry->isRecycled()) {
                continue;
            }

            // Skip entries with empty password
            if (entry->password().isEmpty()) {
                continue;
            }

            // Evaluate this entry
            const auto item = QSharedPointer<Item>(new Item(group, entry, m_checker.evaluate(entry)));
            if (item->exclude) {
                m_anyExcludedEntries = true;
            }

            // Add entry if its password isn't at least "good"
            if (item->health->quality() < PasswordHealth::Quality::Good) {
                m_items.append(item);
            }
        }
    }

    // Sort the result so that the worst passwords (least score)
    // are at the top
    std::sort(m_items.begin(), m_items.end(), [](QSharedPointer<Item> x, QSharedPointer<Item> y) { return *x < *y; });
}

ReportsWidgetHealthcheck::ReportsWidgetHealthcheck(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ReportsWidgetHealthcheck())
    , m_referencesModel(new QStandardItemModel(this))
    , m_modelProxy(new ReportSortProxyModel(this))
{
    m_ui->setupUi(this);

    m_modelProxy->setSourceModel(m_referencesModel.data());
    m_modelProxy->setSortLocaleAware(true);
    m_ui->healthcheckTableView->setModel(m_modelProxy.data());
    m_ui->healthcheckTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_ui->healthcheckTableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(m_ui->healthcheckTableView, SIGNAL(customContextMenuRequested(QPoint)), SLOT(customMenuRequested(QPoint)));
    connect(m_ui->healthcheckTableView, SIGNAL(doubleClicked(QModelIndex)), SLOT(emitEntryActivated(QModelIndex)));
    connect(m_ui->showExcluded, SIGNAL(stateChanged(int)), this, SLOT(calculateHealth()));
    connect(m_ui->showExpired, SIGNAL(stateChanged(int)), this, SLOT(calculateHealth()));

    new QShortcut(Qt::Key_Delete, this, SLOT(deleteSelectedEntries()));
}

ReportsWidgetHealthcheck::~ReportsWidgetHealthcheck() = default;

void ReportsWidgetHealthcheck::addHealthRow(QSharedPointer<PasswordHealth> health,
                                            Group* group,
                                            Entry* entry,
                                            bool excluded)
{
    QString tip;
    QString iconName = "lock-question";
    QColor qualityColor;
    StateColorPalette statePalette;
    const auto quality = health->quality();
    switch (quality) {
    case PasswordHealth::Quality::Bad:
        tip = tr("Bad — password must be changed");
        iconName = "lock-open-alert";
        qualityColor = statePalette.color(StateColorPalette::HealthCritical);
        break;
    case PasswordHealth::Quality::Poor:
        tip = tr("Poor — password should be changed");
        iconName = "lock-open-alert";
        qualityColor = statePalette.color(StateColorPalette::HealthBad);
        break;

    case PasswordHealth::Quality::Weak:
        tip = tr("Weak — consider changing the password");
        iconName = "lock-open";
        qualityColor = statePalette.color(StateColorPalette::HealthWeak);
        break;

    case PasswordHealth::Quality::Good:
    case PasswordHealth::Quality::Excellent:
        iconName = "lock";
        qualityColor = statePalette.color(StateColorPalette::HealthOk);
        break;
    }

    auto title = entry->title();
    if (excluded) {
        title.append(tr(" (Excluded)"));
    }
    if (entry->isExpired()) {
        title.append(tr(" (Expired)"));
    }

    auto row = QList<QStandardItem*>();
    row << new QStandardItem(Icons::instance()->icon(iconName, true, qualityColor), "");
    row << new QStandardItem(Icons::entryIconPixmap(entry), title);
    row << new QStandardItem(Icons::groupIconPixmap(group), group->hierarchy().join("/"));
    row << new QStandardItem(QString::number(health->score()));
    row << new QStandardItem(health->scoreReason());

    // Set background color of first column according to password quality.
    // Set the same as foreground color so the description is usually
    // invisible, it's just for screen readers etc.
    QBrush brush(qualityColor);
    row[0]->setForeground(brush);

    // Set tooltips
    row[0]->setToolTip(tip);
    if (excluded) {
        row[1]->setToolTip(tr("This entry is being excluded from reports"));
    }
    row[4]->setToolTip(health->scoreDetails());

    // Store entry pointer per table row (used in double click handler)
    m_referencesModel->appendRow(row);
    m_rowToEntry.append({group, entry});
}

void ReportsWidgetHealthcheck::loadSettings(QSharedPointer<Database> db)
{
    m_db = std::move(db);
    m_healthCalculated = false;
    m_referencesModel->clear();
    m_rowToEntry.clear();

    auto row = QList<QStandardItem*>();
    row << new QStandardItem(tr("Please wait, health data is being calculated…"));
    m_referencesModel->appendRow(row);
}

void ReportsWidgetHealthcheck::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (!m_healthCalculated) {
        // Perform stats calculation on next event loop to allow widget to appear
        m_healthCalculated = true;
        QTimer::singleShot(0, this, SLOT(calculateHealth()));
    }
}

void ReportsWidgetHealthcheck::calculateHealth()
{
    m_referencesModel->clear();

    // Perform the health check
    const QScopedPointer<Health> health(AsyncTask::runAndWaitForFuture([this] { return new Health(m_db); }));

    // Display the entries
    m_rowToEntry.clear();
    for (const auto& item : health->items()) {
        // Check if the entry should be displayed
        if ((!m_ui->showExcluded->isChecked() && item->exclude)
            || (!m_ui->showExpired->isChecked() && item->entry->isExpired())) {
            continue;
        }

        // Show the entry in the report
        addHealthRow(item->health, item->group, item->entry, item->exclude);
    }

    // Set the table header
    if (m_referencesModel->rowCount() == 0) {
        m_referencesModel->setHorizontalHeaderLabels(QStringList() << tr("Congratulations, everything is healthy!"));
    } else {
        m_referencesModel->setHorizontalHeaderLabels(QStringList() << tr("") << tr("Title") << tr("Path") << tr("Score")
                                                                   << tr("Reason"));
        m_ui->healthcheckTableView->sortByColumn(0, Qt::AscendingOrder);
    }

    m_ui->healthcheckTableView->resizeColumnsToContents();
    m_ui->healthcheckTableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);

    // Only show the "show excluded" checkbox if there are any excluded entries in the database
    m_ui->showExcluded->setVisible(health->anyExcludedEntries());
}

void ReportsWidgetHealthcheck::emitEntryActivated(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    auto mappedIndex = m_modelProxy->mapToSource(index);
    const auto row = m_rowToEntry[mappedIndex.row()];
    const auto group = row.first;
    const auto entry = row.second;
    if (group && entry) {
        emit entryActivated(const_cast<Entry*>(entry));
    }
}

void ReportsWidgetHealthcheck::customMenuRequested(QPoint pos)
{
    auto selected = m_ui->healthcheckTableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }

    // Create the context menu
    const auto menu = new QMenu(this);

    // Create the "edit entry" menu item (only if 1 row is selected)
    if (selected.size() == 1) {
        const auto edit = new QAction(icons()->icon("entry-edit"), tr("Edit Entry…"), this);
        menu->addAction(edit);
        connect(edit, &QAction::triggered, edit, [this, selected] {
            auto row = m_modelProxy->mapToSource(selected[0]).row();
            auto entry = m_rowToEntry[row].second;
            emit entryActivated(entry);
        });
    }

    // Create the "Expire entry" menu item
    const auto expEntry = new QAction(icons()->icon("entry-expire"), tr("Expire Entry(s)…", "", selected.size()), this);
    menu->addAction(expEntry);
    connect(expEntry, &QAction::triggered, this, &ReportsWidgetHealthcheck::expireSelectedEntries);

    // Create the "delete entry" menu item
    const auto delEntry = new QAction(icons()->icon("entry-delete"), tr("Delete Entry(s)…", "", selected.size()), this);
    menu->addAction(delEntry);
    connect(delEntry, &QAction::triggered, this, &ReportsWidgetHealthcheck::deleteSelectedEntries);

    // Create the "exclude from reports" menu item
    const auto exclude = new QAction(icons()->icon("reports-exclude"), tr("Exclude from reports"), this);

    bool isExcluded = false;
    for (auto index : selected) {
        auto row = m_modelProxy->mapToSource(index).row();
        auto entry = m_rowToEntry[row].second;
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
            auto entry = m_rowToEntry[row].second;
            if (entry) {
                entry->setExcludeFromReports(state);
            }
        }
        calculateHealth();
    });

    // Show the context menu
    menu->popup(m_ui->healthcheckTableView->viewport()->mapToGlobal(pos));
}

void ReportsWidgetHealthcheck::saveSettings()
{
    // nothing to do - the tab is passive
}

QList<Entry*> ReportsWidgetHealthcheck::getSelectedEntries()
{
    QList<Entry*> selectedEntries;
    for (auto index : m_ui->healthcheckTableView->selectionModel()->selectedRows()) {
        auto row = m_modelProxy->mapToSource(index).row();
        auto entry = m_rowToEntry[row].second;
        if (entry) {
            selectedEntries << entry;
        }
    }
    return selectedEntries;
}

void ReportsWidgetHealthcheck::expireSelectedEntries()
{
    for (auto entry : getSelectedEntries()) {
        entry->expireNow();
    }

    calculateHealth();
}

void ReportsWidgetHealthcheck::deleteSelectedEntries()
{
    QList<Entry*> selectedEntries = getSelectedEntries();
    bool permanent = !m_db->metadata()->recycleBinEnabled();
    if (GuiTools::confirmDeleteEntries(this, selectedEntries, permanent)) {
        GuiTools::deleteEntriesResolveReferences(this, selectedEntries, permanent);
    }

    calculateHealth();
}
