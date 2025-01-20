/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "EntryAttributesModel.h"

#include "core/EntryAttributes.h"

#include <QCollator>

namespace
{
    void sortLocaleAware(QList<QString>& strings)
    {
        QCollator collator;
        collator.setNumericMode(true);
        std::sort(strings.begin(), strings.end(), collator);
    }
} // namespace

EntryAttributesModel::EntryAttributesModel(QObject* parent)
    : QAbstractListModel(parent)
    , m_entryAttributes(nullptr)
    , m_nextRenameDataChange(false)
{
}

void EntryAttributesModel::setEntryAttributes(EntryAttributes* entryAttributes)
{
    beginResetModel();

    if (m_entryAttributes) {
        m_entryAttributes->disconnect(this);
    }

    m_entryAttributes = entryAttributes;

    if (m_entryAttributes) {
        updateAttributes();
        connect(m_entryAttributes, SIGNAL(customKeyModified(QString)), SLOT(attributeChange(QString)));
        connect(m_entryAttributes, SIGNAL(aboutToBeAdded(QString)), SLOT(attributeAboutToAdd(QString)));
        connect(m_entryAttributes, SIGNAL(added(QString)), SLOT(attributeAdd()));
        connect(m_entryAttributes, SIGNAL(aboutToBeRemoved(QString)), SLOT(attributeAboutToRemove(QString)));
        connect(m_entryAttributes, SIGNAL(removed(QString)), SLOT(attributeRemove()));
        // clang-format off
        connect(
            m_entryAttributes, SIGNAL(aboutToRename(QString,QString)), SLOT(attributeAboutToRename(QString,QString)));
        // clang-format on

        // clang-format off
        connect(m_entryAttributes, SIGNAL(renamed(QString,QString)), SLOT(attributeRename(QString,QString)));
        // clang-format on

        connect(m_entryAttributes, SIGNAL(aboutToBeReset()), SLOT(aboutToReset()));
        connect(m_entryAttributes, SIGNAL(reset()), SLOT(reset()));
    }

    endResetModel();
}

int EntryAttributesModel::rowCount(const QModelIndex& parent) const
{
    if (!m_entryAttributes || parent.isValid()) {
        return 0;
    } else {
        return m_attributes.size();
    }
}

int EntryAttributesModel::columnCount(const QModelIndex& parent) const
{
    Q_UNUSED(parent);

    return 1;
}

QVariant EntryAttributesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if ((orientation == Qt::Horizontal) && (role == Qt::DisplayRole) && (section == 0)) {
        return tr("Name");
    } else {
        return {};
    }
}

QVariant EntryAttributesModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole)) {
        return {};
    }

    return m_attributes.at(index.row());
}

bool EntryAttributesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole || value.type() != QVariant::String || value.toString().isEmpty()) {
        return false;
    }

    QString oldKey = m_attributes.at(index.row());
    QString newKey = value.toString();

    if (EntryAttributes::isDefaultAttribute(newKey) || m_entryAttributes->keys().contains(newKey)) {
        return false;
    }
    m_entryAttributes->rename(oldKey, newKey);

    return true;
}

Qt::ItemFlags EntryAttributesModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    } else {
        return QAbstractItemModel::flags(index) | Qt::ItemIsEditable;
    }
}

QModelIndex EntryAttributesModel::indexByKey(const QString& key) const
{
    int row = m_attributes.indexOf(key);

    if (row == -1) {
        return {};
    } else {
        return index(row, 0);
    }
}

QString EntryAttributesModel::keyByIndex(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return {};
    } else {
        return m_attributes.at(index.row());
    }
}

void EntryAttributesModel::attributeChange(const QString& key)
{
    int row = m_attributes.indexOf(key);
    Q_ASSERT(row != -1);
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

void EntryAttributesModel::attributeAboutToAdd(const QString& key)
{
    QList<QString> rows = m_attributes;
    rows.append(key);
    sortLocaleAware(rows);
    int row = rows.indexOf(key);
    beginInsertRows(QModelIndex(), row, row);
}

void EntryAttributesModel::attributeAdd()
{
    updateAttributes();
    endInsertRows();
}

void EntryAttributesModel::attributeAboutToRemove(const QString& key)
{
    int row = m_attributes.indexOf(key);
    beginRemoveRows(QModelIndex(), row, row);
}

void EntryAttributesModel::attributeRemove()
{
    updateAttributes();
    endRemoveRows();
}

void EntryAttributesModel::attributeAboutToRename(const QString& oldKey, const QString& newKey)
{
    int oldRow = m_attributes.indexOf(oldKey);

    QList<QString> rows = m_attributes;
    rows.removeOne(oldKey);
    rows.append(newKey);
    sortLocaleAware(rows);
    int newRow = rows.indexOf(newKey);
    if (newRow > oldRow) {
        newRow++;
    }

    if (oldRow != newRow) {
        bool result = beginMoveRows(QModelIndex(), oldRow, oldRow, QModelIndex(), newRow);
        Q_UNUSED(result);
        Q_ASSERT(result);
    } else {
        m_nextRenameDataChange = true;
    }
}

void EntryAttributesModel::attributeRename(const QString& oldKey, const QString& newKey)
{
    Q_UNUSED(oldKey);

    updateAttributes();

    if (!m_nextRenameDataChange) {
        endMoveRows();
    } else {
        m_nextRenameDataChange = false;

        QModelIndex keyIndex = index(m_attributes.indexOf(newKey), 0);
        emit dataChanged(keyIndex, keyIndex);
    }
}

void EntryAttributesModel::aboutToReset()
{
    beginResetModel();
}

void EntryAttributesModel::reset()
{
    updateAttributes();
    endResetModel();
}

void EntryAttributesModel::updateAttributes()
{
    m_attributes.clear();

    const QList<QString> attributesKeyList = m_entryAttributes->keys();
    for (const QString& key : attributesKeyList) {
        if (!EntryAttributes::isDefaultAttribute(key)) {
            m_attributes.append(key);
        }
    }
    sortLocaleAware(m_attributes);
}
