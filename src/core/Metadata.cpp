/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#include "Metadata.h"

#include "core/Clock.h"
#include "core/Group.h"

#include <QCryptographicHash>
#include <QJsonDocument>

const int Metadata::DefaultHistoryMaxItems = 10;
const int Metadata::DefaultHistoryMaxSize = 6 * 1024 * 1024;
const int Metadata::DefaultAutosaveDelayMin = 0;

// Fallback icon for return by reference
static const Metadata::CustomIconData NULL_ICON{};

namespace customDataKeys
{
    static const QString savedSearch = QStringLiteral("KPXC_SavedSearch");
    static const QString autosaveDelay = QStringLiteral("KPXC_autosaveDelayMin");
}; // namespace customDataKeys

Metadata::Metadata(QObject* parent)
    : ModifiableObject(parent)
    , m_customData(new CustomData(this))
    , m_updateDatetime(true)
{
    init();
    connect(m_customData, &CustomData::modified, this, &Metadata::modified);
}

void Metadata::init()
{
    m_data.generator = QStringLiteral("KeePassXC");
    m_data.maintenanceHistoryDays = 365;
    m_data.masterKeyChangeRec = -1;
    m_data.masterKeyChangeForce = -1;
    m_data.historyMaxItems = DefaultHistoryMaxItems;
    m_data.historyMaxSize = DefaultHistoryMaxSize;
    m_data.recycleBinEnabled = true;
    m_data.protectTitle = false;
    m_data.protectUsername = false;
    m_data.protectPassword = true;
    m_data.protectUrl = false;
    m_data.protectNotes = false;

    QDateTime now = Clock::currentDateTimeUtc();
    m_data.nameChanged = now;
    m_data.descriptionChanged = now;
    m_data.defaultUserNameChanged = now;
    m_recycleBinChanged = now;
    m_entryTemplatesGroupChanged = now;
    m_masterKeyChanged = now;
    m_settingsChanged = now;
}

void Metadata::clear()
{
    init();
    m_customIcons.clear();
    m_customIconsOrder.clear();
    m_customIconsHashes.clear();
    m_customData->clear();
}

template <class P, class V> bool Metadata::set(P& property, const V& value)
{
    if (property != value) {
        property = value;
        emitModified();
        return true;
    } else {
        return false;
    }
}

template <class P, class V> bool Metadata::set(P& property, const V& value, QDateTime& dateTime)
{
    if (property != value) {
        property = value;
        if (m_updateDatetime) {
            dateTime = Clock::currentDateTimeUtc();
        }
        emitModified();
        return true;
    } else {
        return false;
    }
}

void Metadata::setUpdateDatetime(bool value)
{
    m_updateDatetime = value;
}

void Metadata::copyAttributesFrom(const Metadata* other)
{
    m_data = other->m_data;
}

QString Metadata::generator() const
{
    return m_data.generator;
}

QString Metadata::name() const
{
    return m_data.name;
}

QDateTime Metadata::nameChanged() const
{
    return m_data.nameChanged;
}

QString Metadata::description() const
{
    return m_data.description;
}

QDateTime Metadata::descriptionChanged() const
{
    return m_data.descriptionChanged;
}

QString Metadata::defaultUserName() const
{
    return m_data.defaultUserName;
}

QDateTime Metadata::defaultUserNameChanged() const
{
    return m_data.defaultUserNameChanged;
}

int Metadata::maintenanceHistoryDays() const
{
    return m_data.maintenanceHistoryDays;
}

QString Metadata::color() const
{
    return m_data.color;
}

bool Metadata::protectTitle() const
{
    return m_data.protectTitle;
}

bool Metadata::protectUsername() const
{
    return m_data.protectUsername;
}

bool Metadata::protectPassword() const
{
    return m_data.protectPassword;
}

bool Metadata::protectUrl() const
{
    return m_data.protectUrl;
}

bool Metadata::protectNotes() const
{
    return m_data.protectNotes;
}

const Metadata::CustomIconData& Metadata::customIcon(const QUuid& uuid) const
{
    auto icon = m_customIcons.find(uuid);
    Q_ASSERT(icon != m_customIcons.end());
    if (icon == m_customIcons.end()) {
        return NULL_ICON;
    }
    return icon.value();
}

bool Metadata::hasCustomIcon(const QUuid& uuid) const
{
    return m_customIcons.contains(uuid);
}

QList<QUuid> Metadata::customIconsOrder() const
{
    return m_customIconsOrder;
}

bool Metadata::recycleBinEnabled() const
{
    return m_data.recycleBinEnabled;
}

Group* Metadata::recycleBin()
{
    return m_recycleBin;
}

const Group* Metadata::recycleBin() const
{
    return m_recycleBin;
}

QDateTime Metadata::recycleBinChanged() const
{
    return m_recycleBinChanged;
}

const Group* Metadata::entryTemplatesGroup() const
{
    return m_entryTemplatesGroup;
}

QDateTime Metadata::entryTemplatesGroupChanged() const
{
    return m_entryTemplatesGroupChanged;
}

const Group* Metadata::lastSelectedGroup() const
{
    return m_lastSelectedGroup;
}

const Group* Metadata::lastTopVisibleGroup() const
{
    return m_lastTopVisibleGroup;
}

QDateTime Metadata::databaseKeyChanged() const
{
    return m_masterKeyChanged;
}

int Metadata::databaseKeyChangeRec() const
{
    return m_data.masterKeyChangeRec;
}

int Metadata::databaseKeyChangeForce() const
{
    return m_data.masterKeyChangeForce;
}

int Metadata::historyMaxItems() const
{
    return m_data.historyMaxItems;
}

int Metadata::historyMaxSize() const
{
    return m_data.historyMaxSize;
}

int Metadata::autosaveDelayMin() const
{
    QString autosaveDelayMinStr = m_customData->value(customDataKeys::autosaveDelay);
    if (autosaveDelayMinStr.isNull()) {
        // data is not set yet, use default
        return Metadata::DefaultAutosaveDelayMin;
    }
    bool ok; // check for QString to int op failure
    int autosaveDelayMin = autosaveDelayMinStr.toInt(&ok);
    Q_ASSERT(ok);
    return autosaveDelayMin;
}

CustomData* Metadata::customData()
{
    return m_customData;
}

const CustomData* Metadata::customData() const
{
    return m_customData;
}

void Metadata::setGenerator(const QString& value)
{
    set(m_data.generator, value);
}

void Metadata::setName(const QString& value)
{
    set(m_data.name, value, m_data.nameChanged);
}

void Metadata::setNameChanged(const QDateTime& value)
{
    Q_ASSERT(value.timeSpec() == Qt::UTC);
    m_data.nameChanged = value;
}

void Metadata::setDescription(const QString& value)
{
    set(m_data.description, value, m_data.descriptionChanged);
}

void Metadata::setDescriptionChanged(const QDateTime& value)
{
    Q_ASSERT(value.timeSpec() == Qt::UTC);
    m_data.descriptionChanged = value;
}

void Metadata::setDefaultUserName(const QString& value)
{
    set(m_data.defaultUserName, value, m_data.defaultUserNameChanged);
}

void Metadata::setDefaultUserNameChanged(const QDateTime& value)
{
    Q_ASSERT(value.timeSpec() == Qt::UTC);
    m_data.defaultUserNameChanged = value;
}

void Metadata::setMaintenanceHistoryDays(int value)
{
    set(m_data.maintenanceHistoryDays, value);
}

void Metadata::setColor(const QString& value)
{
    set(m_data.color, value);
}

void Metadata::setProtectTitle(bool value)
{
    set(m_data.protectTitle, value);
}

void Metadata::setProtectUsername(bool value)
{
    set(m_data.protectUsername, value);
}

void Metadata::setProtectPassword(bool value)
{
    set(m_data.protectPassword, value);
}

void Metadata::setProtectUrl(bool value)
{
    set(m_data.protectUrl, value);
}

void Metadata::setProtectNotes(bool value)
{
    set(m_data.protectNotes, value);
}

void Metadata::addCustomIcon(const QUuid& uuid, const CustomIconData& iconData)
{

    Q_ASSERT(!uuid.isNull());
    Q_ASSERT(!m_customIcons.contains(uuid));

    // remove all uuids to prevent duplicates in release mode
    m_customIcons[uuid] = iconData;
    m_customIconsOrder.removeAll(uuid);
    m_customIconsOrder.append(uuid);

    // Associate image hash to uuid
    QByteArray hash = hashIcon(iconData.data);
    m_customIconsHashes[hash] = uuid;
    Q_ASSERT(m_customIcons.count() == m_customIconsOrder.count());

    emitModified();
}

void Metadata::addCustomIcon(const QUuid& uuid,
                             const QByteArray& iconBytes,
                             const QString& name,
                             const QDateTime& lastModified)
{
    addCustomIcon(uuid, {iconBytes, name, lastModified});
}

void Metadata::removeCustomIcon(const QUuid& uuid)
{
    Q_ASSERT(!uuid.isNull());
    Q_ASSERT(m_customIcons.contains(uuid));

    // Remove hash record only if this is the same uuid
    QByteArray hash = hashIcon(m_customIcons[uuid].data);
    if (m_customIconsHashes.contains(hash) && m_customIconsHashes[hash] == uuid) {
        m_customIconsHashes.remove(hash);
    }

    m_customIcons.remove(uuid);
    m_customIconsOrder.removeAll(uuid);
    Q_ASSERT(m_customIcons.count() == m_customIconsOrder.count());
    dynamic_cast<Database*>(parent())->addDeletedObject(uuid);
    emitModified();
}

QUuid Metadata::findCustomIcon(const QByteArray& candidate)
{
    QByteArray hash = hashIcon(candidate);
    return m_customIconsHashes.value(hash, QUuid());
}

void Metadata::copyCustomIcons(const QSet<QUuid>& iconList, const Metadata* otherMetadata)
{
    for (const QUuid& uuid : iconList) {
        Q_ASSERT(otherMetadata->hasCustomIcon(uuid));

        if (!hasCustomIcon(uuid) && otherMetadata->hasCustomIcon(uuid)) {
            addCustomIcon(uuid, otherMetadata->customIcon(uuid));
        }
    }
}

QByteArray Metadata::hashIcon(const QByteArray& iconData)
{
    return QCryptographicHash::hash(iconData, QCryptographicHash::Md5);
}

void Metadata::setRecycleBinEnabled(bool value)
{
    set(m_data.recycleBinEnabled, value);
}

void Metadata::setRecycleBin(Group* group)
{
    set(m_recycleBin, group, m_recycleBinChanged);
}

void Metadata::setRecycleBinChanged(const QDateTime& value)
{
    Q_ASSERT(value.timeSpec() == Qt::UTC);
    m_recycleBinChanged = value;
}

void Metadata::setEntryTemplatesGroup(Group* group)
{
    set(m_entryTemplatesGroup, group, m_entryTemplatesGroupChanged);
}

void Metadata::setEntryTemplatesGroupChanged(const QDateTime& value)
{
    Q_ASSERT(value.timeSpec() == Qt::UTC);
    m_entryTemplatesGroupChanged = value;
}

void Metadata::setLastSelectedGroup(Group* group)
{
    set(m_lastSelectedGroup, group);
}

void Metadata::setLastTopVisibleGroup(Group* group)
{
    set(m_lastTopVisibleGroup, group);
}

void Metadata::setDatabaseKeyChanged(const QDateTime& value)
{
    Q_ASSERT(value.timeSpec() == Qt::UTC);
    m_masterKeyChanged = value;
}

void Metadata::setMasterKeyChangeRec(int value)
{
    set(m_data.masterKeyChangeRec, value);
}

void Metadata::setMasterKeyChangeForce(int value)
{
    set(m_data.masterKeyChangeForce, value);
}

void Metadata::setHistoryMaxItems(int value)
{
    set(m_data.historyMaxItems, value);
}

void Metadata::setHistoryMaxSize(int value)
{
    set(m_data.historyMaxSize, value);
}

void Metadata::setAutosaveDelayMin(int value)
{
    Q_ASSERT(value >= 0 && value <= 420000000);
    m_customData->set(customDataKeys::autosaveDelay, QString::number(value));
}

QDateTime Metadata::settingsChanged() const
{
    return m_settingsChanged;
}

void Metadata::setSettingsChanged(const QDateTime& value)
{
    Q_ASSERT(value.timeSpec() == Qt::UTC);
    m_settingsChanged = value;
}

void Metadata::addSavedSearch(const QString& name, const QString& searchtext)
{
    auto searches = savedSearches();
    searches.insert(name, searchtext);
    auto json = QJsonDocument::fromVariant(searches);
    m_customData->set(customDataKeys::savedSearch, json.toJson());
}

void Metadata::deleteSavedSearch(const QString& name)
{
    auto searches = savedSearches();
    searches.remove(name);
    auto json = QJsonDocument::fromVariant(searches);
    m_customData->set(customDataKeys::savedSearch, json.toJson());
}

QVariantMap Metadata::savedSearches()
{
    auto searches = m_customData->value(customDataKeys::savedSearch);
    auto json = QJsonDocument::fromJson(searches.toUtf8());
    return json.toVariant().toMap();
}
