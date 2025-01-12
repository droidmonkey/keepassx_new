/*
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#include "EntryModel.h"

#include <QFont>
#include <QMimeData>
#include <QPalette>

#include "core/Clock.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/PasswordHealth.h"
#include "gui/DatabaseIcons.h"
#include "gui/Icons.h"
#include "gui/styles/StateColorPalette.h"
#ifdef Q_OS_MACOS
#include "gui/osutils/macutils/MacUtils.h"
#endif

EntryModel::EntryModel(QObject* parent)
    : QAbstractTableModel(parent)
    , m_group(nullptr)
    , HiddenContentDisplay(QString("\u25cf").repeated(6))
{
    connect(config(), &Config::changed, this, &EntryModel::onConfigChanged);
}

Entry* EntryModel::entryFromIndex(const QModelIndex& index) const
{
    Q_ASSERT(index.isValid() && index.row() < m_entries.size());
    return m_entries.at(index.row());
}

QModelIndex EntryModel::indexFromEntry(Entry* entry) const
{
    int row = m_entries.indexOf(entry);
    if (row >= 0) {
        return index(row, 1);
    }
    return {};
}

void EntryModel::setGroup(Group* group)
{
    if (!group || group == m_group) {
        return;
    }

    beginResetModel();

    severConnections();

    m_group = group;
    m_allGroups.clear();
    m_entries = group->entries();
    m_orgEntries.clear();

    makeConnections(group);

    endResetModel();
}

void EntryModel::setEntries(const QList<Entry*>& entries)
{
    beginResetModel();

    severConnections();

    m_group = nullptr;
    m_allGroups.clear();
    m_entries = entries;
    m_orgEntries = entries;

    for (const auto entry : asConst(m_entries)) {
        if (entry->group()) {
            m_allGroups.insert(entry->group());
        }
    }

    for (const auto group : m_allGroups) {
        makeConnections(group);
    }

    endResetModel();
}

int EntryModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    } else {
        return m_entries.size();
    }
}

int EntryModel::columnCount(const QModelIndex& parent) const
{
    // Advised by Qt documentation
    if (parent.isValid()) {
        return 0;
    }

    return 17;
}

QVariant EntryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    Entry* entry = entryFromIndex(index);
    EntryAttributes* attr = entry->attributes();

    if (role == Qt::DisplayRole) {
        QString result;
        switch (index.column()) {
        case ParentGroup:
            if (entry->group()) {
                return entry->group()->name();
            }
            break;
        case ParentGroupPath:
            if (entry->group()) {
                return entry->group()->fullPath();
            }
            break;
        case Title:
            result = entry->resolveMultiplePlaceholders(entry->title());
            if (attr->isReference(EntryAttributes::TitleKey)) {
                result.prepend(tr("Ref: ", "Reference abbreviation"));
            }
            return result;
        case Username:
            if (config()->get(Config::GUI_HideUsernames).toBool()) {
                result = EntryModel::HiddenContentDisplay;
            } else {
                result = entry->resolveMultiplePlaceholders(entry->username());
            }
            if (attr->isReference(EntryAttributes::UserNameKey)) {
                result.prepend(tr("Ref: ", "Reference abbreviation"));
            }
            if (entry->username().isEmpty() && !config()->get(Config::Security_PasswordEmptyPlaceholder).toBool()) {
                result = "";
            }
            return result;
        case Password:
            if (config()->get(Config::GUI_HidePasswords).toBool()) {
                result = EntryModel::HiddenContentDisplay;
            } else {
                result = entry->resolveMultiplePlaceholders(entry->password());
            }
            if (attr->isReference(EntryAttributes::PasswordKey)) {
                result.prepend(tr("Ref: ", "Reference abbreviation"));
            }
            if (entry->password().isEmpty() && !config()->get(Config::Security_PasswordEmptyPlaceholder).toBool()) {
                result = "";
            }
            return result;
        case Url:
            result = entry->resolveMultiplePlaceholders(entry->displayUrl());
            if (attr->isReference(EntryAttributes::URLKey)) {
                result.prepend(tr("Ref: ", "Reference abbreviation"));
            }
            return result;
        case Notes:
            if (!entry->notes().isEmpty()) {
                if (config()->get(Config::Security_HideNotes).toBool()) {
                    result = EntryModel::HiddenContentDisplay;
                } else {
                    // Display only first line of notes in simplified format if not hidden
                    result = entry->notes().section("\n", 0, 0).simplified();
                }
                if (attr->isReference(EntryAttributes::NotesKey)) {
                    result.prepend(tr("Ref: ", "Reference abbreviation"));
                }
            }
            return result;
        case Expires:
            // Display either date of expiry or 'Never'
            result = entry->timeInfo().expires() ? Clock::toString(entry->timeInfo().expiryTime().toLocalTime())
                                                 : tr("Never");
            return result;
        case Created:
            result = Clock::toString(entry->timeInfo().creationTime().toLocalTime());
            return result;
        case Modified:
            result = Clock::toString(entry->timeInfo().lastModificationTime().toLocalTime());
            return result;
        case Accessed:
            result = Clock::toString(entry->timeInfo().lastAccessTime().toLocalTime());
            return result;
        case Attachments: {
            // Display comma-separated list of attachments
            QList<QString> attachments = entry->attachments()->keys();
            for (const auto& attachment : attachments) {
                if (result.isEmpty()) {
                    result.append(attachment);
                    continue;
                }
                result.append(QString(", ") + attachment);
            }
            return result;
        }
        case Size: {
            const int unitsSize = 4;
            QString units[unitsSize] = {"B", "KiB", "MiB", "GiB"};
            float resultInt = entry->size();

            for (int i = 0; i < unitsSize; i++) {
                if (resultInt < 1024 || i == unitsSize - 1) {
                    resultInt = qRound(resultInt * 100) / 100.0;
                    result = QStringLiteral("%1 %2").arg(QString::number(resultInt), units[i]);
                    break;
                }
                resultInt /= 1024.0;
            }

            return result;
        }
        case Color:
            QColor backgroundColor;
            backgroundColor.setNamedColor(entry->backgroundColor());
            if (backgroundColor.isValid()) {
                result = "▍";
                return result;
            }
        }
    } else if (role == Qt::UserRole) { // Qt::UserRole is used as sort role, see EntryView::EntryView()
        switch (index.column()) {
        case Username:
            return entry->resolveMultiplePlaceholders(entry->username());
        case Password:
            return entry->resolveMultiplePlaceholders(entry->password());
        case PasswordStrength: {
            if (!entry->password().isEmpty() && !entry->excludeFromReports()) {
                return entry->passwordHealth()->score();
            }
            return 0;
        }
        case Expires:
            return entry->timeInfo().expires() ? entry->timeInfo().expiryTime()
            // There seems to be no better way of expressing 'infinity'
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
                                               : QDate(9999, 1, 1).startOfDay();
#else
                                               : QDateTime(QDate(9999, 1, 1));
#endif
        case Created:
            return entry->timeInfo().creationTime();
        case Modified:
            return entry->timeInfo().lastModificationTime();
        case Accessed:
            return entry->timeInfo().lastAccessTime();
        case Paperclip:
            // Display entries with attachments above those without when
            // sorting ascendingly (and vice versa when sorting descendingly)
            return !entry->attachments()->isEmpty();
        case Totp:
            return entry->hasTotp();
        case Size:
            return entry->size();
        default:
            // For all other columns, simply use data provided by Qt::Display-
            // Role for sorting
            return data(index, Qt::DisplayRole);
        }
    } else if (role == Qt::DecorationRole) {
        switch (index.column()) {
        case ParentGroup:
            if (entry->group()) {
                return Icons::groupIconPixmap(entry->group());
            }
            break;
        case Title:
            return Icons::entryIconPixmap(entry);
        case Paperclip:
            if (!entry->attachments()->isEmpty()) {
                return icons()->icon("paperclip");
            }
            break;
        case Totp:
            if (entry->hasTotp()) {
                return icons()->icon("totp");
            }
            break;
        case PasswordStrength:
            if (!entry->password().isEmpty() && !entry->excludeFromReports()) {
                QString iconName = "lock-question";
                StateColorPalette statePalette;
                QColor color = statePalette.color(StateColorPalette::Error);

                switch (entry->passwordHealth()->quality()) {
                case PasswordHealth::Quality::Bad:
                case PasswordHealth::Quality::Poor:
                    iconName = "lock-open-alert";
                    color = statePalette.color(StateColorPalette::HealthCritical);
                    break;
                case PasswordHealth::Quality::Weak:
                    iconName = "lock-open";
                    color = statePalette.color(StateColorPalette::HealthBad);
                    break;
                case PasswordHealth::Quality::Good:
                case PasswordHealth::Quality::Excellent:
                    iconName = "lock";
                    color = statePalette.color(StateColorPalette::HealthExcellent);
                    break;
                }

                if (color.isValid()) {
                    return icons()->icon(iconName, true, color);
                }
            }
            break;
        }
    } else if (role == Qt::FontRole) {
        QFont font;
        if (entry->isExpired()) {
            font.setStrikeOut(true);
        }
        return font;
    } else if (role == Qt::ForegroundRole) {

        if (index.column() == Color) {
            QColor backgroundColor;
            backgroundColor.setNamedColor(entry->backgroundColor());
            if (backgroundColor.isValid()) {
                return backgroundColor;
            }
        }

        QColor foregroundColor;
        foregroundColor.setNamedColor(entry->foregroundColor());
        if (entry->hasReferences()) {
            QPalette p;
            foregroundColor = p.color(QPalette::Current, QPalette::Text);
            int lightness =
                qMin(255, qMax(0, foregroundColor.lightness() + (foregroundColor.lightness() < 110 ? 85 : -51)));
            foregroundColor.setHsl(foregroundColor.hue(), foregroundColor.saturation(), lightness);
            return QVariant(foregroundColor);
        } else if (foregroundColor.isValid()) {
            return QVariant(foregroundColor);
        }
    } else if (role == Qt::BackgroundRole) {
        if (m_backgroundColorVisible) {
            QColor backgroundColor;
            backgroundColor.setNamedColor(entry->backgroundColor());
            if (backgroundColor.isValid()) {
                return QVariant(backgroundColor);
            }
        }
    } else if (role == Qt::ToolTipRole) {
        if (index.column() == PasswordStrength && !entry->password().isEmpty() && !entry->excludeFromReports()) {
            return entry->passwordHealth()->scoreReason();
        }
    }

    return {};
}

QVariant EntryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(orientation);

    if (role == Qt::DisplayRole) {
        switch (section) {
        case ParentGroup:
            return tr("Group");
        case ParentGroupPath:
            return tr("Group Path");
        case Title:
            return tr("Title");
        case Username:
            return tr("Username");
        case Password:
            return tr("Password");
        case Url:
            return tr("URL");
        case Notes:
            return tr("Notes");
        case Expires:
            return tr("Expires");
        case Created:
            return tr("Created");
        case Modified:
            return tr("Modified");
        case Accessed:
            return tr("Accessed");
        case Attachments:
            return tr("Attachments");
        case Size:
            return tr("Size");
        }

    } else if (role == Qt::DecorationRole) {
        switch (section) {
        case Paperclip:
            return icons()->icon("paperclip");
        case Totp:
            return icons()->icon("totp");
        case PasswordStrength:
            return icons()->icon("lock-question");
        }
    } else if (role == Qt::ToolTipRole) {
        switch (section) {
        case ParentGroup:
            return tr("Group name");
        case ParentGroupPath:
            return tr("Group Path");
        case Title:
            return tr("Entry title");
        case Username:
            return tr("Username");
        case Password:
            return tr("Password");
        case PasswordStrength:
            return tr("Password Strength");
        case Url:
            return tr("URL");
        case Notes:
            return tr("Entry notes");
        case Expires:
            return tr("Entry expires at");
        case Created:
            return tr("Creation date");
        case Modified:
            return tr("Last modification date");
        case Accessed:
            return tr("Last access date");
        case Attachments:
            return tr("Attached files");
        case Size:
            return tr("Entry size");
        case Paperclip:
            return tr("Has attachments");
        case Totp:
            return tr("Has TOTP");
        case Color:
            return tr("Background Color");
        }
    }

    return {};
}

Qt::DropActions EntryModel::supportedDropActions() const
{
    return Qt::IgnoreAction;
}

Qt::DropActions EntryModel::supportedDragActions() const
{
    return (Qt::MoveAction | Qt::CopyAction);
}

Qt::ItemFlags EntryModel::flags(const QModelIndex& modelIndex) const
{
    if (!modelIndex.isValid()) {
        return Qt::NoItemFlags;
    } else {
        return QAbstractItemModel::flags(modelIndex) | Qt::ItemIsDragEnabled;
    }
}

QStringList EntryModel::mimeTypes() const
{
    QStringList types;
    types << QLatin1String("application/x-keepassx-entry");
    return types;
}

QMimeData* EntryModel::mimeData(const QModelIndexList& indexes) const
{
    if (indexes.isEmpty()) {
        return nullptr;
    }

    auto data = new QMimeData();
    QByteArray encoded;
    QDataStream stream(&encoded, QIODevice::WriteOnly);

    QSet<Entry*> seenEntries;

    for (const QModelIndex& index : indexes) {
        if (!index.isValid()) {
            continue;
        }

        Entry* entry = entryFromIndex(index);
        if (!seenEntries.contains(entry)) {
            // make sure we don't add entries multiple times when we get indexes
            // with the same row but different columns
            stream << entry->group()->database()->uuid() << entry->uuid();
            seenEntries.insert(entry);
        }
    }

    if (seenEntries.isEmpty()) {
        delete data;
        return nullptr;
    } else {
        data->setData(mimeTypes().at(0), encoded);
        return data;
    }
}

void EntryModel::entryAboutToAdd(Entry* entry)
{
    if (!m_group && !m_orgEntries.contains(entry)) {
        return;
    }

    beginInsertRows(QModelIndex(), m_entries.size(), m_entries.size());
    if (!m_group) {
        m_entries.append(entry);
    }
}

void EntryModel::entryAdded(Entry* entry)
{
    if (!m_group && !m_orgEntries.contains(entry)) {
        return;
    }

    if (m_group) {
        m_entries = m_group->entries();
    }
    endInsertRows();
}

void EntryModel::entryAboutToRemove(Entry* entry)
{
    beginRemoveRows(QModelIndex(), m_entries.indexOf(entry), m_entries.indexOf(entry));
    if (!m_group) {
        m_entries.removeAll(entry);
    }
}

void EntryModel::entryRemoved()
{
    if (m_group) {
        m_entries = m_group->entries();
    }
    endRemoveRows();
}

void EntryModel::entryAboutToMoveUp(int row)
{
    beginMoveRows(QModelIndex(), row, row, QModelIndex(), row - 1);
    if (m_group) {
        m_entries.move(row, row - 1);
    }
}

void EntryModel::entryMovedUp()
{
    if (m_group) {
        m_entries = m_group->entries();
    }
    endMoveRows();
}

void EntryModel::entryAboutToMoveDown(int row)
{
    beginMoveRows(QModelIndex(), row, row, QModelIndex(), row + 2);
    if (m_group) {
        m_entries.move(row, row + 1);
    }
}

void EntryModel::entryMovedDown()
{
    if (m_group) {
        m_entries = m_group->entries();
    }
    endMoveRows();
}

void EntryModel::entryDataChanged(Entry* entry)
{
    int row = m_entries.indexOf(entry);
    emit dataChanged(index(row, 0), index(row, columnCount() - 1));
}

void EntryModel::onConfigChanged(Config::ConfigKey key)
{
    switch (key) {
    case Config::GUI_HideUsernames:
        emit dataChanged(index(0, Username), index(rowCount() - 1, Username), {Qt::DisplayRole});
        break;
    case Config::GUI_HidePasswords:
        emit dataChanged(index(0, Password), index(rowCount() - 1, Password), {Qt::DisplayRole});
        break;
    default:
        break;
    }
}

void EntryModel::severConnections()
{
    if (m_group) {
        disconnect(m_group, nullptr, this, nullptr);
    }

    for (const Group* group : asConst(m_allGroups)) {
        disconnect(group, nullptr, this, nullptr);
    }
}

void EntryModel::makeConnections(const Group* group)
{
    connect(group, SIGNAL(entryAboutToAdd(Entry*)), SLOT(entryAboutToAdd(Entry*)));
    connect(group, SIGNAL(entryAdded(Entry*)), SLOT(entryAdded(Entry*)));
    connect(group, SIGNAL(entryAboutToRemove(Entry*)), SLOT(entryAboutToRemove(Entry*)));
    connect(group, SIGNAL(entryRemoved(Entry*)), SLOT(entryRemoved()));
    connect(group, SIGNAL(entryAboutToMoveUp(int)), SLOT(entryAboutToMoveUp(int)));
    connect(group, SIGNAL(entryMovedUp()), SLOT(entryMovedUp()));
    connect(group, SIGNAL(entryAboutToMoveDown(int)), SLOT(entryAboutToMoveDown(int)));
    connect(group, SIGNAL(entryMovedDown()), SLOT(entryMovedDown()));
    connect(group, SIGNAL(entryDataChanged(Entry*)), SLOT(entryDataChanged(Entry*)));
}
void EntryModel::setBackgroundColorVisible(bool visible)
{
    m_backgroundColorVisible = visible;
}
