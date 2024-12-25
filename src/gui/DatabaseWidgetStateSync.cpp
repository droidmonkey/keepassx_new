/*
 * Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 * Copyright (C) 2014 Felix Geyer <debfx@fobos.de>
 * Copyright (C) 2014 Florian Geyer <blueice@fobos.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or (at your option)
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "DatabaseWidgetStateSync.h"

#include <QCoreApplication>

DatabaseWidgetStateSync::DatabaseWidgetStateSync(QObject* parent)
    : QObject(parent)
    , m_activeDbWidget(nullptr)
    , m_blockUpdates(false)
{
    m_splitterSizes = {
        {Config::GUI_SplitterState, variantToIntList(config()->get(Config::GUI_SplitterState))},
        {Config::GUI_PreviewSplitterState, variantToIntList(config()->get(Config::GUI_PreviewSplitterState))},
        {Config::GUI_GroupSplitterState, variantToIntList(config()->get(Config::GUI_GroupSplitterState))}};
    m_listViewState = config()->get(Config::GUI_ListViewState).toByteArray();
    m_searchViewState = config()->get(Config::GUI_SearchViewState).toByteArray();

    m_syncTimer.setSingleShot(true);
    m_syncTimer.setInterval(100);
    connect(&m_syncTimer, &QTimer::timeout, this, &DatabaseWidgetStateSync::sync);
}

DatabaseWidgetStateSync::~DatabaseWidgetStateSync() = default;

/**
 * Sync state with persistent storage.
 */
void DatabaseWidgetStateSync::sync()
{
    m_syncTimer.stop();
    config()->set(Config::GUI_SplitterState, intListToVariant(m_splitterSizes.value(Config::GUI_SplitterState)));
    config()->set(Config::GUI_PreviewSplitterState,
                  intListToVariant(m_splitterSizes.value(Config::GUI_PreviewSplitterState)));
    config()->set(Config::GUI_GroupSplitterState,
                  intListToVariant(m_splitterSizes.value(Config::GUI_GroupSplitterState)));
    config()->set(Config::GUI_ListViewState, m_listViewState);
    config()->set(Config::GUI_SearchViewState, m_searchViewState);
    config()->sync();
}

void DatabaseWidgetStateSync::setActive(DatabaseWidget* dbWidget)
{
    if (m_activeDbWidget) {
        if (m_activeDbWidget->currentMode() != DatabaseWidget::Mode::LockedMode) {
            // Update settings from previously active database if unlocked
            updateAll();
        }
        disconnect(m_activeDbWidget, nullptr, this, nullptr);
    }

    m_activeDbWidget = dbWidget;

    if (m_activeDbWidget) {
        if (m_activeDbWidget->currentMode() != DatabaseWidget::Mode::LockedMode) {
            // Immediately apply settings to active database if already unlocked
            applySplitterSizes();
            applyViewState();
        }

        connect(m_activeDbWidget, SIGNAL(databaseAboutToUnlock()), SLOT(blockUpdates()));
        connect(m_activeDbWidget, SIGNAL(databaseUnlocked()), SLOT(applySplitterSizes()));
        connect(m_activeDbWidget, SIGNAL(databaseUnlocked()), SLOT(applyViewState()));
        connect(m_activeDbWidget, &DatabaseWidget::databaseLocked, this, [this] { updateAll(true); });
        connect(m_activeDbWidget, SIGNAL(splitterSizesChanged()), SLOT(updateSplitterSizes()));
        connect(m_activeDbWidget, SIGNAL(entryViewStateChanged()), SLOT(updateViewState()));
        connect(m_activeDbWidget, SIGNAL(listModeActivated()), SLOT(applyViewState()));
        connect(m_activeDbWidget, SIGNAL(searchModeActivated()), SLOT(applyViewState()));
        connect(m_activeDbWidget, SIGNAL(listModeAboutToActivate()), SLOT(blockUpdates()));
        connect(m_activeDbWidget, SIGNAL(searchModeAboutToActivate()), SLOT(blockUpdates()));
    }
}

void DatabaseWidgetStateSync::applySplitterSizes()
{
    if (!m_activeDbWidget) {
        return;
    }

    m_blockUpdates = true;

    m_activeDbWidget->setSplitterSizes(m_splitterSizes);

    m_blockUpdates = false;
}

/**
 * Restore entry view list view state
 *
 * NOTE:
 * If m_listViewState is empty, the list view has been activated for the first
 * time after starting with a clean (or invalid) config.
 */
void DatabaseWidgetStateSync::applyViewState()
{
    if (!m_activeDbWidget) {
        return;
    }

    m_blockUpdates = true;

    if (m_activeDbWidget->isSearchActive()) {
        if (!m_searchViewState.isEmpty()) {
            m_activeDbWidget->setEntryViewState(m_searchViewState);
        }
    } else {
        if (!m_listViewState.isEmpty()) {
            m_activeDbWidget->setEntryViewState(m_listViewState);
        }
    }

    m_blockUpdates = false;
}

void DatabaseWidgetStateSync::blockUpdates()
{
    m_blockUpdates = true;
}

void DatabaseWidgetStateSync::updateAll(bool forceSync)
{
    updateSplitterSizes();
    updateViewState();
    if (forceSync) {
        sync();
    }
}

void DatabaseWidgetStateSync::updateSplitterSizes()
{
    if (!m_blockUpdates) {
        m_splitterSizes = m_activeDbWidget->splitterSizes();
        m_syncTimer.start();
    }
}

/**
 * Update entry view list/search view state
 */
void DatabaseWidgetStateSync::updateViewState()
{
    if (m_blockUpdates) {
        return;
    }

    if (m_activeDbWidget->isSearchActive()) {
        m_searchViewState = m_activeDbWidget->entryViewState();
    } else {
        m_listViewState = m_activeDbWidget->entryViewState();
    }

    m_syncTimer.start();
}

QList<int> DatabaseWidgetStateSync::variantToIntList(const QVariant& variant)
{
    const QVariantList list = variant.toList();
    QList<int> result;

    for (const QVariant& var : list) {
        bool ok;
        int size = var.toInt(&ok);
        if (ok) {
            result.append(size);
        } else {
            result.clear();
            break;
        }
    }

    return result;
}

QVariant DatabaseWidgetStateSync::intListToVariant(const QList<int>& list)
{
    QVariantList result;

    for (int value : list) {
        result.append(value);
    }

    return result;
}
