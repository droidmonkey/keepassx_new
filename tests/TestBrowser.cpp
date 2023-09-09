/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
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

#include "TestBrowser.h"

#include "browser/BrowserMessageBuilder.h"
#include "browser/BrowserSettings.h"
#include "core/Group.h"
#include "core/Tools.h"
#include "crypto/Crypto.h"

#include <QJsonObject>
#include <QTest>

#include <botan/sodium.h>

using namespace Botan::Sodium;

QTEST_GUILESS_MAIN(TestBrowser)

const QString PUBLICKEY = "UIIPObeoya1G8g1M5omgyoPR/j1mR1HlYHu0wHCgMhA=";
const QString SECRETKEY = "B8ei4ZjQJkWzZU2SK/tBsrYRwp+6ztEMf5GFQV+i0yI=";
const QString SERVERPUBLICKEY = "lKnbLhrVCOqzEjuNoUz1xj9EZlz8xeO4miZBvLrUPVQ=";
const QString SERVERSECRETKEY = "tbPQcghxfOgbmsnEqG2qMIj1W2+nh+lOJcNsHncaz1Q=";
const QString NONCE = "zBKdvTjL5bgWaKMCTut/8soM/uoMrFoZ";
const QString INCREMENTEDNONCE = "zRKdvTjL5bgWaKMCTut/8soM/uoMrFoZ";
const QString CLIENTID = "testClient";

void TestBrowser::initTestCase()
{
    QVERIFY(Crypto::init());
    m_browserService = browserService();
    browserSettings()->setBestMatchOnly(false);
}

void TestBrowser::init()
{
    m_browserAction.reset(new BrowserAction());
}

/**
 * Tests for BrowserAction
 */

void TestBrowser::testChangePublicKeys()
{
    QJsonObject json;
    json["action"] = "change-public-keys";
    json["publicKey"] = PUBLICKEY;
    json["nonce"] = NONCE;

    auto response = m_browserAction->processClientMessage(nullptr, json);
    QCOMPARE(response["action"].toString(), QString("change-public-keys"));
    QCOMPARE(response["publicKey"].toString() == PUBLICKEY, false);
    QCOMPARE(response["success"].toString(), TRUE_STR);
}

void TestBrowser::testEncryptMessage()
{
    QJsonObject message;
    message["action"] = "test-action";

    m_browserAction->m_publicKey = SERVERPUBLICKEY;
    m_browserAction->m_secretKey = SERVERSECRETKEY;
    m_browserAction->m_clientPublicKey = PUBLICKEY;
    auto encrypted = browserMessageBuilder()->encryptMessage(message, NONCE, PUBLICKEY, SERVERSECRETKEY);

    QCOMPARE(encrypted, QString("+zjtntnk4rGWSl/Ph7Vqip/swvgeupk4lNgHEm2OO3ujNr0OMz6eQtGwjtsj+/rP"));
}

void TestBrowser::testDecryptMessage()
{
    QString message = "+zjtntnk4rGWSl/Ph7Vqip/swvgeupk4lNgHEm2OO3ujNr0OMz6eQtGwjtsj+/rP";
    m_browserAction->m_publicKey = SERVERPUBLICKEY;
    m_browserAction->m_secretKey = SERVERSECRETKEY;
    m_browserAction->m_clientPublicKey = PUBLICKEY;
    auto decrypted = browserMessageBuilder()->decryptMessage(message, NONCE, PUBLICKEY, SERVERSECRETKEY);

    QCOMPARE(decrypted["action"].toString(), QString("test-action"));
}

void TestBrowser::testGetBase64FromKey()
{
    unsigned char pk[crypto_box_PUBLICKEYBYTES];

    for (unsigned int i = 0; i < crypto_box_PUBLICKEYBYTES; ++i) {
        pk[i] = i;
    }

    auto response = browserMessageBuilder()->getBase64FromKey(pk, crypto_box_PUBLICKEYBYTES);
    QCOMPARE(response, QString("AAECAwQFBgcICQoLDA0ODxAREhMUFRYXGBkaGxwdHh8="));
}

void TestBrowser::testIncrementNonce()
{
    auto result = browserMessageBuilder()->incrementNonce(NONCE);
    QCOMPARE(result, INCREMENTEDNONCE);
}

void TestBrowser::testBuildResponse()
{
    const auto object = QJsonObject{{"test", true}};
    const QJsonArray arr = {QJsonObject{{"test", true}}};
    const auto val = QString("value1");

    // Note: Passing a const QJsonObject will fail
    const Parameters params{
        {"test-param-1", val}, {"test-param-2", 2}, {"test-param-3", false}, {"object", object}, {"arr", arr}};

    const auto action = QString("test-action");
    const auto message = browserMessageBuilder()->buildResponse(action, NONCE, params, PUBLICKEY, SERVERSECRETKEY);
    QVERIFY(!message.isEmpty());
    QCOMPARE(message["action"].toString(), action);
    QCOMPARE(message["nonce"].toString(), NONCE);

    const auto decrypted =
        browserMessageBuilder()->decryptMessage(message["message"].toString(), NONCE, PUBLICKEY, SERVERSECRETKEY);
    QVERIFY(!decrypted.isEmpty());
    QCOMPARE(decrypted["test-param-1"].toString(), QString("value1"));
    QCOMPARE(decrypted["test-param-2"].toInt(), 2);
    QCOMPARE(decrypted["test-param-3"].toBool(), false);

    const auto objectResult = decrypted["object"].toObject();
    QCOMPARE(objectResult["test"].toBool(), true);

    const auto arrResult = decrypted["arr"].toArray();
    QCOMPARE(arrResult.size(), 1);

    const auto firstArr = arrResult[0].toObject();
    QCOMPARE(firstArr["test"].toBool(), true);
}

void TestBrowser::testSortPriority()
{
    QFETCH(QString, entryUrl);
    QFETCH(QString, siteUrl);
    QFETCH(QString, formUrl);
    QFETCH(int, expectedScore);

    QScopedPointer<Entry> entry(new Entry());
    entry->setUrl(entryUrl);

    QCOMPARE(m_browserService->sortPriority(entry->getAllUrls(), siteUrl, formUrl), expectedScore);
}

void TestBrowser::testSortPriority_data()
{
    const QString siteUrl = "https://github.com/login";
    const QString formUrl = "https://github.com/session";

    QTest::addColumn<QString>("entryUrl");
    QTest::addColumn<QString>("siteUrl");
    QTest::addColumn<QString>("formUrl");
    QTest::addColumn<int>("expectedScore");

    QTest::newRow("Exact Match") << siteUrl << siteUrl << siteUrl << 100;
    QTest::newRow("Exact Match (site)") << siteUrl << siteUrl << formUrl << 100;
    QTest::newRow("Exact Match (form)") << siteUrl << "https://github.net" << siteUrl << 100;
    QTest::newRow("Exact Match No Trailing Slash") << "https://github.com" << "https://github.com/" << formUrl << 100;
    QTest::newRow("Exact Match No Scheme") << "github.com/login" << siteUrl << formUrl << 100;
    QTest::newRow("Exact Match with Query")
        << "https://github.com/login?test=test#fragment" << "https://github.com/login?test=test" << formUrl << 100;

    QTest::newRow("Site Query Mismatch") << siteUrl << siteUrl + "?test=test" << formUrl << 90;

    QTest::newRow("Path Mismatch (site)") << "https://github.com/" << siteUrl << formUrl << 85;
    QTest::newRow("Path Mismatch (site) No Scheme") << "github.com" << siteUrl << formUrl << 85;
    QTest::newRow("Path Mismatch (form)") << "https://github.com/" << "https://github.net" << formUrl << 85;
    QTest::newRow("Path Mismatch (diff parent)") << "https://github.com/keepassxreboot" << siteUrl << formUrl << 80;
    QTest::newRow("Path Mismatch (diff parent, form)")
        << "https://github.com/keepassxreboot" << "https://github.net" << formUrl << 70;

    QTest::newRow("Subdomain Mismatch (site)") << siteUrl << "https://sub.github.com/" << "https://github.net/" << 60;
    QTest::newRow("Subdomain Mismatch (form)") << siteUrl << "https://github.net/" << "https://sub.github.com/" << 50;

    QTest::newRow("Scheme Mismatch") << "http://github.com" << siteUrl << formUrl << 0;
    QTest::newRow("Scheme Mismatch w/path") << "http://github.com/login" << siteUrl << formUrl << 0;
    QTest::newRow("Invalid URL") << "http://github" << siteUrl << formUrl << 0;
}

void TestBrowser::testSearchEntries()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    QStringList urls = {
        "https://github.com/login_page",
        "https://github.com/login",
        "https://github.com/",
        "github.com/login",
        "http://github.com",
        "http://github.com/login",
        "github.com",
        "github.com/login",
        "https://github", // Invalid URL
        "github.com",
        "\"https://github.com\"" // Exact URL
    };

    createEntries(urls, root);

    browserSettings()->setMatchUrlScheme(false);
    auto result =
        m_browserService->searchEntries(db, "https://github.com", "https://github.com/session"); // db, url, submitUrl

    QCOMPARE(result.length(), 10);
    QCOMPARE(result[0]->url(), QString("https://github.com/login_page"));
    QCOMPARE(result[1]->url(), QString("https://github.com/login"));
    QCOMPARE(result[2]->url(), QString("https://github.com/"));
    QCOMPARE(result[3]->url(), QString("github.com/login"));
    QCOMPARE(result[4]->url(), QString("http://github.com"));
    QCOMPARE(result[5]->url(), QString("http://github.com/login"));

    // With matching there should be only 4 results + 4 without a scheme
    browserSettings()->setMatchUrlScheme(true);
    result = m_browserService->searchEntries(db, "https://github.com", "https://github.com/session");
    QCOMPARE(result.length(), 8);
    QCOMPARE(result[0]->url(), QString("https://github.com/login_page"));
    QCOMPARE(result[1]->url(), QString("https://github.com/login"));
    QCOMPARE(result[2]->url(), QString("https://github.com/"));
    QCOMPARE(result[3]->url(), QString("github.com/login"));
}

void TestBrowser::testSearchEntriesByPath()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    QStringList urlsRoot = {"https://root.example.com/", "root.example.com/login"};
    auto entriesRoot = createEntries(urlsRoot, root);

    auto* groupLevel1 = new Group();
    groupLevel1->setParent(root);
    groupLevel1->setName("TestGroup1");
    QStringList urlsLevel1 = {"https://1.example.com/", "1.example.com/login"};
    auto entriesLevel1 = createEntries(urlsLevel1, groupLevel1);

    auto* groupLevel2 = new Group();
    groupLevel2->setParent(groupLevel1);
    groupLevel2->setName("TestGroup2");
    QStringList urlsLevel2 = {"https://2.example.com/", "2.example.com/login"};
    auto entriesLevel2 = createEntries(urlsLevel2, groupLevel2);

    compareEntriesByPath(db, entriesRoot, "");
    compareEntriesByPath(db, entriesLevel1, "TestGroup1/");
    compareEntriesByPath(db, entriesLevel2, "TestGroup1/TestGroup2/");
}

void TestBrowser::compareEntriesByPath(QSharedPointer<Database> db, QList<Entry*> entries, QString path)
{
    for (Entry* entry : entries) {
        QString testUrl = "keepassxc://by-path/" + path + entry->title();
        /* Look for an entry with that path. First using handleEntry, then through the search */
        QCOMPARE(m_browserService->shouldIncludeEntry(entry, testUrl, ""), true);
        auto result = m_browserService->searchEntries(db, testUrl, "");
        QCOMPARE(result.length(), 1);
        QCOMPARE(result[0], entry);
    }
}

void TestBrowser::testSearchEntriesByUUID()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    /* The URLs don't really matter for this test, we just need some entries */
    QStringList urls = {"https://github.com/login_page",
                        "https://github.com/login",
                        "https://github.com/",
                        "github.com/login",
                        "http://github.com",
                        "http://github.com/login",
                        "github.com",
                        "github.com/login",
                        "https://github",
                        "github.com",
                        "",
                        "not an URL"};
    auto entries = createEntries(urls, root);

    for (Entry* entry : entries) {
        QString testUrl = "keepassxc://by-uuid/" + entry->uuidToHex();
        /* Look for an entry with that UUID. First using shouldIncludeEntry, then through the search */
        QCOMPARE(m_browserService->shouldIncludeEntry(entry, testUrl, ""), true);
        auto result = m_browserService->searchEntries(db, testUrl, "");
        QCOMPARE(result.length(), 1);
        QCOMPARE(result[0], entry);
    }

    /* Test for entries that don't exist */
    QStringList uuids = {"00000000000000000000000000000000",
                         "00000000000000000000000000000001",
                         "00000000000000000000000000000002/",
                         "invalid uuid",
                         "000000000000000000000000000000000000000"
                         "00000000000000000000000"};

    for (QString uuid : uuids) {
        QString testUrl = "keepassxc://by-uuid/" + uuid;

        for (Entry* entry : entries) {
            QCOMPARE(m_browserService->shouldIncludeEntry(entry, testUrl, ""), false);
        }

        auto result = m_browserService->searchEntries(db, testUrl, "");
        QCOMPARE(result.length(), 0);
    }
}

void TestBrowser::testSearchEntriesByReference()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    /* The URLs don't really matter for this test, we just need some entries */
    QStringList urls = {"https://subdomain.example.com",
                        "example.com", // Only includes a partial URL for references
                        "https://another.domain.com", // Additional URL as full reference
                        "https://subdomain.somesite.com", // Additional URL as partial reference
                        "", // Full reference will be added to https://subdomain.example.com
                        "" // Partial reference will be added to https://subdomain.example.com
                        "https://www.notincluded.com"}; // Should not show in search
    auto entries = createEntries(urls, root);

    auto firstEntryUuid = entries.first()->uuidToHex();
    auto secondEntryUuid = entries[1]->uuidToHex();
    auto fullReference = QString("{REF:A@I:%1}").arg(firstEntryUuid);
    auto partialReference = QString("https://subdomain.{REF:A@I:%1}").arg(secondEntryUuid);
    entries[2]->attributes()->set(EntryAttributes::AdditionalUrlAttribute, fullReference);
    entries[3]->attributes()->set(EntryAttributes::AdditionalUrlAttribute, partialReference);
    entries[4]->setUrl(fullReference);
    entries[5]->setUrl(partialReference);

    auto result = m_browserService->searchEntries(db, "https://subdomain.example.com", "");
    QCOMPARE(result.length(), 6);
    QCOMPARE(result[0]->url(), urls[0]);
    QCOMPARE(result[1]->url(), urls[1]);
    QCOMPARE(result[2]->url(), urls[2]);
    QCOMPARE(
        result[2]->resolveMultiplePlaceholders(result[2]->attributes()->value(EntryAttributes::AdditionalUrlAttribute)),
        urls[0]);
    QCOMPARE(result[3]->url(), urls[3]);
    QCOMPARE(
        result[3]->resolveMultiplePlaceholders(result[3]->attributes()->value(EntryAttributes::AdditionalUrlAttribute)),
        urls[0]);
    QCOMPARE(result[4]->url(), fullReference);
    QCOMPARE(result[4]->resolveMultiplePlaceholders(result[4]->url()), urls[0]); // Should be resolved to the main entry
    QCOMPARE(result[5]->url(), partialReference);
    QCOMPARE(result[5]->resolveMultiplePlaceholders(result[5]->url()), urls[0]); // Should be resolved to the main entry
}

void TestBrowser::testSearchEntriesWithPort()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    QStringList urls = {"http://127.0.0.1:443", "http://127.0.0.1:80"};

    createEntries(urls, root);

    auto result = m_browserService->searchEntries(db, "http://127.0.0.1:443", "http://127.0.0.1");
    QCOMPARE(result.length(), 1);
    QCOMPARE(result[0]->url(), QString("http://127.0.0.1:443"));
}

void TestBrowser::testSearchEntriesWithAdditionalURLs()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    QStringList urls = {"https://github.com/", "https://www.example.com", "http://domain.com"};

    auto entries = createEntries(urls, root);

    // Add an additional URL to the first entry
    entries.first()->attributes()->set(EntryAttributes::AdditionalUrlAttribute, "https://keepassxc.org");

    auto result = m_browserService->searchEntries(db, "https://github.com", "https://github.com/session");
    QCOMPARE(result.length(), 1);
    QCOMPARE(result[0]->url(), QString("https://github.com/"));

    // Search the additional URL. It should return the same entry
    auto additionalResult = m_browserService->searchEntries(db, "https://keepassxc.org", "https://keepassxc.org");
    QCOMPARE(additionalResult.length(), 1);
    QCOMPARE(additionalResult[0]->url(), QString("https://github.com/"));
}

void TestBrowser::testSearchEntriesWithWildcardURLs()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    QStringList urls = {"https://github.com/login_page/*",
                        "https://github.com/*/second",
                        "https://github.com/*",
                        "http://github.com/*",
                        "github.com/*", // Defaults to https
                        "https://*.github.com/*",
                        "https://subdomain.*.github.com/*/second",
                        "https://*.sub.github.com/*",
                        "https://********", // Invalid wildcard URL
                        "https://subdomain.yes.github.com/*",
                        "https://example.com:8448/*",
                        "https://example.com/*/*",
                        "https://example.com/$/*",
                        "https://127.128.129.*:8448/",
                        "https://127.128.*/",
                        "https://127.160.*.2/login",
                        "http://[2001:db8:85a3:8d3:1319:8a2e:370:*]/",
                        "https://[2001:db8:85a3:8d3:*]:443/",
                        "fe80::1ff:fe23:4567:890a",
                        "2001-db8-85a3-8d3-1319-8a2e-370-7348.ipv6-literal.net"};

    createEntries(urls, root);
    browserSettings()->setMatchUrlScheme(false);

    auto result = m_browserService->searchEntries(
        db, "https://github.com/login_page/second", "https://github.com/login_page/second");
    QCOMPARE(result.length(), 6);
    QCOMPARE(result[0]->url(), QString("https://github.com/login_page/*"));
    QCOMPARE(result[1]->url(), QString("https://github.com/*/second"));
    QCOMPARE(result[2]->url(), QString("https://github.com/*"));
    QCOMPARE(result[3]->url(), QString("http://github.com/*"));
    QCOMPARE(result[4]->url(), QString("github.com/*"));
    QCOMPARE(result[5]->url(), QString("https://*.github.com/*"));

    result = m_browserService->searchEntries(
        db, "https://subdomain.sub.github.com/login_page/second", "https://subdomain.sub.github.com/login_page/second");
    QCOMPARE(result.length(), 3);
    QCOMPARE(result[0]->url(), QString("https://*.github.com/*"));
    QCOMPARE(result[1]->url(), QString("https://subdomain.*.github.com/*/second"));
    QCOMPARE(result[2]->url(), QString("https://*.sub.github.com/*"));

    result = m_browserService->searchEntries(
        db, "https://subdomain.sub.github.com/other_page", "https://subdomain.sub.github.com/other_page");
    QCOMPARE(result.length(), 2);
    QCOMPARE(result[0]->url(), QString("https://*.github.com/*"));
    QCOMPARE(result[1]->url(), QString("https://*.sub.github.com/*"));

    result = m_browserService->searchEntries(
        db, "https://subdomain.yes.github.com/other_page/second", "https://subdomain.yes.github.com/other_page/second");
    QCOMPARE(result.length(), 3);
    QCOMPARE(result[0]->url(), QString("https://*.github.com/*"));
    QCOMPARE(result[1]->url(), QString("https://subdomain.*.github.com/*/second"));
    QCOMPARE(result[2]->url(), QString("https://subdomain.yes.github.com/*"));

    result = m_browserService->searchEntries(
        db, "https://example.com:8448/login/page", "https://example.com:8448/login/page");
    QCOMPARE(result.length(), 2);
    QCOMPARE(result[0]->url(), QString("https://example.com:8448/*"));
    QCOMPARE(result[1]->url(), QString("https://example.com/*/*"));

    result = m_browserService->searchEntries(
        db, "https://example.com:8449/login/page", "https://example.com:8449/login/page");
    QCOMPARE(result.length(), 1);
    QCOMPARE(result[0]->url(), QString("https://example.com/*/*"));

    result =
        m_browserService->searchEntries(db, "https://example.com/$/login_page", "https://example.com/$/login_page");
    QCOMPARE(result.length(), 2);
    QCOMPARE(result[0]->url(), QString("https://example.com/*/*"));
    QCOMPARE(result[1]->url(), QString("https://example.com/$/*"));

    result = m_browserService->searchEntries(db, "https://127.128.129.130:8448/", "https://127.128.129.130:8448/");
    QCOMPARE(result.length(), 2);

    result = m_browserService->searchEntries(db, "https://127.128.129.130/", "https://127.128.129.130/");
    QCOMPARE(result.length(), 1);
    QCOMPARE(result[0]->url(), QString("https://127.128.*/"));

    result = m_browserService->searchEntries(db, "https://127.1.129.130/", "https://127.1.129.130/");
    QCOMPARE(result.length(), 0);

    result = m_browserService->searchEntries(db, "https://127.160.8.2/login", "https://127.160.8.2/login");
    QCOMPARE(result.length(), 1);
    QCOMPARE(result[0]->url(), QString("https://127.160.*.2/login"));

    // With scheme matching enabled
    browserSettings()->setMatchUrlScheme(true);
    result = m_browserService->searchEntries(
        db, "https://github.com/login_page/second", "https://github.com/login_page/second");

    QCOMPARE(result.length(), 5);
    QCOMPARE(result[0]->url(), QString("https://github.com/login_page/*"));
    QCOMPARE(result[1]->url(), QString("https://github.com/*/second"));
    QCOMPARE(result[2]->url(), QString("https://github.com/*"));
    QCOMPARE(result[3]->url(), QString("github.com/*")); // Defaults to https
    QCOMPARE(result[4]->url(), QString("https://*.github.com/*"));
}

void TestBrowser::testInvalidEntries()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();
    const QString url("https://github.com");
    const QString submitUrl("https://github.com/session");

    QStringList urls = {
        "https://github.com/login",
        "https:///github.com/", // Extra '/'
        "http://github.com/**//*",
        "http://*.github.com/login",
        "//github.com", // fromUserInput() corrects this one.
        "github.com/{}<>",
        "http:/example.com",
    };

    createEntries(urls, root);

    browserSettings()->setMatchUrlScheme(true);
    auto result = m_browserService->searchEntries(db, "https://github.com", "https://github.com/session");
    QCOMPARE(result.length(), 2);
    QCOMPARE(result[0]->url(), QString("https://github.com/login"));
    QCOMPARE(result[1]->url(), QString("//github.com"));

    // Test the URL's directly
    QCOMPARE(m_browserService->handleURL(urls[0], url, submitUrl), true);
    QCOMPARE(m_browserService->handleURL(urls[1], url, submitUrl), false);
    QCOMPARE(m_browserService->handleURL(urls[2], url, submitUrl), false);
    QCOMPARE(m_browserService->handleURL(urls[3], url, submitUrl), false);
    QCOMPARE(m_browserService->handleURL(urls[4], url, submitUrl), true);
    QCOMPARE(m_browserService->handleURL(urls[5], url, submitUrl), false);
}

void TestBrowser::testSubdomainsAndPaths()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    QStringList urls = {
        "https://www.github.com/login/page.xml",
        "https://login.github.com/",
        "https://github.com",
        "http://www.github.com",
        "http://login.github.com/pathtonowhere",
        ".github.com", // Invalid URL
        "www.github.com/",
        "https://github", // Invalid URL
        "https://hub.com" // Should not return
    };

    createEntries(urls, root);

    browserSettings()->setMatchUrlScheme(false);
    auto result = m_browserService->searchEntries(db, "https://github.com", "https://github.com/session");
    QCOMPARE(result.length(), 1);
    QCOMPARE(result[0]->url(), QString("https://github.com"));

    // With www subdomain
    result = m_browserService->searchEntries(db, "https://www.github.com", "https://www.github.com/session");
    QCOMPARE(result.length(), 4);
    QCOMPARE(result[0]->url(), QString("https://www.github.com/login/page.xml"));
    QCOMPARE(result[1]->url(), QString("https://github.com")); // Accepts any subdomain
    QCOMPARE(result[2]->url(), QString("http://www.github.com"));
    QCOMPARE(result[3]->url(), QString("www.github.com/"));

    // With www subdomain omitted
    root->setCustomDataTriState(BrowserService::OPTION_OMIT_WWW, Group::Enable);
    result = m_browserService->searchEntries(db, "https://github.com", "https://github.com/session");
    root->setCustomDataTriState(BrowserService::OPTION_OMIT_WWW, Group::Inherit);
    QCOMPARE(result.length(), 4);
    QCOMPARE(result[0]->url(), QString("https://www.github.com/login/page.xml"));
    QCOMPARE(result[1]->url(), QString("https://github.com"));
    QCOMPARE(result[2]->url(), QString("http://www.github.com"));
    QCOMPARE(result[3]->url(), QString("www.github.com/"));

    // With scheme matching there should be only 1 result
    browserSettings()->setMatchUrlScheme(true);
    result = m_browserService->searchEntries(db, "https://github.com", "https://github.com/session");
    QCOMPARE(result.length(), 1);
    QCOMPARE(result[0]->url(), QString("https://github.com"));

    // Test site with subdomain in the site URL
    QStringList entryURLs = {
        "https://accounts.example.com",
        "https://accounts.example.com/path",
        "https://subdomain.example.com/",
        "https://another.accounts.example.com/",
        "https://another.subdomain.example.com/",
        "https://example.com/",
        "https://example" // Invalid URL
    };

    createEntries(entryURLs, root);

    result = m_browserService->searchEntries(db, "https://accounts.example.com/", "https://accounts.example.com/");
    QCOMPARE(result.length(), 3);
    QCOMPARE(result[0]->url(), QString("https://accounts.example.com"));
    QCOMPARE(result[1]->url(), QString("https://accounts.example.com/path"));
    QCOMPARE(result[2]->url(), QString("https://example.com/")); // Accepts any subdomain

    result = m_browserService->searchEntries(
        db, "https://another.accounts.example.com/", "https://another.accounts.example.com/");
    QCOMPARE(result.length(), 4);
    QCOMPARE(result[0]->url(),
             QString("https://accounts.example.com")); // Accepts any subdomain under accounts.example.com
    QCOMPARE(result[1]->url(), QString("https://accounts.example.com/path"));
    QCOMPARE(result[2]->url(), QString("https://another.accounts.example.com/"));
    QCOMPARE(result[3]->url(), QString("https://example.com/")); // Accepts one or more subdomains

    // Test local files. It should be a direct match.
    QStringList localFiles = {"file:///Users/testUser/tests/test.html"};

    createEntries(localFiles, root);

    // With local files, url is always set to the file scheme + ://. Submit URL holds the actual URL.
    result = m_browserService->searchEntries(db, "file://", "file:///Users/testUser/tests/test.html");
    QCOMPARE(result.length(), 1);
}

QList<Entry*> TestBrowser::createEntries(QStringList& urls, Group* root) const
{
    QList<Entry*> entries;
    for (int i = 0; i < urls.length(); ++i) {
        auto entry = new Entry();
        entry->setGroup(root);
        entry->beginUpdate();
        entry->setUrl(urls[i]);
        entry->setUsername(QString("User %1").arg(i));
        entry->setUuid(QUuid::createUuid());
        entry->setTitle(QString("Name_%1").arg(entry->uuidToHex()));
        entry->endUpdate();
        entries.push_back(entry);
    }

    return entries;
}

void TestBrowser::testBestMatchingCredentials()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    // Test with simple URL entries
    QStringList urls = {"https://github.com/loginpage", "https://github.com/justsomepage", "https://github.com/"};

    auto entries = createEntries(urls, root);

    browserSettings()->setBestMatchOnly(true);

    QString siteUrl = "https://github.com/loginpage";
    auto result = m_browserService->searchEntries(db, siteUrl, siteUrl);
    auto sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 1);
    QCOMPARE(sorted[0]->url(), siteUrl);

    siteUrl = "https://github.com/justsomepage";
    result = m_browserService->searchEntries(db, siteUrl, siteUrl);
    sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 1);
    QCOMPARE(sorted[0]->url(), siteUrl);

    siteUrl = "https://github.com/";
    result = m_browserService->searchEntries(db, siteUrl, siteUrl);
    sorted = m_browserService->sortEntries(entries, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 1);
    QCOMPARE(sorted[0]->url(), siteUrl);

    // Without best-matching the URL with the path should be returned first
    browserSettings()->setBestMatchOnly(false);
    siteUrl = "https://github.com/loginpage";
    result = m_browserService->searchEntries(db, siteUrl, siteUrl);
    sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 3);
    QCOMPARE(sorted[0]->url(), siteUrl);

    // Test with subdomains
    QStringList subdomainsUrls = {"https://sub.github.com/loginpage",
                                  "https://sub.github.com/justsomepage",
                                  "https://bus.github.com/justsomepage",
                                  "https://subdomain.example.com/",
                                  "https://subdomain.example.com",
                                  "https://example.com"};

    entries = createEntries(subdomainsUrls, root);

    browserSettings()->setBestMatchOnly(true);
    siteUrl = "https://sub.github.com/justsomepage";
    result = m_browserService->searchEntries(db, siteUrl, siteUrl);
    sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 1);
    QCOMPARE(sorted[0]->url(), siteUrl);

    siteUrl = "https://github.com/justsomepage";
    result = m_browserService->searchEntries(db, siteUrl, siteUrl);
    sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 1);
    QCOMPARE(sorted[0]->url(), siteUrl);

    siteUrl = "https://sub.github.com/justsomepage?wehavesomeextra=here";
    result = m_browserService->searchEntries(db, siteUrl, siteUrl);
    sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 1);
    QCOMPARE(sorted[0]->url(), QString("https://sub.github.com/justsomepage"));

    // The matching should not care if there's a / path or not.
    siteUrl = "https://subdomain.example.com/";
    result = m_browserService->searchEntries(db, siteUrl, siteUrl);
    sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 2);
    QCOMPARE(sorted[0]->url(), QString("https://subdomain.example.com"));
    QCOMPARE(sorted[1]->url(), QString("https://subdomain.example.com/"));

    // Entries with https://example.com should be still returned even if the site URL has a subdomain. Those have the
    // best match.
    db = QSharedPointer<Database>::create();
    root = db->rootGroup();
    QStringList domainUrls = {"https://example.com", "https://example.com", "https://other.example.com"};
    entries = createEntries(domainUrls, root);
    siteUrl = "https://subdomain.example.com";
    result = m_browserService->searchEntries(db, siteUrl, siteUrl);
    sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);

    QCOMPARE(sorted.size(), 2);
    QCOMPARE(sorted[0]->url(), QString("https://example.com"));
    QCOMPARE(sorted[1]->url(), QString("https://example.com"));

    // https://github.com/keepassxreboot/keepassxc/issues/4754
    db = QSharedPointer<Database>::create();
    root = db->rootGroup();
    QStringList fooUrls = {"https://example.com/foo", "https://example.com/bar"};
    entries = createEntries(fooUrls, root);

    for (const auto& url : fooUrls) {
        result = m_browserService->searchEntries(db, url, url);
        sorted = m_browserService->sortEntries(result, url, url);
        QCOMPARE(sorted.size(), 1);
        QCOMPARE(sorted[0]->url(), QString(url));
    }

    // https://github.com/keepassxreboot/keepassxc/issues/4734
    db = QSharedPointer<Database>::create();
    root = db->rootGroup();
    QStringList testUrls = {"http://some.domain.tld/somePath", "http://some.domain.tld/otherPath"};
    entries = createEntries(testUrls, root);

    for (const auto& url : testUrls) {
        result = m_browserService->searchEntries(db, url, url);
        sorted = m_browserService->sortEntries(result, url, url);
        QCOMPARE(sorted.size(), 1);
        QCOMPARE(sorted[0]->url(), QString(url));
    }
}

void TestBrowser::testBestMatchingWithAdditionalURLs()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    QStringList urls = {"https://github.com/loginpage", "https://test.github.com/", "https://github.com/"};

    auto entries = createEntries(urls, root);
    browserSettings()->setBestMatchOnly(true);

    // Add an additional URL to the first entry
    entries.first()->attributes()->set(EntryAttributes::AdditionalUrlAttribute, "https://test.github.com/anotherpage");

    // The first entry should be triggered
    auto result = m_browserService->searchEntries(
        db, "https://test.github.com/anotherpage", "https://test.github.com/anotherpage");
    auto sorted = m_browserService->sortEntries(
        result, "https://test.github.com/anotherpage", "https://test.github.com/anotherpage");
    QCOMPARE(sorted.length(), 1);
    QCOMPARE(sorted[0]->url(), urls[0]);
}

void TestBrowser::testRestrictBrowserKey()
{
    auto db = QSharedPointer<Database>::create();
    auto* root = db->rootGroup();

    // Group 0 (root): No browser key restriction given
    QStringList urlsRoot = {"https://example.com/0"};
    auto entriesRoot = createEntries(urlsRoot, root);

    // Group 1: restricted to browser with 'key1'
    auto* group1 = new Group();
    group1->setParent(root);
    group1->setName("TestGroup1");
    group1->customData()->set(BrowserService::OPTION_RESTRICT_KEY, "key1");
    QStringList urls1 = {"https://example.com/1"};
    auto entries1 = createEntries(urls1, group1);

    // Group 2: restricted to browser with 'key2'
    auto* group2 = new Group();
    group2->setParent(root);
    group2->setName("TestGroup2");
    group2->customData()->set(BrowserService::OPTION_RESTRICT_KEY, "key2");
    QStringList urls2 = {"https://example.com/2"};
    auto entries2 = createEntries(urls2, group2);

    // Group 2b: inherits parent group (2) restriction
    auto* group2b = new Group();
    group2b->setParent(group2);
    group2b->setName("TestGroup2b");
    QStringList urls2b = {"https://example.com/2b"};
    auto entries2b = createEntries(urls2b, group2b);

    // Group 3: inherits parent group (root) - any browser can see
    auto* group3 = new Group();
    group3->setParent(root);
    group3->setName("TestGroup3");
    QStringList urls3 = {"https://example.com/3"};
    auto entries3 = createEntries(urls3, group3);

    // Browser 'key0': Groups 1 and 2 are excluded, so entries 0 and 3 will be found
    auto siteUrl = QString("https://example.com");
    auto result = m_browserService->searchEntries(db, siteUrl, siteUrl, {"key0"});
    auto sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 2);
    QCOMPARE(sorted[0]->url(), QString("https://example.com/3"));
    QCOMPARE(sorted[1]->url(), QString("https://example.com/0"));

    // Browser 'key1': Group 2 will be excluded, so entries 0, 1, and 3 will be found
    result = m_browserService->searchEntries(db, siteUrl, siteUrl, {"key1"});
    sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 3);
    QCOMPARE(sorted[0]->url(), QString("https://example.com/3"));
    QCOMPARE(sorted[1]->url(), QString("https://example.com/1"));
    QCOMPARE(sorted[2]->url(), QString("https://example.com/0"));

    // Browser 'key2': Group 1 will be excluded, so entries 0, 2, 2b, 3 will be found
    result = m_browserService->searchEntries(db, siteUrl, siteUrl, {"key2"});
    sorted = m_browserService->sortEntries(result, siteUrl, siteUrl);
    QCOMPARE(sorted.size(), 4);
    QCOMPARE(sorted[0]->url(), QString("https://example.com/3"));
    QCOMPARE(sorted[1]->url(), QString("https://example.com/2b"));
    QCOMPARE(sorted[2]->url(), QString("https://example.com/2"));
    QCOMPARE(sorted[3]->url(), QString("https://example.com/0"));
}
