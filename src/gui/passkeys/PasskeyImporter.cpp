/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PasskeyImporter.h"
#include "PasskeyImportDialog.h"
#include "browser/BrowserMessageBuilder.h"
#include "browser/BrowserPasskeys.h"
#include "browser/BrowserService.h"
#include "core/Entry.h"
#include "core/Group.h"
#include "core/Tools.h"
#include "gui/FileDialog.h"
#include "gui/MessageBox.h"
#include <QFileInfo>
#include <QUuid>

static const QString IMPORTED_PASSKEYS_GROUP = QStringLiteral("Imported Passkeys");

PasskeyImporter::PasskeyImporter(QWidget* parent)
    : m_parent(parent)
{
}

void PasskeyImporter::importPasskey(QSharedPointer<Database>& database, Entry* entry)
{
    auto filter = QString("%1 (*.passkey);;%2 (*)").arg(tr("Passkey file"), tr("All files"));
    auto fileName =
        fileDialog()->getOpenFileName(m_parent, tr("Open passkey file"), FileDialog::getLastDir("passkey"), filter);
    if (fileName.isEmpty()) {
        return;
    }

    FileDialog::saveLastDir("passkey", fileName, true);

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        MessageBox::information(
            nullptr, tr("Cannot open file"), tr("Cannot open file \"%1\" for reading.").arg(fileName));
        return;
    }

    importSelectedFile(file, database, entry);
}

void PasskeyImporter::importSelectedFile(QFile& file, QSharedPointer<Database>& database, Entry* entry)
{
    const auto fileData = file.readAll();
    const auto passkeyObject = browserMessageBuilder()->getJsonObject(fileData);
    if (passkeyObject.isEmpty()) {
        MessageBox::information(m_parent,
                                tr("Cannot import passkey"),
                                tr("Cannot import passkey file \"%1\". Data is missing.").arg(file.fileName()));
        return;
    }

    const auto privateKey = passkeyObject["privateKey"].toString();
    const auto missingKeys = Tools::getMissingValuesFromList<QString>(
        passkeyObject.keys(),
        QStringList() << "relyingParty" << "url" << "username" << "credentialId" << "userHandle" << "privateKey");
    if (!missingKeys.isEmpty()) {
        MessageBox::information(m_parent,
                                tr("Cannot import passkey"),
                                tr("Cannot import passkey file \"%1\".\nThe following data is missing:\n%2")
                                    .arg(file.fileName(), missingKeys.join(", ")));
    } else if (!privateKey.startsWith(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_START)
               || !privateKey.trimmed().endsWith(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_END)) {
        MessageBox::information(
            m_parent,
            tr("Cannot import passkey"),
            tr("Cannot import passkey file \"%1\". Private key is missing or malformed.").arg(file.fileName()));
    } else {
        const auto relyingParty = passkeyObject["relyingParty"].toString();
        const auto url = passkeyObject["url"].toString();
        const auto username = passkeyObject["username"].toString();
        const auto credentialId = passkeyObject["credentialId"].toString();
        const auto userHandle = passkeyObject["userHandle"].toString();
        showImportDialog(database, entry, url, relyingParty, username, credentialId, userHandle, privateKey);
    }
}

bool PasskeyImporter::showImportDialog(QSharedPointer<Database>& database,
                                       Entry* entry,
                                       const QString& url,
                                       const QString& relyingParty,
                                       const QString& username,
                                       const QString& credentialId,
                                       const QString& userHandle,
                                       const QString& privateKey,
                                       const QString& titleText,
                                       const QString& infoText,
                                       const QString& importButtonText)
{
    PasskeyImportDialog passkeyImportDialog(m_parent);
    passkeyImportDialog.setInfo(
        relyingParty, username, database, entry != nullptr, titleText, infoText, importButtonText);

    auto ret = passkeyImportDialog.exec();
    if (ret != QDialog::Accepted) {
        return false;
    }

    auto db = passkeyImportDialog.getSelectedDatabase();
    if (!db) {
        db = database;
    }

    // Store to entry if given directly
    if (entry) {
        browserService()->addPasskeyToEntry(
            entry, relyingParty, relyingParty, username, credentialId, userHandle, privateKey);
        return true;
    }

    // Import to entry selected instead of creating a new one
    if (!passkeyImportDialog.createNewEntry()) {
        auto groupUuid = passkeyImportDialog.getSelectedGroupUuid();
        auto group = db->rootGroup()->findGroupByUuid(groupUuid);

        if (group) {
            auto selectedEntry = group->findEntryByUuid(passkeyImportDialog.getSelectedEntryUuid());
            if (selectedEntry) {
                browserService()->addPasskeyToEntry(
                    selectedEntry, relyingParty, relyingParty, username, credentialId, userHandle, privateKey);
            }
        }

        return true;
    }

    // Group settings. Use default group "Imported Passkeys" if user did not select a specific one.
    Group* group = nullptr;

    // Attempt to use the selected group
    if (!passkeyImportDialog.useDefaultGroup()) {
        auto groupUuid = passkeyImportDialog.getSelectedGroupUuid();
        group = db->rootGroup()->findGroupByUuid(groupUuid);
    }

    // Use default group if requested or if the selected group does not exist
    if (!group) {
        group = getDefaultGroup(db);
    }

    browserService()->addPasskeyToGroup(
        db, group, url, relyingParty, relyingParty, username, credentialId, userHandle, privateKey);

    return true;
}

Group* PasskeyImporter::getDefaultGroup(QSharedPointer<Database>& database) const
{
    auto defaultGroup = database->rootGroup()->findGroupByPath(IMPORTED_PASSKEYS_GROUP);

    // Create the default group if it does not exist
    if (!defaultGroup) {
        defaultGroup = new Group();
        defaultGroup->setName(IMPORTED_PASSKEYS_GROUP);
        defaultGroup->setUuid(QUuid::createUuid());
        defaultGroup->setParent(database->rootGroup());
    }

    return defaultGroup;
}
