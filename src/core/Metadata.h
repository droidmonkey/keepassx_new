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

#ifndef KEEPASSX_METADATA_H
#define KEEPASSX_METADATA_H

#include <QDateTime>
#include <QHash>
#include <QPointer>
#include <QUuid>
#include <QVariantMap>

#include "core/CustomData.h"
#include "core/Global.h"

class Database;
class Group;

class Metadata : public ModifiableObject
{
    Q_OBJECT

public:
    explicit Metadata(QObject* parent = nullptr);

    struct MetadataData
    {
        QString generator;
        QString name;
        QDateTime nameChanged;
        QString description;
        QDateTime descriptionChanged;
        QString defaultUserName;
        QDateTime defaultUserNameChanged;
        int maintenanceHistoryDays;
        QString color;
        bool recycleBinEnabled;
        int historyMaxItems;
        int historyMaxSize;
        int masterKeyChangeRec;
        int masterKeyChangeForce;

        bool protectTitle;
        bool protectUsername;
        bool protectPassword;
        bool protectUrl;
        bool protectNotes;
    };

    struct CustomIconData
    {
        QByteArray data;
        QString name;
        QDateTime lastModified;

        bool operator==(const CustomIconData& rhs) const
        {
            // Compare only actual icon data
            return data == rhs.data;
        }
    };

    void init();
    void clear();

    QString generator() const;
    QString name() const;
    QDateTime nameChanged() const;
    QString description() const;
    QDateTime descriptionChanged() const;
    QString defaultUserName() const;
    QDateTime defaultUserNameChanged() const;
    QDateTime settingsChanged() const;
    int maintenanceHistoryDays() const;
    QString color() const;
    bool protectTitle() const;
    bool protectUsername() const;
    bool protectPassword() const;
    bool protectUrl() const;
    bool protectNotes() const;
    const CustomIconData& customIcon(const QUuid& uuid) const;
    bool hasCustomIcon(const QUuid& uuid) const;
    QList<QUuid> customIconsOrder() const;
    bool recycleBinEnabled() const;
    Group* recycleBin();
    const Group* recycleBin() const;
    QDateTime recycleBinChanged() const;
    const Group* entryTemplatesGroup() const;
    QDateTime entryTemplatesGroupChanged() const;
    const Group* lastSelectedGroup() const;
    const Group* lastTopVisibleGroup() const;
    QDateTime databaseKeyChanged() const;
    int databaseKeyChangeRec() const;
    int databaseKeyChangeForce() const;
    int historyMaxItems() const;
    int historyMaxSize() const;
    int autosaveDelayMin() const;
    CustomData* customData();
    const CustomData* customData() const;

    static const int DefaultHistoryMaxItems;
    static const int DefaultHistoryMaxSize;
    static const int DefaultAutosaveDelayMin;

    void setGenerator(const QString& value);
    void setName(const QString& value);
    void setNameChanged(const QDateTime& value);
    void setDescription(const QString& value);
    void setDescriptionChanged(const QDateTime& value);
    void setDefaultUserName(const QString& value);
    void setDefaultUserNameChanged(const QDateTime& value);
    void setSettingsChanged(const QDateTime& value);
    void setMaintenanceHistoryDays(int value);
    void setColor(const QString& value);
    void setProtectTitle(bool value);
    void setProtectUsername(bool value);
    void setProtectPassword(bool value);
    void setProtectUrl(bool value);
    void setProtectNotes(bool value);
    void addCustomIcon(const QUuid& uuid, const CustomIconData& iconData);
    void addCustomIcon(const QUuid& uuid,
                       const QByteArray& iconBytes,
                       const QString& name = {},
                       const QDateTime& lastModified = {});
    void removeCustomIcon(const QUuid& uuid);
    void copyCustomIcon(const QUuid& iconUuid, const Metadata* otherMetadata);
    void copyCustomIcons(const QSet<QUuid>& iconList, const Metadata* otherMetadata);
    QUuid findCustomIcon(const QByteArray& candidate);
    void setRecycleBinEnabled(bool value);
    void setRecycleBin(Group* group);
    void setRecycleBinChanged(const QDateTime& value);
    void setEntryTemplatesGroup(Group* group);
    void setEntryTemplatesGroupChanged(const QDateTime& value);
    void setLastSelectedGroup(Group* group);
    void setLastTopVisibleGroup(Group* group);
    void setDatabaseKeyChanged(const QDateTime& value);
    void setMasterKeyChangeRec(int value);
    void setMasterKeyChangeForce(int value);
    void setHistoryMaxItems(int value);
    void setHistoryMaxSize(int value);
    void setAutosaveDelayMin(int value);
    void setUpdateDatetime(bool value);
    void addSavedSearch(const QString& name, const QString& searchtext);
    void deleteSavedSearch(const QString& name);
    QVariantMap savedSearches();
    /*
     * Copy all attributes from other except:
     * - Group pointers/uuids
     * - Database key changed date
     * - Custom icons
     * - Custom fields
     * - Settings changed date
     */
    void copyAttributesFrom(const Metadata* other);

private:
    template <class P, class V> bool set(P& property, const V& value);
    template <class P, class V> bool set(P& property, const V& value, QDateTime& dateTime);

    QByteArray hashIcon(const QByteArray& iconData);

    MetadataData m_data;

    QList<QUuid> m_customIconsOrder;
    QHash<QUuid, CustomIconData> m_customIcons;
    QHash<QByteArray, QUuid> m_customIconsHashes;

    QPointer<Group> m_recycleBin;
    QDateTime m_recycleBinChanged;
    QPointer<Group> m_entryTemplatesGroup;
    QDateTime m_entryTemplatesGroupChanged;
    QPointer<Group> m_lastSelectedGroup;
    QPointer<Group> m_lastTopVisibleGroup;

    QDateTime m_masterKeyChanged;
    QDateTime m_settingsChanged;

    QPointer<CustomData> m_customData;

    bool m_updateDatetime;
};

#endif // KEEPASSX_METADATA_H
