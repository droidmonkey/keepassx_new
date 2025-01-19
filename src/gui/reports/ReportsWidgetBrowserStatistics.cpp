/*
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

#include "ReportsWidgetBrowserStatistics.h"
#include "ui_ReportsWidgetBrowserStatistics.h"

#include "browser/BrowserService.h"
#include "core/AsyncTask.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "gui/GuiTools.h"
#include "gui/Icons.h"
#include "gui/styles/StateColorPalette.h"

#include <QJsonDocument>
#include <QMenu>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>

namespace
{
    class BrowserStatistics
    {
    public:
        struct Item
        {
            QPointer<Group> group;
            QPointer<Entry> entry;
            bool hasUrls;
            bool hasSettings;
            bool exclude = false;

            Item(Group* g, Entry* e, bool hU, bool hS)
                : group(g)
                , entry(e)
                , hasUrls(hU)
                , hasSettings(hS)
                , exclude(e->excludeFromReports())
            {
            }
        };

        explicit BrowserStatistics(QSharedPointer<Database>);

        const QList<QSharedPointer<Item>>& items() const
        {
            return m_items;
        }

    private:
        QSharedPointer<Database> m_db;
        QList<QSharedPointer<Item>> m_items;
    };
} // namespace

BrowserStatistics::BrowserStatistics(QSharedPointer<Database> db)
    : m_db(db)
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

            auto hasUrls = !entry->getAllUrls().isEmpty();
            auto hasSettings = entry->customData()->contains(BrowserService::KEEPASSXCBROWSER_NAME);

            const auto item = QSharedPointer<Item>(new Item(group, entry, hasUrls, hasSettings));
            m_items.append(item);
        }
    }
}

ReportsWidgetBrowserStatistics::ReportsWidgetBrowserStatistics(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ReportsWidgetBrowserStatistics())
    , m_referencesModel(new QStandardItemModel(this))
    , m_modelProxy(new QSortFilterProxyModel(this))
{
    m_ui->setupUi(this);

    m_modelProxy->setSourceModel(m_referencesModel.data());
    m_modelProxy->setSortLocaleAware(true);
    m_ui->browserStatisticsTableView->setModel(m_modelProxy.data());
    m_ui->browserStatisticsTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_ui->browserStatisticsTableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(m_ui->browserStatisticsTableView,
            SIGNAL(customContextMenuRequested(QPoint)),
            SLOT(customMenuRequested(QPoint)));
    connect(
        m_ui->browserStatisticsTableView, SIGNAL(doubleClicked(QModelIndex)), SLOT(emitEntryActivated(QModelIndex)));
    connect(m_ui->showEntriesWithUrlOnlyCheckBox, SIGNAL(stateChanged(int)), this, SLOT(calculateBrowserStatistics()));
    connect(m_ui->showAllowDenyCheckBox, SIGNAL(stateChanged(int)), this, SLOT(calculateBrowserStatistics()));
    connect(m_ui->showExpired, SIGNAL(stateChanged(int)), this, SLOT(calculateBrowserStatistics()));

    new QShortcut(Qt::Key_Delete, this, SLOT(deleteSelectedEntries()));
}

ReportsWidgetBrowserStatistics::~ReportsWidgetBrowserStatistics()
{
}

void ReportsWidgetBrowserStatistics::addStatisticsRow(bool hasUrls,
                                                      bool hasSettings,
                                                      Group* group,
                                                      Entry* entry,
                                                      bool excluded)
{
    StateColorPalette statePalette;

    auto urlList = entry->getAllUrls();
    auto urlToolTip = hasUrls ? tr("List of entry URLs") : tr("Entry has no URLs set");

    auto browserConfig = getBrowserConfigFromEntry(entry);
    auto allowedUrlsList = browserConfig["Allow"];
    auto deniedUrlsList = browserConfig["Deny"];

    auto allowedUrlsToolTip = hasSettings ? tr("Allowed URLs") : tr("Entry has no Browser Integration settings");
    auto deniedUrlsToolTip = hasSettings ? tr("Denied URLs") : tr("Entry has no Browser Integration settings");

    auto title = entry->title();
    if (excluded) {
        title.append(tr(" (Excluded)"));
    }
    if (entry->isExpired()) {
        title.append(tr(" (Expired)"));
    }

    auto row = QList<QStandardItem*>();
    row << new QStandardItem(Icons::entryIconPixmap(entry), title);
    row << new QStandardItem(Icons::groupIconPixmap(group), group->hierarchy().join("/"));
    row << new QStandardItem(urlList.join('\n'));
    row << new QStandardItem(allowedUrlsList.join('\n'));
    row << new QStandardItem(deniedUrlsList.join('\n'));

    // Set tooltips
    row[2]->setToolTip(urlToolTip);
    row[3]->setToolTip(allowedUrlsToolTip);
    row[4]->setToolTip(deniedUrlsToolTip);
    if (excluded) {
        row[0]->setToolTip(tr("This entry is being excluded from reports"));
    }

    // Store entry pointer per table row (used in double click handler)
    m_referencesModel->appendRow(row);
    m_rowToEntry.append({group, entry});
}

void ReportsWidgetBrowserStatistics::loadSettings(QSharedPointer<Database> db)
{
    m_db = std::move(db);
    m_statisticsCalculated = false;
    m_referencesModel->clear();
    m_rowToEntry.clear();

    auto row = QList<QStandardItem*>();
    row << new QStandardItem(tr("Please wait, browser statistics is being calculated…"));
    m_referencesModel->appendRow(row);
}

void ReportsWidgetBrowserStatistics::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    if (!m_statisticsCalculated) {
        // Perform stats calculation on next event loop to allow widget to appear
        m_statisticsCalculated = true;
        QTimer::singleShot(0, this, SLOT(calculateBrowserStatistics()));
    }
}

void ReportsWidgetBrowserStatistics::calculateBrowserStatistics()
{
    m_referencesModel->clear();

    // Perform the statistics check
    const QScopedPointer<BrowserStatistics> browserStatistics(
        AsyncTask::runAndWaitForFuture([this] { return new BrowserStatistics(m_db); }));

    const auto showExpired = m_ui->showExpired->isChecked();
    const auto showEntriesWithUrlOnly = m_ui->showEntriesWithUrlOnlyCheckBox->isChecked();
    const auto showOnlyEntriesWithSettings = m_ui->showAllowDenyCheckBox->isChecked();

    // Display the entries
    m_rowToEntry.clear();
    for (const auto& item : browserStatistics->items()) {
        // Check if the entry should be displayed
        if (!showExpired && item->entry->isExpired()) {
            continue;
        }

        // Exclude this entry if URL are not set
        if (showEntriesWithUrlOnly && !item->hasUrls) {
            continue;
        }

        // Exclude this entry if it doesn't have any Browser Integration settings
        if (showOnlyEntriesWithSettings
            && !item->entry->customData()->contains(BrowserService::KEEPASSXCBROWSER_NAME)) {
            continue;
        }

        // Show the entry in the report
        addStatisticsRow(item->hasUrls, item->hasSettings, item->group, item->entry, item->exclude);
    }

    // Set the table header
    if (m_referencesModel->rowCount() == 0) {
        m_referencesModel->setHorizontalHeaderLabels(
            QStringList() << tr("No entries with a URL, or none has browser extension settings saved."));
    } else {
        m_referencesModel->setHorizontalHeaderLabels(QStringList() << tr("Title") << tr("Path") << tr("URLs")
                                                                   << tr("Allowed URLs") << tr("Denied URLs"));
        m_ui->browserStatisticsTableView->sortByColumn(0, Qt::AscendingOrder);
    }

    m_ui->browserStatisticsTableView->resizeColumnsToContents();
}

void ReportsWidgetBrowserStatistics::emitEntryActivated(const QModelIndex& index)
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

void ReportsWidgetBrowserStatistics::customMenuRequested(QPoint pos)
{
    auto selected = m_ui->browserStatisticsTableView->selectionModel()->selectedRows();
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

    // Create the "expire entry" menu item
    const auto expEntry = new QAction(icons()->icon("entry-expire"), tr("Expire Entry(s)…", "", selected.size()), this);
    menu->addAction(expEntry);
    connect(expEntry, &QAction::triggered, this, &ReportsWidgetBrowserStatistics::expireSelectedEntries);

    // Create the "delete entry" menu item
    const auto deleteEntry =
        new QAction(icons()->icon("entry-delete"), tr("Delete Entry(s)…", "", selected.size()), this);
    menu->addAction(deleteEntry);
    connect(deleteEntry, &QAction::triggered, this, &ReportsWidgetBrowserStatistics::deleteSelectedEntries);

    // Create the "delete plugin data" menu item
    const auto deletePluginData =
        new QAction(icons()->icon("entry-delete"), tr("Delete plugin data from Entry(s)…", "", selected.size()), this);
    menu->addAction(deletePluginData);
    connect(deletePluginData,
            &QAction::triggered,
            this,
            &ReportsWidgetBrowserStatistics::deletePluginDataFromSelectedEntries);

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
        calculateBrowserStatistics();
    });

    // Show the context menu
    menu->popup(m_ui->browserStatisticsTableView->viewport()->mapToGlobal(pos));
}

void ReportsWidgetBrowserStatistics::saveSettings()
{
    // Nothing to do - the tab is passive
}

QList<Entry*> ReportsWidgetBrowserStatistics::getSelectedEntries()
{
    QList<Entry*> selectedEntries;
    for (auto index : m_ui->browserStatisticsTableView->selectionModel()->selectedRows()) {
        auto row = m_modelProxy->mapToSource(index).row();
        auto entry = m_rowToEntry[row].second;
        if (entry) {
            selectedEntries << entry;
        }
    }
    return selectedEntries;
}

void ReportsWidgetBrowserStatistics::expireSelectedEntries()
{
    for (auto entry : getSelectedEntries()) {
        entry->expireNow();
    }

    calculateBrowserStatistics();
}

void ReportsWidgetBrowserStatistics::deleteSelectedEntries()
{
    const auto& selectedEntries = getSelectedEntries();
    bool permanent = !m_db->metadata()->recycleBinEnabled();

    if (GuiTools::confirmDeleteEntries(this, selectedEntries, permanent)) {
        GuiTools::deleteEntriesResolveReferences(this, selectedEntries, permanent);
    }

    calculateBrowserStatistics();
}

void ReportsWidgetBrowserStatistics::deletePluginDataFromSelectedEntries()
{
    const auto& selectedEntries = getSelectedEntries();
    if (GuiTools::confirmDeletePluginData(this, selectedEntries)) {
        for (auto& entry : selectedEntries) {
            browserService()->removePluginData(entry);
        }
    }

    calculateBrowserStatistics();
}

QMap<QString, QStringList> ReportsWidgetBrowserStatistics::getBrowserConfigFromEntry(Entry* entry) const
{
    QMap<QString, QStringList> configList;

    auto config = entry->customData()->value(BrowserService::KEEPASSXCBROWSER_NAME);
    if (!config.isEmpty()) {
        QJsonDocument doc = QJsonDocument::fromJson(config.toUtf8());
        if (!doc.isNull()) {
            auto jsonObject = doc.object();
            auto allowedSites = jsonObject["Allow"].toArray();
            auto deniedSites = jsonObject["Deny"].toArray();

            QStringList allowed;
            foreach (const auto& value, allowedSites) {
                auto url = value.toString();
                if (!url.isEmpty()) {
                    allowed << url;
                }
            }

            QStringList denied;
            foreach (const auto& value, deniedSites) {
                auto url = value.toString();
                if (!url.isEmpty()) {
                    denied << url;
                }
            }

            configList.insert("Allow", allowed);
            configList.insert("Deny", denied);
        }
    }

    return configList;
}

QList<Entry*> ReportsWidgetBrowserStatistics::getSelectedEntries() const
{
    QList<Entry*> selectedEntries;
    for (auto index : m_ui->browserStatisticsTableView->selectionModel()->selectedRows()) {
        auto row = m_modelProxy->mapToSource(index).row();
        auto entry = m_rowToEntry[row].second;
        if (entry) {
            selectedEntries << entry;
        }
    }

    return selectedEntries;
}
