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

#ifndef KEEPASSX_ENTRY_H
#define KEEPASSX_ENTRY_H

#include <QMap>
#include <QPointer>
#include <QUuid>

#include "core/AutoTypeAssociations.h"
#include "core/CustomData.h"
#include "core/EntryAttachments.h"
#include "core/EntryAttributes.h"
#include "core/Global.h"
#include "core/TimeInfo.h"

class Database;
class Group;
class PasswordHealth;

namespace Totp
{
    struct Settings;
}

enum class EntryReferenceType
{
    Unknown,
    Title,
    UserName,
    Password,
    Url,
    Notes,
    QUuid,
    CustomAttributes
};

struct EntryData
{
    int iconNumber;
    QUuid customIcon;
    QString foregroundColor;
    QString backgroundColor;
    QString overrideUrl;
    QStringList tags;
    bool autoTypeEnabled;
    int autoTypeObfuscation;
    QString defaultAutoTypeSequence;
    TimeInfo timeInfo;
    QSharedPointer<Totp::Settings> totpSettings;
    QSharedPointer<PasswordHealth> passwordHealth;
    bool excludeFromReports;
    QUuid previousParentGroupUuid;

    bool operator==(const EntryData& other) const;
    bool operator!=(const EntryData& other) const;
    bool equals(const EntryData& other, CompareItemOptions options) const;
};

class Entry : public ModifiableObject
{
    Q_OBJECT

public:
    Entry();
    ~Entry() override;
    const QUuid& uuid() const;
    const QString uuidToHex() const;
    int iconNumber() const;
    const QUuid& iconUuid() const;
    QString foregroundColor() const;
    QString backgroundColor() const;
    QString overrideUrl() const;
    QString tags() const;
    QStringList tagList() const;
    const TimeInfo& timeInfo() const;
    bool autoTypeEnabled() const;
    bool groupAutoTypeEnabled() const;
    int autoTypeObfuscation() const;
    QString defaultAutoTypeSequence() const;
    QString effectiveAutoTypeSequence() const;
    QList<QString> autoTypeSequences(const QString& pattern = {}) const;
    AutoTypeAssociations* autoTypeAssociations();
    const AutoTypeAssociations* autoTypeAssociations() const;
    QString title() const;
    QString url() const;
    QStringList getAllUrls() const;
    QString webUrl() const;
    QString displayUrl() const;
    QString username() const;
    QString password() const;
    QString notes() const;
    QString attribute(const QString& key) const;
    QString totp() const;
    QString totpSettingsString() const;
    QSharedPointer<Totp::Settings> totpSettings() const;
    Group* previousParentGroup();
    const Group* previousParentGroup() const;
    QUuid previousParentGroupUuid() const;
    int size() const;
    QString path() const;
    const QSharedPointer<PasswordHealth> passwordHealth();
    const QSharedPointer<PasswordHealth> passwordHealth() const;
    bool excludeFromReports() const;
    void setExcludeFromReports(bool state);

    bool hasPasskey() const;
    void removePasskey();

    bool hasTotp() const;
    bool isExpired() const;
    bool willExpireInDays(int days) const;
    bool isRecycled() const;
    bool isAttributeReference(const QString& key) const;
    bool isAttributeReferenceOf(const QString& key, const QUuid& uuid) const;
    void replaceReferencesWithValues(const Entry* other);
    bool hasReferences() const;
    bool hasReferencesTo(const QUuid& uuid) const;
    EntryAttributes* attributes();
    const EntryAttributes* attributes() const;
    EntryAttachments* attachments();
    const EntryAttachments* attachments() const;
    CustomData* customData();
    const CustomData* customData() const;

    void setUuid(const QUuid& uuid);
    void setIcon(int iconNumber);
    void setIcon(const QUuid& uuid);
    void setForegroundColor(const QString& color);
    void setBackgroundColor(const QString& color);
    void setOverrideUrl(const QString& url);
    void setTags(const QString& tags);
    void setTimeInfo(const TimeInfo& timeInfo);
    void setAutoTypeEnabled(bool enable);
    void setAutoTypeObfuscation(int obfuscation);
    void setDefaultAutoTypeSequence(const QString& sequence);
    void setTitle(const QString& title);
    void setUrl(const QString& url);
    void setUsername(const QString& username);
    void setPassword(const QString& password);
    void setNotes(const QString& notes);
    void setDefaultAttribute(const QString& attribute, const QString& value);
    void setExpires(const bool& value);
    void setExpiryTime(const QDateTime& dateTime);
    void setTotp(QSharedPointer<Totp::Settings> settings);
    void setPreviousParentGroup(const Group* group);
    void setPreviousParentGroupUuid(const QUuid& uuid);

    void addTag(const QString& tag);
    void removeTag(const QString& tag);

    QList<Entry*> historyItems();
    const QList<Entry*>& historyItems() const;
    void addHistoryItem(Entry* entry);
    void setHistoryOwner(Entry* entry);
    Entry* historyOwner() const;
    void removeHistoryItems(const QList<Entry*>& historyEntries);
    void truncateHistory();

    bool equals(const Entry* other, CompareItemOptions options = CompareItemDefault) const;

    enum CloneFlag
    {
        CloneNoFlags = 0,
        CloneNewUuid = 1, // generate a random uuid for the clone
        CloneResetTimeInfo = 2, // set all TimeInfo attributes to the current time
        CloneIncludeHistory = 4, // clone the history items
        CloneDefault = CloneNewUuid | CloneResetTimeInfo,
        CloneCopy = CloneNewUuid | CloneResetTimeInfo | CloneIncludeHistory,
        CloneRenameTitle = 8, // add "-Clone" after the original title
        CloneUserAsRef = 16, // Add the user as a reference to the original entry
        ClonePassAsRef = 32, // Add the password as a reference to the original entry
    };
    Q_DECLARE_FLAGS(CloneFlags, CloneFlag)

    enum class PlaceholderType
    {
        NotPlaceholder,
        Unknown,
        Title,
        UserName,
        Password,
        Notes,
        Totp,
        Url,
        UrlWithoutScheme,
        UrlScheme,
        UrlHost,
        UrlPort,
        UrlPath,
        UrlQuery,
        UrlFragment,
        UrlUserInfo,
        UrlUserName,
        UrlPassword,
        Reference,
        CustomAttribute,
        DateTimeSimple,
        DateTimeYear,
        DateTimeMonth,
        DateTimeDay,
        DateTimeHour,
        DateTimeMinute,
        DateTimeSecond,
        DateTimeUtcSimple,
        DateTimeUtcYear,
        DateTimeUtcMonth,
        DateTimeUtcDay,
        DateTimeUtcHour,
        DateTimeUtcMinute,
        DateTimeUtcSecond,
        DbDir,
        Conversion,
        Regex
    };

    static const int DefaultIconNumber;

    /**
     * Creates a duplicate of this entry except that the returned entry isn't
     * part of any group.
     * Note that you need to copy the custom icons manually when inserting the
     * new entry into another database.
     */
    Entry* clone(CloneFlags flags = CloneDefault) const;
    void copyDataFrom(const Entry* other);
    QString maskPasswordPlaceholders(const QString& str) const;
    Entry* resolveReference(const QString& str) const;
    QString resolveMultiplePlaceholders(const QString& str) const;
    QString resolvePlaceholder(const QString& str) const;
    QString resolveUrlPlaceholder(const QString& str, PlaceholderType placeholderType) const;
    QString resolveDateTimePlaceholder(PlaceholderType placeholderType) const;
    QString resolveConversionPlaceholder(const QString& str, QString* error = nullptr) const;
    QString resolveRegexPlaceholder(const QString& str, QString* error = nullptr) const;
    PlaceholderType placeholderType(const QString& placeholder) const;
    QString resolveUrl(const QString& url) const;

    /**
     * Call before and after set*() methods to create a history item
     * if the entry has been changed.
     */
    void beginUpdate();
    bool endUpdate();

    void moveUp();
    void moveDown();

    Group* group();
    const Group* group() const;
    void setGroup(Group* group, bool trackPrevious = true);
    const Database* database() const;
    Database* database();

    bool canUpdateTimeinfo() const;
    void setUpdateTimeinfo(bool value);

signals:
    /**
     * Emitted when a default attribute has been changed.
     */
    void entryDataChanged(Entry* entry);

private slots:
    void emitDataChanged();
    void updateTimeinfo();
    void updateModifiedSinceBegin();
    void updateTotp();

private:
    QString resolveMultiplePlaceholdersRecursive(const QString& str, int maxDepth) const;
    QString resolvePlaceholderRecursive(const QString& placeholder, int maxDepth) const;
    QString resolveReferencePlaceholderRecursive(const QString& placeholder, int maxDepth) const;
    QString referenceFieldValue(EntryReferenceType referenceType) const;

    static QString buildReference(const QUuid& uuid, const QString& field);
    static EntryReferenceType referenceType(const QString& referenceStr);

    template <class T> bool set(T& property, const T& value);

    QUuid m_uuid;
    EntryData m_data;
    QPointer<EntryAttributes> m_attributes;
    QPointer<EntryAttachments> m_attachments;
    QPointer<AutoTypeAssociations> m_autoTypeAssociations;
    QPointer<CustomData> m_customData;
    QList<Entry*> m_history; // Items sorted from oldest to newest
    QPointer<Entry> m_historyOwner;

    QScopedPointer<Entry> m_tmpHistoryItem;
    bool m_modifiedSinceBegin;
    QPointer<Group> m_group;
    bool m_updateTimeinfo;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Entry::CloneFlags)

#endif // KEEPASSX_ENTRY_H
