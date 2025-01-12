/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>  
 *  Copyright (C) 2019 Aetf <aetf@unlimitedcodeworks.xyz>
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

#include "Item.h"

#include "fdosecrets/FdoSecretsPlugin.h"
#include "fdosecrets/objects/Collection.h"
#include "fdosecrets/objects/Prompt.h"
#include "fdosecrets/objects/Service.h"
#include "fdosecrets/objects/Session.h"

#include "core/EntryAttributes.h"
#include "core/Group.h"

#include <QMimeDatabase>
#include <QSet>
#include <QStringDecoder>

namespace FdoSecrets
{
    struct EntryUpdater
    {
        Entry* entry;
        explicit EntryUpdater(Entry* entry)
            : entry(entry)
        {
            entry->beginUpdate();
        }

        ~EntryUpdater()
        {
            entry->endUpdate();
        }
    };

    const QSet<QString> Item::ReadOnlyAttributes(QSet<QString>() << ItemAttributes::UuidKey << ItemAttributes::PathKey
                                                                 << ItemAttributes::TotpKey);

    static DBusResult setEntrySecret(Entry* entry, const QByteArray& data, const QString& contentType);
    static Secret getEntrySecret(Entry* entry);

    namespace
    {
        constexpr auto FDO_SECRETS_DATA = "FDO_SECRETS_DATA";
        constexpr auto FDO_SECRETS_CONTENT_TYPE = "FDO_SECRETS_CONTENT_TYPE";
    } // namespace

    Item* Item::Create(Collection* parent, Entry* backend)
    {
        QScopedPointer<Item> res{new Item(parent, backend)};
        if (!res->dbus()->registerObject(res.data())) {
            return nullptr;
        }

        return res.take();
    }

    Item::Item(Collection* parent, Entry* backend)
        : DBusObject(parent)
        , m_backend(backend)
    {
        connect(m_backend, &Entry::modified, this, &Item::itemChanged);
    }

    DBusResult Item::locked(const DBusClientPtr& client, bool& locked) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = collection()->locked(locked);
        if (ret.err()) {
            return ret;
        }
        locked = locked || !client->itemAuthorized(m_backend->uuid());
        return {};
    }

    DBusResult Item::attributes(StringStringMap& attrs) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }

        // add default attributes except password
        auto entryAttrs = m_backend->attributes();
        for (const auto& attr : EntryAttributes::DefaultAttributes) {
            if (entryAttrs->isProtected(attr) || attr == EntryAttributes::PasswordKey) {
                continue;
            }

            auto value = entryAttrs->value(attr);
            if (entryAttrs->isReference(attr)) {
                value = m_backend->maskPasswordPlaceholders(value);
                value = m_backend->resolveMultiplePlaceholders(value);
            }
            attrs[attr] = value;
        }

        // add custom attributes
        const auto customKeys = entryAttrs->customKeys();
        for (const auto& attr : customKeys) {
            attrs[attr] = entryAttrs->value(attr);
        }

        // add some informative and readonly attributes
        attrs[ItemAttributes::UuidKey] = m_backend->uuidToHex();
        attrs[ItemAttributes::PathKey] = path();
        if (m_backend->hasTotp()) {
            attrs[ItemAttributes::TotpKey] = m_backend->totp();
        }
        return {};
    }

    DBusResult Item::setAttributes(const StringStringMap& attrs)
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }

        // set on a temp variable first and check for errors to avoid partial updates
        EntryAttributes tempAttrs;
        tempAttrs.copyDataFrom(m_backend->attributes());
        for (auto it = attrs.constBegin(); it != attrs.constEnd(); ++it) {
            if (tempAttrs.isProtected(it.key()) || it.key() == EntryAttributes::PasswordKey) {
                return QDBusError::InvalidArgs;
            }

            if (ReadOnlyAttributes.contains(it.key())) {
                return QDBusError::InvalidArgs;
            }

            if (EntryAttributes::matchReference(it.value()).hasMatch()) {
                return QDBusError::InvalidArgs;
            }

            tempAttrs.set(it.key(), it.value());
        }

        // actually update the backend
        EntryUpdater eu(m_backend);
        m_backend->attributes()->copyDataFrom(&tempAttrs);
        return {};
    }

    DBusResult Item::label(QString& label) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }

        label = m_backend->title();
        return {};
    }

    DBusResult Item::setLabel(const QString& label)
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }

        if (EntryAttributes::matchReference(label).hasMatch()) {
            return QDBusError::InvalidArgs;
        }

        EntryUpdater eu(m_backend);
        m_backend->setTitle(label);
        return {};
    }

    DBusResult Item::created(qulonglong& created) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }

        created = static_cast<qulonglong>(m_backend->timeInfo().creationTime().toMSecsSinceEpoch() / 1000);
        return {};
    }

    DBusResult Item::modified(qulonglong& modified) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }

        modified = static_cast<qulonglong>(m_backend->timeInfo().lastModificationTime().toMSecsSinceEpoch() / 1000);
        return {};
    }

    DBusResult Item::remove(PromptBase*& prompt)
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        prompt = PromptBase::Create<DeleteItemPrompt>(service(), this);
        if (!prompt) {
            return QDBusError::InternalError;
        }
        return {};
    }

    DBusResult Item::getSecret(const DBusClientPtr& client, Session* session, Secret& secret)
    {
        auto ret = getSecretNoNotification(client, session, secret);
        if (ret.ok()) {
            service()->plugin()->emitRequestShowNotification(
                tr(R"(Entry "%1" from database "%2" was used by %3)")
                    .arg(m_backend->title(), collection()->name(), client->name()));
        }
        return ret;
    }

    DBusResult Item::getSecretNoNotification(const DBusClientPtr& client, Session* session, Secret& secret) const
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }
        if (!client->itemAuthorizedResetOnce(backend()->uuid())) {
            return DBusResult(DBUS_ERROR_SECRET_IS_LOCKED);
        }

        if (!session) {
            return DBusResult(DBUS_ERROR_SECRET_NO_SESSION);
        }

        secret = getEntrySecret(m_backend);

        // encode using session
        secret = session->encode(secret);

        return {};
    }

    DBusResult Item::setSecret(const DBusClientPtr& client, const Secret& secret)
    {
        auto ret = ensureBackend();
        if (ret.err()) {
            return ret;
        }
        ret = ensureUnlocked();
        if (ret.err()) {
            return ret;
        }
        if (!client->itemAuthorizedResetOnce(backend()->uuid())) {
            return DBusResult(DBUS_ERROR_SECRET_IS_LOCKED);
        }

        if (!secret.session) {
            return DBusResult(DBUS_ERROR_SECRET_NO_SESSION);
        }

        // decode using session
        auto decoded = secret.session->decode(secret);

        // block references
        if (EntryAttributes::matchReference(decoded.value).hasMatch()) {
            return QDBusError::InvalidArgs;
        }

        EntryUpdater eu(m_backend);
        // set in backend
        return setEntrySecret(m_backend, decoded.value, decoded.contentType);
    }

    DBusResult Item::setProperties(const QVariantMap& properties)
    {
        auto label = properties.value(DBUS_INTERFACE_SECRET_ITEM + ".Label").toString();

        auto ret = setLabel(label);
        if (ret.err()) {
            return ret;
        }

        auto attributes = properties.value(DBUS_INTERFACE_SECRET_ITEM + ".Attributes").value<StringStringMap>();
        ret = setAttributes(attributes);
        if (ret.err()) {
            return ret;
        }

        return {};
    }

    Collection* Item::collection() const
    {
        return qobject_cast<Collection*>(parent());
    }

    DBusResult Item::ensureBackend() const
    {
        if (!m_backend) {
            return DBusResult(DBUS_ERROR_SECRET_NO_SUCH_OBJECT);
        }
        return {};
    }

    DBusResult Item::ensureUnlocked() const
    {
        bool l;
        auto ret = collection()->locked(l);
        if (ret.err()) {
            return ret;
        }
        if (l) {
            return DBusResult(DBUS_ERROR_SECRET_IS_LOCKED);
        }
        return {};
    }

    Entry* Item::backend() const
    {
        return m_backend;
    }

    bool Item::doDelete()
    {
        Q_ASSERT(m_backend);

        return collection()->doDeleteEntry(m_backend);
    }

    void Item::removeFromDBus()
    {
        emit itemAboutToDelete();

        // Unregister current path early, do not rely on deleteLater's call to destructor
        // as in case of Entry moving between groups, new Item will be created at the same DBus path
        // before the current Item is deleted in the event loop.
        dbus()->unregisterObject(this);

        m_backend = nullptr;
        deleteLater();
    }

    Service* Item::service() const
    {
        return collection()->service();
    }

    QString Item::path() const
    {
        QStringList pathComponents{m_backend->title()};

        Group* group = m_backend->group();
        while (group && group != collection()->exposedRootGroup()) {
            pathComponents.prepend(group->name());
            group = group->parentGroup();
        }
        // we should always reach the exposed root group
        Q_ASSERT(group);

        // root group is represented by a single slash, thus adding an empty component.
        pathComponents.prepend("");

        return pathComponents.join('/');
    }

    DBusResult setEntrySecret(Entry* entry, const QByteArray& data, const QString& contentType)
    {
        auto mimeName = contentType.split(';').takeFirst().trimmed();

        // find the mime type
        QMimeDatabase db;
        auto mimeType = db.mimeTypeForName(mimeName);

        // find a suitable codec
        QStringDecoder decoder(QStringDecoder::Utf8);
        static const QRegularExpression charsetPattern(QStringLiteral(R"re(charset=(?<encode>.+)$)re"));
        auto match = charsetPattern.match(contentType);
        if (match.hasMatch()) {
            auto codecName = match.captured(QStringLiteral("encode")).toUtf8();
            auto encoding = QStringDecoder::encodingForName(codecName);
            if (encoding != std::nullopt) {
                decoder = QStringDecoder(encoding.value());
            }
        }

        if (!mimeType.isValid() || !mimeType.inherits(QStringLiteral("text/plain"))) {
            if (EntryAttributes::matchReference(contentType).hasMatch()) {
                return QDBusError::InvalidArgs;
            }
            // we can't handle this content type, save the data as attachment, and clear the password field
            entry->setPassword("");
            entry->attachments()->set(FDO_SECRETS_DATA, data);
            entry->attributes()->set(FDO_SECRETS_CONTENT_TYPE, contentType);
        } else {
            auto password = decoder.decode(data);
            if (EntryAttributes::matchReference(password).hasMatch()) {
                return QDBusError::InvalidArgs;
            }
            // save the data to password field
            entry->setPassword(password);
            if (entry->attachments()->hasKey(FDO_SECRETS_DATA)) {
                entry->attachments()->remove(FDO_SECRETS_DATA);
            }
            if (entry->attributes()->hasKey(FDO_SECRETS_CONTENT_TYPE)) {
                entry->attributes()->remove(FDO_SECRETS_CONTENT_TYPE);
            }
        }
        return {};
    }

    Secret getEntrySecret(Entry* entry)
    {
        Secret ss{};

        if (entry->attachments()->hasKey(FDO_SECRETS_DATA)) {
            ss.value = entry->attachments()->value(FDO_SECRETS_DATA);
            if (entry->attributes()->hasKey(FDO_SECRETS_CONTENT_TYPE)) {
                ss.contentType = entry->attributes()->value(FDO_SECRETS_CONTENT_TYPE);
            } else {
                // the entry is somehow corrupted, maybe the user deleted it.
                // set to binary and hope for the best...
                ss.contentType = QStringLiteral("application/octet-stream");
            }
            return ss;
        }

        ss.value = entry->resolveMultiplePlaceholders(entry->password()).toUtf8();
        ss.contentType = QStringLiteral("text/plain");
        return ss;
    }

} // namespace FdoSecrets
