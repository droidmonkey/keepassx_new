/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#include "Entry.h"

#include "core/Config.h"
#include "core/Database.h"
#include "core/Global.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/PasswordHealth.h"
#include "core/Tools.h"
#include "core/Totp.h"

#include <QDir>
#include <QRegularExpression>
#include <QStringBuilder>
#include <QUrl>

const int Entry::DefaultIconNumber = 0;

namespace
{
    const int ResolveMaximumDepth = 10;
    const QString AutoTypeSequenceUsername = "{USERNAME}{ENTER}";
    const QString AutoTypeSequencePassword = "{PASSWORD}{ENTER}";
    const QRegularExpression TagDelimiterRegex(R"([,;\t])");
} // namespace

Entry::Entry()
    : m_attributes(new EntryAttributes(this))
    , m_attachments(new EntryAttachments(this))
    , m_autoTypeAssociations(new AutoTypeAssociations(this))
    , m_customData(new CustomData(this))
    , m_modifiedSinceBegin(false)
    , m_updateTimeinfo(true)
{
    m_data.iconNumber = DefaultIconNumber;
    m_data.autoTypeEnabled = true;
    m_data.autoTypeObfuscation = 0;
    m_data.excludeFromReports = false;

    connect(m_attributes, &EntryAttributes::modified, this, &Entry::updateTotp);
    connect(m_attributes, &EntryAttributes::modified, this, &Entry::modified);
    connect(m_attributes, &EntryAttributes::defaultKeyModified, this, &Entry::emitDataChanged);
    connect(m_attachments, &EntryAttachments::modified, this, &Entry::modified);
    connect(m_autoTypeAssociations, &AutoTypeAssociations::modified, this, &Entry::modified);
    connect(m_customData, &CustomData::modified, this, &Entry::modified);

    connect(this, &Entry::modified, this, &Entry::updateTimeinfo);
    connect(this, &Entry::modified, this, &Entry::updateModifiedSinceBegin);
}

Entry::~Entry()
{
    setUpdateTimeinfo(false);
    if (m_group) {
        m_group->removeEntry(this);

        if (m_group->database()) {
            m_group->database()->addDeletedObject(m_uuid);
        }
    }

    qDeleteAll(m_history);
}

template <class T> inline bool Entry::set(T& property, const T& value)
{
    if (property != value) {
        property = value;
        emitModified();
        return true;
    }
    return false;
}

void Entry::updateTimeinfo()
{
    if (m_updateTimeinfo) {
        m_data.timeInfo.setLastModificationTime(Clock::currentDateTimeUtc());
        m_data.timeInfo.setLastAccessTime(Clock::currentDateTimeUtc());
    }
}

bool Entry::canUpdateTimeinfo() const
{
    return m_updateTimeinfo;
}

void Entry::setUpdateTimeinfo(bool value)
{
    m_updateTimeinfo = value;
}

QString Entry::buildReference(const QUuid& uuid, const QString& field)
{
    Q_ASSERT(EntryAttributes::DefaultAttributes.count(field) > 0);

    QString uuidStr = Tools::uuidToHex(uuid).toUpper();
    QString shortField;

    if (field == EntryAttributes::TitleKey) {
        shortField = "T";
    } else if (field == EntryAttributes::UserNameKey) {
        shortField = "U";
    } else if (field == EntryAttributes::PasswordKey) {
        shortField = "P";
    } else if (field == EntryAttributes::URLKey) {
        shortField = "A";
    } else if (field == EntryAttributes::NotesKey) {
        shortField = "N";
    }

    if (shortField.isEmpty()) {
        return {};
    }

    return QString("{REF:%1@I:%2}").arg(shortField, uuidStr);
}

EntryReferenceType Entry::referenceType(const QString& referenceStr)
{
    const QString referenceLowerStr = referenceStr.toLower();
    EntryReferenceType result = EntryReferenceType::Unknown;
    if (referenceLowerStr == QLatin1String("t")) {
        result = EntryReferenceType::Title;
    } else if (referenceLowerStr == QLatin1String("u")) {
        result = EntryReferenceType::UserName;
    } else if (referenceLowerStr == QLatin1String("p")) {
        result = EntryReferenceType::Password;
    } else if (referenceLowerStr == QLatin1String("a")) {
        result = EntryReferenceType::Url;
    } else if (referenceLowerStr == QLatin1String("n")) {
        result = EntryReferenceType::Notes;
    } else if (referenceLowerStr == QLatin1String("i")) {
        result = EntryReferenceType::QUuid;
    } else if (referenceLowerStr == QLatin1String("o")) {
        result = EntryReferenceType::CustomAttributes;
    }

    return result;
}

const QUuid& Entry::uuid() const
{
    return m_uuid;
}

const QString Entry::uuidToHex() const
{
    return Tools::uuidToHex(m_uuid);
}

int Entry::iconNumber() const
{
    return m_data.iconNumber;
}

const QUuid& Entry::iconUuid() const
{
    return m_data.customIcon;
}

QString Entry::foregroundColor() const
{
    return m_data.foregroundColor;
}

QString Entry::backgroundColor() const
{
    return m_data.backgroundColor;
}

QString Entry::overrideUrl() const
{
    return m_data.overrideUrl;
}

QString Entry::tags() const
{
    return m_data.tags.join(",");
}

QStringList Entry::tagList() const
{
    return m_data.tags;
}

const TimeInfo& Entry::timeInfo() const
{
    return m_data.timeInfo;
}

bool Entry::autoTypeEnabled() const
{
    return m_data.autoTypeEnabled;
}

bool Entry::groupAutoTypeEnabled() const
{
    return group() && group()->resolveAutoTypeEnabled();
}

int Entry::autoTypeObfuscation() const
{
    return m_data.autoTypeObfuscation;
}

QString Entry::defaultAutoTypeSequence() const
{
    return m_data.defaultAutoTypeSequence;
}

const QSharedPointer<PasswordHealth> Entry::passwordHealth()
{
    if (!m_data.passwordHealth) {
        m_data.passwordHealth.reset(new PasswordHealth(resolvePlaceholder(password())));
    }
    return m_data.passwordHealth;
}

const QSharedPointer<PasswordHealth> Entry::passwordHealth() const
{
    if (!m_data.passwordHealth) {
        return QSharedPointer<PasswordHealth>::create(resolvePlaceholder(password()));
    }
    return m_data.passwordHealth;
}

bool Entry::excludeFromReports() const
{
    return m_data.excludeFromReports
           || (customData()->contains(CustomData::ExcludeFromReportsLegacy)
               && customData()->value(CustomData::ExcludeFromReportsLegacy) == TRUE_STR);
}

void Entry::setExcludeFromReports(bool state)
{
    set(m_data.excludeFromReports, state);
}

/**
 * Determine the effective sequence that will be injected
 * This function return an empty string if a parent group has autotype disabled or if the entry has no parent
 */
QString Entry::effectiveAutoTypeSequence() const
{
    if (!autoTypeEnabled()) {
        return {};
    }

    const Group* parent = group();
    if (!parent) {
        return {};
    }

    QString sequence = parent->effectiveAutoTypeSequence();
    if (sequence.isEmpty()) {
        return {};
    }

    if (!m_data.defaultAutoTypeSequence.isEmpty()) {
        return m_data.defaultAutoTypeSequence;
    }

    if (sequence == Group::RootAutoTypeSequence && (!username().isEmpty() || !password().isEmpty())) {
        if (username().isEmpty()) {
            return AutoTypeSequencePassword;
        } else if (password().isEmpty()) {
            return AutoTypeSequenceUsername;
        }
        return Group::RootAutoTypeSequence;
    }

    return sequence;
}

/**
 * Retrieve the Auto-Type sequences matches for a given windowTitle
 * This returns a list with priority ordering. If you don't want duplicates, convert it to a QSet<QString>.
 */
QList<QString> Entry::autoTypeSequences(const QString& windowTitle) const
{
    // If no window just return the effective sequence
    if (windowTitle.isEmpty()) {
        return {effectiveAutoTypeSequence()};
    }

    // Define helper functions to match window titles
    auto windowMatches = [&](const QString& pattern) {
        // Regex searching
        if (pattern.startsWith("//") && pattern.endsWith("//") && pattern.size() >= 4) {
            QRegularExpression regExp(pattern.mid(2, pattern.size() - 4), QRegularExpression::CaseInsensitiveOption);
            return regExp.match(windowTitle).hasMatch();
        }

        // Wildcard searching
        const auto regExp = Tools::convertToRegex(
            pattern, Tools::RegexConvertOpts::EXACT_MATCH | Tools::RegexConvertOpts::WILDCARD_UNLIMITED_MATCH);
        return regExp.match(windowTitle).hasMatch();
    };

    auto windowMatchesTitle = [&](const QString& entryTitle) {
        return !entryTitle.isEmpty() && windowTitle.contains(entryTitle, Qt::CaseInsensitive);
    };

    auto windowMatchesUrl = [&](const QString& entryUrl) {
        if (!entryUrl.isEmpty() && windowTitle.contains(entryUrl, Qt::CaseInsensitive)) {
            return true;
        }

        QUrl url(entryUrl);
        if (url.isValid() && !url.host().isEmpty()) {
            return windowTitle.contains(url.host(), Qt::CaseInsensitive);
        }

        return false;
    };

    QList<QString> sequenceList;

    // Add window association matches
    const auto assocList = autoTypeAssociations()->getAll();
    for (const auto& assoc : assocList) {
        auto window = resolveMultiplePlaceholders(assoc.window);
        if (!assoc.window.isEmpty() && windowMatches(window)) {
            if (!assoc.sequence.isEmpty()) {
                sequenceList << assoc.sequence;
            } else {
                sequenceList << effectiveAutoTypeSequence();
            }
        }
    }

    // Try to match window title
    if (config()->get(Config::AutoTypeEntryTitleMatch).toBool() && windowMatchesTitle(resolvePlaceholder(title()))) {
        sequenceList << effectiveAutoTypeSequence();
    }

    // Try to match url in window title
    if (config()->get(Config::AutoTypeEntryURLMatch).toBool() && windowMatchesUrl(resolvePlaceholder(url()))) {
        sequenceList << effectiveAutoTypeSequence();
    }

    return sequenceList;
}

AutoTypeAssociations* Entry::autoTypeAssociations()
{
    return m_autoTypeAssociations;
}

const AutoTypeAssociations* Entry::autoTypeAssociations() const
{
    return m_autoTypeAssociations;
}

QString Entry::title() const
{
    return m_attributes->value(EntryAttributes::TitleKey);
}

QString Entry::url() const
{
    return m_attributes->value(EntryAttributes::URLKey);
}

QStringList Entry::getAllUrls() const
{
    QStringList urlList;
    auto entryUrl = url();

    if (!entryUrl.isEmpty()) {
        urlList << (EntryAttributes::matchReference(entryUrl).hasMatch() ? resolveMultiplePlaceholders(entryUrl)
                                                                         : entryUrl);
    }

    for (const auto& key : m_attributes->keys()) {
        if (key.startsWith(EntryAttributes::AdditionalUrlAttribute)
            || key == QString("%1_RELYING_PARTY").arg(EntryAttributes::PasskeyAttribute)) {
            auto additionalUrl = m_attributes->value(key);
            if (!additionalUrl.isEmpty()) {
                urlList << resolveMultiplePlaceholders(additionalUrl);
            }
        }
    }

    return urlList;
}

QString Entry::webUrl() const
{
    QString url = resolveMultiplePlaceholders(m_attributes->value(EntryAttributes::URLKey));
    return resolveUrl(url);
}

QString Entry::displayUrl() const
{
    QString url = maskPasswordPlaceholders(m_attributes->value(EntryAttributes::URLKey));
    return resolveMultiplePlaceholders(url);
}

QString Entry::username() const
{
    return m_attributes->value(EntryAttributes::UserNameKey);
}

QString Entry::password() const
{
    return m_attributes->value(EntryAttributes::PasswordKey);
}

QString Entry::notes() const
{
    return m_attributes->value(EntryAttributes::NotesKey);
}

QString Entry::attribute(const QString& key) const
{
    return m_attributes->value(key);
}

int Entry::size() const
{
    int size = 0;
    size += attributes()->attributesSize();
    size += autoTypeAssociations()->associationsSize();
    size += attachments()->attachmentsSize();
    size += customData()->dataSize();
    for (const QString& tag : tags().split(TagDelimiterRegex, Qt::SkipEmptyParts)) {
        size += tag.toUtf8().size();
    }

    return size;
}

bool Entry::isExpired() const
{
    return willExpireInDays(0);
}

bool Entry::willExpireInDays(int days) const
{
    return m_data.timeInfo.expires() && m_data.timeInfo.expiryTime() < Clock::currentDateTime().addDays(days);
}

bool Entry::isRecycled() const
{
    const Database* db = database();
    if (!db) {
        return false;
    }

    return m_group->isRecycled();
}

bool Entry::isAttributeReference(const QString& key) const
{
    return m_attributes->isReference(key);
}

bool Entry::isAttributeReferenceOf(const QString& key, const QUuid& uuid) const
{
    if (!m_attributes->isReference(key)) {
        return false;
    }

    return m_attributes->value(key).contains(Tools::uuidToHex(uuid), Qt::CaseInsensitive);
}

bool Entry::hasReferences() const
{
    for (const QString& key : EntryAttributes::DefaultAttributes) {
        if (m_attributes->isReference(key)) {
            return true;
        }
    }
    return false;
}

bool Entry::hasReferencesTo(const QUuid& uuid) const
{
    for (const QString& key : EntryAttributes::DefaultAttributes) {
        if (isAttributeReferenceOf(key, uuid)) {
            return true;
        }
    }
    return false;
}

void Entry::replaceReferencesWithValues(const Entry* other)
{
    for (const QString& key : EntryAttributes::DefaultAttributes) {
        if (isAttributeReferenceOf(key, other->uuid())) {
            setDefaultAttribute(key, other->attribute(key));
        }
    }
}

EntryAttributes* Entry::attributes()
{
    return m_attributes;
}

const EntryAttributes* Entry::attributes() const
{
    return m_attributes;
}

EntryAttachments* Entry::attachments()
{
    return m_attachments;
}

const EntryAttachments* Entry::attachments() const
{
    return m_attachments;
}

CustomData* Entry::customData()
{
    return m_customData;
}

const CustomData* Entry::customData() const
{
    return m_customData;
}

bool Entry::hasTotp() const
{
    return !m_data.totpSettings.isNull();
}

bool Entry::hasPasskey() const
{
    return m_attributes->hasPasskey();
}

void Entry::removePasskey()
{
    m_attributes->removePasskeyAttributes();
    removeTag(tr("Passkey"));
}

QString Entry::totp() const
{
    if (hasTotp()) {
        return Totp::generateTotp(m_data.totpSettings);
    }
    return {};
}

void Entry::setTotp(QSharedPointer<Totp::Settings> settings)
{
    beginUpdate();
    m_attributes->remove(Totp::ATTRIBUTE_OTP);
    m_attributes->remove(Totp::ATTRIBUTE_SEED);
    m_attributes->remove(Totp::ATTRIBUTE_SETTINGS);

    if (!settings || settings->key.isEmpty()) {
        m_data.totpSettings.reset();
    } else {
        m_data.totpSettings = std::move(settings);
        auto text = Totp::writeSettings(
            m_data.totpSettings, resolveMultiplePlaceholders(title()), resolveMultiplePlaceholders(username()));
        if (m_data.totpSettings->format != Totp::StorageFormat::LEGACY) {
            m_attributes->set(Totp::ATTRIBUTE_OTP, text, true);
        } else {
            m_attributes->set(Totp::ATTRIBUTE_SEED, m_data.totpSettings->key, true);
            m_attributes->set(Totp::ATTRIBUTE_SETTINGS, text);
        }
    }
    endUpdate();
}

void Entry::updateTotp()
{
    if (m_attributes->contains(Totp::ATTRIBUTE_SETTINGS)) {
        m_data.totpSettings = Totp::parseSettings(m_attributes->value(Totp::ATTRIBUTE_SETTINGS),
                                                  m_attributes->value(Totp::ATTRIBUTE_SEED));
    } else if (m_attributes->contains(Totp::ATTRIBUTE_OTP)) {
        m_data.totpSettings = Totp::parseSettings(m_attributes->value(Totp::ATTRIBUTE_OTP));
    } else if (m_attributes->contains(Totp::KP2_TOTP_SECRET)) {
        m_data.totpSettings = Totp::fromKeePass2Totp(m_attributes->value(Totp::KP2_TOTP_SECRET),
                                                     m_attributes->value(Totp::KP2_TOTP_ALGORITHM),
                                                     m_attributes->value(Totp::KP2_TOTP_LENGTH),
                                                     m_attributes->value(Totp::KP2_TOTP_PERIOD));
    } else {
        m_data.totpSettings.reset();
    }
}

QSharedPointer<Totp::Settings> Entry::totpSettings() const
{
    return m_data.totpSettings;
}

QString Entry::totpSettingsString() const
{
    if (m_data.totpSettings) {
        return Totp::writeSettings(
            m_data.totpSettings, resolveMultiplePlaceholders(title()), resolveMultiplePlaceholders(username()), true);
    }
    return {};
}

QString Entry::path() const
{
    auto path = group()->hierarchy();
    path << title();
    return path.mid(1).join("/");
}

void Entry::setUuid(const QUuid& uuid)
{
    Q_ASSERT(!uuid.isNull());
    set(m_uuid, uuid);
}

void Entry::setIcon(int iconNumber)
{
    Q_ASSERT(iconNumber >= 0);

    if (m_data.iconNumber != iconNumber || !m_data.customIcon.isNull()) {
        m_data.iconNumber = iconNumber;
        m_data.customIcon = QUuid();

        emitModified();
        emitDataChanged();
    }
}

void Entry::setIcon(const QUuid& uuid)
{
    Q_ASSERT(!uuid.isNull());

    if (m_data.customIcon != uuid) {
        m_data.customIcon = uuid;
        m_data.iconNumber = 0;

        emitModified();
        emitDataChanged();
    }
}

void Entry::setForegroundColor(const QString& colorStr)
{
    set(m_data.foregroundColor, colorStr);
}

void Entry::setBackgroundColor(const QString& colorStr)
{
    set(m_data.backgroundColor, colorStr);
}

void Entry::setOverrideUrl(const QString& url)
{
    set(m_data.overrideUrl, url);
}

void Entry::setTags(const QString& tags)
{
    auto taglist = tags.split(TagDelimiterRegex, Qt::SkipEmptyParts);
    // Trim whitespace before/after tag text
    for (auto& tag : taglist) {
        tag = tag.trimmed();
    }
    // Remove duplicates
    taglist = Tools::asSet(taglist).values();
    // Sort alphabetically
    taglist.sort();
    set(m_data.tags, taglist);
}

void Entry::addTag(const QString& tag)
{
    auto cleanTag = tag.trimmed();
    cleanTag.remove(TagDelimiterRegex);

    auto taglist = m_data.tags;
    if (!taglist.contains(cleanTag)) {
        taglist.append(cleanTag);
        taglist.sort();
        set(m_data.tags, taglist);
    }
}

void Entry::removeTag(const QString& tag)
{
    auto cleanTag = tag.trimmed();
    cleanTag.remove(TagDelimiterRegex);

    auto taglist = m_data.tags;
    if (taglist.removeAll(tag) > 0) {
        set(m_data.tags, taglist);
    }
}

void Entry::setTimeInfo(const TimeInfo& timeInfo)
{
    m_data.timeInfo = timeInfo;
}

void Entry::setAutoTypeEnabled(bool enable)
{
    set(m_data.autoTypeEnabled, enable);
}

void Entry::setAutoTypeObfuscation(int obfuscation)
{
    set(m_data.autoTypeObfuscation, obfuscation);
}

void Entry::setDefaultAutoTypeSequence(const QString& sequence)
{
    set(m_data.defaultAutoTypeSequence, sequence);
}

void Entry::setTitle(const QString& title)
{
    m_attributes->set(EntryAttributes::TitleKey, title, m_attributes->isProtected(EntryAttributes::TitleKey));
}

void Entry::setUrl(const QString& url)
{
    bool remove = url != m_attributes->value(EntryAttributes::URLKey)
                  && (m_attributes->value(EntryAttributes::RememberCmdExecAttr) == "1"
                      || m_attributes->value(EntryAttributes::RememberCmdExecAttr) == "0");
    if (remove) {
        m_attributes->remove(EntryAttributes::RememberCmdExecAttr);
    }
    m_attributes->set(EntryAttributes::URLKey, url, m_attributes->isProtected(EntryAttributes::URLKey));
}

void Entry::setUsername(const QString& username)
{
    m_attributes->set(EntryAttributes::UserNameKey, username, m_attributes->isProtected(EntryAttributes::UserNameKey));
}

void Entry::setPassword(const QString& password)
{
    // Reset Password Health
    m_data.passwordHealth.reset();
    m_attributes->set(EntryAttributes::PasswordKey, password, m_attributes->isProtected(EntryAttributes::PasswordKey));
}

void Entry::setNotes(const QString& notes)
{
    m_attributes->set(EntryAttributes::NotesKey, notes, m_attributes->isProtected(EntryAttributes::NotesKey));
}

void Entry::setDefaultAttribute(const QString& attribute, const QString& value)
{
    Q_ASSERT(EntryAttributes::isDefaultAttribute(attribute));

    if (!EntryAttributes::isDefaultAttribute(attribute)) {
        return;
    }

    m_attributes->set(attribute, value, m_attributes->isProtected(attribute));
}

void Entry::setExpires(const bool& value)
{
    if (m_data.timeInfo.expires() != value) {
        m_data.timeInfo.setExpires(value);
        emitModified();
    }
}

void Entry::setExpiryTime(const QDateTime& dateTime)
{
    if (m_data.timeInfo.expiryTime() != dateTime) {
        m_data.timeInfo.setExpiryTime(dateTime);
        emitModified();
    }
}

QList<Entry*> Entry::historyItems()
{
    return m_history;
}

const QList<Entry*>& Entry::historyItems() const
{
    return m_history;
}

void Entry::addHistoryItem(Entry* entry)
{
    Q_ASSERT(!entry->parent());

    entry->setHistoryOwner(this);
    m_history.append(entry);
    emitModified();
}

void Entry::setHistoryOwner(Entry* entry)
{
    m_historyOwner = entry;
}

Entry* Entry::historyOwner() const
{
    return m_historyOwner;
}

void Entry::removeHistoryItems(const QList<Entry*>& historyEntries)
{
    if (historyEntries.isEmpty()) {
        return;
    }

    for (Entry* entry : historyEntries) {
        Q_ASSERT(!entry->parent());
        Q_ASSERT(entry->uuid().isNull() || entry->uuid() == uuid());
        Q_ASSERT(m_history.contains(entry));

        m_history.removeOne(entry);
        delete entry;
    }

    emitModified();
}

void Entry::truncateHistory()
{
    const Database* db = database();

    if (!db) {
        return;
    }

    bool changed = false;
    int histMaxItems = db->metadata()->historyMaxItems();
    if (histMaxItems > -1) {
        int historyCount = 0;
        QMutableListIterator<Entry*> i(m_history);
        i.toBack();
        while (i.hasPrevious()) {
            historyCount++;
            Entry* entry = i.previous();
            if (historyCount > histMaxItems) {
                delete entry;
                i.remove();
                changed = true;
            }
        }
    }

    int histMaxSize = db->metadata()->historyMaxSize();
    if (histMaxSize > -1) {
        int size = 0;

        QMutableListIterator<Entry*> i(m_history);
        i.toBack();
        while (i.hasPrevious()) {
            Entry* historyItem = i.previous();

            // don't calculate size if it's already above the maximum
            if (size <= histMaxSize) {
                size += historyItem->size();
            }

            if (size > histMaxSize) {
                delete historyItem;
                i.remove();
                changed = true;
            }
        }
    }

    if (changed) {
        emitModified();
    }
}

bool Entry::equals(const Entry* other, CompareItemOptions options) const
{
    if (!other) {
        return false;
    }
    if (m_uuid != other->uuid()) {
        return false;
    }
    if (!m_data.equals(other->m_data, options)) {
        return false;
    }
    if (*m_customData != *other->m_customData) {
        return false;
    }
    if (*m_attributes != *other->m_attributes) {
        return false;
    }
    if (*m_attachments != *other->m_attachments) {
        return false;
    }
    if (*m_autoTypeAssociations != *other->m_autoTypeAssociations) {
        return false;
    }
    if (!options.testFlag(CompareItemIgnoreHistory)) {
        if (m_history.count() != other->m_history.count()) {
            return false;
        }
        for (int i = 0; i < m_history.count(); ++i) {
            if (!m_history[i]->equals(other->m_history[i], options)) {
                return false;
            }
        }
    }
    return true;
}

Entry* Entry::clone(CloneFlags flags) const
{
    auto entry = new Entry();
    entry->setUpdateTimeinfo(false);
    if (flags & CloneNewUuid) {
        entry->m_uuid = QUuid::createUuid();
    } else {
        entry->m_uuid = m_uuid;
    }
    entry->m_data = m_data;
    entry->m_customData->copyDataFrom(m_customData);
    entry->m_attributes->copyDataFrom(m_attributes);
    entry->m_attachments->copyDataFrom(m_attachments);

    if (flags & CloneUserAsRef) {
        entry->m_attributes->set(EntryAttributes::UserNameKey,
                                 buildReference(uuid(), EntryAttributes::UserNameKey),
                                 m_attributes->isProtected(EntryAttributes::UserNameKey));
    }

    if (flags & ClonePassAsRef) {
        entry->m_attributes->set(EntryAttributes::PasswordKey,
                                 buildReference(uuid(), EntryAttributes::PasswordKey),
                                 m_attributes->isProtected(EntryAttributes::PasswordKey));
    }

    entry->m_autoTypeAssociations->copyDataFrom(m_autoTypeAssociations);
    if (flags & CloneIncludeHistory) {
        for (Entry* historyItem : m_history) {
            Entry* historyItemClone =
                historyItem->clone(flags & ~CloneIncludeHistory & ~CloneNewUuid & ~CloneResetTimeInfo);
            historyItemClone->setUpdateTimeinfo(false);
            historyItemClone->setUuid(entry->uuid());
            historyItemClone->setUpdateTimeinfo(true);
            entry->addHistoryItem(historyItemClone);
        }
    }

    if (flags & CloneResetTimeInfo) {
        QDateTime now = Clock::currentDateTimeUtc();
        entry->m_data.timeInfo.setCreationTime(now);
        entry->m_data.timeInfo.setLastModificationTime(now);
        entry->m_data.timeInfo.setLastAccessTime(now);
        entry->m_data.timeInfo.setLocationChanged(now);
    }

    if (flags & CloneRenameTitle) {
        entry->setTitle(tr("%1 - Clone").arg(entry->title()));
    }

    entry->setUpdateTimeinfo(true);

    return entry;
}

void Entry::copyDataFrom(const Entry* other)
{
    setUpdateTimeinfo(false);
    m_data = other->m_data;
    m_customData->copyDataFrom(other->m_customData);
    m_attributes->copyDataFrom(other->m_attributes);
    m_attachments->copyDataFrom(other->m_attachments);
    m_autoTypeAssociations->copyDataFrom(other->m_autoTypeAssociations);
    setUpdateTimeinfo(true);
}

void Entry::beginUpdate()
{
    Q_ASSERT(m_tmpHistoryItem.isNull());

    m_tmpHistoryItem.reset(new Entry());
    m_tmpHistoryItem->setUpdateTimeinfo(false);
    m_tmpHistoryItem->m_uuid = m_uuid;
    m_tmpHistoryItem->m_data = m_data;
    m_tmpHistoryItem->m_attributes->copyDataFrom(m_attributes);
    m_tmpHistoryItem->m_attachments->copyDataFrom(m_attachments);
    m_tmpHistoryItem->m_autoTypeAssociations->copyDataFrom(m_autoTypeAssociations);

    m_modifiedSinceBegin = false;
}

bool Entry::endUpdate()
{
    Q_ASSERT(!m_tmpHistoryItem.isNull());
    if (m_modifiedSinceBegin) {
        m_tmpHistoryItem->setUpdateTimeinfo(true);
        addHistoryItem(m_tmpHistoryItem.take());
        truncateHistory();
    }

    m_tmpHistoryItem.reset();

    return m_modifiedSinceBegin;
}

void Entry::updateModifiedSinceBegin()
{
    m_modifiedSinceBegin = true;
}

QString Entry::resolveMultiplePlaceholdersRecursive(const QString& str, int maxDepth) const
{
    static const QRegularExpression placeholderRegEx("({(?>[^{}]+?|(?1))+?})");

    if (--maxDepth < 0) {
        qWarning("Maximum depth of replacement has been reached. Entry uuid: %s", uuid().toString().toLatin1().data());
        return str;
    }

    QString result;
    auto matches = placeholderRegEx.globalMatch(str);
    int capEnd = 0;
    while (matches.hasNext()) {
        const auto match = matches.next();
        result += str.midRef(capEnd, match.capturedStart() - capEnd);
        result += resolvePlaceholderRecursive(match.captured(), maxDepth);
        capEnd = match.capturedEnd();
    }
    result += str.rightRef(str.length() - capEnd);
    return result;
}

QString Entry::resolvePlaceholderRecursive(const QString& placeholder, int maxDepth) const
{
    if (--maxDepth < 0) {
        qWarning("Maximum depth of replacement has been reached. Entry uuid: %s", uuid().toString().toLatin1().data());
        return placeholder;
    }

    const PlaceholderType typeOfPlaceholder = placeholderType(placeholder);
    switch (typeOfPlaceholder) {
    case PlaceholderType::NotPlaceholder:
        return resolveMultiplePlaceholdersRecursive(placeholder, maxDepth);
    case PlaceholderType::Unknown: {
        return "{" % resolveMultiplePlaceholdersRecursive(placeholder.mid(1, placeholder.length() - 2), maxDepth) % "}";
    }
    case PlaceholderType::Title:
        return resolveMultiplePlaceholdersRecursive(title(), maxDepth);
    case PlaceholderType::UserName:
        return resolveMultiplePlaceholdersRecursive(username(), maxDepth);
    case PlaceholderType::Password:
        return resolveMultiplePlaceholdersRecursive(password(), maxDepth);
    case PlaceholderType::Notes:
        return resolveMultiplePlaceholdersRecursive(notes(), maxDepth);
    case PlaceholderType::Url:
        return resolveMultiplePlaceholdersRecursive(url(), maxDepth);
    case PlaceholderType::DbDir: {
        QFileInfo fileInfo(database()->filePath());
        return fileInfo.absoluteDir().absolutePath();
    }
    case PlaceholderType::UrlWithoutScheme:
    case PlaceholderType::UrlScheme:
    case PlaceholderType::UrlHost:
    case PlaceholderType::UrlPort:
    case PlaceholderType::UrlPath:
    case PlaceholderType::UrlQuery:
    case PlaceholderType::UrlFragment:
    case PlaceholderType::UrlUserInfo:
    case PlaceholderType::UrlUserName:
    case PlaceholderType::UrlPassword: {
        const QString strUrl = resolveMultiplePlaceholdersRecursive(url(), maxDepth);
        return resolveUrlPlaceholder(strUrl, typeOfPlaceholder);
    }
    case PlaceholderType::Totp:
        // totp can't have placeholder inside
        return totp();
    case PlaceholderType::CustomAttribute: {
        const QString key = placeholder.mid(3, placeholder.length() - 4); // {S:attr} => mid(3, len - 4)
        return attributes()->hasKey(key) ? resolveMultiplePlaceholdersRecursive(attributes()->value(key), maxDepth)
                                         : QString();
    }
    case PlaceholderType::Reference:
        return resolveReferencePlaceholderRecursive(placeholder, ++maxDepth);
    case PlaceholderType::DateTimeSimple:
    case PlaceholderType::DateTimeYear:
    case PlaceholderType::DateTimeMonth:
    case PlaceholderType::DateTimeDay:
    case PlaceholderType::DateTimeHour:
    case PlaceholderType::DateTimeMinute:
    case PlaceholderType::DateTimeSecond:
    case PlaceholderType::DateTimeUtcSimple:
    case PlaceholderType::DateTimeUtcYear:
    case PlaceholderType::DateTimeUtcMonth:
    case PlaceholderType::DateTimeUtcDay:
    case PlaceholderType::DateTimeUtcHour:
    case PlaceholderType::DateTimeUtcMinute:
    case PlaceholderType::DateTimeUtcSecond:
        return resolveMultiplePlaceholdersRecursive(resolveDateTimePlaceholder(typeOfPlaceholder), maxDepth);
    case PlaceholderType::Conversion:
        return resolveMultiplePlaceholdersRecursive(resolveConversionPlaceholder(placeholder), maxDepth);
    case PlaceholderType::Regex:
        return resolveMultiplePlaceholdersRecursive(resolveRegexPlaceholder(placeholder), maxDepth);
    }

    return placeholder;
}

QString Entry::resolveDateTimePlaceholder(Entry::PlaceholderType placeholderType) const
{
    const QDateTime time = Clock::currentDateTime();
    const QDateTime time_utc = Clock::currentDateTimeUtc();

    switch (placeholderType) {
    case PlaceholderType::DateTimeSimple:
        return time.toString("yyyyMMddhhmmss");
    case PlaceholderType::DateTimeYear:
        return time.toString("yyyy");
    case PlaceholderType::DateTimeMonth:
        return time.toString("MM");
    case PlaceholderType::DateTimeDay:
        return time.toString("dd");
    case PlaceholderType::DateTimeHour:
        return time.toString("hh");
    case PlaceholderType::DateTimeMinute:
        return time.toString("mm");
    case PlaceholderType::DateTimeSecond:
        return time.toString("ss");
    case PlaceholderType::DateTimeUtcSimple:
        return time_utc.toString("yyyyMMddhhmmss");
    case PlaceholderType::DateTimeUtcYear:
        return time_utc.toString("yyyy");
    case PlaceholderType::DateTimeUtcMonth:
        return time_utc.toString("MM");
    case PlaceholderType::DateTimeUtcDay:
        return time_utc.toString("dd");
    case PlaceholderType::DateTimeUtcHour:
        return time_utc.toString("hh");
    case PlaceholderType::DateTimeUtcMinute:
        return time_utc.toString("mm");
    case PlaceholderType::DateTimeUtcSecond:
        return time_utc.toString("ss");
    default: {
        Q_ASSERT_X(false, "Entry::resolveDateTimePlaceholder", "Bad DateTime placeholder type");
        break;
    }
    }

    return {};
}

QString Entry::resolveConversionPlaceholder(const QString& str, QString* error) const
{
    if (error) {
        error->clear();
    }

    // Extract the inner conversion from the placeholder
    QRegularExpression conversionRegEx("^{?t-conv:(.*)}?$", QRegularExpression::CaseInsensitiveOption);
    auto placeholder = conversionRegEx.match(str).captured(1);
    if (!placeholder.isEmpty()) {
        // Determine the separator character and split, include empty groups
        auto sep = placeholder[0];
        auto parts = placeholder.split(sep);
        if (parts.size() >= 4) {
            auto resolved = resolveMultiplePlaceholders(parts[1]);
            auto type = parts[2].toLower();

            if (type == "base64") {
                resolved = resolved.toUtf8().toBase64();
            } else if (type == "hex") {
                resolved = resolved.toUtf8().toHex();
            } else if (type == "uri") {
                resolved = QUrl::toPercentEncoding(resolved.toUtf8());
            } else if (type == "uri-dec") {
                resolved = QUrl::fromPercentEncoding(resolved.toUtf8());
            } else if (type.startsWith("u")) {
                resolved = resolved.toUpper();
            } else if (type.startsWith("l")) {
                resolved = resolved.toLower();
            } else {
                if (error) {
                    *error = tr("Invalid conversion type: %1").arg(type);
                }
                return {};
            }
            return resolved;
        }
    }

    if (error) {
        *error = tr("Invalid conversion syntax: %1").arg(str);
    }
    return {};
}

QString Entry::resolveRegexPlaceholder(const QString& str, QString* error) const
{
    if (error) {
        error->clear();
    }

    // Extract the inner regex from the placeholder
    QRegularExpression conversionRegEx("^{?t-replace-rx:(.*)}?$", QRegularExpression::CaseInsensitiveOption);
    auto placeholder = conversionRegEx.match(str).captured(1);
    if (!placeholder.isEmpty()) {
        // Determine the separator character and split, include empty groups
        auto sep = placeholder[0];
        auto parts = placeholder.split(sep);
        if (parts.size() >= 5) {
            auto resolvedText = resolveMultiplePlaceholders(parts[1]);
            auto resolvedSearch = resolveMultiplePlaceholders(parts[2]);
            auto resolvedReplace = resolveMultiplePlaceholders(parts[3]);
            // Replace $<num> with \\<num> to support Qt substitutions
            resolvedReplace.replace(QRegularExpression(R"(\$(\d+))"), R"(\\1)");

            auto searchRegex = QRegularExpression(resolvedSearch);
            if (!searchRegex.isValid()) {
                if (error) {
                    *error =
                        tr("Invalid regular expression syntax %1\n%2").arg(resolvedSearch, searchRegex.errorString());
                }
                return {};
            }

            return resolvedText.replace(searchRegex, resolvedReplace);
        }
    }

    if (error) {
        *error = tr("Invalid conversion syntax: %1").arg(str);
    }
    return {};
}

QString Entry::resolveReferencePlaceholderRecursive(const QString& placeholder, int maxDepth) const
{
    if (--maxDepth < 0) {
        qWarning("Maximum depth of replacement has been reached. Entry uuid: %s", uuid().toString().toLatin1().data());
        return placeholder;
    }

    // resolving references in format: {REF:<WantedField>@<SearchIn>:<SearchText>}
    // using format from http://keepass.info/help/base/fieldrefs.html at the time of writing

    const QRegularExpressionMatch match = EntryAttributes::matchReference(placeholder);
    if (!match.hasMatch() || !m_group || !m_group->database()) {
        return placeholder;
    }

    QString result;
    const QString searchIn = match.captured(EntryAttributes::SearchInGroupName);
    const QString searchText = match.captured(EntryAttributes::SearchTextGroupName);

    const EntryReferenceType searchInType = Entry::referenceType(searchIn);

    const Entry* refEntry = m_group->database()->rootGroup()->findEntryBySearchTerm(searchText, searchInType);

    if (refEntry) {
        const QString wantedField = match.captured(EntryAttributes::WantedFieldGroupName);
        result = refEntry->referenceFieldValue(Entry::referenceType(wantedField));

        // Referencing fields of other entries only works with standard fields, not with custom user strings.
        // If you want to reference a custom user string, you need to place a redirection in a standard field
        // of the entry with the custom string, using {S:<Name>}, and reference the standard field.
        result = refEntry->resolveMultiplePlaceholdersRecursive(result, maxDepth);
    }

    return result;
}

QString Entry::referenceFieldValue(EntryReferenceType referenceType) const
{
    switch (referenceType) {
    case EntryReferenceType::Title:
        return title();
    case EntryReferenceType::UserName:
        return username();
    case EntryReferenceType::Password:
        return password();
    case EntryReferenceType::Url:
        return url();
    case EntryReferenceType::Notes:
        return notes();
    case EntryReferenceType::QUuid:
        return uuidToHex();
    default:
        break;
    }
    return {};
}

void Entry::moveUp()
{
    if (m_group) {
        m_group->moveEntryUp(this);
    }
}

void Entry::moveDown()
{
    if (m_group) {
        m_group->moveEntryDown(this);
    }
}

Group* Entry::group()
{
    return m_group;
}

const Group* Entry::group() const
{
    return m_group;
}

void Entry::setGroup(Group* group, bool trackPrevious)
{
    Q_ASSERT(group);

    if (m_group == group) {
        return;
    }

    if (m_group) {
        m_group->removeEntry(this);
        if (m_group->database() && m_group->database() != group->database()) {
            setPreviousParentGroup(nullptr);
            m_group->database()->addDeletedObject(m_uuid);

            // copy custom icon to the new database
            if (!iconUuid().isNull() && group->database() && m_group->database()->metadata()->hasCustomIcon(iconUuid())
                && !group->database()->metadata()->hasCustomIcon(iconUuid())) {
                group->database()->metadata()->addCustomIcon(iconUuid(),
                                                             m_group->database()->metadata()->customIcon(iconUuid()));
            }
        } else if (trackPrevious && m_group->database() && group != m_group) {
            setPreviousParentGroup(m_group);
        }
    }

    QObject::setParent(group);

    m_group = group;
    group->addEntry(this);

    if (m_updateTimeinfo) {
        m_data.timeInfo.setLocationChanged(Clock::currentDateTimeUtc());
    }
}

void Entry::emitDataChanged()
{
    emit entryDataChanged(this);
}

const Database* Entry::database() const
{
    if (m_group) {
        return m_group->database();
    }
    return nullptr;
}

Database* Entry::database()
{
    if (m_group) {
        return m_group->database();
    }
    return nullptr;
}

QString Entry::maskPasswordPlaceholders(const QString& str) const
{
    return QString{str}.replace(QStringLiteral("{PASSWORD}"), QStringLiteral("******"), Qt::CaseInsensitive);
}

Entry* Entry::resolveReference(const QString& str) const
{
    QRegularExpressionMatch match = EntryAttributes::matchReference(str);
    if (!match.hasMatch()) {
        return nullptr;
    }

    const QString searchIn = match.captured(EntryAttributes::SearchInGroupName);
    const QString searchText = match.captured(EntryAttributes::SearchTextGroupName);

    const EntryReferenceType searchInType = Entry::referenceType(searchIn);
    return m_group->database()->rootGroup()->findEntryBySearchTerm(searchText, searchInType);
}

QString Entry::resolveMultiplePlaceholders(const QString& str) const
{
    return resolveMultiplePlaceholdersRecursive(str, ResolveMaximumDepth);
}

QString Entry::resolvePlaceholder(const QString& placeholder) const
{
    return resolvePlaceholderRecursive(placeholder, ResolveMaximumDepth);
}

QString Entry::resolveUrlPlaceholder(const QString& str, Entry::PlaceholderType placeholderType) const
{
    if (str.isEmpty()) {
        return {};
    }

    const QUrl qurl(str);
    switch (placeholderType) {
    case PlaceholderType::UrlWithoutScheme:
        return qurl.toString(QUrl::RemoveScheme | QUrl::FullyDecoded);
    case PlaceholderType::UrlScheme:
        return qurl.scheme();
    case PlaceholderType::UrlHost:
        return qurl.host();
    case PlaceholderType::UrlPort:
        return QString::number(qurl.port());
    case PlaceholderType::UrlPath:
        return qurl.path();
    case PlaceholderType::UrlQuery:
        return qurl.query();
    case PlaceholderType::UrlFragment:
        return qurl.fragment();
    case PlaceholderType::UrlUserInfo:
        return qurl.userInfo();
    case PlaceholderType::UrlUserName:
        return qurl.userName();
    case PlaceholderType::UrlPassword:
        return qurl.password();
    default: {
        Q_ASSERT_X(false, "Entry::resolveUrlPlaceholder", "Bad url placeholder type");
        break;
    }
    }

    return {};
}

Entry::PlaceholderType Entry::placeholderType(const QString& placeholder) const
{
    if (!placeholder.startsWith(QStringLiteral("{")) || !placeholder.endsWith(QStringLiteral("}"))) {
        return PlaceholderType::NotPlaceholder;
    }
    if (placeholder.startsWith(QStringLiteral("{S:"))) {
        return PlaceholderType::CustomAttribute;
    }
    if (placeholder.startsWith(QStringLiteral("{REF:"))) {
        return PlaceholderType::Reference;
    }
    if (placeholder.startsWith(QStringLiteral("{T-CONV:"), Qt::CaseInsensitive)) {
        return PlaceholderType::Conversion;
    }
    if (placeholder.startsWith(QStringLiteral("{T-REPLACE-RX:"), Qt::CaseInsensitive)) {
        return PlaceholderType::Regex;
    }

    static const QMap<QString, PlaceholderType> placeholders{
        {QStringLiteral("{TITLE}"), PlaceholderType::Title},
        {QStringLiteral("{USERNAME}"), PlaceholderType::UserName},
        {QStringLiteral("{PASSWORD}"), PlaceholderType::Password},
        {QStringLiteral("{NOTES}"), PlaceholderType::Notes},
        {QStringLiteral("{TOTP}"), PlaceholderType::Totp},
        {QStringLiteral("{URL}"), PlaceholderType::Url},
        {QStringLiteral("{URL:RMVSCM}"), PlaceholderType::UrlWithoutScheme},
        {QStringLiteral("{URL:WITHOUTSCHEME}"), PlaceholderType::UrlWithoutScheme},
        {QStringLiteral("{URL:SCM}"), PlaceholderType::UrlScheme},
        {QStringLiteral("{URL:SCHEME}"), PlaceholderType::UrlScheme},
        {QStringLiteral("{URL:HOST}"), PlaceholderType::UrlHost},
        {QStringLiteral("{URL:PORT}"), PlaceholderType::UrlPort},
        {QStringLiteral("{URL:PATH}"), PlaceholderType::UrlPath},
        {QStringLiteral("{URL:QUERY}"), PlaceholderType::UrlQuery},
        {QStringLiteral("{URL:FRAGMENT}"), PlaceholderType::UrlFragment},
        {QStringLiteral("{URL:USERINFO}"), PlaceholderType::UrlUserInfo},
        {QStringLiteral("{URL:USERNAME}"), PlaceholderType::UrlUserName},
        {QStringLiteral("{URL:PASSWORD}"), PlaceholderType::UrlPassword},
        {QStringLiteral("{DT_SIMPLE}"), PlaceholderType::DateTimeSimple},
        {QStringLiteral("{DT_YEAR}"), PlaceholderType::DateTimeYear},
        {QStringLiteral("{DT_MONTH}"), PlaceholderType::DateTimeMonth},
        {QStringLiteral("{DT_DAY}"), PlaceholderType::DateTimeDay},
        {QStringLiteral("{DT_HOUR}"), PlaceholderType::DateTimeHour},
        {QStringLiteral("{DT_MINUTE}"), PlaceholderType::DateTimeMinute},
        {QStringLiteral("{DT_SECOND}"), PlaceholderType::DateTimeSecond},
        {QStringLiteral("{DT_UTC_SIMPLE}"), PlaceholderType::DateTimeUtcSimple},
        {QStringLiteral("{DT_UTC_YEAR}"), PlaceholderType::DateTimeUtcYear},
        {QStringLiteral("{DT_UTC_MONTH}"), PlaceholderType::DateTimeUtcMonth},
        {QStringLiteral("{DT_UTC_DAY}"), PlaceholderType::DateTimeUtcDay},
        {QStringLiteral("{DT_UTC_HOUR}"), PlaceholderType::DateTimeUtcHour},
        {QStringLiteral("{DT_UTC_MINUTE}"), PlaceholderType::DateTimeUtcMinute},
        {QStringLiteral("{DT_UTC_SECOND}"), PlaceholderType::DateTimeUtcSecond},
        {QStringLiteral("{DB_DIR}"), PlaceholderType::DbDir}};

    return placeholders.value(placeholder.toUpper(), PlaceholderType::Unknown);
}

QString Entry::resolveUrl(const QString& url) const
{
    QString newUrl = url;

    static const QRegularExpression fileRegEx(R"(^(?:[A-Za-z]:)?[\\/])");
    if (url.contains(fileRegEx)) {
        // Match possible file paths without the scheme and convert it to a file URL
        newUrl = QDir::fromNativeSeparators(newUrl);
        newUrl = QUrl::fromLocalFile(newUrl).toString();
    } else if (url.startsWith("cmd://")) {
        QStringList cmdList = newUrl.split(" ");
        for (int i = 1; i < cmdList.size(); ++i) {
            QString& cmd = cmdList[i];
            // Don't pass arguments to the resolveUrl function (they look like URL's)
            if (!cmd.startsWith("-") && !cmd.startsWith("/")) {
                static const QRegularExpression quotesRegEx("['\"]");
                return resolveUrl(cmd.remove(quotesRegEx));
            }
        }

        // No URL in this command
        return {};
    }

    if (!newUrl.isEmpty() && !newUrl.contains("://")) {
        // URL doesn't have a protocol, add https by default
        newUrl.prepend("https://");
    }

    // Validate the URL
    QUrl tempUrl(newUrl);
    if (tempUrl.isValid()
        && (tempUrl.scheme() == "http" || tempUrl.scheme() == "https" || tempUrl.scheme() == "file")) {
        return tempUrl.url();
    }

    // No valid http URLs found
    return {};
}

Group* Entry::previousParentGroup()
{
    if (!database() || !database()->rootGroup()) {
        return nullptr;
    }
    return database()->rootGroup()->findGroupByUuid(m_data.previousParentGroupUuid);
}

const Group* Entry::previousParentGroup() const
{
    if (!database() || !database()->rootGroup()) {
        return nullptr;
    }
    return database()->rootGroup()->findGroupByUuid(m_data.previousParentGroupUuid);
}

QUuid Entry::previousParentGroupUuid() const
{
    return m_data.previousParentGroupUuid;
}

void Entry::setPreviousParentGroupUuid(const QUuid& uuid)
{
    set(m_data.previousParentGroupUuid, uuid);
}

void Entry::setPreviousParentGroup(const Group* group)
{
    setPreviousParentGroupUuid(group ? group->uuid() : QUuid());
}

bool EntryData::operator==(const EntryData& other) const
{
    return equals(other, CompareItemDefault);
}

bool EntryData::operator!=(const EntryData& other) const
{
    return !(*this == other);
}

bool EntryData::equals(const EntryData& other, CompareItemOptions options) const
{
    if (::compare(iconNumber, other.iconNumber, options) != 0) {
        return false;
    }
    if (::compare(customIcon, other.customIcon, options) != 0) {
        return false;
    }
    if (::compare(foregroundColor, other.foregroundColor, options) != 0) {
        return false;
    }
    if (::compare(backgroundColor, other.backgroundColor, options) != 0) {
        return false;
    }
    if (::compare(overrideUrl, other.overrideUrl, options) != 0) {
        return false;
    }
    if (::compare(tags, other.tags, options) != 0) {
        return false;
    }
    if (::compare(autoTypeEnabled, other.autoTypeEnabled, options) != 0) {
        return false;
    }
    if (::compare(autoTypeObfuscation, other.autoTypeObfuscation, options) != 0) {
        return false;
    }
    if (::compare(defaultAutoTypeSequence, other.defaultAutoTypeSequence, options) != 0) {
        return false;
    }
    if (!timeInfo.equals(other.timeInfo, options)) {
        return false;
    }
    if (!totpSettings.isNull() && !other.totpSettings.isNull()) {
        // Both have TOTP settings, compare them
        if (::compare(totpSettings->key, other.totpSettings->key, options) != 0) {
            return false;
        }
        if (::compare(totpSettings->digits, other.totpSettings->digits, options) != 0) {
            return false;
        }
        if (::compare(totpSettings->step, other.totpSettings->step, options) != 0) {
            return false;
        }
    } else if (totpSettings.isNull() != other.totpSettings.isNull()) {
        // The existence of TOTP has changed between these entries
        return false;
    }
    if (::compare(excludeFromReports, other.excludeFromReports, options) != 0) {
        return false;
    }
    if (::compare(previousParentGroupUuid, other.previousParentGroupUuid, options) != 0) {
        return false;
    }

    return true;
}
