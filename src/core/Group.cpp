/*
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#include "Group.h"

#include "core/Config.h"
#include "core/Global.h"
#include "core/Metadata.h"
#include "core/Tools.h"

#include <QtConcurrent>
#include <QtConcurrentFilter>

const int Group::DefaultIconNumber = 48;
const int Group::OpenFolderIconNumber = 49;
const int Group::RecycleBinIconNumber = 43;
const QString Group::RootAutoTypeSequence = "{USERNAME}{TAB}{PASSWORD}{ENTER}";

Group::Group()
    : m_customData(new CustomData(this))
    , m_updateTimeinfo(true)
{
    m_data.iconNumber = DefaultIconNumber;
    m_data.isExpanded = true;
    m_data.autoTypeEnabled = Inherit;
    m_data.searchingEnabled = Inherit;
    m_data.mergeMode = Default;

    connect(m_customData, &CustomData::modified, this, &Group::modified);
    connect(this, &Group::modified, this, &Group::updateTimeinfo);
    connect(this, &Group::groupNonDataChange, this, &Group::updateTimeinfo);
}

Group::~Group()
{
    setUpdateTimeinfo(false);
    // Destroy entries and children manually so DeletedObjects can be added
    // to database.
    const QList<Entry*> entries = m_entries;
    for (Entry* entry : entries) {
        delete entry;
    }

    const QList<Group*> children = m_children;
    for (Group* group : children) {
        delete group;
    }

    if (m_db && m_parent) {
        DeletedObject delGroup;
        delGroup.deletionTime = Clock::currentDateTimeUtc();
        delGroup.uuid = m_uuid;
        m_db->addDeletedObject(delGroup);
    }

    cleanupParent();
}

template <class P, class V> inline bool Group::set(P& property, const V& value)
{
    if (property != value) {
        property = value;
        emitModified();
        return true;
    } else {
        return false;
    }
}

bool Group::canUpdateTimeinfo() const
{
    return m_updateTimeinfo;
}

void Group::updateTimeinfo()
{
    if (m_updateTimeinfo) {
        m_data.timeInfo.setLastModificationTime(Clock::currentDateTimeUtc());
        m_data.timeInfo.setLastAccessTime(Clock::currentDateTimeUtc());
    }
}

void Group::setUpdateTimeinfo(bool value)
{
    m_updateTimeinfo = value;
}

const QUuid& Group::uuid() const
{
    return m_uuid;
}

const QString Group::uuidToHex() const
{
    return Tools::uuidToHex(m_uuid);
}

QString Group::name() const
{
    return m_data.name;
}

QString Group::notes() const
{
    return m_data.notes;
}

QString Group::tags() const
{
    return m_data.tags;
}

QString Group::fullPath() const
{
    QString fullPath;
    auto group = this;

    do {
        fullPath.insert(0, "/" + group->name());
        group = group->parentGroup();
    } while (group);

    return fullPath;
}

int Group::iconNumber() const
{
    return m_data.iconNumber;
}

const QUuid& Group::iconUuid() const
{
    return m_data.customIcon;
}

const TimeInfo& Group::timeInfo() const
{
    return m_data.timeInfo;
}

bool Group::isExpanded() const
{
    return m_data.isExpanded;
}

QString Group::defaultAutoTypeSequence() const
{
    return m_data.defaultAutoTypeSequence;
}

/**
 * Determine the effective sequence that will be injected
 * This function return an empty string if the current group or any parent has autotype disabled
 */
QString Group::effectiveAutoTypeSequence() const
{
    QString sequence;

    const Group* group = this;
    do {
        if (group->autoTypeEnabled() == Group::Disable) {
            return {};
        }

        sequence = group->defaultAutoTypeSequence();
        group = group->parentGroup();
    } while (group && sequence.isEmpty());

    if (sequence.isEmpty()) {
        sequence = RootAutoTypeSequence;
    }

    return sequence;
}

Group::TriState Group::autoTypeEnabled() const
{
    return m_data.autoTypeEnabled;
}

Group::TriState Group::searchingEnabled() const
{
    return m_data.searchingEnabled;
}

Group::MergeMode Group::mergeMode() const
{
    if (m_data.mergeMode == Group::MergeMode::Default) {
        if (m_parent) {
            return m_parent->mergeMode();
        }
        return Group::MergeMode::KeepNewer; // fallback
    }
    return m_data.mergeMode;
}

Entry* Group::lastTopVisibleEntry() const
{
    return m_lastTopVisibleEntry;
}

bool Group::isRecycled() const
{
    auto group = this;
    auto db = group->database();
    if (db) {
        auto recycleBin = db->metadata()->recycleBin();
        do {
            if (group == recycleBin) {
                return true;
            }
            group = group->m_parent;
        } while (group);
    }

    return false;
}

bool Group::isExpired() const
{
    return m_data.timeInfo.expires() && m_data.timeInfo.expiryTime() < Clock::currentDateTimeUtc();
}

bool Group::isEmpty() const
{
    return !hasChildren() && m_entries.isEmpty();
}

// TODO: Refactor this when KeeShare is refactored
bool Group::isShared() const
{
    auto group = this;
    do {
        if (group->customData()->contains("KeeShare/Reference")) {
            return true;
        }
        group = group->m_parent;
    } while (group);

    return false;
}

CustomData* Group::customData()
{
    return m_customData;
}

const CustomData* Group::customData() const
{
    return m_customData;
}

Group::TriState Group::resolveCustomDataTriState(const QString& key, bool checkParent) const
{
    // If not defined, check our parent up to the root group
    if (!m_customData->contains(key)) {
        if (!m_parent || !checkParent) {
            return Inherit;
        } else {
            return m_parent->resolveCustomDataTriState(key);
        }
    }

    return m_customData->value(key) == TRUE_STR ? Enable : Disable;
}

void Group::setCustomDataTriState(const QString& key, const Group::TriState& value)
{
    switch (value) {
    case Enable:
        m_customData->set(key, TRUE_STR);
        break;
    case Disable:
        m_customData->set(key, FALSE_STR);
        break;
    case Inherit:
        m_customData->remove(key);
        break;
    }
}

// Note that this returns an empty string both if the key is missing *or* if the key is present but value is empty.
QString Group::resolveCustomDataString(const QString& key, bool checkParent) const
{
    // If not defined, check our parent up to the root group
    if (!m_customData->contains(key)) {
        if (!m_parent || !checkParent) {
            return QString();
        } else {
            return m_parent->resolveCustomDataString(key);
        }
    }

    return m_customData->value(key);
}

bool Group::equals(const Group* other, CompareItemOptions options) const
{
    if (!other) {
        return false;
    }
    if (m_uuid != other->m_uuid) {
        return false;
    }
    if (!m_data.equals(other->m_data, options)) {
        return false;
    }
    if (m_customData != other->m_customData) {
        return false;
    }
    if (m_children.count() != other->m_children.count()) {
        return false;
    }
    if (m_entries.count() != other->m_entries.count()) {
        return false;
    }
    for (int i = 0; i < m_children.count(); ++i) {
        if (m_children[i]->uuid() != other->m_children[i]->uuid()) {
            return false;
        }
    }
    for (int i = 0; i < m_entries.count(); ++i) {
        if (m_entries[i]->uuid() != other->m_entries[i]->uuid()) {
            return false;
        }
    }
    return true;
}

void Group::setUuid(const QUuid& uuid)
{
    set(m_uuid, uuid);
}

void Group::setName(const QString& name)
{
    if (set(m_data.name, name)) {
        emit groupDataChanged(this);
    }
}

void Group::setNotes(const QString& notes)
{
    set(m_data.notes, notes);
}

void Group::setTags(const QString& tags)
{
    set(m_data.tags, tags);
}

void Group::setIcon(int iconNumber)
{
    if (iconNumber >= 0 && (m_data.iconNumber != iconNumber || !m_data.customIcon.isNull())) {
        m_data.iconNumber = iconNumber;
        m_data.customIcon = QUuid();
        emitModified();
        emit groupDataChanged(this);
    }
}

void Group::setIcon(const QUuid& uuid)
{
    if (!uuid.isNull() && m_data.customIcon != uuid) {
        m_data.customIcon = uuid;
        m_data.iconNumber = 0;
        emitModified();
        emit groupDataChanged(this);
    }
}

void Group::setTimeInfo(const TimeInfo& timeInfo)
{
    m_data.timeInfo = timeInfo;
}

void Group::setExpanded(bool expanded)
{
    if (m_data.isExpanded != expanded) {
        m_data.isExpanded = expanded;
        emit groupNonDataChange();
    }
}

void Group::setDefaultAutoTypeSequence(const QString& sequence)
{
    set(m_data.defaultAutoTypeSequence, sequence);
}

void Group::setAutoTypeEnabled(TriState enable)
{
    set(m_data.autoTypeEnabled, enable);
}

void Group::setSearchingEnabled(TriState enable)
{
    set(m_data.searchingEnabled, enable);
}

void Group::setLastTopVisibleEntry(Entry* entry)
{
    set(m_lastTopVisibleEntry, entry);
}

void Group::setExpires(bool value)
{
    if (m_data.timeInfo.expires() != value) {
        m_data.timeInfo.setExpires(value);
        emitModified();
    }
}

void Group::setExpiryTime(const QDateTime& dateTime)
{
    if (m_data.timeInfo.expiryTime() != dateTime) {
        m_data.timeInfo.setExpiryTime(dateTime);
        emitModified();
    }
}

void Group::setMergeMode(MergeMode newMode)
{
    set(m_data.mergeMode, newMode);
}

Group* Group::parentGroup()
{
    return m_parent;
}

const Group* Group::parentGroup() const
{
    return m_parent;
}

void Group::setParent(Group* parent, int index, bool trackPrevious)
{
    Q_ASSERT(parent);
    Q_ASSERT(index >= -1 && index <= parent->children().size());
    // setting a new parent for root groups is not allowed
    Q_ASSERT(!m_db || (m_db->rootGroup() != this));

    bool moveWithinDatabase = (m_db && m_db == parent->m_db);

    if (index == -1) {
        index = parent->children().size();

        if (parentGroup() == parent) {
            index--;
        }
    }

    if (m_parent == parent && parent->children().indexOf(this) == index) {
        return;
    }

    if (!moveWithinDatabase) {
        cleanupParent();
        m_parent = parent;
        if (m_db) {
            setPreviousParentGroup(nullptr);
            recCreateDelObjects();

            // copy custom icon to the new database
            if (!iconUuid().isNull() && parent->m_db && m_db->metadata()->hasCustomIcon(iconUuid())
                && !parent->m_db->metadata()->hasCustomIcon(iconUuid())) {
                parent->m_db->metadata()->addCustomIcon(iconUuid(), m_db->metadata()->customIcon(iconUuid()));
            }
        }
        if (m_db != parent->m_db) {
            connectDatabaseSignalsRecursive(parent->m_db);
        }
        QObject::setParent(parent);
        emit groupAboutToAdd(this, index);
        Q_ASSERT(index <= parent->m_children.size());
        parent->m_children.insert(index, this);
    } else {
        emit aboutToMove(this, parent, index);
        if (trackPrevious && m_parent != parent) {
            setPreviousParentGroup(m_parent);
        }
        m_parent->m_children.removeAll(this);
        m_parent = parent;
        QObject::setParent(parent);
        Q_ASSERT(index <= parent->m_children.size());
        parent->m_children.insert(index, this);
    }

    if (m_updateTimeinfo) {
        m_data.timeInfo.setLocationChanged(Clock::currentDateTimeUtc());
    }

    emitModified();

    if (!moveWithinDatabase) {
        emit groupAdded();
    } else {
        emit groupMoved();
    }
}

void Group::setParent(Database* db)
{
    Q_ASSERT(db);
    Q_ASSERT(db->rootGroup() == this);

    cleanupParent();

    m_parent = nullptr;
    connectDatabaseSignalsRecursive(db);

    QObject::setParent(db);
}

QStringList Group::hierarchy(int height) const
{
    QStringList hierarchy;
    const Group* group = this;
    const Group* parent = m_parent;

    if (height == 0) {
        return hierarchy;
    }

    hierarchy.prepend(group->name());

    int level = 1;
    bool heightReached = level == height;

    while (parent && !heightReached) {
        group = group->parentGroup();
        parent = group->parentGroup();
        heightReached = ++level == height;

        hierarchy.prepend(group->name());
    }

    return hierarchy;
}

bool Group::hasChildren() const
{
    return !children().isEmpty();
}

Database* Group::database()
{
    return m_db;
}

const Database* Group::database() const
{
    return m_db;
}

QList<Group*> Group::children()
{
    return m_children;
}

const QList<Group*>& Group::children() const
{
    return m_children;
}

QList<Entry*> Group::entries()
{
    return m_entries;
}

const QList<Entry*>& Group::entries() const
{
    return m_entries;
}

QList<Entry*> Group::entriesRecursive(bool includeHistoryItems) const
{
    QList<Entry*> entryList;

    entryList.append(m_entries);

    if (includeHistoryItems) {
        for (Entry* entry : m_entries) {
            entryList.append(entry->historyItems());
        }
    }

    for (Group* group : m_children) {
        entryList.append(group->entriesRecursive(includeHistoryItems));
    }

    return entryList;
}

QList<Entry*> Group::referencesRecursive(const Entry* entry) const
{
    auto entries = entriesRecursive();
    return QtConcurrent::blockingFiltered(entries,
                                          [entry](const Entry* e) { return e->hasReferencesTo(entry->uuid()); });
}

Entry* Group::findEntryByUuid(const QUuid& uuid, bool recursive) const
{
    if (uuid.isNull()) {
        return nullptr;
    }

    auto entries = m_entries;
    if (recursive) {
        entries = entriesRecursive(false);
    }

    for (auto entry : entries) {
        if (entry->uuid() == uuid) {
            return entry;
        }
    }

    return nullptr;
}

Entry* Group::findEntryByPath(const QString& entryPath) const
{
    if (entryPath.isEmpty()) {
        return nullptr;
    }

    // Add a beginning slash if the search string contains a slash
    // We don't add a slash by default to allow searching by entry title
    QString normalizedEntryPath = entryPath;
    if (!normalizedEntryPath.startsWith("/") && normalizedEntryPath.contains("/")) {
        normalizedEntryPath = "/" + normalizedEntryPath;
    }
    return findEntryByPathRecursive(normalizedEntryPath, "/");
}

Entry* Group::findEntryBySearchTerm(const QString& term, EntryReferenceType referenceType)
{
    Q_ASSERT_X(referenceType != EntryReferenceType::Unknown,
               "Database::findEntryRecursive",
               "Can't search entry with \"referenceType\" parameter equal to \"Unknown\"");

    const QList<Group*> groups = groupsRecursive(true);

    for (const Group* group : groups) {
        bool found = false;
        const QList<Entry*>& entryList = group->entries();
        for (Entry* entry : entryList) {
            switch (referenceType) {
            case EntryReferenceType::Unknown:
                return nullptr;
            case EntryReferenceType::Title:
                found = entry->title() == term;
                break;
            case EntryReferenceType::UserName:
                found = entry->username() == term;
                break;
            case EntryReferenceType::Password:
                found = entry->password() == term;
                break;
            case EntryReferenceType::Url:
                found = entry->url() == term;
                break;
            case EntryReferenceType::Notes:
                found = entry->notes() == term;
                break;
            case EntryReferenceType::QUuid:
                found = entry->uuid() == QUuid::fromRfc4122(QByteArray::fromHex(term.toLatin1()));
                break;
            case EntryReferenceType::CustomAttributes:
                found = entry->attributes()->containsValue(term);
                break;
            }

            if (found) {
                return entry;
            }
        }
    }

    return nullptr;
}

Entry* Group::findEntryByPathRecursive(const QString& entryPath, const QString& basePath) const
{
    // Return the first entry that matches the full path OR if there is no leading
    // slash, return the first entry title that matches
    for (Entry* entry : entries()) {
        // clang-format off
        if (entryPath == (basePath + entry->title())
            || (!entryPath.startsWith("/") && entry->title() == entryPath)) {
            return entry;
        }
        // clang-format on
    }

    for (Group* group : children()) {
        Entry* entry = group->findEntryByPathRecursive(entryPath, basePath + group->name() + "/");
        if (entry != nullptr) {
            return entry;
        }
    }

    return nullptr;
}

Group* Group::findGroupByPath(const QString& groupPath)
{
    // normalize the groupPath by adding missing front and rear slashes. once.
    QString normalizedGroupPath;

    if (groupPath.isEmpty()) {
        normalizedGroupPath = QString("/"); // root group
    } else {
        // clang-format off
        normalizedGroupPath = (groupPath.startsWith("/") ? "" : "/")
            + groupPath
            + (groupPath.endsWith("/") ? "" : "/");
        // clang-format on
    }
    return findGroupByPathRecursive(normalizedGroupPath, "/");
}

Group* Group::findGroupByPathRecursive(const QString& groupPath, const QString& basePath)
{
    // paths must be normalized
    Q_ASSERT(groupPath.startsWith("/") && groupPath.endsWith("/"));
    Q_ASSERT(basePath.startsWith("/") && basePath.endsWith("/"));

    if (groupPath == basePath) {
        return this;
    }

    for (Group* innerGroup : children()) {
        QString innerBasePath = basePath + innerGroup->name() + "/";
        Group* group = innerGroup->findGroupByPathRecursive(groupPath, innerBasePath);
        if (group != nullptr) {
            return group;
        }
    }

    return nullptr;
}

QString Group::print(bool recursive, bool flatten, int depth)
{
    QString response;
    QString prefix;

    if (flatten) {
        const QString separator("/");
        prefix = hierarchy(depth).join(separator);
        if (!prefix.isEmpty()) {
            prefix += separator;
        }
    } else {
        prefix = QString("  ").repeated(depth);
    }

    if (entries().isEmpty() && children().isEmpty()) {
        response += prefix + tr("[empty]", "group has no children") + "\n";
        return response;
    }

    for (Entry* entry : entries()) {
        response += prefix + entry->title() + "\n";
    }

    for (Group* innerGroup : children()) {
        response += prefix + innerGroup->name() + "/\n";
        if (recursive) {
            response += innerGroup->print(recursive, flatten, depth + 1);
        }
    }

    return response;
}

QList<const Group*> Group::groupsRecursive(bool includeSelf) const
{
    QList<const Group*> groupList;
    if (includeSelf) {
        groupList.append(this);
    }

    for (const Group* group : asConst(m_children)) {
        groupList.append(group->groupsRecursive(true));
    }

    return groupList;
}

QList<Group*> Group::groupsRecursive(bool includeSelf)
{
    QList<Group*> groupList;
    if (includeSelf) {
        groupList.append(this);
    }

    for (Group* group : asConst(m_children)) {
        groupList.append(group->groupsRecursive(true));
    }

    return groupList;
}

QSet<QUuid> Group::customIconsRecursive() const
{
    QSet<QUuid> result;

    if (!iconUuid().isNull()) {
        result.insert(iconUuid());
    }

    const QList<Entry*> entryList = entriesRecursive(true);
    for (Entry* entry : entryList) {
        if (!entry->iconUuid().isNull()) {
            result.insert(entry->iconUuid());
        }
    }

    for (Group* group : m_children) {
        result.unite(group->customIconsRecursive());
    }

    return result;
}

QList<QString> Group::usernamesRecursive(int topN) const
{
    // Collect all usernames and sort for easy counting
    QHash<QString, int> countedUsernames;
    for (const auto* entry : entriesRecursive()) {
        const auto username = entry->username();
        if (!username.isEmpty() && !entry->isAttributeReference(EntryAttributes::UserNameKey)) {
            countedUsernames.insert(username, ++countedUsernames[username]);
        }
    }

    // Sort username/frequency pairs by frequency and name
    QList<QPair<QString, int>> sortedUsernames;
    for (const auto& key : countedUsernames.keys()) {
        sortedUsernames.append({key, countedUsernames[key]});
    }

    auto comparator = [](const QPair<QString, int>& arg1, const QPair<QString, int>& arg2) {
        if (arg1.second == arg2.second) {
            return arg1.first < arg2.first;
        }
        return arg1.second > arg2.second;
    };

    std::sort(sortedUsernames.begin(), sortedUsernames.end(), comparator);

    // Take first topN usernames if set
    QList<QString> usernames;
    int actualUsernames = topN < 0 ? sortedUsernames.size() : std::min(topN, sortedUsernames.size());
    for (int i = 0; i < actualUsernames; i++) {
        usernames.append(sortedUsernames[i].first);
    }

    return usernames;
}

Group* Group::findGroupByUuid(const QUuid& uuid)
{
    if (uuid.isNull()) {
        return nullptr;
    }

    for (Group* group : groupsRecursive(true)) {
        if (group->uuid() == uuid) {
            return group;
        }
    }

    return nullptr;
}

const Group* Group::findGroupByUuid(const QUuid& uuid) const
{
    if (uuid.isNull()) {
        return nullptr;
    }

    for (const Group* group : groupsRecursive(true)) {
        if (group->uuid() == uuid) {
            return group;
        }
    }

    return nullptr;
}

Group* Group::findChildByName(const QString& name)
{
    for (Group* group : asConst(m_children)) {
        if (group->name() == name) {
            return group;
        }
    }

    return nullptr;
}

/**
 * Creates a duplicate of this group.
 * Note that you need to copy the custom icons manually when inserting the
 * new group into another database.
 */
Group* Group::clone(Entry::CloneFlags entryFlags, Group::CloneFlags groupFlags) const
{
    auto clonedGroup = new Group();

    clonedGroup->setUpdateTimeinfo(false);

    if (groupFlags & Group::CloneNewUuid) {
        clonedGroup->setUuid(QUuid::createUuid());
    } else {
        clonedGroup->setUuid(this->uuid());
    }

    clonedGroup->m_data = m_data;
    clonedGroup->m_customData->copyDataFrom(m_customData);

    if (groupFlags & Group::CloneIncludeEntries) {
        const QList<Entry*> entryList = entries();
        for (Entry* entry : entryList) {
            Entry* clonedEntry = entry->clone(entryFlags);
            clonedEntry->setGroup(clonedGroup);
        }

        const QList<Group*> childrenGroups = children();
        for (Group* groupChild : childrenGroups) {
            Group* clonedGroupChild = groupChild->clone(entryFlags, groupFlags);
            clonedGroupChild->setParent(clonedGroup);
        }
    }

    clonedGroup->setUpdateTimeinfo(true);
    if (groupFlags & Group::CloneResetTimeInfo) {

        QDateTime now = Clock::currentDateTimeUtc();
        clonedGroup->m_data.timeInfo.setCreationTime(now);
        clonedGroup->m_data.timeInfo.setLastModificationTime(now);
        clonedGroup->m_data.timeInfo.setLastAccessTime(now);
        clonedGroup->m_data.timeInfo.setLocationChanged(now);
    }

    if (groupFlags & Group::CloneRenameTitle) {
        clonedGroup->setName(tr("%1 - Clone").arg(name()));
    }

    return clonedGroup;
}

void Group::copyDataFrom(const Group* other)
{
    if (set(m_data, other->m_data)) {
        emit groupDataChanged(this);
    }
    m_customData->copyDataFrom(other->m_customData);
    m_lastTopVisibleEntry = other->m_lastTopVisibleEntry;
}

void Group::addEntry(Entry* entry)
{
    Q_ASSERT(entry);
    Q_ASSERT(!m_entries.contains(entry));

    emit entryAboutToAdd(entry);

    m_entries << entry;
    connect(entry, &Entry::entryDataChanged, this, &Group::entryDataChanged);
    if (m_db) {
        connect(entry, &Entry::modified, m_db, &Database::markAsModified);
    }

    emitModified();
    emit entryAdded(entry);
}

void Group::removeEntry(Entry* entry)
{
    Q_ASSERT_X(m_entries.contains(entry),
               Q_FUNC_INFO,
               QString("Group %1 does not contain %2").arg(this->name(), entry->title()).toLatin1());

    emit entryAboutToRemove(entry);

    entry->disconnect(this);
    if (m_db) {
        entry->disconnect(m_db);
    }
    m_entries.removeAll(entry);
    emitModified();
    emit entryRemoved(entry);
}

void Group::moveEntryUp(Entry* entry)
{
    int row = m_entries.indexOf(entry);
    if (row <= 0) {
        return;
    }

    emit entryAboutToMoveUp(row);
    m_entries.move(row, row - 1);
    emit entryMovedUp();
    emit groupNonDataChange();
}

void Group::moveEntryDown(Entry* entry)
{
    int row = m_entries.indexOf(entry);
    if (row >= m_entries.size() - 1) {
        return;
    }

    emit entryAboutToMoveDown(row);
    m_entries.move(row, row + 1);
    emit entryMovedDown();
    emit groupNonDataChange();
}

void Group::connectDatabaseSignalsRecursive(Database* db)
{
    if (m_db) {
        disconnect(m_db);
    }

    for (Entry* entry : asConst(m_entries)) {
        if (m_db) {
            entry->disconnect(m_db);
        }
        if (db) {
            connect(entry, &Entry::modified, db, &Database::markAsModified);
        }
    }

    if (db) {
        // clang-format off
        connect(this, &Group::groupDataChanged, db, &Database::groupDataChanged);
        connect(this, &Group::groupAboutToRemove, db, &Database::groupAboutToRemove);
        connect(this, &Group::groupRemoved, db, &Database::groupRemoved);
        connect(this, &Group::groupAboutToAdd, db, &Database::groupAboutToAdd);
        connect(this, &Group::groupAdded, db, &Database::groupAdded);
        connect(this, &Group::aboutToMove, db, &Database::groupAboutToMove);
        connect(this, &Group::groupMoved, db, &Database::groupMoved);
        connect(this, &Group::groupNonDataChange, db, &Database::markNonDataChange);
        connect(this, &Group::modified, db, &Database::markAsModified);
        // clang-format on
    }

    m_db = db;

    for (Group* group : asConst(m_children)) {
        group->connectDatabaseSignalsRecursive(db);
    }
}

void Group::cleanupParent()
{
    if (m_parent) {
        emit groupAboutToRemove(this);
        m_parent->m_children.removeAll(this);
        emitModified();
        emit groupRemoved();
    }
}

void Group::recCreateDelObjects()
{
    if (m_db) {
        for (Entry* entry : asConst(m_entries)) {
            m_db->addDeletedObject(entry->uuid());
        }

        for (Group* group : asConst(m_children)) {
            group->recCreateDelObjects();
        }
        m_db->addDeletedObject(m_uuid);
    }
}

bool Group::resolveSearchingEnabled() const
{
    switch (m_data.searchingEnabled) {
    case Inherit:
        if (!m_parent) {
            return true;
        } else {
            return m_parent->resolveSearchingEnabled();
        }
    case Enable:
        return true;
    case Disable:
        return false;
    default:
        Q_ASSERT(false);
        return false;
    }
}

bool Group::resolveAutoTypeEnabled() const
{
    switch (m_data.autoTypeEnabled) {
    case Inherit:
        if (!m_parent) {
            return true;
        } else {
            return m_parent->resolveAutoTypeEnabled();
        }
    case Enable:
        return true;
    case Disable:
        return false;
    default:
        Q_ASSERT(false);
        return false;
    }
}

Entry* Group::addEntryWithPath(const QString& entryPath)
{
    if (entryPath.isEmpty() || findEntryByPath(entryPath)) {
        return nullptr;
    }

    QStringList groups = entryPath.split("/");
    QString entryTitle = groups.takeLast();
    QString groupPath = groups.join("/");

    Group* group = findGroupByPath(groupPath);
    if (!group) {
        return nullptr;
    }

    auto* entry = new Entry();
    entry->setTitle(entryTitle);
    entry->setUuid(QUuid::createUuid());
    entry->setGroup(group);

    return entry;
}

void Group::applyGroupIconOnCreateTo(Entry* entry)
{
    Q_ASSERT(entry);

    if (!config()->get(Config::UseGroupIconOnEntryCreation).toBool()) {
        return;
    }

    if ((iconNumber() == Group::DefaultIconNumber || iconNumber() == Group::OpenFolderIconNumber)
        && iconUuid().isNull()) {
        return;
    }

    applyGroupIconTo(entry);
}

void Group::applyGroupIconTo(Entry* entry)
{
    Q_ASSERT(entry);

    if (iconUuid().isNull()) {
        entry->setIcon(iconNumber());
    } else {
        entry->setIcon(iconUuid());
    }
}

void Group::applyGroupIconTo(Group* other)
{
    Q_ASSERT(other);

    if (iconUuid().isNull()) {
        other->setIcon(iconNumber());
    } else {
        other->setIcon(iconUuid());
    }
}

void Group::applyGroupIconToChildGroups()
{
    for (Group* recursiveChild : groupsRecursive(false)) {
        applyGroupIconTo(recursiveChild);
    }
}

void Group::applyGroupIconToChildEntries()
{
    for (Entry* recursiveEntry : entriesRecursive(false)) {
        applyGroupIconTo(recursiveEntry);
    }
}

void Group::sortChildrenRecursively(bool reverse)
{
    Group* recycleBin = nullptr;
    if (database()) {
        recycleBin = database()->metadata()->recycleBin();
    }
    std::sort(m_children.begin(), m_children.end(), [=](const Group* childGroup1, const Group* childGroup2) -> bool {
        if (childGroup1 == recycleBin) {
            return false;
        }
        QString name1 = childGroup1->name();
        QString name2 = childGroup2->name();
        return reverse ? name1.compare(name2, Qt::CaseInsensitive) > 0 : name1.compare(name2, Qt::CaseInsensitive) < 0;
    });

    for (auto child : m_children) {
        child->sortChildrenRecursively(reverse);
    }

    emitModified();
}

const Group* Group::previousParentGroup() const
{
    if (!database() || !database()->rootGroup()) {
        return nullptr;
    }
    return database()->rootGroup()->findGroupByUuid(m_data.previousParentGroupUuid);
}

QUuid Group::previousParentGroupUuid() const
{
    return m_data.previousParentGroupUuid;
}

void Group::setPreviousParentGroupUuid(const QUuid& uuid)
{
    set(m_data.previousParentGroupUuid, uuid);
}

void Group::setPreviousParentGroup(const Group* group)
{
    setPreviousParentGroupUuid(group ? group->uuid() : QUuid());
}

bool Group::GroupData::operator==(const Group::GroupData& other) const
{
    return equals(other, CompareItemDefault);
}

bool Group::GroupData::operator!=(const Group::GroupData& other) const
{
    return !(*this == other);
}

bool Group::GroupData::equals(const Group::GroupData& other, CompareItemOptions options) const
{
    if (::compare(name, other.name, options) != 0) {
        return false;
    }
    if (::compare(notes, other.notes, options) != 0) {
        return false;
    }
    if (::compare(tags, other.tags, options) != 0) {
        return false;
    }
    if (::compare(iconNumber, other.iconNumber) != 0) {
        return false;
    }
    if (::compare(customIcon, other.customIcon) != 0) {
        return false;
    }
    if (!timeInfo.equals(other.timeInfo, options)) {
        return false;
    }
    // TODO HNH: Some properties are configurable - should they be ignored?
    if (::compare(isExpanded, other.isExpanded, options) != 0) {
        return false;
    }
    if (::compare(defaultAutoTypeSequence, other.defaultAutoTypeSequence, options) != 0) {
        return false;
    }
    if (::compare(autoTypeEnabled, other.autoTypeEnabled, options) != 0) {
        return false;
    }
    if (::compare(searchingEnabled, other.searchingEnabled, options) != 0) {
        return false;
    }
    if (::compare(mergeMode, other.mergeMode, options) != 0) {
        return false;
    }
    return true;
}
