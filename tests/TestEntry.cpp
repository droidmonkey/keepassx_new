/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2013 Felix Geyer <debfx@fobos.de>
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

#include <QTest>

#include "TestEntry.h"
#include "core/Clock.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/TimeInfo.h"
#include "crypto/Crypto.h"

QTEST_GUILESS_MAIN(TestEntry)

void TestEntry::initTestCase()
{
    QVERIFY(Crypto::init());
}

void TestEntry::testHistoryItemDeletion()
{
    QScopedPointer<Entry> entry(new Entry());
    QPointer<Entry> historyEntry = new Entry();

    entry->addHistoryItem(historyEntry);
    QCOMPARE(entry->historyItems().size(), 1);

    QList<Entry*> historyEntriesToRemove;
    historyEntriesToRemove.append(historyEntry);
    entry->removeHistoryItems(historyEntriesToRemove);
    QCOMPARE(entry->historyItems().size(), 0);
    QVERIFY(historyEntry.isNull());
}

void TestEntry::testCopyDataFrom()
{
    QScopedPointer<Entry> entry(new Entry());

    entry->setTitle("testtitle");
    entry->attributes()->set("attr1", "abc");
    entry->attributes()->set("attr2", "def");

    entry->attachments()->set("test", "123");
    entry->attachments()->set("test2", "456");

    AutoTypeAssociations::Association assoc;
    assoc.window = "1";
    assoc.sequence = "2";
    entry->autoTypeAssociations()->add(assoc);
    assoc.window = "3";
    assoc.sequence = "4";
    entry->autoTypeAssociations()->add(assoc);

    QScopedPointer<Entry> entry2(new Entry());
    entry2->copyDataFrom(entry.data());

    QCOMPARE(entry2->title(), QString("testtitle"));
    QCOMPARE(entry2->attributes()->value("attr1"), QString("abc"));
    QCOMPARE(entry2->attributes()->value("attr2"), QString("def"));

    QCOMPARE(entry2->attachments()->keys().size(), 2);
    QCOMPARE(entry2->attachments()->value("test"), QByteArray("123"));
    QCOMPARE(entry2->attachments()->value("test2"), QByteArray("456"));

    QCOMPARE(entry2->autoTypeAssociations()->size(), 2);
    QCOMPARE(entry2->autoTypeAssociations()->get(0).window, QString("1"));
    QCOMPARE(entry2->autoTypeAssociations()->get(1).window, QString("3"));
}

void TestEntry::testClone()
{
    QScopedPointer<Entry> entryOrg(new Entry());
    entryOrg->setUuid(QUuid::createUuid());
    entryOrg->setPassword("pass");
    entryOrg->setTitle("Original Title");
    entryOrg->beginUpdate();
    entryOrg->setTitle("New Title");
    entryOrg->endUpdate();
    TimeInfo entryOrgTime = entryOrg->timeInfo();
    QDateTime dateTime = Clock::datetimeUtc(60);
    entryOrgTime.setCreationTime(dateTime);
    entryOrg->setTimeInfo(entryOrgTime);

    QScopedPointer<Entry> entryCloneNone(entryOrg->clone(Entry::CloneNoFlags));
    QCOMPARE(entryCloneNone->uuid(), entryOrg->uuid());
    QCOMPARE(entryCloneNone->title(), QString("New Title"));
    QCOMPARE(entryCloneNone->historyItems().size(), 0);
    QCOMPARE(entryCloneNone->timeInfo().creationTime(), entryOrg->timeInfo().creationTime());

    QScopedPointer<Entry> entryCloneNewUuid(entryOrg->clone(Entry::CloneNewUuid));
    QVERIFY(entryCloneNewUuid->uuid() != entryOrg->uuid());
    QVERIFY(!entryCloneNewUuid->uuid().isNull());
    QCOMPARE(entryCloneNewUuid->title(), QString("New Title"));
    QCOMPARE(entryCloneNewUuid->historyItems().size(), 0);
    QCOMPARE(entryCloneNewUuid->timeInfo().creationTime(), entryOrg->timeInfo().creationTime());

    // Reset modification time
    entryOrgTime.setLastModificationTime(Clock::datetimeUtc(60));
    entryOrg->setTimeInfo(entryOrgTime);

    QScopedPointer<Entry> entryCloneRename(entryOrg->clone(Entry::CloneRenameTitle));
    QCOMPARE(entryCloneRename->uuid(), entryOrg->uuid());
    QCOMPARE(entryCloneRename->title(), QString("New Title - Clone"));
    // Cloning should not modify time info unless explicitly requested
    QCOMPARE(entryCloneRename->timeInfo(), entryOrg->timeInfo());

    QScopedPointer<Entry> entryCloneResetTime(entryOrg->clone(Entry::CloneResetTimeInfo));
    QCOMPARE(entryCloneResetTime->uuid(), entryOrg->uuid());
    QCOMPARE(entryCloneResetTime->title(), QString("New Title"));
    QCOMPARE(entryCloneResetTime->historyItems().size(), 0);
    QVERIFY(entryCloneResetTime->timeInfo().creationTime() != entryOrg->timeInfo().creationTime());

    // Date back history of original entry
    Entry* firstHistoryItem = entryOrg->historyItems()[0];
    TimeInfo entryOrgHistoryTimeInfo = firstHistoryItem->timeInfo();
    QDateTime datedBackEntryOrgModificationTime = entryOrgHistoryTimeInfo.lastModificationTime().addMSecs(-10);
    entryOrgHistoryTimeInfo.setLastModificationTime(datedBackEntryOrgModificationTime);
    entryOrgHistoryTimeInfo.setCreationTime(datedBackEntryOrgModificationTime);
    firstHistoryItem->setTimeInfo(entryOrgHistoryTimeInfo);

    QScopedPointer<Entry> entryCloneHistory(entryOrg->clone(Entry::CloneIncludeHistory | Entry::CloneResetTimeInfo));
    QCOMPARE(entryCloneHistory->uuid(), entryOrg->uuid());
    QCOMPARE(entryCloneHistory->title(), QString("New Title"));
    QCOMPARE(entryCloneHistory->historyItems().size(), entryOrg->historyItems().size());
    QCOMPARE(entryCloneHistory->historyItems().at(0)->title(), QString("Original Title"));
    QVERIFY(entryCloneHistory->timeInfo().creationTime() != entryOrg->timeInfo().creationTime());
    // Timeinfo of history items should not be modified
    QList<Entry*> entryOrgHistory = entryOrg->historyItems(), clonedHistory = entryCloneHistory->historyItems();
    auto entryOrgHistoryItem = entryOrgHistory.constBegin();
    for (auto entryCloneHistoryItem = clonedHistory.constBegin(); entryCloneHistoryItem != clonedHistory.constEnd();
         ++entryCloneHistoryItem, ++entryOrgHistoryItem) {
        QCOMPARE((*entryOrgHistoryItem)->timeInfo(), (*entryCloneHistoryItem)->timeInfo());
    }

    Database db;
    auto* entryOrgClone = entryOrg->clone(Entry::CloneNoFlags);
    entryOrgClone->setGroup(db.rootGroup());

    Entry* entryCloneUserRef = entryOrgClone->clone(Entry::CloneUserAsRef);
    entryCloneUserRef->setGroup(db.rootGroup());
    QCOMPARE(entryCloneUserRef->uuid(), entryOrgClone->uuid());
    QCOMPARE(entryCloneUserRef->title(), QString("New Title"));
    QCOMPARE(entryCloneUserRef->historyItems().size(), 0);
    QCOMPARE(entryCloneUserRef->timeInfo().creationTime(), entryOrgClone->timeInfo().creationTime());
    QVERIFY(entryCloneUserRef->attributes()->isReference(EntryAttributes::UserNameKey));
    QCOMPARE(entryCloneUserRef->resolvePlaceholder(entryCloneUserRef->username()), entryOrgClone->username());

    Entry* entryClonePassRef = entryOrgClone->clone(Entry::ClonePassAsRef);
    entryClonePassRef->setGroup(db.rootGroup());
    QCOMPARE(entryClonePassRef->uuid(), entryOrgClone->uuid());
    QCOMPARE(entryClonePassRef->title(), QString("New Title"));
    QCOMPARE(entryClonePassRef->historyItems().size(), 0);
    QCOMPARE(entryClonePassRef->timeInfo().creationTime(), entryOrgClone->timeInfo().creationTime());
    QVERIFY(entryClonePassRef->attributes()->isReference(EntryAttributes::PasswordKey));
    QCOMPARE(entryClonePassRef->resolvePlaceholder(entryCloneUserRef->password()), entryOrg->password());
    QCOMPARE(entryClonePassRef->attributes()->referenceUuid(EntryAttributes::PasswordKey), entryOrgClone->uuid());
}

void TestEntry::testResolveUrl()
{
    QScopedPointer<Entry> entry(new Entry());
    QString testUrl("www.google.com");
    QString testCmd("cmd://firefox " + testUrl);
    QString testFileUnix("/home/example/test.txt");
    QString testFileWindows("c:/WINDOWS/test.txt");
    QString testComplexCmd("cmd://firefox --start-now --url 'http://" + testUrl + "' --quit");
    QString nonHttpUrl("ftp://google.com");
    QString noUrl("random text inserted here");

    // Test standard URL's
    QCOMPARE(entry->resolveUrl(""), QString(""));
    QCOMPARE(entry->resolveUrl(testUrl), "https://" + testUrl);
    QCOMPARE(entry->resolveUrl("http://" + testUrl), "http://" + testUrl);
    // Test file:// URL's
    QCOMPARE(entry->resolveUrl("file://" + testFileUnix), "file://" + testFileUnix);
    QCOMPARE(entry->resolveUrl(testFileUnix), "file://" + testFileUnix);
    QCOMPARE(entry->resolveUrl("file:///" + testFileWindows), "file:///" + testFileWindows);
    QCOMPARE(entry->resolveUrl(testFileWindows), "file:///" + testFileWindows);
    // Test cmd:// with no URL
    QCOMPARE(entry->resolveUrl("cmd://firefox"), QString(""));
    QCOMPARE(entry->resolveUrl("cmd://firefox --no-url"), QString(""));
    // Test cmd:// with URL's
    QCOMPARE(entry->resolveUrl(testCmd), "https://" + testUrl);
    QCOMPARE(entry->resolveUrl(testComplexCmd), "http://" + testUrl);
    // Test non-http URL
    QCOMPARE(entry->resolveUrl(nonHttpUrl), QString(""));
    // Test no URL
    QCOMPARE(entry->resolveUrl(noUrl), QString(""));
}

void TestEntry::testResolveUrlPlaceholders()
{
    Entry entry;
    entry.setUrl("https://user:pw@keepassxc.org:80/path/example.php?q=e&s=t+2#fragment");

    QString rmvscm("//user:pw@keepassxc.org:80/path/example.php?q=e&s=t+2#fragment"); // Entry URL without scheme name.
    QString scm("https"); // Scheme name of the entry URL.
    QString host("keepassxc.org"); // Host component of the entry URL.
    QString port("80"); // Port number of the entry URL.
    QString path("/path/example.php"); // Path component of the entry URL.
    QString query("q=e&s=t+2"); // Query information of the entry URL.
    QString userinfo("user:pw"); // User information of the entry URL.
    QString username("user"); // User name of the entry URL.
    QString password("pw"); // Password of the entry URL.
    QString fragment("fragment"); // Fragment of the entry URL.

    QCOMPARE(entry.resolvePlaceholder("{URL:RMVSCM}"), rmvscm);
    QCOMPARE(entry.resolvePlaceholder("{URL:WITHOUTSCHEME}"), rmvscm);
    QCOMPARE(entry.resolvePlaceholder("{URL:SCM}"), scm);
    QCOMPARE(entry.resolvePlaceholder("{URL:SCHEME}"), scm);
    QCOMPARE(entry.resolvePlaceholder("{URL:HOST}"), host);
    QCOMPARE(entry.resolvePlaceholder("{URL:PORT}"), port);
    QCOMPARE(entry.resolvePlaceholder("{URL:PATH}"), path);
    QCOMPARE(entry.resolvePlaceholder("{URL:QUERY}"), query);
    QCOMPARE(entry.resolvePlaceholder("{URL:USERINFO}"), userinfo);
    QCOMPARE(entry.resolvePlaceholder("{URL:USERNAME}"), username);
    QCOMPARE(entry.resolvePlaceholder("{URL:PASSWORD}"), password);
    QCOMPARE(entry.resolvePlaceholder("{URL:FRAGMENT}"), fragment);
}

void TestEntry::testResolveRecursivePlaceholders()
{
    Database db;
    auto* root = db.rootGroup();

    auto* entry1 = new Entry();
    entry1->setGroup(root);
    entry1->setUuid(QUuid::createUuid());
    entry1->setTitle("{USERNAME}");
    entry1->setUsername("{PASSWORD}");
    entry1->setPassword("{URL}");
    entry1->setUrl("{S:CustomTitle}");
    entry1->attributes()->set("CustomTitle", "RecursiveValue");
    QCOMPARE(entry1->resolveMultiplePlaceholders(entry1->title()), QString("RecursiveValue"));

    auto* entry2 = new Entry();
    entry2->setGroup(root);
    entry2->setUuid(QUuid::createUuid());
    entry2->setTitle("Entry2Title");
    entry2->setUsername("{S:CustomUserNameAttribute}");
    entry2->setPassword(QString("{REF:P@I:%1}").arg(entry1->uuidToHex()));
    entry2->setUrl("http://{S:IpAddress}:{S:Port}/{S:Uri}");
    entry2->attributes()->set("CustomUserNameAttribute", "CustomUserNameValue");
    entry2->attributes()->set("IpAddress", "127.0.0.1");
    entry2->attributes()->set("Port", "1234");
    entry2->attributes()->set("Uri", "uri/path");

    auto* entry3 = new Entry();
    entry3->setGroup(root);
    entry3->setUuid(QUuid::createUuid());
    entry3->setTitle(QString("{REF:T@I:%1}").arg(entry2->uuidToHex()));
    entry3->setUsername(QString("{REF:U@I:%1}").arg(entry2->uuidToHex()));
    entry3->setPassword(QString("{REF:P@I:%1}").arg(entry2->uuidToHex()));
    entry3->setUrl(QString("{REF:A@I:%1}").arg(entry2->uuidToHex()));

    QCOMPARE(entry3->resolveMultiplePlaceholders(entry3->title()), QString("Entry2Title"));
    QCOMPARE(entry3->resolveMultiplePlaceholders(entry3->username()), QString("CustomUserNameValue"));
    QCOMPARE(entry3->resolveMultiplePlaceholders(entry3->password()), QString("RecursiveValue"));
    QCOMPARE(entry3->resolveMultiplePlaceholders(entry3->url()), QString("http://127.0.0.1:1234/uri/path"));

    auto* entry4 = new Entry();
    entry4->setGroup(root);
    entry4->setUuid(QUuid::createUuid());
    entry4->setTitle(QString("{REF:T@I:%1}").arg(entry3->uuidToHex()));
    entry4->setUsername(QString("{REF:U@I:%1}").arg(entry3->uuidToHex()));
    entry4->setPassword(QString("{REF:P@I:%1}").arg(entry3->uuidToHex()));
    entry4->setUrl(QString("{REF:A@I:%1}").arg(entry3->uuidToHex()));

    QCOMPARE(entry4->resolveMultiplePlaceholders(entry4->title()), QString("Entry2Title"));
    QCOMPARE(entry4->resolveMultiplePlaceholders(entry4->username()), QString("CustomUserNameValue"));
    QCOMPARE(entry4->resolveMultiplePlaceholders(entry4->password()), QString("RecursiveValue"));
    QCOMPARE(entry4->resolveMultiplePlaceholders(entry4->url()), QString("http://127.0.0.1:1234/uri/path"));

    auto* entry5 = new Entry();
    entry5->setGroup(root);
    entry5->setUuid(QUuid::createUuid());
    entry5->attributes()->set("Scheme", "http");
    entry5->attributes()->set("Host", "host.org");
    entry5->attributes()->set("Port", "2017");
    entry5->attributes()->set("Path", "/some/path");
    entry5->attributes()->set("UserName", "username");
    entry5->attributes()->set("Password", "password");
    entry5->attributes()->set("Query", "q=e&t=s");
    entry5->attributes()->set("Fragment", "fragment");
    entry5->setUrl("{S:Scheme}://{S:UserName}:{S:Password}@{S:Host}:{S:Port}{S:Path}?{S:Query}#{S:Fragment}");
    entry5->setTitle("title+{URL:Path}+{URL:Fragment}+title");

    const QString url("http://username:password@host.org:2017/some/path?q=e&t=s#fragment");
    QCOMPARE(entry5->resolveMultiplePlaceholders(entry5->url()), url);
    QCOMPARE(entry5->resolveMultiplePlaceholders(entry5->title()), QString("title+/some/path+fragment+title"));

    auto* entry6 = new Entry();
    entry6->setGroup(root);
    entry6->setUuid(QUuid::createUuid());
    entry6->setTitle(QString("{REF:T@I:%1}").arg(entry3->uuidToHex()));
    entry6->setUsername(QString("{TITLE}"));
    entry6->setPassword(QString("{PASSWORD}"));

    QCOMPARE(entry6->resolvePlaceholder(entry6->title()), QString("Entry2Title"));
    QCOMPARE(entry6->resolvePlaceholder(entry6->username()), QString("Entry2Title"));
    QCOMPARE(entry6->resolvePlaceholder(entry6->password()), QString("{PASSWORD}"));

    auto* entry7 = new Entry();
    entry7->setGroup(root);
    entry7->setUuid(QUuid::createUuid());
    entry7->setTitle(QString("{REF:T@I:%1} and something else").arg(entry3->uuidToHex()));
    entry7->setUsername(QString("{TITLE}"));
    entry7->setPassword(QString("PASSWORD"));
    entry7->setNotes(QString("{lots} {of} {braces}"));

    QCOMPARE(entry7->resolvePlaceholder(entry7->title()), QString("Entry2Title and something else"));
    QCOMPARE(entry7->resolvePlaceholder(entry7->username()), QString("Entry2Title and something else"));
    QCOMPARE(entry7->resolvePlaceholder(entry7->password()), QString("PASSWORD"));
    QCOMPARE(entry7->resolvePlaceholder(entry7->notes()), QString("{lots} {of} {braces}"));
}

void TestEntry::testResolveReferencePlaceholders()
{
    Database db;
    auto* root = db.rootGroup();

    auto* entry1 = new Entry();
    entry1->setGroup(root);
    entry1->setUuid(QUuid::createUuid());
    entry1->setTitle("Title1");
    entry1->setUsername("Username1");
    entry1->setPassword("Password1");
    entry1->setUrl("Url1");
    entry1->setNotes("Notes1");
    entry1->attributes()->set("CustomAttribute1", "CustomAttributeValue1");

    auto* group = new Group();
    group->setParent(root);
    auto* entry2 = new Entry();
    entry2->setGroup(group);
    entry2->setUuid(QUuid::createUuid());
    entry2->setTitle("Title2");
    entry2->setUsername("Username2");
    entry2->setPassword("Password2");
    entry2->setUrl("Url2");
    entry2->setNotes("Notes2");
    entry2->attributes()->set("CustomAttribute2", "CustomAttributeValue2");

    auto* entry3 = new Entry();
    entry3->setGroup(group);
    entry3->setUuid(QUuid::createUuid());
    entry3->setTitle("{S:AttributeTitle}");
    entry3->setUsername("{S:AttributeUsername}");
    entry3->setPassword("{S:AttributePassword}");
    entry3->setUrl("{S:AttributeUrl}");
    entry3->setNotes("{S:AttributeNotes}");
    entry3->attributes()->set("AttributeTitle", "TitleValue");
    entry3->attributes()->set("AttributeUsername", "UsernameValue");
    entry3->attributes()->set("AttributePassword", "PasswordValue");
    entry3->attributes()->set("AttributeUrl", "UrlValue");
    entry3->attributes()->set("AttributeNotes", "NotesValue");

    auto* tstEntry = new Entry();
    tstEntry->setGroup(root);
    tstEntry->setUuid(QUuid::createUuid());

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@I:%1}").arg(entry1->uuidToHex())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@T:%1}").arg(entry1->title())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@U:%1}").arg(entry1->username())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@P:%1}").arg(entry1->password())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@A:%1}").arg(entry1->url())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@N:%1}").arg(entry1->notes())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(
                 QString("{REF:T@O:%1}").arg(entry1->attributes()->value("CustomAttribute1"))),
             entry1->title());

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@I:%1}").arg(entry1->uuidToHex())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@T:%1}").arg(entry1->title())), entry1->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:U@U:%1}").arg(entry1->username())),
             entry1->username());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:P@P:%1}").arg(entry1->password())),
             entry1->password());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:A@A:%1}").arg(entry1->url())), entry1->url());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:N@N:%1}").arg(entry1->notes())), entry1->notes());

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@I:%1}").arg(entry2->uuidToHex())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@T:%1}").arg(entry2->title())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@U:%1}").arg(entry2->username())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@P:%1}").arg(entry2->password())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@A:%1}").arg(entry2->url())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@N:%1}").arg(entry2->notes())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(
                 QString("{REF:T@O:%1}").arg(entry2->attributes()->value("CustomAttribute2"))),
             entry2->title());

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@T:%1}").arg(entry2->title())), entry2->title());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:U@U:%1}").arg(entry2->username())),
             entry2->username());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:P@P:%1}").arg(entry2->password())),
             entry2->password());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:A@A:%1}").arg(entry2->url())), entry2->url());
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:N@N:%1}").arg(entry2->notes())), entry2->notes());

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@I:%1}").arg(entry3->uuidToHex())),
             entry3->attributes()->value("AttributeTitle"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:U@I:%1}").arg(entry3->uuidToHex())),
             entry3->attributes()->value("AttributeUsername"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:P@I:%1}").arg(entry3->uuidToHex())),
             entry3->attributes()->value("AttributePassword"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:A@I:%1}").arg(entry3->uuidToHex())),
             entry3->attributes()->value("AttributeUrl"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:N@I:%1}").arg(entry3->uuidToHex())),
             entry3->attributes()->value("AttributeNotes"));

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:T@I:%1}").arg(entry3->uuidToHex().toUpper())),
             entry3->attributes()->value("AttributeTitle"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:U@I:%1}").arg(entry3->uuidToHex().toUpper())),
             entry3->attributes()->value("AttributeUsername"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:P@I:%1}").arg(entry3->uuidToHex().toUpper())),
             entry3->attributes()->value("AttributePassword"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:A@I:%1}").arg(entry3->uuidToHex().toUpper())),
             entry3->attributes()->value("AttributeUrl"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:N@I:%1}").arg(entry3->uuidToHex().toUpper())),
             entry3->attributes()->value("AttributeNotes"));

    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:t@i:%1}").arg(entry3->uuidToHex().toLower())),
             entry3->attributes()->value("AttributeTitle"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:u@i:%1}").arg(entry3->uuidToHex().toLower())),
             entry3->attributes()->value("AttributeUsername"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:p@i:%1}").arg(entry3->uuidToHex().toLower())),
             entry3->attributes()->value("AttributePassword"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:a@i:%1}").arg(entry3->uuidToHex().toLower())),
             entry3->attributes()->value("AttributeUrl"));
    QCOMPARE(tstEntry->resolveMultiplePlaceholders(QString("{REF:n@i:%1}").arg(entry3->uuidToHex().toLower())),
             entry3->attributes()->value("AttributeNotes"));
}

void TestEntry::testResolveNonIdPlaceholdersToUuid()
{
    Database db;
    auto* root = db.rootGroup();

    auto* referencedEntryTitle = new Entry();
    referencedEntryTitle->setGroup(root);
    referencedEntryTitle->setTitle("myTitle");
    referencedEntryTitle->setUuid(QUuid::createUuid());

    auto* referencedEntryUsername = new Entry();
    referencedEntryUsername->setGroup(root);
    referencedEntryUsername->setUsername("myUser");
    referencedEntryUsername->setUuid(QUuid::createUuid());

    auto* referencedEntryPassword = new Entry();
    referencedEntryPassword->setGroup(root);
    referencedEntryPassword->setPassword("myPassword");
    referencedEntryPassword->setUuid(QUuid::createUuid());

    auto* referencedEntryUrl = new Entry();
    referencedEntryUrl->setGroup(root);
    referencedEntryUrl->setUrl("myUrl");
    referencedEntryUrl->setUuid(QUuid::createUuid());

    auto* referencedEntryNotes = new Entry();
    referencedEntryNotes->setGroup(root);
    referencedEntryNotes->setNotes("myNotes");
    referencedEntryNotes->setUuid(QUuid::createUuid());

    const QList<QChar> placeholders{'T', 'U', 'P', 'A', 'N'};
    for (const QChar& searchIn : placeholders) {
        const Entry* referencedEntry = nullptr;
        QString newEntryNotesRaw("{REF:I@%1:%2}");

        switch (searchIn.toLatin1()) {
        case 'T':
            referencedEntry = referencedEntryTitle;
            newEntryNotesRaw = newEntryNotesRaw.arg(searchIn, referencedEntry->title());
            break;
        case 'U':
            referencedEntry = referencedEntryUsername;
            newEntryNotesRaw = newEntryNotesRaw.arg(searchIn, referencedEntry->username());
            break;
        case 'P':
            referencedEntry = referencedEntryPassword;
            newEntryNotesRaw = newEntryNotesRaw.arg(searchIn, referencedEntry->password());
            break;
        case 'A':
            referencedEntry = referencedEntryUrl;
            newEntryNotesRaw = newEntryNotesRaw.arg(searchIn, referencedEntry->url());
            break;
        case 'N':
            referencedEntry = referencedEntryNotes;
            newEntryNotesRaw = newEntryNotesRaw.arg(searchIn, referencedEntry->notes());
            break;
        default:
            break;
        }

        auto* newEntry = new Entry();
        newEntry->setGroup(root);
        newEntry->setNotes(newEntryNotesRaw);

        const QString newEntryNotesResolved = newEntry->resolveMultiplePlaceholders(newEntry->notes());
        QCOMPARE(newEntryNotesResolved, referencedEntry->uuidToHex());
    }
}

void TestEntry::testResolveConversionPlaceholders()
{
    Database db;
    auto* root = db.rootGroup();

    auto* entry1 = new Entry();
    entry1->setGroup(root);
    entry1->setUuid(QUuid::createUuid());
    entry1->setTitle("Title1 {T-CONV:/{USERNAME}/lower/} {T-CONV:/{PASSWORD}/upper/}");
    entry1->setUsername("Username1");
    entry1->setPassword("Password1");
    entry1->setUrl("https://example.com/?test=3423&h=sdsds");

    auto* entry2 = new Entry();
    entry2->setGroup(root);
    entry2->setUuid(QUuid::createUuid());
    entry2->setTitle("Title2");
    entry2->setUsername(QString("{T-CONV:/{REF:U@I:%1}/UPPER/}").arg(entry1->uuidToHex()));
    entry2->setPassword(QString("{REF:P@I:%1}").arg(entry1->uuidToHex()));
    entry2->setUrl("cmd://ssh {USERNAME}@server.com -p {PASSWORD}");

    // Test complicated and nested conversions
    QCOMPARE(entry1->resolveMultiplePlaceholders(entry1->title()), QString("Title1 username1 PASSWORD1"));
    QCOMPARE(entry2->resolveMultiplePlaceholders(entry2->url()),
             QString("cmd://ssh USERNAME1@server.com -p Password1"));
    // Test base64 and hex conversions
    QCOMPARE(entry1->resolveMultiplePlaceholders("{T-CONV:/{PASSWORD}/base64/}"), QString("UGFzc3dvcmQx"));
    QCOMPARE(entry1->resolveMultiplePlaceholders("{T-CONV:/{PASSWORD}/hex/}"), QString("50617373776f726431"));
    // Test URL encode and decode
    auto encodedURL = entry1->resolveMultiplePlaceholders("{T-CONV:/{URL}/uri/}");
    QCOMPARE(encodedURL, QString("https%3A%2F%2Fexample.com%2F%3Ftest%3D3423%26h%3Dsdsds"));
    QCOMPARE(entry1->resolveMultiplePlaceholders(
                 "{T-CONV:/https%3A%2F%2Fexample.com%2F%3Ftest%3D3423%26h%3Dsdsds/uri-dec/}"),
             entry1->url());
    // Test invalid syntax
    QString error;
    entry1->resolveConversionPlaceholder("{T-CONV:/{USERNAME}/junk/}", &error);
    QVERIFY(!error.isEmpty());
    entry1->resolveConversionPlaceholder("{T-CONV:}", &error);
    QVERIFY(!error.isEmpty());
    // Check that error gets cleared
    entry1->resolveConversionPlaceholder("{T-CONV:/a/upper/}", &error);
    QVERIFY(error.isEmpty());
}

void TestEntry::testResolveReplacePlaceholders()
{
    Database db;
    auto* root = db.rootGroup();

    auto* entry1 = new Entry();
    entry1->setGroup(root);
    entry1->setUuid(QUuid::createUuid());
    entry1->setTitle("Title1");
    entry1->setUsername("Username1");
    entry1->setPassword("Password1");

    auto* entry2 = new Entry();
    entry2->setGroup(root);
    entry2->setUuid(QUuid::createUuid());
    entry2->setTitle("SAP server1 12345");
    entry2->setUsername(QString("{T-REPLACE-RX:/{REF:U@I:%1}/\\d$/2/}").arg(entry1->uuidToHex()));
    entry2->setPassword(QString("{REF:P@I:%1}").arg(entry1->uuidToHex()));
    entry2->setUrl(
        R"(cmd://sap.exe -system={T-REPLACE-RX:/{Title}/(?i)^(.* )?(\w+(?=(\s* \d+$)))\3/$2/} -client={T-REPLACE-RX:/{Title}/(?i)^.* (?=\d+$)//} -user={USERNAME} -pw={PASSWORD})");

    // Test complicated and nested replacements
    QCOMPARE(entry2->resolveMultiplePlaceholders(entry2->url()),
             QString("cmd://sap.exe -system=server1 -client=12345 -user=Username2 -pw=Password1"));
    // Test invalid syntax
    QString error;
    entry1->resolveRegexPlaceholder("{T-REPLACE-RX:/{USERNAME}/.*+?/test/}", &error); // invalid regex
    QVERIFY(!error.isEmpty());
    entry1->resolveRegexPlaceholder("{T-REPLACE-RX:/{USERNAME}/.*/}", &error); // no replacement
    QVERIFY(!error.isEmpty());
    // Check that error gets cleared
    entry1->resolveRegexPlaceholder("{T-REPLACE-RX:/{USERNAME}/\\d/2/}", &error);
    QVERIFY(error.isEmpty());
}

void TestEntry::testResolveClonedEntry()
{
    Database db;
    auto* root = db.rootGroup();

    auto* original = new Entry();
    original->setGroup(root);
    original->setUuid(QUuid::createUuid());
    original->setTitle("Title");
    original->setUsername("SomeUsername");
    original->setPassword("SomePassword");

    QCOMPARE(original->resolveMultiplePlaceholders(original->username()), original->username());
    QCOMPARE(original->resolveMultiplePlaceholders(original->password()), original->password());

    // Top-level clones.
    Entry* clone1 = original->clone(Entry::CloneNewUuid);
    clone1->setGroup(root);
    Entry* clone2 = original->clone(Entry::CloneUserAsRef | Entry::CloneNewUuid);
    clone2->setGroup(root);
    Entry* clone3 = original->clone(Entry::ClonePassAsRef | Entry::CloneNewUuid);
    clone3->setGroup(root);
    Entry* clone4 = original->clone(Entry::CloneUserAsRef | Entry::ClonePassAsRef | Entry::CloneNewUuid);
    clone4->setGroup(root);

    QCOMPARE(clone1->resolveMultiplePlaceholders(clone1->username()), original->username());
    QCOMPARE(clone1->resolveMultiplePlaceholders(clone1->password()), original->password());
    QCOMPARE(clone2->resolveMultiplePlaceholders(clone2->username()), original->username());
    QCOMPARE(clone2->resolveMultiplePlaceholders(clone2->password()), original->password());
    QCOMPARE(clone3->resolveMultiplePlaceholders(clone3->username()), original->username());
    QCOMPARE(clone3->resolveMultiplePlaceholders(clone3->password()), original->password());
    QCOMPARE(clone4->resolveMultiplePlaceholders(clone4->username()), original->username());
    QCOMPARE(clone4->resolveMultiplePlaceholders(clone4->password()), original->password());

    // Second-level clones.
    Entry* cclone1 = clone4->clone(Entry::CloneNewUuid);
    cclone1->setGroup(root);
    Entry* cclone2 = clone4->clone(Entry::CloneUserAsRef | Entry::CloneNewUuid);
    cclone2->setGroup(root);
    Entry* cclone3 = clone4->clone(Entry::ClonePassAsRef | Entry::CloneNewUuid);
    cclone3->setGroup(root);
    Entry* cclone4 = clone4->clone(Entry::CloneUserAsRef | Entry::ClonePassAsRef | Entry::CloneNewUuid);
    cclone4->setGroup(root);

    QCOMPARE(cclone1->resolveMultiplePlaceholders(cclone1->username()), original->username());
    QCOMPARE(cclone1->resolveMultiplePlaceholders(cclone1->password()), original->password());
    QCOMPARE(cclone2->resolveMultiplePlaceholders(cclone2->username()), original->username());
    QCOMPARE(cclone2->resolveMultiplePlaceholders(cclone2->password()), original->password());
    QCOMPARE(cclone3->resolveMultiplePlaceholders(cclone3->username()), original->username());
    QCOMPARE(cclone3->resolveMultiplePlaceholders(cclone3->password()), original->password());
    QCOMPARE(cclone4->resolveMultiplePlaceholders(cclone4->username()), original->username());
    QCOMPARE(cclone4->resolveMultiplePlaceholders(cclone4->password()), original->password());

    // Change the original's attributes and make sure that the changes are tracked.
    QString oldUsername = original->username();
    QString oldPassword = original->password();
    original->setUsername("DifferentUsername");
    original->setPassword("DifferentPassword");

    QCOMPARE(clone1->resolveMultiplePlaceholders(clone1->username()), oldUsername);
    QCOMPARE(clone1->resolveMultiplePlaceholders(clone1->password()), oldPassword);
    QCOMPARE(clone2->resolveMultiplePlaceholders(clone2->username()), original->username());
    QCOMPARE(clone2->resolveMultiplePlaceholders(clone2->password()), oldPassword);
    QCOMPARE(clone3->resolveMultiplePlaceholders(clone3->username()), oldUsername);
    QCOMPARE(clone3->resolveMultiplePlaceholders(clone3->password()), original->password());
    QCOMPARE(clone4->resolveMultiplePlaceholders(clone4->username()), original->username());
    QCOMPARE(clone4->resolveMultiplePlaceholders(clone4->password()), original->password());

    QCOMPARE(cclone1->resolveMultiplePlaceholders(cclone1->username()), original->username());
    QCOMPARE(cclone1->resolveMultiplePlaceholders(cclone1->password()), original->password());
    QCOMPARE(cclone2->resolveMultiplePlaceholders(cclone2->username()), original->username());
    QCOMPARE(cclone2->resolveMultiplePlaceholders(cclone2->password()), original->password());
    QCOMPARE(cclone3->resolveMultiplePlaceholders(cclone3->username()), original->username());
    QCOMPARE(cclone3->resolveMultiplePlaceholders(cclone3->password()), original->password());
    QCOMPARE(cclone4->resolveMultiplePlaceholders(cclone4->username()), original->username());
    QCOMPARE(cclone4->resolveMultiplePlaceholders(cclone4->password()), original->password());
}

void TestEntry::testIsRecycled()
{
    auto entry = new Entry();
    QVERIFY(!entry->isRecycled());

    Database db;
    Group* root = db.rootGroup();
    QVERIFY(root);
    entry->setGroup(root);
    QVERIFY(!entry->isRecycled());

    QVERIFY(db.metadata()->recycleBinEnabled());
    db.recycleEntry(entry);
    QVERIFY(entry->isRecycled());

    auto group1 = new Group();
    group1->setParent(root);

    auto entry1 = new Entry();
    entry1->setGroup(group1);
    QVERIFY(!entry1->isRecycled());
    db.recycleGroup(group1);
    QVERIFY(entry1->isRecycled());
}

void TestEntry::testMoveUpDown()
{
    Database db;
    Group* root = db.rootGroup();
    QVERIFY(root);

    auto entry0 = new Entry();
    QVERIFY(entry0);
    entry0->setGroup(root);
    auto entry1 = new Entry();
    QVERIFY(entry1);
    entry1->setGroup(root);
    auto entry2 = new Entry();
    QVERIFY(entry2);
    entry2->setGroup(root);
    auto entry3 = new Entry();
    QVERIFY(entry3);
    entry3->setGroup(root);
    // default order, straight
    QCOMPARE(root->entries().at(0), entry0);
    QCOMPARE(root->entries().at(1), entry1);
    QCOMPARE(root->entries().at(2), entry2);
    QCOMPARE(root->entries().at(3), entry3);

    entry0->moveDown();
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry0);
    QCOMPARE(root->entries().at(2), entry2);
    QCOMPARE(root->entries().at(3), entry3);

    entry0->moveDown();
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry0);
    QCOMPARE(root->entries().at(3), entry3);

    entry0->moveDown();
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry3);
    QCOMPARE(root->entries().at(3), entry0);

    // no effect
    entry0->moveDown();
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry3);
    QCOMPARE(root->entries().at(3), entry0);

    entry0->moveUp();
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry0);
    QCOMPARE(root->entries().at(3), entry3);

    entry0->moveUp();
    QCOMPARE(root->entries().at(0), entry1);
    QCOMPARE(root->entries().at(1), entry0);
    QCOMPARE(root->entries().at(2), entry2);
    QCOMPARE(root->entries().at(3), entry3);

    entry0->moveUp();
    QCOMPARE(root->entries().at(0), entry0);
    QCOMPARE(root->entries().at(1), entry1);
    QCOMPARE(root->entries().at(2), entry2);
    QCOMPARE(root->entries().at(3), entry3);

    // no effect
    entry0->moveUp();
    QCOMPARE(root->entries().at(0), entry0);
    QCOMPARE(root->entries().at(1), entry1);
    QCOMPARE(root->entries().at(2), entry2);
    QCOMPARE(root->entries().at(3), entry3);

    entry2->moveUp();
    QCOMPARE(root->entries().at(0), entry0);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry1);
    QCOMPARE(root->entries().at(3), entry3);

    entry0->moveDown();
    QCOMPARE(root->entries().at(0), entry2);
    QCOMPARE(root->entries().at(1), entry0);
    QCOMPARE(root->entries().at(2), entry1);
    QCOMPARE(root->entries().at(3), entry3);

    entry3->moveUp();
    QCOMPARE(root->entries().at(0), entry2);
    QCOMPARE(root->entries().at(1), entry0);
    QCOMPARE(root->entries().at(2), entry3);
    QCOMPARE(root->entries().at(3), entry1);

    entry3->moveUp();
    QCOMPARE(root->entries().at(0), entry2);
    QCOMPARE(root->entries().at(1), entry3);
    QCOMPARE(root->entries().at(2), entry0);
    QCOMPARE(root->entries().at(3), entry1);

    entry2->moveDown();
    QCOMPARE(root->entries().at(0), entry3);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry0);
    QCOMPARE(root->entries().at(3), entry1);

    entry1->moveUp();
    QCOMPARE(root->entries().at(0), entry3);
    QCOMPARE(root->entries().at(1), entry2);
    QCOMPARE(root->entries().at(2), entry1);
    QCOMPARE(root->entries().at(3), entry0);
}

void TestEntry::testPreviousParentGroup()
{
    Database db;
    auto* root = db.rootGroup();
    root->setUuid(QUuid::createUuid());
    QVERIFY(!root->uuid().isNull());

    auto* group1 = new Group();
    group1->setUuid(QUuid::createUuid());
    group1->setParent(root);
    QVERIFY(!group1->uuid().isNull());

    auto* group2 = new Group();
    group2->setParent(root);
    group2->setUuid(QUuid::createUuid());
    QVERIFY(!group2->uuid().isNull());

    auto* entry = new Entry();
    QVERIFY(entry);
    QVERIFY(entry->previousParentGroupUuid().isNull());
    QVERIFY(!entry->previousParentGroup());

    entry->setGroup(root);
    QVERIFY(entry->previousParentGroupUuid().isNull());
    QVERIFY(!entry->previousParentGroup());

    // Previous parent shouldn't be recorded if new and old parent are the same
    entry->setGroup(root);
    QVERIFY(entry->previousParentGroupUuid().isNull());
    QVERIFY(!entry->previousParentGroup());

    entry->setGroup(group1);
    QVERIFY(entry->previousParentGroupUuid() == root->uuid());
    QVERIFY(entry->previousParentGroup() == root);

    entry->setGroup(group2);
    QVERIFY(entry->previousParentGroupUuid() == group1->uuid());
    QVERIFY(entry->previousParentGroup() == group1);

    entry->setGroup(group2);
    QVERIFY(entry->previousParentGroupUuid() == group1->uuid());
    QVERIFY(entry->previousParentGroup() == group1);
}
