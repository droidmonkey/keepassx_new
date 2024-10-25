/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#include "EntryAttributes.h"
#include "core/Global.h"
#include "core/Tools.h"

#include <QRegularExpression>
#include <QUuid>

const QString EntryAttributes::TitleKey = "Title";
const QString EntryAttributes::UserNameKey = "UserName";
const QString EntryAttributes::PasswordKey = "Password";
const QString EntryAttributes::URLKey = "URL";
const QString EntryAttributes::NotesKey = "Notes";
const QStringList EntryAttributes::DefaultAttributes(QStringList()
                                                     << TitleKey << UserNameKey << PasswordKey << URLKey << NotesKey);
const QString EntryAttributes::WantedFieldGroupName = "WantedField";
const QString EntryAttributes::SearchInGroupName = "SearchIn";
const QString EntryAttributes::SearchTextGroupName = "SearchText";

const QString EntryAttributes::RememberCmdExecAttr = "_EXEC_CMD";
const QString EntryAttributes::AdditionalUrlAttribute = "KP2A_URL";

// Passkey related attributes
const QString EntryAttributes::PasskeyAttribute = "KPEX_PASSKEY";
const QString EntryAttributes::KPEX_PASSKEY_USERNAME = QStringLiteral("KPEX_PASSKEY_USERNAME");
const QString EntryAttributes::KPEX_PASSKEY_CREDENTIAL_ID = QStringLiteral("KPEX_PASSKEY_CREDENTIAL_ID");
const QString EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_PEM = QStringLiteral("KPEX_PASSKEY_PRIVATE_KEY_PEM");
const QString EntryAttributes::KPEX_PASSKEY_RELYING_PARTY = QStringLiteral("KPEX_PASSKEY_RELYING_PARTY");
const QString EntryAttributes::KPEX_PASSKEY_USER_HANDLE = QStringLiteral("KPEX_PASSKEY_USER_HANDLE");
const QString EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_START = QStringLiteral("-----BEGIN PRIVATE KEY-----");
const QString EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_END = QStringLiteral("-----END PRIVATE KEY-----");

// For compatibility with StrongBox
const QString EntryAttributes::KPEX_PASSKEY_GENERATED_USER_ID = QStringLiteral("KPEX_PASSKEY_GENERATED_USER_ID");
const QString EntryAttributes::KPXC_PASSKEY_USERNAME = QStringLiteral("KPXC_PASSKEY_USERNAME");

EntryAttributes::EntryAttributes(QObject* parent)
    : ModifiableObject(parent)
{
    clear();
}

QList<QString> EntryAttributes::keys() const
{
    return m_attributes.keys();
}

bool EntryAttributes::hasKey(const QString& key) const
{
    return m_attributes.contains(key);
}

bool EntryAttributes::hasPasskey() const
{
    const auto keyList = keys();
    for (const auto& key : keyList) {
        if (isPasskeyAttribute(key)) {
            return true;
        }
    }

    return false;
}

void EntryAttributes::removePasskeyAttributes()
{
    const auto keyList = keys();
    for (const auto& key : keyList) {
        if (isPasskeyAttribute(key)) {
            remove(key);
        }
    }
}

QList<QString> EntryAttributes::customKeys() const
{
    QList<QString> customKeys;
    const QList<QString> keyList = keys();
    for (const QString& key : keyList) {
        if (!isDefaultAttribute(key) && !isPasskeyAttribute(key)) {
            customKeys.append(key);
        }
    }
    return customKeys;
}

QString EntryAttributes::value(const QString& key) const
{
    return m_attributes.value(key);
}

QList<QString> EntryAttributes::values(const QList<QString>& keys) const
{
    QList<QString> values;
    for (const QString& key : keys) {
        values.append(m_attributes.value(key));
    }
    return values;
}

bool EntryAttributes::contains(const QString& key) const
{
    return m_attributes.contains(key);
}

bool EntryAttributes::containsValue(const QString& value) const
{
    return asConst(m_attributes).values().contains(value);
}

bool EntryAttributes::isProtected(const QString& key) const
{
    return m_protectedAttributes.contains(key);
}

bool EntryAttributes::isReference(const QString& key) const
{
    if (!m_attributes.contains(key)) {
        Q_ASSERT(false);
        return false;
    }

    const QString data = value(key);
    return matchReference(data).hasMatch();
}

void EntryAttributes::set(const QString& key, const QString& value, bool protect)
{
    bool shouldEmitModified = false;

    bool addAttribute = !m_attributes.contains(key);
    bool changeValue = !addAttribute && (m_attributes.value(key) != value);
    bool defaultAttribute = isDefaultAttribute(key);

    if (addAttribute && !defaultAttribute) {
        emit aboutToBeAdded(key);
    }

    if (addAttribute || changeValue) {
        m_attributes.insert(key, value);
        shouldEmitModified = true;
    }

    if (protect) {
        if (!m_protectedAttributes.contains(key)) {
            shouldEmitModified = true;
        }
        m_protectedAttributes.insert(key);
    } else if (m_protectedAttributes.remove(key)) {
        shouldEmitModified = true;
    }

    if (shouldEmitModified) {
        emitModified();
    }

    if (defaultAttribute && changeValue) {
        emit defaultKeyModified();
    } else if (addAttribute) {
        emit added(key);
    } else if (shouldEmitModified) {
        emit customKeyModified(key);
    }
}

void EntryAttributes::remove(const QString& key)
{
    Q_ASSERT(!isDefaultAttribute(key));

    if (!m_attributes.contains(key)) {
        return;
    }

    emit aboutToBeRemoved(key);

    m_attributes.remove(key);
    m_protectedAttributes.remove(key);

    emit removed(key);
    emitModified();
}

void EntryAttributes::rename(const QString& oldKey, const QString& newKey)
{
    Q_ASSERT(!isDefaultAttribute(oldKey));
    Q_ASSERT(!isDefaultAttribute(newKey));

    if (!m_attributes.contains(oldKey)) {
        Q_ASSERT(false);
        return;
    }

    if (m_attributes.contains(newKey)) {
        Q_ASSERT(false);
        return;
    }

    QString data = value(oldKey);
    bool protect = isProtected(oldKey);

    emit aboutToRename(oldKey, newKey);

    m_attributes.remove(oldKey);
    m_attributes.insert(newKey, data);
    if (protect) {
        m_protectedAttributes.remove(oldKey);
        m_protectedAttributes.insert(newKey);
    }

    emitModified();
    emit renamed(oldKey, newKey);
}

void EntryAttributes::copyCustomKeysFrom(const EntryAttributes* other)
{
    if (!areCustomKeysDifferent(other)) {
        return;
    }

    emit aboutToBeReset();

    // remove all non-default keys
    const QList<QString> keyList = keys();
    for (const QString& key : keyList) {
        if (!isDefaultAttribute(key)) {
            m_attributes.remove(key);
            m_protectedAttributes.remove(key);
        }
    }

    const QList<QString> otherKeyList = other->keys();
    for (const QString& key : otherKeyList) {
        if (!isDefaultAttribute(key)) {
            m_attributes.insert(key, other->value(key));
            if (other->isProtected(key)) {
                m_protectedAttributes.insert(key);
            }
        }
    }

    emit reset();
    emitModified();
}

bool EntryAttributes::areCustomKeysDifferent(const EntryAttributes* other)
{
    // check if they are equal ignoring the order of the keys
    if (Tools::asSet(keys()) != Tools::asSet(other->keys())) {
        return true;
    }

    const QList<QString> keyList = keys();
    for (const QString& key : keyList) {
        if (isDefaultAttribute(key)) {
            continue;
        }

        if (isProtected(key) != other->isProtected(key) || value(key) != other->value(key)) {
            return true;
        }
    }

    return false;
}

void EntryAttributes::copyDataFrom(const EntryAttributes* other)
{
    if (*this != *other) {
        emit aboutToBeReset();

        m_attributes = other->m_attributes;
        m_protectedAttributes = other->m_protectedAttributes;

        emit reset();
        emitModified();
    }
}

QUuid EntryAttributes::referenceUuid(const QString& key) const
{
    if (!m_attributes.contains(key)) {
        Q_ASSERT(false);
        return {};
    }

    auto match = matchReference(value(key));
    if (match.hasMatch()) {
        const QString uuid = match.captured("SearchText");
        if (!uuid.isEmpty()) {
            return QUuid::fromRfc4122(QByteArray::fromHex(uuid.toLatin1()));
        }
    }

    return {};
}

bool EntryAttributes::operator==(const EntryAttributes& other) const
{
    return (m_attributes == other.m_attributes && m_protectedAttributes == other.m_protectedAttributes);
}

bool EntryAttributes::operator!=(const EntryAttributes& other) const
{
    return (m_attributes != other.m_attributes || m_protectedAttributes != other.m_protectedAttributes);
}

QRegularExpressionMatch EntryAttributes::matchReference(const QString& text)
{
    static const QRegularExpression referenceRegExp(
        R"(\{REF:(?<WantedField>[TUPANI])@(?<SearchIn>[TUPANIO]):(?<SearchText>[^}]+)\})",
        QRegularExpression::CaseInsensitiveOption);

    return referenceRegExp.match(text);
}

void EntryAttributes::clear()
{
    emit aboutToBeReset();

    m_attributes.clear();
    m_protectedAttributes.clear();

    for (const QString& key : DefaultAttributes) {
        m_attributes.insert(key, "");
    }

    emit reset();
    emitModified();
}

int EntryAttributes::attributesSize() const
{
    int size = 0;
    for (auto it = m_attributes.constBegin(); it != m_attributes.constEnd(); ++it) {
        size += it.key().toUtf8().size() + it.value().toUtf8().size();
    }
    return size;
}

bool EntryAttributes::isDefaultAttribute(const QString& key)
{
    return DefaultAttributes.contains(key);
}

bool EntryAttributes::isPasskeyAttribute(const QString& key)
{
    return key.startsWith(PasskeyAttribute);
}
