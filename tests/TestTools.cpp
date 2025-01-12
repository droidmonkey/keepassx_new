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

#include "TestTools.h"

#include "core/Clock.h"

#include <QRegularExpression>
#include <QTest>
#include <QUuid>

QTEST_GUILESS_MAIN(TestTools)

namespace
{
    QString createDecimal(QString wholes, QString fractions, QString unit)
    {
        return wholes + QLocale().decimalPoint() + fractions + " " + unit;
    }
} // namespace

void TestTools::testHumanReadableFileSize()
{
    constexpr auto kibibyte = 1024u;
    using namespace Tools;

    QCOMPARE(QString("1 B"), humanReadableFileSize(1));
    QCOMPARE(createDecimal("1", "00", "KiB"), humanReadableFileSize(kibibyte));
    QCOMPARE(createDecimal("1", "00", "MiB"), humanReadableFileSize(kibibyte * kibibyte));
    QCOMPARE(createDecimal("1", "00", "GiB"), humanReadableFileSize(kibibyte * kibibyte * kibibyte));

    QCOMPARE(QString("100 B"), humanReadableFileSize(100, 0));
    QCOMPARE(createDecimal("1", "10", "KiB"), humanReadableFileSize(kibibyte + 100));
    QCOMPARE(createDecimal("1", "001", "KiB"), humanReadableFileSize(kibibyte + 1, 3));
    QCOMPARE(createDecimal("15", "00", "KiB"), humanReadableFileSize(kibibyte * 15));
}

void TestTools::testIsHex()
{
    QVERIFY(Tools::isHex("0123456789abcdefABCDEF"));
    QVERIFY(!Tools::isHex(QByteArray("0xnothex")));
}

void TestTools::testIsBase64()
{
    QVERIFY(Tools::isBase64(QByteArray("1234")));
    QVERIFY(Tools::isBase64(QByteArray("123=")));
    QVERIFY(Tools::isBase64(QByteArray("12==")));
    QVERIFY(Tools::isBase64(QByteArray("abcd9876MN==")));
    QVERIFY(Tools::isBase64(QByteArray("abcd9876DEFGhijkMNO=")));
    QVERIFY(Tools::isBase64(QByteArray("abcd987/DEFGh+jk/NO=")));
    QVERIFY(!Tools::isBase64(QByteArray("abcd123==")));
    QVERIFY(!Tools::isBase64(QByteArray("abc_")));
    QVERIFY(!Tools::isBase64(QByteArray("123")));
}

void TestTools::testIsAsciiString()
{
    QVERIFY(Tools::isAsciiString("abcd9876DEFGhijkMNO"));
    QVERIFY(Tools::isAsciiString("-!&5a?`~"));
    QVERIFY(!Tools::isAsciiString("Štest"));
    QVERIFY(!Tools::isAsciiString("Ãß"));
}

void TestTools::testEnvSubstitute()
{
    QProcessEnvironment environment;

#if defined(Q_OS_WIN)
    environment.insert("HOMEDRIVE", "C:");
    environment.insert("HOMEPATH", "\\Users\\User");
    environment.insert("USERPROFILE", "C:\\Users\\User");

    QCOMPARE(Tools::envSubstitute("%HOMEDRIVE%%HOMEPATH%\\.ssh\\id_rsa", environment),
             QString("C:\\Users\\User\\.ssh\\id_rsa"));
    QCOMPARE(Tools::envSubstitute("start%EMPTY%%EMPTY%%%HOMEDRIVE%%end", environment), QString("start%C:%end"));
    QCOMPARE(Tools::envSubstitute("%USERPROFILE%\\.ssh\\id_rsa", environment),
             QString("C:\\Users\\User\\.ssh\\id_rsa"));
    QCOMPARE(Tools::envSubstitute("~\\.ssh\\id_rsa", environment), QString("C:\\Users\\User\\.ssh\\id_rsa"));
#else
    environment.insert("HOME", QString("/home/user"));
    environment.insert("USER", QString("user"));

    QCOMPARE(Tools::envSubstitute("~/.ssh/id_rsa", environment), QString("/home/user/.ssh/id_rsa"));
    QCOMPARE(Tools::envSubstitute("$HOME/.ssh/id_rsa", environment), QString("/home/user/.ssh/id_rsa"));
    QCOMPARE(Tools::envSubstitute("start/$EMPTY$$EMPTY$HOME/end", environment), QString("start/$/home/user/end"));
#endif
}

void TestTools::testValidUuid()
{
    auto validUuid = Tools::uuidToHex(QUuid::createUuid());
    auto nonRfc4122Uuid = "1234567890abcdef1234567890abcdef";
    auto emptyUuid = QString();
    auto shortUuid = validUuid.left(10);
    auto longUuid = validUuid + "baddata";
    auto nonHexUuid = Tools::uuidToHex(QUuid::createUuid()).replace(0, 1, 'p');

    QVERIFY(Tools::isValidUuid(validUuid));
    /* Before https://github.com/keepassxreboot/keepassxc/pull/1770/, entry
     * UUIDs are simply random 16-byte strings. Such older entries should be
     * accepted as well. */
    QVERIFY(Tools::isValidUuid(nonRfc4122Uuid));
    QVERIFY(!Tools::isValidUuid(emptyUuid));
    QVERIFY(!Tools::isValidUuid(shortUuid));
    QVERIFY(!Tools::isValidUuid(longUuid));
    QVERIFY(!Tools::isValidUuid(nonHexUuid));
}

void TestTools::testBackupFilePatternSubstitution_data()
{
    QTest::addColumn<QString>("pattern");
    QTest::addColumn<QString>("dbFilePath");
    QTest::addColumn<QString>("expectedSubstitution");

    static const auto DEFAULT_DB_FILE_NAME = QStringLiteral("KeePassXC");
    static const auto DEFAULT_DB_FILE_PATH = QStringLiteral("/tmp/") + DEFAULT_DB_FILE_NAME + QStringLiteral(".kdbx");
    static const auto NOW = Clock::currentDateTime();
    auto DEFAULT_FORMATTED_TIME = NOW.toString("dd_MM_yyyy_hh-mm-ss");

    QTest::newRow("Null pattern") << QString() << DEFAULT_DB_FILE_PATH << QString();
    QTest::newRow("Empty pattern") << QString("") << DEFAULT_DB_FILE_PATH << QString("");
    QTest::newRow("Null database path") << "valid_pattern" << QString() << QString();
    QTest::newRow("Empty database path") << "valid_pattern" << QString("") << QString();
    QTest::newRow("Unclosed/invalid pattern") << "{DB_FILENAME" << DEFAULT_DB_FILE_PATH << "{DB_FILENAME";
    QTest::newRow("Unknown pattern") << "{NO_MATCH}" << DEFAULT_DB_FILE_PATH << "{NO_MATCH}";
    QTest::newRow("Do not replace escaped patterns (filename)")
        << "\\{DB_FILENAME\\}" << DEFAULT_DB_FILE_PATH << "{DB_FILENAME}";
    QTest::newRow("Do not replace escaped patterns (time)")
        << "\\{TIME:dd.MM.yyyy\\}" << DEFAULT_DB_FILE_PATH << "{TIME:dd.MM.yyyy}";
    QTest::newRow("Multiple patterns should be replaced")
        << "{DB_FILENAME} {TIME} {DB_FILENAME}" << DEFAULT_DB_FILE_PATH
        << DEFAULT_DB_FILE_NAME + QStringLiteral(" ") + DEFAULT_FORMATTED_TIME + QStringLiteral(" ")
               + DEFAULT_DB_FILE_NAME;
    QTest::newRow("Default time pattern") << "{TIME}" << DEFAULT_DB_FILE_PATH << DEFAULT_FORMATTED_TIME;
    QTest::newRow("Default time pattern (empty formatter)")
        << "{TIME:}" << DEFAULT_DB_FILE_PATH << DEFAULT_FORMATTED_TIME;
    QTest::newRow("Custom time pattern") << "{TIME:dd-ss}" << DEFAULT_DB_FILE_PATH << NOW.toString("dd-ss");
    QTest::newRow("Time pattern twice") << "{TIME:yy} {TIME}" << DEFAULT_DB_FILE_PATH
                                        << NOW.toString("yy") + QStringLiteral(" ") + DEFAULT_FORMATTED_TIME;
    QTest::newRow("Complex custom time pattern")
        << "./{TIME:yy}/{DB_FILENAME}_{TIME:yyyyMMdd_HHmmss}.old.kdbx" << DEFAULT_DB_FILE_PATH
        << QStringLiteral("./") + NOW.toString("yy") + QStringLiteral("/") + DEFAULT_DB_FILE_NAME + QStringLiteral("_")
               + NOW.toString("yyyyMMdd_HHmmss") + QStringLiteral(".old.kdbx");
    QTest::newRow("Invalid custom time pattern") << "{TIME:dd/-ss}" << DEFAULT_DB_FILE_PATH << NOW.toString("dd/-ss");
    QTest::newRow("Recursive substitution") << "{TIME:'{TIME}'}" << DEFAULT_DB_FILE_PATH << DEFAULT_FORMATTED_TIME;
    QTest::newRow("{DB_FILENAME} substitution")
        << "some {DB_FILENAME} thing" << DEFAULT_DB_FILE_PATH
        << QStringLiteral("some ") + DEFAULT_DB_FILE_NAME + QStringLiteral(" thing");
    QTest::newRow("{DB_FILENAME} substitution with multiple extensions")
        << "some {DB_FILENAME} thing" << "/tmp/KeePassXC.kdbx.ext" << "some KeePassXC.kdbx thing";
    // Not relevant right now, added test anyway
    QTest::newRow("There should be no substitution loops")
        << "{DB_FILENAME}" << "{TIME:'{DB_FILENAME}'}.ext" << "{TIME:'{DB_FILENAME}'}";
}

void TestTools::testBackupFilePatternSubstitution()
{
    QFETCH(QString, pattern);
    QFETCH(QString, dbFilePath);
    QFETCH(QString, expectedSubstitution);

    QCOMPARE(Tools::substituteBackupFilePath(pattern, dbFilePath), expectedSubstitution);
}

void TestTools::testEscapeRegex_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QString all_regular_characters = "0123456789";
    for (char c = 'a'; c != 'z'; ++c) {
        all_regular_characters += QChar::fromLatin1(c);
    }
    for (char c = 'A'; c != 'Z'; ++c) {
        all_regular_characters += QChar::fromLatin1(c);
    }

    QTest::newRow("Regular characters should not be escaped") << all_regular_characters << all_regular_characters;
    QTest::newRow("Special characters should be escaped")
        << R"(.^$*+-?()[]{}|\)" << R"(\.\^\$\*\+\-\?\(\)\[\]\{\}\|\\)";
    QTest::newRow("Null character") << QString::fromLatin1("ab\0c", 4) << "ab\\0c";
}

void TestTools::testEscapeRegex()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    auto actual = Tools::escapeRegex(input);
    QCOMPARE(actual, expected);
}

void TestTools::testConvertToRegex()
{
    QFETCH(QString, input);
    QFETCH(int, options);
    QFETCH(QString, expected);

    auto regex = Tools::convertToRegex(input, options).pattern();
    QCOMPARE(regex, expected);
}

void TestTools::testConvertToRegex_data()
{
    const QString input = R"(te|st*t?[5]^(test);',.)";

    QTest::addColumn<QString>("input");
    QTest::addColumn<int>("options");
    QTest::addColumn<QString>("expected");

    QTest::newRow("No Options") << input << static_cast<int>(Tools::RegexConvertOpts::DEFAULT)
                                << QString(R"(te|st*t?[5]^(test);',.)");
    // Escape regex
    QTest::newRow("Escape Regex") << input << static_cast<int>(Tools::RegexConvertOpts::ESCAPE_REGEX)
                                  << Tools::escapeRegex(input);
    QTest::newRow("Escape Regex and exact match")
        << input << static_cast<int>(Tools::RegexConvertOpts::ESCAPE_REGEX | Tools::RegexConvertOpts::EXACT_MATCH)
        << "^(?:" + Tools::escapeRegex(input) + ")$";

    // Exact match does not escape the pattern
    QTest::newRow("Exact Match") << input << static_cast<int>(Tools::RegexConvertOpts::EXACT_MATCH)
                                 << QString(R"(^(?:te|st*t?[5]^(test);',.)$)");

    // Exact match with improper regex
    QTest::newRow("Exact Match") << ")av(" << static_cast<int>(Tools::RegexConvertOpts::EXACT_MATCH)
                                 << QString(R"(^(?:)av()$)");

    QTest::newRow("Exact Match & Wildcard")
        << input << static_cast<int>(Tools::RegexConvertOpts::EXACT_MATCH | Tools::RegexConvertOpts::WILDCARD_ALL)
        << QString(R"(^(?:te|st.*t.\[5\]\^\(test\)\;\'\,\.)$)");
    QTest::newRow("Wildcard Single Match") << input << static_cast<int>(Tools::RegexConvertOpts::WILDCARD_SINGLE_MATCH)
                                           << QString(R"(te\|st\*t.\[5\]\^\(test\)\;\'\,\.)");
    QTest::newRow("Wildcard OR") << input << static_cast<int>(Tools::RegexConvertOpts::WILDCARD_LOGICAL_OR)
                                 << QString(R"(te|st\*t\?\[5\]\^\(test\)\;\'\,\.)");
    QTest::newRow("Wildcard Unlimited Match")
        << input << static_cast<int>(Tools::RegexConvertOpts::WILDCARD_UNLIMITED_MATCH)
        << QString(R"(te\|st.*t\?\[5\]\^\(test\)\;\'\,\.)");
}

void TestTools::testArrayContainsValues()
{
    const auto values = QStringList() << "first" << "second" << "third";

    // One missing
    const auto result1 =
        Tools::getMissingValuesFromList<QString>(values, QStringList() << "first" << "second" << "none");
    QCOMPARE(result1.length(), 1);
    QCOMPARE(result1.first(), QString("none"));

    // All found
    const auto result2 =
        Tools::getMissingValuesFromList<QString>(values, QStringList() << "first" << "second" << "third");
    QCOMPARE(result2.length(), 0);

    // None are found
    const auto numberValues = QList<int>({1, 2, 3, 4, 5});
    const auto result3 = Tools::getMissingValuesFromList<int>(numberValues, QList<int>({6, 7, 8}));
    QCOMPARE(result3.length(), 3);
}

void TestTools::testMimeTypes()
{
    const QStringList TextMimeTypes = {
        "text/plain", // Plain text
        "text/html", // HTML documents
        "text/css", // CSS stylesheets
        "text/javascript", // JavaScript files
        "text/markdown", // Markdown documents
        "text/xml", // XML documents
        "text/rtf", // Rich Text Format
        "text/vcard", // vCard files
        "text/tab-separated-values", // Tab-separated values
        "application/json", // JSON data
        "application/xml", // XML data
        "application/soap+xml", // SOAP messages
        "application/x-yaml", // YAML data
        "application/protobuf", // Protocol Buffers
    };

    const QStringList ImageMimeTypes = {
        "image/jpeg", // JPEG images
        "image/png", // PNG images
        "image/gif", // GIF images
        "image/bmp", // BMP images
        "image/webp", // WEBP images
        "image/svg+xml" // SVG images
    };

    const QStringList UnknownMimeTypes = {
        "audio/mpeg", // MPEG audio files
        "video/mp4", // MP4 video files
        "application/pdf", // PDF documents
        "application/zip", // ZIP archives
        "application/x-tar", // TAR archives
        "application/x-rar-compressed", // RAR archives
        "application/x-7z-compressed", // 7z archives
        "application/x-shockwave-flash", // Adobe Flash files
        "application/vnd.ms-excel", // Microsoft Excel files
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", // Microsoft Excel (OpenXML) files
        "application/vnd.ms-powerpoint", // Microsoft PowerPoint files
        "application/vnd.openxmlformats-officedocument.presentationml.presentation", // Microsoft PowerPoint (OpenXML)
                                                                                     // files
        "application/msword", // Microsoft Word files
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document", // Microsoft Word (OpenXML) files
        "application/vnd.oasis.opendocument.text", // OpenDocument Text
        "application/vnd.oasis.opendocument.spreadsheet", // OpenDocument Spreadsheet
        "application/vnd.oasis.opendocument.presentation", // OpenDocument Presentation
        "application/x-httpd-php", // PHP files
        "application/x-perl", // Perl scripts
        "application/x-python", // Python scripts
        "application/x-ruby", // Ruby scripts
        "application/x-shellscript", // Shell scripts
    };

    for (const auto& mime : TextMimeTypes) {
        QCOMPARE(Tools::toMimeType(mime), Tools::MimeType::PlainText);
    }

    for (const auto& mime : ImageMimeTypes) {
        QCOMPARE(Tools::toMimeType(mime), Tools::MimeType::Image);
    }

    for (const auto& mime : UnknownMimeTypes) {
        QCOMPARE(Tools::toMimeType(mime), Tools::MimeType::Unknown);
    }
}
