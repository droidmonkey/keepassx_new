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

#include "TestImports.h"

#include "config-keepassx-tests.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Totp.h"
#include "crypto/Crypto.h"
#include "format/BitwardenReader.h"
#include "format/OPUXReader.h"
#include "format/OpVaultReader.h"
#include "format/ProtonPassReader.h"

#include <QJsonObject>
#include <QList>
#include <QTest>

QTEST_GUILESS_MAIN(TestImports)

void TestImports::initTestCase()
{
    QVERIFY(Crypto::init());
}

void TestImports::testOPUX()
{
    auto opuxPath = QStringLiteral("%1/%2").arg(KEEPASSX_TEST_DATA_DIR, QStringLiteral("/1PasswordExport.1pux"));

    OPUXReader reader;
    auto db = reader.convert(opuxPath);
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QVERIFY(db);

    // Confirm specific entry details are valid
    auto entry = db->rootGroup()->findEntryByPath("/Personal/Login");
    QVERIFY(entry);
    QCOMPARE(entry->title(), QStringLiteral("Login"));
    QCOMPARE(entry->username(), QStringLiteral("team@keepassxc.org"));
    QCOMPARE(entry->password(), QStringLiteral("password"));
    QCOMPARE(entry->url(), QStringLiteral("https://keepassxc.org"));
    QCOMPARE(entry->notes(), QStringLiteral("Note to self"));
    // Check extra URL's
    QCOMPARE(entry->attribute("KP2A_URL_1"), QStringLiteral("https://twitter.com"));
    // Check TOTP
    QVERIFY(entry->hasTotp());
    QVERIFY(!entry->attribute("otp_1").isEmpty());
    // Check tags
    QVERIFY(entry->tagList().contains("Favorite"));
    QVERIFY(entry->tagList().contains("website"));

    // Check attachments
    entry = db->rootGroup()->findEntryByPath("/Personal/KeePassXC Logo");
    auto attachments = entry->attachments();
    QCOMPARE(attachments->keys().count(), 1);
    QCOMPARE(attachments->keys()[0], QString("keepassxc.png"));

    // Confirm advanced attributes
    // NOTE: 1PUX does not support an explicit expiration field
    entry = db->rootGroup()->findEntryByPath("/Personal/Credit Card");
    QVERIFY(entry);
    auto tmpl = QString("Credit Card Fields_%1");
    auto attr = entry->attributes();
    QCOMPARE(attr->value(tmpl.arg("cardholder name")), QStringLiteral("KeePassXC"));
    QCOMPARE(attr->value(tmpl.arg("expiry date")), QStringLiteral("202206"));
    QCOMPARE(attr->value(tmpl.arg("verification number")), QStringLiteral("123"));
    QVERIFY(attr->isProtected(tmpl.arg("verification number")));

    // Confirm address fields
    entry = db->rootGroup()->findEntryByPath("/Personal/Identity");
    QVERIFY(entry);
    attr = entry->attributes();
    QCOMPARE(attr->value("Address_address"), QStringLiteral("123 Avenue Rd\nBoston, MA 12345\nus"));

    // Check archived entries
    entry = db->rootGroup()->findEntryByPath("/Personal/Login Archived");
    QVERIFY(entry);
    QVERIFY(entry->tagList().contains("Archived"));

    // Check vault to group structure
    entry = db->rootGroup()->findEntryByPath("/Shared/Bank Account");
    QVERIFY(entry);
    // Check custom group icon
    QVERIFY(!entry->group()->iconUuid().isNull());

    // Check Category UUID 05 Passwords
    entry = db->rootGroup()->findEntryByPath("/Personal/UUID 005 Password");
    QVERIFY(entry);
    QCOMPARE(entry->password(), QStringLiteral("uuid005password"));
}

void TestImports::testOPVault()
{
    auto opVaultPath = QStringLiteral("%1/%2").arg(KEEPASSX_TEST_DATA_DIR, QStringLiteral("/keepassxc.opvault"));

    auto categories = QStringList({QStringLiteral("Login"),
                                   QStringLiteral("Credit Card"),
                                   QStringLiteral("Secure Note"),
                                   QStringLiteral("Identity"),
                                   QStringLiteral("Password"),
                                   QStringLiteral("Tombstone"),
                                   QStringLiteral("Software License"),
                                   QStringLiteral("Bank Account"),
                                   QStringLiteral("Database"),
                                   QStringLiteral("Driver License"),
                                   QStringLiteral("Outdoor License"),
                                   QStringLiteral("Membership"),
                                   QStringLiteral("Passport"),
                                   QStringLiteral("Rewards"),
                                   QStringLiteral("SSN"),
                                   QStringLiteral("Router"),
                                   QStringLiteral("Server"),
                                   QStringLiteral("Email")});

    QDir opVaultDir(opVaultPath);

    OpVaultReader reader;
    auto db = reader.convert(opVaultDir, "a");
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QVERIFY(db);

    // Confirm specific entry details are valid
    auto entry = db->rootGroup()->findEntryByPath("/Login/KeePassXC");
    QVERIFY(entry);
    QCOMPARE(entry->title(), QStringLiteral("KeePassXC"));
    QCOMPARE(entry->username(), QStringLiteral("keepassxc"));
    QCOMPARE(entry->password(), QStringLiteral("opvault"));
    QCOMPARE(entry->url(), QStringLiteral("https://www.keepassxc.org"));
    QCOMPARE(entry->notes(), QStringLiteral("KeePassXC Account"));
    // Check extra URL's
    QCOMPARE(entry->attribute("KP2A_URL_1"), QStringLiteral("https://snapshot.keepassxc.org"));
    // Check TOTP
    QVERIFY(entry->hasTotp());
    // Check attachments
    auto attachments = entry->attachments();
    QCOMPARE(attachments->keys().count(), 1);
    QCOMPARE(*attachments->values().begin(), QByteArray("attachment"));

    // Confirm expired entries
    entry = db->rootGroup()->findEntryByPath("/Login/Expired Login");
    QVERIFY(entry->isExpired());

    // Confirm advanced attributes
    entry = db->rootGroup()->findEntryByPath("/Credit Card/My Credit Card");
    QVERIFY(entry);
    auto attr = entry->attributes();
    QCOMPARE(attr->value("cardholder name"), QStringLiteral("Team KeePassXC"));
    QVERIFY(!attr->value("valid from").isEmpty());
    QCOMPARE(attr->value("Additional Details_PIN"), QStringLiteral("1234"));
    QVERIFY(attr->isProtected("Additional Details_PIN"));

    // Confirm address fields
    entry = db->rootGroup()->findEntryByPath("/Identity/Team KeePassXC");
    QVERIFY(entry);
    attr = entry->attributes();
    QCOMPARE(attr->value("address_street"), QStringLiteral("123 Password Lane"));

    // Confirm complex passwords
    entry = db->rootGroup()->findEntryByPath("/Password/Complex Password");
    QVERIFY(entry);
    QCOMPARE(entry->password(), QStringLiteral("HfgcHjEL}iO}^3N!?*cv~O:9GJZQ0>oC"));
    QVERIFY(entry->hasTotp());
    auto totpSettings = entry->totpSettings();
    QCOMPARE(totpSettings->digits, static_cast<unsigned int>(8));
    QCOMPARE(totpSettings->step, static_cast<unsigned int>(45));

    // Add another OTP to this entry to confirm it doesn't overwrite the existing one
    auto field = QJsonObject::fromVariantMap({{"n", "TOTP_SETTINGS"}, {"v", "otpauth://test.url?digits=6"}});
    reader.fillFromSectionField(entry, "", field);
    QVERIFY(entry->hasTotp());
    totpSettings = entry->totpSettings();
    QCOMPARE(totpSettings->digits, static_cast<unsigned int>(8));
    QCOMPARE(totpSettings->step, static_cast<unsigned int>(45));
    QVERIFY(entry->attributes()->contains("otp_1"));

    // Confirm trashed entries are sent to the recycle bin
    auto recycleBin = db->metadata()->recycleBin();
    QVERIFY(recycleBin);
    QVERIFY(!recycleBin->isEmpty());
    QVERIFY(recycleBin->findEntryByPath("Trashed Password"));

    // Confirm created groups align with category names
    for (const auto group : db->rootGroup()->children()) {
        if (group == recycleBin) {
            continue;
        }
        QVERIFY2(categories.contains(group->name()),
                 qPrintable(QStringLiteral("Invalid group name: %1").arg(group->name())));
        // Confirm each group is not empty
        QVERIFY2(!group->isEmpty(), qPrintable(QStringLiteral("Group %1 is empty").arg(group->name())));
    }
}

void TestImports::testBitwarden()
{
    auto bitwardenPath = QStringLiteral("%1/%2").arg(KEEPASSX_TEST_DATA_DIR, QStringLiteral("/bitwarden_export.json"));

    BitwardenReader reader;
    auto db = reader.convert(bitwardenPath);
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QVERIFY(db);

    // Confirm Login fields
    auto entry = db->rootGroup()->findEntryByPath("/My Folder/Login Name");
    QVERIFY(entry);
    QCOMPARE(entry->title(), QStringLiteral("Login Name"));
    QCOMPARE(entry->username(), QStringLiteral("myusername@gmail.com"));
    QCOMPARE(entry->password(), QStringLiteral("mypassword"));
    QCOMPARE(entry->url(), QStringLiteral("https://mail.google.com"));
    QCOMPARE(entry->notes(), QStringLiteral("1st line of note text\n2nd Line of note text"));
    // Check extra URL's
    QCOMPARE(entry->attribute("KP2A_URL_1"), QStringLiteral("https://google.com"));
    QCOMPARE(entry->attribute("KP2A_URL_2"), QStringLiteral("https://gmail.com"));
    // Check TOTP
    QVERIFY(entry->hasTotp());
    // NOTE: Bitwarden does not export attachments
    // NOTE: Bitwarden does not export expiration dates

    // Confirm Identity fields
    entry = db->rootGroup()->findEntryByPath("/My Folder/My Identity");
    QVERIFY(entry);
    auto attr = entry->attributes();
    // NOTE: The extra spaces are deliberate to test unmodified ingest of data
    QCOMPARE(attr->value("identity_address"),
             QStringLiteral(" 1 North Calle Cesar Chavez \nSanta Barbara, CA 93103\nUnited States "));
    QCOMPARE(attr->value("identity_name"), QStringLiteral("Mrs Jane A Doe"));
    QCOMPARE(attr->value("identity_ssn"), QStringLiteral("123-12-1234"));
    QVERIFY(attr->isProtected("identity_ssn"));

    // Confirm Secure Note
    entry = db->rootGroup()->findEntryByPath("/My Folder/My Secure Note");
    QVERIFY(entry);
    QCOMPARE(entry->notes(),
             QStringLiteral("1st line of secure note\n2nd line of secure note\n3rd line of secure note"));

    // Confirm Credit Card
    entry = db->rootGroup()->findEntryByPath("/Second Folder/Card Name");
    QVERIFY(entry);
    attr = entry->attributes();
    QCOMPARE(attr->value("card_cardholderName"), QStringLiteral("Jane Doe"));
    QCOMPARE(attr->value("card_number"), QStringLiteral("1234567891011121"));
    QCOMPARE(attr->value("card_code"), QStringLiteral("123"));
    QVERIFY(attr->isProtected("card_code"));
}

void TestImports::testBitwardenEncrypted()
{
    // We already tested the parser so just test that decryption works properly

    // First test PBKDF2 password stretching (KDF Type 0)
    auto bitwardenPath =
        QStringLiteral("%1/%2").arg(KEEPASSX_TEST_DATA_DIR, QStringLiteral("/bitwarden_encrypted_export.json"));

    BitwardenReader reader;
    auto db = reader.convert(bitwardenPath, "a");
    if (reader.hasError()) {
        QFAIL(qPrintable(reader.errorString()));
    }
    QVERIFY(db);

    // Now test Argon2id password stretching (KDF Type 1)
    bitwardenPath = QStringLiteral("%1/%2").arg(KEEPASSX_TEST_DATA_DIR,
                                                QStringLiteral("/bitwarden_encrypted_argon2id_export.json"));

    db = reader.convert(bitwardenPath, "a");
    if (reader.hasError()) {
        QFAIL(qPrintable(reader.errorString()));
    }
    QVERIFY(db);
}

void TestImports::testBitwardenPasskey()
{
    auto bitwardenPath =
        QStringLiteral("%1/%2").arg(KEEPASSX_TEST_DATA_DIR, QStringLiteral("/bitwarden_passkey_export.json"));

    BitwardenReader reader;
    auto db = reader.convert(bitwardenPath);
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QVERIFY(db);

    // Confirm Login fields
    auto entry = db->rootGroup()->findEntryByPath("/webauthn.io");
    QVERIFY(entry);
    QCOMPARE(entry->title(), QStringLiteral("webauthn.io"));
    QCOMPARE(entry->username(), QStringLiteral("KPXC_BITWARDEN"));
    QCOMPARE(entry->url(), QStringLiteral("https://webauthn.io/"));

    // Confirm passkey attributes
    auto attr = entry->attributes();
    QCOMPARE(attr->value(EntryAttributes::KPEX_PASSKEY_CREDENTIAL_ID), QStringLiteral("o-FfiyfBQq6Qz6YVrYeFTw"));
    QCOMPARE(
        attr->value(EntryAttributes::KPEX_PASSKEY_PRIVATE_KEY_PEM),
        QStringLiteral(
            "-----BEGIN PRIVATE "
            "KEY-----"
            "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgmr4GQQjerojFuf0ZouOuUllMvAwxZSZAfB6gwDYcLiehRANCAAT0WR5zVS"
            "p6ieusvjkLkzaGc7fjGBmwpiuLPxR/d+ZjqMI9L2DKh+takp6wGt2x0n4jzr1KA352NZg0vjZX9CHh-----END PRIVATE KEY-----"));
    QCOMPARE(attr->value(EntryAttributes::KPEX_PASSKEY_USERNAME), QStringLiteral("KPXC_BITWARDEN"));
    QCOMPARE(attr->value(EntryAttributes::KPEX_PASSKEY_RELYING_PARTY), QStringLiteral("webauthn.io"));
    QCOMPARE(attr->value(EntryAttributes::KPEX_PASSKEY_USER_HANDLE),
             QStringLiteral("aTFtdmFnOHYtS2dxVEJ0by1rSFpLWGg0enlTVC1iUVJReDZ5czJXa3c2aw"));
}

void TestImports::testProtonPass()
{
    auto protonPassPath =
        QStringLiteral("%1/%2").arg(KEEPASSX_TEST_DATA_DIR, QStringLiteral("/protonpass_export.json"));

    ProtonPassReader reader;
    auto db = reader.convert(protonPassPath);
    QVERIFY2(!reader.hasError(), qPrintable(reader.errorString()));
    QVERIFY(db);

    // Confirm Login fields
    auto entry = db->rootGroup()->findEntryByPath("/Personal/Test Login");
    QVERIFY(entry);
    QCOMPARE(entry->title(), QStringLiteral("Test Login"));
    QCOMPARE(entry->username(), QStringLiteral("Username"));
    QCOMPARE(entry->password(), QStringLiteral("Password"));
    QCOMPARE(entry->url(), QStringLiteral("https://example.com/"));
    QCOMPARE(entry->notes(), QStringLiteral("My login secure note."));
    // Check extra URL's
    QCOMPARE(entry->attribute("KP2A_URL_1"), QStringLiteral("https://example2.com/"));
    // Check TOTP
    QVERIFY(entry->hasTotp());
    // Check attributes
    auto attr = entry->attributes();
    QVERIFY(attr->isProtected("hidden field"));
    QCOMPARE(attr->value("second 2fa secret"), QStringLiteral("TOTPCODE"));
    // NOTE: Proton Pass does not export attachments
    // NOTE: Proton Pass does not export expiration dates

    // Confirm Secure Note
    entry = db->rootGroup()->findEntryByPath("/Personal/My Secure Note");
    QVERIFY(entry);
    QCOMPARE(entry->notes(), QStringLiteral("Secure note contents."));

    // Confirm Credit Card
    entry = db->rootGroup()->findEntryByPath("/Personal/Test Card");
    QVERIFY(entry);
    QCOMPARE(entry->username(), QStringLiteral("1234222233334444"));
    QCOMPARE(entry->password(), QStringLiteral("333"));
    attr = entry->attributes();
    QCOMPARE(attr->value("card_cardholderName"), QStringLiteral("Test name"));
    QCOMPARE(attr->value("card_expirationDate"), QStringLiteral("2025-01"));
    QCOMPARE(attr->value("card_pin"), QStringLiteral("1234"));
    QVERIFY(attr->isProtected("card_pin"));

    // Confirm Expired (deleted) entry
    entry = db->rootGroup()->findEntryByPath("/Personal/My Deleted Note");
    QVERIFY(entry);
    QTRY_VERIFY(entry->isExpired());

    // Confirm second group (vault)
    entry = db->rootGroup()->findEntryByPath("/Test/Other vault login");
    QVERIFY(entry);
}
