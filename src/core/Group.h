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

#ifndef KEEPASSX_GROUP_H
#define KEEPASSX_GROUP_H

#include <QPointer>
#include <QList>
#include <utility>

#include "core/CustomData.h"
#include "core/Database.h"
#include "core/Entry.h"


class Entry;
class Group;


template <typename TCallable> concept CGroupVisitor = std::is_invocable_v<TCallable, Group*>;
template <typename TCallable> concept CGroupConstVisitor = std::is_invocable_v<TCallable, const Group*>;
template <typename TCallable> concept CEntryVisitor = std::is_invocable_v<TCallable, Entry*>;
template <typename TCallable> concept CEntryConstVisitor = std::is_invocable_v<TCallable, const Entry*>;


class Group : public ModifiableObject
{
    Q_OBJECT

public:
    enum TriState
    {
        Inherit,
        Enable,
        Disable
    };
    enum MergeMode
    {
        Default, // Determine merge strategy from parent or fallback (Synchronize)
        KeepNewer, // merge history
        Synchronize, // merge history keeping most recent as top entry and applying deletions
    };

    enum CloneFlag
    {
        CloneNoFlags = 0,
        CloneNewUuid = 1, // generate a random uuid for the clone
        CloneResetCreationTime = 2, // set timeInfo.CreationTime to the current time
        CloneResetLastAccessTime = 4, // set timeInfo.LastAccessTime to the current time
        CloneResetLocationChangedTime = 8, // set timeInfo.LocationChangedTime to the current time
        CloneIncludeEntries = 16, // clone the group entries
        CloneRenameTitle = 32, // add "- Clone" after the original title

        CloneResetTimeInfo = CloneResetCreationTime | CloneResetLastAccessTime | CloneResetLocationChangedTime,
        CloneExactCopy = CloneIncludeEntries,
        CloneCopy = CloneExactCopy | CloneNewUuid | CloneResetTimeInfo,
        CloneDefault = CloneCopy,
    };
    Q_DECLARE_FLAGS(CloneFlags, CloneFlag)

    struct GroupData
    {
        QString name;
        QString notes;
        int iconNumber;
        QUuid customIcon;
        TimeInfo timeInfo;
        bool isExpanded;
        QString defaultAutoTypeSequence;
        Group::TriState autoTypeEnabled;
        Group::TriState searchingEnabled;
        Group::MergeMode mergeMode;
        QString tags;
        QUuid previousParentGroupUuid;

        bool operator==(const GroupData& other) const;
        bool operator!=(const GroupData& other) const;
        bool equals(const GroupData& other, CompareItemOptions options) const;
    };

    Group();
    ~Group() override;

    const QUuid& uuid() const;
    const QString uuidToHex() const;
    QString name() const;
    QString notes() const;
    QString tags() const;
    QString fullPath() const;
    int iconNumber() const;
    const QUuid& iconUuid() const;
    const TimeInfo& timeInfo() const;
    bool isExpanded() const;
    QString defaultAutoTypeSequence() const;
    QString effectiveAutoTypeSequence() const;
    Group::TriState autoTypeEnabled() const;
    Group::TriState searchingEnabled() const;
    Group::MergeMode mergeMode() const;
    bool resolveSearchingEnabled() const;
    bool resolveAutoTypeEnabled() const;
    Entry* lastTopVisibleEntry() const;
    bool isExpired() const;
    bool isRecycled() const;
    bool isEmpty() const;
    bool isShared() const;
    CustomData* customData();
    const CustomData* customData() const;
    Group::TriState resolveCustomDataTriState(const QString& key, bool checkParent = true) const;
    void setCustomDataTriState(const QString& key, const Group::TriState& value);
    QString resolveCustomDataString(const QString& key, bool checkParent = true) const;
    const Group* previousParentGroup() const;
    QUuid previousParentGroupUuid() const;

    bool equals(const Group* other, CompareItemOptions options) const;

    static const int DefaultIconNumber;
    static const int OpenFolderIconNumber;
    static const int RecycleBinIconNumber;
    static const QString RootAutoTypeSequence;

    Group* findChildByName(const QString& name);
    Entry* findEntryByUuid(const QUuid& uuid, bool recursive = true) const;
    Entry* findEntryByPath(const QString& entryPath) const;
    Entry* findEntryBySearchTerm(const QString& term, EntryReferenceType referenceType);
    Group* findGroupByUuid(const QUuid& uuid);
    const Group* findGroupByUuid(const QUuid& uuid) const;
    Group* findGroupByPath(const QString& groupPath);
    Entry* addEntryWithPath(const QString& entryPath);
    void setUuid(const QUuid& uuid);
    void setName(const QString& name);
    void setNotes(const QString& notes);
    void setTags(const QString& tags);
    void setIcon(int iconNumber);
    void setIcon(const QUuid& uuid);
    void setTimeInfo(const TimeInfo& timeInfo);
    void setExpanded(bool expanded);
    void setDefaultAutoTypeSequence(const QString& sequence);
    void setAutoTypeEnabled(TriState enable);
    void setSearchingEnabled(TriState enable);
    void setLastTopVisibleEntry(Entry* entry);
    void setExpires(bool value);
    void setExpiryTime(const QDateTime& dateTime);
    void setMergeMode(MergeMode newMode);
    void setPreviousParentGroup(const Group* group);
    void setPreviousParentGroupUuid(const QUuid& uuid);

    bool canUpdateTimeinfo() const;
    void setUpdateTimeinfo(bool value);

    Group* parentGroup();
    const Group* parentGroup() const;
    void setParent(Group* parent, int index = -1, bool trackPrevious = true);
    QStringList hierarchy(int height = -1) const;
    bool hasChildren() const;
    bool isDescendantOf(const Group* group) const;

    Database* database();
    const Database* database() const;
    QList<Group*> children();
    const QList<Group*>& children() const;
    QList<Entry*> entries();
    const QList<Entry*>& entries() const;
    QList<Entry*> referencesRecursive(const Entry* entry) const;
    QList<Entry*> entriesRecursive(bool includeHistoryItems = false) const;
    QList<const Group*> groupsRecursive(bool includeSelf) const;
    QList<Group*> groupsRecursive(bool includeSelf);

    /**
    * Walk methods for traversing the tree (depth-first search)
    *
    * @param[in] includeSelf is the current group to be included or excluded
    *     if `false` the current group's entries will not be included either
    * @param[in] groupVisitor functor that takes a single argument: ([const] Group*)
    *     the functor may return a bool to indicate whether to stop=`true` or continue=`false` traversing
    *     for a non-`bool` return-type the value is ignored and the traversing will continue as if `false` had been returned
    * @param[in] entryVisitor functor that takes a single argument: ([const] Entry*)
    *     the functor may return a bool to indicate whether to stop=`true` or continue=`false` traversing
    *     for a non-`bool` return-type the value is ignored and the traversing will continue as if `false` had been returned
    * @return `false` if the traversing completed without stop, or `true` otherwise
    */
    template <CGroupVisitor TGroupCallable, CEntryVisitor TEntryCallable>
    bool walk(bool includeSelf, TGroupCallable&& groupVisitor, TEntryCallable&& entryVisitor)
    {
        return walk<TGroupCallable, TEntryCallable, false, true, true>(
            includeSelf, std::forward<TGroupCallable>(groupVisitor), std::forward<TEntryCallable>(entryVisitor));
    }
    template <CGroupConstVisitor TGroupCallable, CEntryConstVisitor TEntryCallable>
    bool walk(bool includeSelf, TGroupCallable&& groupVisitor, TEntryCallable&& entryVisitor) const
    {
        return walk<TGroupCallable, TEntryCallable, true, true, true>(
            includeSelf, std::forward<TGroupCallable>(groupVisitor), std::forward<TEntryCallable>(entryVisitor));
    }
    template <CGroupConstVisitor TGroupCallable> bool walkGroups(bool includeSelf, TGroupCallable&& groupVisitor) const
    {
        return walk<TGroupCallable, void*, true, true, false>(
            includeSelf, std::forward<TGroupCallable>(groupVisitor), nullptr);
    }
    template <CGroupVisitor TGroupCallable> bool walkGroups(bool includeSelf, TGroupCallable&& groupVisitor)
    {
        return walk<TGroupCallable, void*, false, true, false>(
            includeSelf, std::forward<TGroupCallable>(groupVisitor), nullptr);
    }
    template <CEntryConstVisitor TEntryCallable> bool walkEntries(TEntryCallable&& entryVisitor) const
    {
        return walk<void*, TEntryCallable, true, false, true>(
            true, nullptr, std::forward<TEntryCallable>(entryVisitor));
    }
    template <CEntryVisitor TEntryCallable> bool walkEntries(TEntryCallable&& entryVisitor)
    {
        return walk<void*, TEntryCallable, false, false, true>(
            true, nullptr, std::forward<TEntryCallable>(entryVisitor));
    }

    QSet<QUuid> customIconsRecursive() const;
    QList<QString> usernamesRecursive(int topN = -1) const;

    Group* clone(Entry::CloneFlags entryFlags = Entry::CloneDefault,
                 Group::CloneFlags groupFlags = Group::CloneDefault) const;

    void copyDataFrom(const Group* other);
    QString print(bool recursive = false, bool flatten = false, int depth = 0);

    void addEntry(Entry* entry);
    void removeEntry(Entry* entry);
    void moveEntryUp(Entry* entry);
    void moveEntryDown(Entry* entry);

    void applyGroupIconOnCreateTo(Entry* entry);
    void applyGroupIconTo(Entry* entry);
    void applyGroupIconTo(Group* other);
    void applyGroupIconToChildGroups();
    void applyGroupIconToChildEntries();

    void sortChildrenRecursively(bool reverse = false);

signals:
    void groupDataChanged(Group* group);
    void groupAboutToAdd(Group* group, int index);
    void groupAdded();
    void groupAboutToRemove(Group* group);
    void groupRemoved();
    void aboutToMove(Group* group, Group* toGroup, int index);
    void groupMoved();
    void groupNonDataChange();
    void entryAboutToAdd(Entry* entry);
    void entryAdded(Entry* entry);
    void entryAboutToRemove(Entry* entry);
    void entryRemoved(Entry* entry);
    void entryAboutToMoveUp(int row);
    void entryMovedUp();
    void entryAboutToMoveDown(int row);
    void entryMovedDown();
    void entryDataChanged(Entry* entry);

private slots:
    void updateTimeinfo();

private:
    template <typename TGroupCallable, typename TEntryCallable, bool kIsConst, bool kVisitGroups, bool kVisitEntries>
    bool walk(bool includeSelf, TGroupCallable&& groupVisitor, TEntryCallable&& entryVisitor) const;
    template <class P, class V> bool set(P& property, const V& value, bool preserveTimeinfo = false);

    void emitModifiedEx(bool preserveTimeinfo);
    void setParent(Database* db);

    void connectDatabaseSignalsRecursive(Database* db);
    void cleanupParent();
    void recCreateDelObjects();

    Entry* findEntryByPathRecursive(const QString& entryPath, const QString& basePath) const;
    Group* findGroupByPathRecursive(const QString& groupPath, const QString& basePath);

    QPointer<Database> m_db;
    QUuid m_uuid;
    GroupData m_data;
    QPointer<Entry> m_lastTopVisibleEntry;
    QList<Group*> m_children;
    QList<Entry*> m_entries;

    QPointer<CustomData> m_customData;

    QPointer<Group> m_parent;

    bool m_updateTimeinfo;

    friend Group* Database::setRootGroup(Group* group);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Group::CloneFlags)

// helpers to support non-bool returning callables
template <bool kDefaultRetVal, typename TCallable, typename... Args>
bool visitorPredicateImpl(std::true_type, TCallable&& callable, Args&&... args)
{
    return callable(std::forward<Args>(args)...);
}

template <bool kDefaultRetVal, typename TCallable, typename... Args>
bool visitorPredicateImpl(std::false_type, TCallable&& callable, Args&&... args)
{
    callable(std::forward<Args>(args)...);
    return kDefaultRetVal;
}

template <bool kDefaultRetVal, typename TCallable, typename... Args>
bool visitorPredicate(TCallable&& callable, Args&&... args)
{
    using RetType = decltype(callable(args...));
    return visitorPredicateImpl<kDefaultRetVal>(
        std::is_same<RetType, bool>{}, std::forward<TCallable>(callable), std::forward<Args>(args)...);
}

template<typename TGroupCallable, typename TEntryCallable, bool kIsConst, bool kVisitGroups, bool kVisitEntries>
bool Group::walk(bool includeSelf, TGroupCallable&& groupVisitor, TEntryCallable&& entryVisitor) const
{
    using GroupType = typename std::conditional<kIsConst,const Group, Group>::type;
    QList<Group*> groupsToVisit;
    if (includeSelf) {
        groupsToVisit.append(const_cast<Group*>(this));
    } else {
        groupsToVisit.append(m_children);
    }
    while (!groupsToVisit.isEmpty()) {
        GroupType* group = groupsToVisit.takeLast(); // right-to-left
        if constexpr (kVisitGroups) {
            if (visitorPredicate<false>(groupVisitor, group)) {
                return true;
            }
        }
        if constexpr (kVisitEntries) {
            for (auto* entry : group->m_entries) {
                if (visitorPredicate<false>(entryVisitor, entry)) {
                    return true;
                }
            }
        }
        groupsToVisit.append(group->m_children);
    }
    return false;
}

#endif // KEEPASSX_GROUP_H
