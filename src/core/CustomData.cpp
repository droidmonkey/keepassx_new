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

#include "CustomData.h"

#include "core/Clock.h"
#include "core/Global.h"

const QString CustomData::LastModified = QStringLiteral("_LAST_MODIFIED");
const QString CustomData::Created = QStringLiteral("_CREATED_");
const QString CustomData::BrowserKeyPrefix = QStringLiteral("KPXC_BROWSER_");
const QString CustomData::BrowserLegacyKeyPrefix = QStringLiteral("Public Key: ");
const QString CustomData::ExcludeFromReportsLegacy = QStringLiteral("KnownBad");
const QString CustomData::FdoSecretsExposedGroup = QStringLiteral("FDO_SECRETS_EXPOSED_GROUP");
const QString CustomData::RandomSlug = QStringLiteral("KPXC_RANDOM_SLUG");

// Fallback item for return by reference
static const CustomData::CustomDataItem NULL_ITEM{};

CustomData::CustomData(QObject* parent)
    : ModifiableObject(parent)
{
}

QList<QString> CustomData::keys() const
{
    return m_data.keys();
}

bool CustomData::hasKey(const QString& key) const
{
    return m_data.contains(key);
}

QString CustomData::value(const QString& key) const
{
    return m_data.value(key).value;
}

const CustomData::CustomDataItem& CustomData::item(const QString& key) const
{
    auto item = m_data.find(key);
    Q_ASSERT(item != m_data.end());
    if (item == m_data.end()) {
        return NULL_ITEM;
    }
    return item.value();
}

bool CustomData::contains(const QString& key) const
{
    return m_data.contains(key);
}

bool CustomData::containsValue(const QString& value) const
{
    for (auto i = m_data.constBegin(); i != m_data.constEnd(); ++i) {
        if (i.value().value == value) {
            return true;
        }
    }
    return false;
}

void CustomData::set(const QString& key, CustomDataItem item)
{
    bool addAttribute = !m_data.contains(key);
    bool changeValue = !addAttribute && (m_data.value(key).value != item.value);

    if (addAttribute) {
        emit aboutToBeAdded(key);
    }

    if (!item.lastModified.isValid()) {
        item.lastModified = Clock::currentDateTimeUtc();
    }
    if (addAttribute || changeValue) {
        m_data.insert(key, item);
        updateLastModified();
        emitModified();
    }

    if (addAttribute) {
        emit added(key);
    }
}

void CustomData::set(const QString& key, const QString& value, const QDateTime& lastModified)
{
    set(key, {value, lastModified});
}

void CustomData::remove(const QString& key)
{
    emit aboutToBeRemoved(key);

    if (m_data.contains(key)) {
        m_data.remove(key);
        updateLastModified();
        emitModified();
    }

    emit removed(key);
}

void CustomData::rename(const QString& oldKey, const QString& newKey)
{
    const bool containsOldKey = m_data.contains(oldKey);
    const bool containsNewKey = m_data.contains(newKey);
    Q_ASSERT(containsOldKey && !containsNewKey);
    if (!containsOldKey || containsNewKey) {
        return;
    }

    CustomDataItem data = m_data.value(oldKey);

    emit aboutToRename(oldKey, newKey);

    m_data.remove(oldKey);
    data.lastModified = Clock::currentDateTimeUtc();
    m_data.insert(newKey, data);

    updateLastModified();
    emitModified();
    emit renamed(oldKey, newKey);
}

void CustomData::copyDataFrom(const CustomData* other)
{
    if (*this == *other) {
        return;
    }

    emit aboutToBeReset();

    m_data = other->m_data;

    updateLastModified();
    emit reset();
    emitModified();
}

QDateTime CustomData::lastModified() const
{
    if (m_data.contains(LastModified)) {
        return Clock::parse(m_data.value(LastModified).value);
    }

    // Try to find the latest modification time in items as a fallback
    QDateTime modified;
    for (auto i = m_data.constBegin(); i != m_data.constEnd(); ++i) {
        if (i->lastModified.isValid() && (!modified.isValid() || i->lastModified > modified)) {
            modified = i->lastModified;
        }
    }
    return modified;
}

QDateTime CustomData::lastModified(const QString& key) const
{
    return m_data.value(key).lastModified;
}

void CustomData::updateLastModified(QDateTime lastModified)
{
    if (m_data.isEmpty() || (m_data.size() == 1 && m_data.contains(LastModified))) {
        m_data.remove(LastModified);
        return;
    }

    if (!lastModified.isValid()) {
        lastModified = Clock::currentDateTimeUtc();
    }
    m_data.insert(LastModified, {lastModified.toString(), QDateTime()});
}

bool CustomData::isProtected(const QString& key) const
{
    return key.startsWith(BrowserKeyPrefix) || key == Created || key == FdoSecretsExposedGroup;
}

bool CustomData::isAutoGenerated(const QString& key) const
{
    return key == LastModified || key == RandomSlug;
}

bool CustomData::operator==(const CustomData& other) const
{
    return (m_data == other.m_data);
}

bool CustomData::operator!=(const CustomData& other) const
{
    return (m_data != other.m_data);
}

void CustomData::clear()
{
    emit aboutToBeReset();

    m_data.clear();

    emit reset();
    emitModified();
}

bool CustomData::isEmpty() const
{
    return m_data.isEmpty();
}

int CustomData::size() const
{
    return m_data.size();
}

int CustomData::dataSize() const
{
    int size = 0;

    QHashIterator<QString, CustomDataItem> i(m_data);
    while (i.hasNext()) {
        i.next();

        // In theory, we should be adding the datetime string size as well, but it makes
        // length calculations rather unpredictable. We also don't know if this instance
        // is entry/group-level CustomData or global CustomData (the only CustomData that
        // actually retains the datetime in the KDBX file).
        size += i.key().toUtf8().size() + i.value().value.toUtf8().size();
    }
    return size;
}
