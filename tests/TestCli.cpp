/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
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

#include "TestCli.h"

#include "config-keepassx-tests.h"
#include "core/Bootstrap.h"
#include "core/Config.h"
#include "core/Group.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "crypto/Crypto.h"
#include "keys/FileKey.h"
#include "keys/drivers/YubiKey.h"

#include "cli/Add.h"
#include "cli/AddGroup.h"
#include "cli/Analyze.h"
#include "cli/AttachmentExport.h"
#include "cli/AttachmentImport.h"
#include "cli/AttachmentRemove.h"
#include "cli/Clip.h"
#include "cli/DatabaseCreate.h"
#include "cli/DatabaseEdit.h"
#include "cli/DatabaseInfo.h"
#include "cli/Diceware.h"
#include "cli/Edit.h"
#include "cli/Estimate.h"
#include "cli/Export.h"
#include "cli/Generate.h"
#include "cli/Help.h"
#include "cli/Import.h"
#include "cli/List.h"
#include "cli/Merge.h"
#include "cli/Move.h"
#include "cli/Open.h"
#include "cli/Remove.h"
#include "cli/RemoveGroup.h"
#include "cli/Search.h"
#include "cli/Show.h"
#include "cli/Utils.h"

#include <QClipboard>
#include <QSignalSpy>
#include <QTest>
#include <QtConcurrent>
#include <zxcvbn.h>

QTEST_MAIN(TestCli)

void TestCli::initTestCase()
{
    QVERIFY(Crypto::init());

    Config::createTempFileInstance();
    QLocale::setDefault(QLocale::c());
    Bootstrap::bootstrap();

    m_devNull.reset(new QFile());
#ifdef Q_OS_WIN
    m_devNull->open(fopen("nul", "w"), QIODevice::WriteOnly);
#else
    m_devNull->open(fopen("/dev/null", "w"), QIODevice::WriteOnly);
#endif
    Utils::DEVNULL.setDevice(m_devNull.data());
}

void TestCli::init()
{
    const auto file = QString(KEEPASSX_TEST_DATA_DIR).append("/%1");

    m_dbFile.reset(new TemporaryFile());
    m_dbFile->copyFromFile(file.arg("NewDatabase.kdbx"));

    m_dbFile2.reset(new TemporaryFile());
    m_dbFile2->copyFromFile(file.arg("NewDatabase2.kdbx"));

    m_dbFileMulti.reset(new TemporaryFile());
    m_dbFileMulti->copyFromFile(file.arg("NewDatabaseMulti.kdbx"));

    m_xmlFile.reset(new TemporaryFile());
    m_xmlFile->copyFromFile(file.arg("NewDatabase.xml"));

    m_keyFileProtectedDbFile.reset(new TemporaryFile());
    m_keyFileProtectedDbFile->copyFromFile(file.arg("KeyFileProtected.kdbx"));

    m_keyFileProtectedNoPasswordDbFile.reset(new TemporaryFile());
    m_keyFileProtectedNoPasswordDbFile->copyFromFile(file.arg("KeyFileProtectedNoPassword.kdbx"));

    m_yubiKeyProtectedDbFile.reset(new TemporaryFile());
    m_yubiKeyProtectedDbFile->copyFromFile(file.arg("YubiKeyProtectedPasswords.kdbx"));

    m_nonAsciiDbFile.reset(new TemporaryFile());
    m_nonAsciiDbFile->copyFromFile(file.arg("NonAscii.kdbx"));

    m_stdout.reset(new QBuffer());
    m_stdout->open(QIODevice::ReadWrite);
    Utils::STDOUT.setDevice(m_stdout.data());

    m_stderr.reset(new QBuffer());
    m_stderr->open(QIODevice::ReadWrite);
    Utils::STDERR.setDevice(m_stderr.data());

    m_stdin.reset(new QBuffer());
    m_stdin->open(QIODevice::ReadWrite);
    Utils::STDIN.setDevice(m_stdin.data());
}

void TestCli::cleanup()
{
    m_dbFile.reset();
    m_dbFile2.reset();
    m_dbFileMulti.reset();
    m_keyFileProtectedDbFile.reset();
    m_keyFileProtectedNoPasswordDbFile.reset();
    m_yubiKeyProtectedDbFile.reset();

    Utils::STDOUT.setDevice(nullptr);
    Utils::STDERR.setDevice(nullptr);
    Utils::STDIN.setDevice(nullptr);
}

void TestCli::cleanupTestCase()
{
    m_devNull.reset();
}

QSharedPointer<Database> TestCli::readDatabase(const QString& filename, const QString& pw, const QString& keyfile)
{
    auto db = QSharedPointer<Database>::create();
    auto key = QSharedPointer<CompositeKey>::create();

    if (filename.isEmpty()) {
        // Open the default test database
        key->addKey(QSharedPointer<PasswordKey>::create("a"));
        if (!db->open(m_dbFile->fileName(), key)) {
            return {};
        }
    } else {
        // Open the specified database file using supplied credentials
        key->addKey(QSharedPointer<PasswordKey>::create(pw));
        if (!keyfile.isEmpty()) {
            auto filekey = QSharedPointer<FileKey>::create();
            filekey->load(keyfile);
            key->addKey(filekey);
        }

        if (!db->open(filename, key)) {
            return {};
        }
    }

    return db;
}

int TestCli::execCmd(Command& cmd, const QStringList& args) const
{
    // Move to end of stream
    m_stdout->readAll();
    m_stderr->readAll();

    // Record stream position
    auto outPos = m_stdout->pos();
    auto errPos = m_stderr->pos();

    // Execute command
    int ret = cmd.execute(args);

    // Move back to recorded position
    m_stdout->seek(outPos);
    m_stderr->seek(errPos);

    // Skip over blank lines
    QByteArray newline("\n");
    while (m_stdout->peek(1) == newline) {
        m_stdout->readLine();
    }
    while (m_stderr->peek(1) == newline) {
        m_stderr->readLine();
    }

    return ret;
}

bool TestCli::isTotp(const QString& value)
{
    static const QRegularExpression totp("^\\d{6}$");
    return totp.match(value.trimmed()).hasMatch();
}

void TestCli::setInput(const QString& input)
{
    setInput(QStringList(input));
}

void TestCli::setInput(const QStringList& input)
{
    auto ba = input.join("\n").toLatin1();
    // Always end in newline
    if (!ba.endsWith("\n")) {
        ba.append("\n");
    }
    auto pos = m_stdin->pos();
    m_stdin->write(ba);
    m_stdin->seek(pos);
}

void TestCli::testBatchCommands()
{
    Commands::setupCommands(false);
    QVERIFY(Commands::getCommand("add"));
    QVERIFY(Commands::getCommand("analyze"));
    QVERIFY(Commands::getCommand("attachment-export"));
    QVERIFY(Commands::getCommand("attachment-import"));
    QVERIFY(Commands::getCommand("attachment-rm"));
    QVERIFY(Commands::getCommand("clip"));
    QVERIFY(Commands::getCommand("close"));
    QVERIFY(Commands::getCommand("db-create"));
    QVERIFY(Commands::getCommand("db-info"));
    QVERIFY(Commands::getCommand("diceware"));
    QVERIFY(Commands::getCommand("edit"));
    QVERIFY(Commands::getCommand("estimate"));
    QVERIFY(Commands::getCommand("export"));
    QVERIFY(Commands::getCommand("generate"));
    QVERIFY(Commands::getCommand("help"));
    QVERIFY(Commands::getCommand("import"));
    QVERIFY(Commands::getCommand("ls"));
    QVERIFY(Commands::getCommand("merge"));
    QVERIFY(Commands::getCommand("mkdir"));
    QVERIFY(Commands::getCommand("mv"));
    QVERIFY(Commands::getCommand("open"));
    QVERIFY(Commands::getCommand("rm"));
    QVERIFY(Commands::getCommand("rmdir"));
    QVERIFY(Commands::getCommand("show"));
    QVERIFY(Commands::getCommand("search"));
    QVERIFY(!Commands::getCommand("doesnotexist"));
    QCOMPARE(Commands::getCommands().size(), 26);
}

void TestCli::testInteractiveCommands()
{
    Commands::setupCommands(true);
    QVERIFY(Commands::getCommand("add"));
    QVERIFY(Commands::getCommand("analyze"));
    QVERIFY(Commands::getCommand("attachment-export"));
    QVERIFY(Commands::getCommand("attachment-import"));
    QVERIFY(Commands::getCommand("attachment-rm"));
    QVERIFY(Commands::getCommand("clip"));
    QVERIFY(Commands::getCommand("close"));
    QVERIFY(Commands::getCommand("db-create"));
    QVERIFY(Commands::getCommand("db-info"));
    QVERIFY(Commands::getCommand("diceware"));
    QVERIFY(Commands::getCommand("edit"));
    QVERIFY(Commands::getCommand("estimate"));
    QVERIFY(Commands::getCommand("exit"));
    QVERIFY(Commands::getCommand("generate"));
    QVERIFY(Commands::getCommand("help"));
    QVERIFY(Commands::getCommand("ls"));
    QVERIFY(Commands::getCommand("merge"));
    QVERIFY(Commands::getCommand("mkdir"));
    QVERIFY(Commands::getCommand("mv"));
    QVERIFY(Commands::getCommand("open"));
    QVERIFY(Commands::getCommand("quit"));
    QVERIFY(Commands::getCommand("rm"));
    QVERIFY(Commands::getCommand("rmdir"));
    QVERIFY(Commands::getCommand("show"));
    QVERIFY(Commands::getCommand("search"));
    QVERIFY(!Commands::getCommand("doesnotexist"));
    QCOMPARE(Commands::getCommands().size(), 26);
}

void TestCli::testAdd()
{
    Add addCmd;
    QVERIFY(!addCmd.name.isEmpty());
    QVERIFY(addCmd.getDescriptionLine().contains(addCmd.name));

    setInput("a");
    execCmd(addCmd,
            {"add",
             "-u",
             "newuser",
             "--url",
             "https://example.com/",
             "-g",
             "-L",
             "20",
             "--notes",
             "some notes",
             m_dbFile->fileName(),
             "/newuser-entry"});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QVERIFY(m_stdout->readAll().contains("Successfully added entry newuser-entry."));

    auto db = readDatabase();
    auto* entry = db->rootGroup()->findEntryByPath("/newuser-entry");
    QVERIFY(entry);
    QCOMPARE(entry->username(), QString("newuser"));
    QCOMPARE(entry->url(), QString("https://example.com/"));
    QCOMPARE(entry->password().size(), 20);
    QCOMPARE(entry->notes(), QString("some notes"));

    // Quiet option
    setInput("a");
    execCmd(addCmd, {"add", "-q", "-u", "newuser", "-g", "-L", "20", m_dbFile->fileName(), "/newentry-quiet"});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray());
    db = readDatabase();
    entry = db->rootGroup()->findEntryByPath("/newentry-quiet");
    QVERIFY(entry);
    QCOMPARE(entry->password().size(), 20);

    setInput({"a", "newpassword"});
    execCmd(addCmd,
            {"add", "-u", "newuser2", "--url", "https://example.net/", "-p", m_dbFile->fileName(), "/newuser-entry2"});
    QVERIFY(m_stdout->readAll().contains("Successfully added entry newuser-entry2."));

    db = readDatabase();
    entry = db->rootGroup()->findEntryByPath("/newuser-entry2");
    QVERIFY(entry);
    QCOMPARE(entry->username(), QString("newuser2"));
    QCOMPARE(entry->url(), QString("https://example.net/"));
    QCOMPARE(entry->password(), QString("newpassword"));

    // Password generation options
    setInput("a");
    execCmd(addCmd, {"add", "-u", "newuser3", "-g", "-L", "34", m_dbFile->fileName(), "/newuser-entry3"});
    QVERIFY(m_stdout->readAll().contains("Successfully added entry newuser-entry3."));

    db = readDatabase();
    entry = db->rootGroup()->findEntryByPath("/newuser-entry3");
    QVERIFY(entry);
    QCOMPARE(entry->username(), QString("newuser3"));
    QCOMPARE(entry->password().size(), 34);
    QRegularExpression defaultPasswordClassesRegex("^[a-zA-Z0-9]+$");
    QVERIFY(defaultPasswordClassesRegex.match(entry->password()).hasMatch());

    setInput("a");
    execCmd(addCmd,
            {"add",
             "-u",
             "newuser4",
             "-g",
             "-L",
             "20",
             "--every-group",
             "-s",
             "-n",
             "-U",
             "-l",
             m_dbFile->fileName(),
             "/newuser-entry4"});
    QVERIFY(m_stdout->readAll().contains("Successfully added entry newuser-entry4."));

    db = readDatabase();
    entry = db->rootGroup()->findEntryByPath("/newuser-entry4");
    QVERIFY(entry);
    QCOMPARE(entry->username(), QString("newuser4"));
    QCOMPARE(entry->password().size(), 20);
    QVERIFY(!defaultPasswordClassesRegex.match(entry->password()).hasMatch());

    setInput("a");
    execCmd(addCmd, {"add", "-u", "newuser5", "--notes", "test\\nnew line", m_dbFile->fileName(), "/newuser-entry5"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray(""));
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully added entry newuser-entry5.\n"));

    db = readDatabase();
    entry = db->rootGroup()->findEntryByPath("/newuser-entry5");
    QVERIFY(entry);
    QCOMPARE(entry->username(), QString("newuser5"));
    QCOMPARE(entry->notes(), QString("test\nnew line"));
}

void TestCli::testAddGroup()
{
    AddGroup addGroupCmd;
    QVERIFY(!addGroupCmd.name.isEmpty());
    QVERIFY(addGroupCmd.getDescriptionLine().contains(addGroupCmd.name));

    setInput("a");
    execCmd(addGroupCmd, {"mkdir", m_dbFile->fileName(), "/new_group"});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully added group new_group.\n"));

    auto db = readDatabase();
    auto* group = db->rootGroup()->findGroupByPath("new_group");
    QVERIFY(group);
    QCOMPARE(group->name(), QString("new_group"));

    // Trying to add the same group should fail.
    setInput("a");
    execCmd(addGroupCmd, {"mkdir", m_dbFile->fileName(), "/new_group"});
    QVERIFY(m_stderr->readAll().contains("Group /new_group already exists!"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Should be able to add groups down the tree.
    setInput("a");
    execCmd(addGroupCmd, {"mkdir", m_dbFile->fileName(), "/new_group/newer_group"});
    QVERIFY(m_stdout->readAll().contains("Successfully added group newer_group."));

    db = readDatabase();
    group = db->rootGroup()->findGroupByPath("new_group/newer_group");
    QVERIFY(group);
    QCOMPARE(group->name(), QString("newer_group"));

    // Should fail if the path is invalid.
    setInput("a");
    execCmd(addGroupCmd, {"mkdir", m_dbFile->fileName(), "/invalid_group/newer_group"});
    QVERIFY(m_stderr->readAll().contains("Group /invalid_group not found."));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Should fail to add the root group.
    setInput("a");
    execCmd(addGroupCmd, {"mkdir", m_dbFile->fileName(), "/"});
    QVERIFY(m_stderr->readAll().contains("Group / already exists!"));
    QCOMPARE(m_stdout->readAll(), QByteArray());
}

void TestCli::testAnalyze()
{
    Analyze analyzeCmd;
    QVERIFY(!analyzeCmd.name.isEmpty());
    QVERIFY(analyzeCmd.getDescriptionLine().contains(analyzeCmd.name));

    const QString hibpPath = QString(KEEPASSX_TEST_DATA_DIR).append("/hibp.txt");

    setInput("a");
    execCmd(analyzeCmd, {"analyze", "--hibp", hibpPath, m_dbFile->fileName()});
    auto output = m_stdout->readAll();
    QVERIFY(output.contains("Sample Entry"));
    QVERIFY(output.contains("123"));
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
}

void TestCli::testAttachmentExport()
{
    AttachmentExport attachmentExportCmd;
    QVERIFY(!attachmentExportCmd.name.isEmpty());
    QVERIFY(attachmentExportCmd.getDescriptionLine().contains(attachmentExportCmd.name));

    TemporaryFile exportOutput;
    exportOutput.open(QIODevice::WriteOnly);
    exportOutput.close();

    // Try exporting an attachment of a non-existent entry
    setInput("a");
    execCmd(attachmentExportCmd,
            {"attachment-export",
             m_dbFile->fileName(),
             "invalid_entry_path",
             "invalid_attachment_name",
             exportOutput.fileName()});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Could not find entry with path invalid_entry_path.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Try exporting a non-existent attachment
    setInput("a");
    execCmd(attachmentExportCmd,
            {"attachment-export",
             m_dbFile->fileName(),
             "/Sample Entry",
             "invalid_attachment_name",
             exportOutput.fileName()});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Could not find attachment with name invalid_attachment_name.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Export an existing attachment to a file
    setInput("a");
    execCmd(
        attachmentExportCmd,
        {"attachment-export", m_dbFile->fileName(), "/Sample Entry", "Sample attachment.txt", exportOutput.fileName()});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray(qPrintable(QString("Successfully exported attachment %1 of entry %2 to %3.\n")
                                       .arg("Sample attachment.txt", "/Sample Entry", exportOutput.fileName()))));

    exportOutput.open(QIODevice::ReadOnly);
    QCOMPARE(exportOutput.readAll(), QByteArray("Sample content\n"));

    // Export an existing attachment to stdout
    setInput("a");
    execCmd(attachmentExportCmd,
            {"attachment-export", "--stdout", m_dbFile->fileName(), "/Sample Entry", "Sample attachment.txt"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray("Sample content\n"));

    // Ensure --stdout works even in quiet mode
    setInput("a");
    execCmd(
        attachmentExportCmd,
        {"attachment-export", "--quiet", "--stdout", m_dbFile->fileName(), "/Sample Entry", "Sample attachment.txt"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray("Sample content\n"));
}

void TestCli::testAttachmentImport()
{
    AttachmentImport attachmentImportCmd;
    QVERIFY(!attachmentImportCmd.name.isEmpty());
    QVERIFY(attachmentImportCmd.getDescriptionLine().contains(attachmentImportCmd.name));

    const QString attachmentPath = QString(KEEPASSX_TEST_DATA_DIR).append("/Attachment.txt");
    QVERIFY(QFile::exists(attachmentPath));

    // Try importing an attachment to a non-existent entry
    setInput("a");
    execCmd(attachmentImportCmd,
            {"attachment-import",
             m_dbFile->fileName(),
             "invalid_entry_path",
             "invalid_attachment_name",
             "invalid_attachment_path"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Could not find entry with path invalid_entry_path.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Try importing an attachment with an occupied name without -f option
    setInput("a");
    execCmd(attachmentImportCmd,
            {"attachment-import",
             m_dbFile->fileName(),
             "/Sample Entry",
             "Sample attachment.txt",
             "invalid_attachment_path"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(),
             QByteArray("Attachment Sample attachment.txt already exists for entry /Sample Entry.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Try importing a non-existent attachment
    setInput("a");
    execCmd(attachmentImportCmd,
            {"attachment-import",
             m_dbFile->fileName(),
             "/Sample Entry",
             "Sample attachment 2.txt",
             "invalid_attachment_path"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Could not open attachment file invalid_attachment_path.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Try importing an attachment with an occupied name with -f option
    setInput("a");
    execCmd(
        attachmentImportCmd,
        {"attachment-import", "-f", m_dbFile->fileName(), "/Sample Entry", "Sample attachment.txt", attachmentPath});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray(qPrintable(
                 QString("Successfully imported attachment %1 as Sample attachment.txt to entry /Sample Entry.\n")
                     .arg(attachmentPath))));

    // Try importing an attachment with an unoccupied name
    setInput("a");
    execCmd(attachmentImportCmd,
            {"attachment-import", m_dbFile->fileName(), "/Sample Entry", "Attachment.txt", attachmentPath});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(
        m_stdout->readAll(),
        QByteArray(qPrintable(QString("Successfully imported attachment %1 as Attachment.txt to entry /Sample Entry.\n")
                                  .arg(attachmentPath))));
}

void TestCli::testAttachmentRemove()
{
    AttachmentRemove attachmentRemoveCmd;
    QVERIFY(!attachmentRemoveCmd.name.isEmpty());
    QVERIFY(attachmentRemoveCmd.getDescriptionLine().contains(attachmentRemoveCmd.name));

    // Try deleting an attachment belonging to an non-existent entry
    setInput("a");
    execCmd(attachmentRemoveCmd,
            {"attachment-rm", m_dbFile->fileName(), "invalid_entry_path", "invalid_attachment_name"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Could not find entry with path invalid_entry_path.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Try deleting a non-existent attachment from an entry
    setInput("a");
    execCmd(attachmentRemoveCmd, {"attachment-rm", m_dbFile->fileName(), "/Sample Entry", "invalid_attachment_name"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Could not find attachment with name invalid_attachment_name.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Finally delete an existing attachment from an existing entry
    auto db = readDatabase();
    QVERIFY(db);

    const Entry* entry = db->rootGroup()->findEntryByPath("/Sample Entry");
    QVERIFY(entry);

    QVERIFY(entry->attachments()->hasKey("Sample attachment.txt"));

    setInput("a");
    execCmd(attachmentRemoveCmd, {"attachment-rm", m_dbFile->fileName(), "/Sample Entry", "Sample attachment.txt"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Successfully removed attachment Sample attachment.txt from entry /Sample Entry.\n"));

    db = readDatabase();
    QVERIFY(db);
    QVERIFY(!db->rootGroup()->findEntryByPath("/Sample Entry")->attachments()->hasKey("Sample attachment.txt"));
}

void TestCli::testClip()
{
    if (QProcessEnvironment::systemEnvironment().contains("WAYLAND_DISPLAY")) {
        QSKIP("Clip test skipped due to QClipboard and Wayland issues on Linux");
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->clear();

    Clip clipCmd;
    QVERIFY(!clipCmd.name.isEmpty());
    QVERIFY(clipCmd.getDescriptionLine().contains(clipCmd.name));

    // Password
    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFile->fileName(), "/Sample Entry", "0"});
    QString errorOutput(m_stderr->readAll());

    if (errorOutput.contains("Unable to start program")
        || errorOutput.contains("No program defined for clipboard manipulation")) {
        QSKIP("Clip test skipped due to missing clipboard tool");
    }
    QVERIFY(!errorOutput.contains("All clipping programs failed"));

    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QTRY_COMPARE(clipboard->text(), QString("Password"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Entry's \"Password\" attribute copied to the clipboard!\n"));

    // Quiet option
    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFile->fileName(), "/Sample Entry", "0", "-q"});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray());
    QTRY_COMPARE(clipboard->text(), QString("Password"));

    // Username
    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFile->fileName(), "/Sample Entry", "0", "-a", "username"});
    QTRY_COMPARE(clipboard->text(), QString("User Name"));

    // Uuid (top-level field)
    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFile->fileName(), "/Sample Entry", "0", "-a", "Uuid"});
    QTRY_COMPARE(clipboard->text(), QString("{9f4544c2-ab00-c74a-8a1a-6eaf26cf57e9}"));

    // TOTP
    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFile->fileName(), "/Sample Entry", "0", "--totp"});
    QTRY_VERIFY(isTotp(clipboard->text()));
    QCOMPARE(m_stdout->readLine(), QByteArray("Entry's \"totp\" attribute copied to the clipboard!\n"));

    // Test Unicode
    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFile2->fileName(), "/General/Unicode", "0", "-a", "username"});
    QTRY_COMPARE(clipboard->text(), QString(R"(¯\_(ツ)_/¯)"));

    // Password with timeout
    setInput("a");
    // clang-format off
    QFuture<void> future = QtConcurrent::run(&clipCmd,
                                             static_cast<int(Clip::*)(const QStringList&)>(&DatabaseCommand::execute),
                                             QStringList{"clip", m_dbFile->fileName(), "/Sample Entry", "1"});
    // clang-format on

    QTRY_COMPARE(clipboard->text(), QString("Password"));
    QTRY_COMPARE_WITH_TIMEOUT(clipboard->text(), QString(""), 3000);

    future.waitForFinished();

    // TOTP with timeout
    setInput("a");
    future = QtConcurrent::run(&clipCmd,
                               static_cast<int (Clip::*)(const QStringList&)>(&DatabaseCommand::execute),
                               QStringList{"clip", m_dbFile->fileName(), "/Sample Entry", "1", "-t"});

    QTRY_VERIFY(isTotp(clipboard->text()));
    QTRY_COMPARE_WITH_TIMEOUT(clipboard->text(), QString(""), 3000);

    future.waitForFinished();

    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFile->fileName(), "--totp", "/Sample Entry", "bleuh"});
    QVERIFY(m_stderr->readAll().contains("Invalid timeout value bleuh.\n"));

    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFile2->fileName(), "--totp", "/Sample Entry", "0"});
    QVERIFY(m_stderr->readAll().contains("Entry with path /Sample Entry has no TOTP set up.\n"));

    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFile->fileName(), "-a", "TESTAttribute1", "/Sample Entry", "0"});
    QVERIFY(m_stderr->readAll().contains("ERROR: attribute TESTAttribute1 is ambiguous"));

    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFile2->fileName(), "--attribute", "Username", "--totp", "/Sample Entry", "0"});
    QVERIFY(m_stderr->readAll().contains("ERROR: Please specify one of --attribute or --totp, not both.\n"));

    // Best option
    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFileMulti->fileName(), "Multi", "0", "-b"});
    QByteArray errorChoices = m_stderr->readAll();
    QVERIFY(errorChoices.contains("Multi Entry 1"));
    QVERIFY(errorChoices.contains("Multi Entry 2"));

    setInput("a");
    execCmd(clipCmd, {"clip", m_dbFileMulti->fileName(), "Entry 2", "0", "-b"});
    QTRY_COMPARE(clipboard->text(), QString("Password2"));
}

void TestCli::testCreate()
{
    DatabaseCreate createCmd;
    QVERIFY(!createCmd.name.isEmpty());
    QVERIFY(createCmd.getDescriptionLine().contains(createCmd.name));

    QScopedPointer<QTemporaryDir> testDir(new QTemporaryDir());
    QString dbFilename;

    // Testing password option, password mismatch
    dbFilename = testDir->path() + "/testCreate_pw.kdbx";
    setInput({"a", "b"});
    execCmd(createCmd, {"db-create", dbFilename, "-p"});

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Repeat password: \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Error: Passwords do not match.\n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Failed to set database password.\n"));

    // Testing password option
    setInput({"a", "a"});
    execCmd(createCmd, {"db-create", dbFilename, "-p"});

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Repeat password: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Successfully created new database.\n"));

    auto db = readDatabase(dbFilename, "a");
    QVERIFY(db);

    // Testing with empty password (deny it)
    dbFilename = testDir->path() + "/testCreate_blankpw.kdbx";
    setInput({"", "n"});
    execCmd(createCmd, {"db-create", dbFilename, "-p"});

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QVERIFY(m_stderr->readLine().contains("empty password"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Failed to set database password.\n"));

    // Testing with empty password (accept it)
    setInput({"", "y"});
    execCmd(createCmd, {"db-create", dbFilename, "-p"});

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QVERIFY(m_stderr->readLine().contains("empty password"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Successfully created new database.\n"));

    db = readDatabase(dbFilename, "");
    QVERIFY(db);

    // Should refuse to create the database if it already exists.
    execCmd(createCmd, {"db-create", dbFilename, "-p"});
    // Output should be empty when there is an error.
    QCOMPARE(m_stdout->readAll(), QByteArray());
    QString errorMessage = QString("File " + dbFilename + " already exists.\n");
    QCOMPARE(m_stderr->readAll(), errorMessage.toUtf8());

    // Should refuse to create without any key provided.
    dbFilename = testDir->path() + "/testCreate_key.kdbx";
    execCmd(createCmd, {"db-create", dbFilename});
    QCOMPARE(m_stdout->readAll(), QByteArray());
    QCOMPARE(m_stderr->readLine(), QByteArray("No key is set. Aborting database creation.\n"));

    // Testing with keyfile creation
    dbFilename = testDir->path() + "/testCreate_key2.kdbx";
    QString keyfilePath = testDir->path() + "/keyfile.txt";
    setInput({"a", "a"});
    execCmd(createCmd, {"db-create", dbFilename, "-p", "-k", keyfilePath});

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Repeat password: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Successfully created new database.\n"));

    db = readDatabase(dbFilename, "a", keyfilePath);
    QVERIFY(db);

    // Testing with existing keyfile
    dbFilename = testDir->path() + "/testCreate_key3.kdbx";
    setInput({"a", "a"});
    execCmd(createCmd, {"db-create", dbFilename, "-p", "-k", keyfilePath});

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Repeat password: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Successfully created new database.\n"));

    db = readDatabase(dbFilename, "a", keyfilePath);
    QVERIFY(db);

    // Invalid decryption time (format).
    dbFilename = testDir->path() + "/testCreate_time.kdbx";
    execCmd(createCmd, {"db-create", dbFilename, "-p", "-t", "NAN"});

    QCOMPARE(m_stdout->readAll(), QByteArray());
    QCOMPARE(m_stderr->readAll(), QByteArray("Invalid decryption time NAN.\n"));

    // Invalid decryption time (range).
    execCmd(createCmd, {"db-create", dbFilename, "-p", "-t", "10"});

    QCOMPARE(m_stdout->readAll(), QByteArray());
    QVERIFY(m_stderr->readAll().contains(QByteArray("Target decryption time must be between")));

    int encryptionTime = 500;
    // Custom encryption time
    setInput({"a", "a"});
    int epochBefore = QDateTime::currentMSecsSinceEpoch();
    execCmd(createCmd, {"db-create", dbFilename, "-p", "-t", QString::number(encryptionTime)});
    // Removing 100ms to make sure we account for changes in computation time.
    QVERIFY(QDateTime::currentMSecsSinceEpoch() > (epochBefore + encryptionTime - 100));

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Repeat password: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Benchmarking key derivation function for 500ms delay.\n"));
    QVERIFY(m_stdout->readLine().contains(QByteArray("rounds for key derivation function.\n")));

    db = readDatabase(dbFilename, "a");
    QVERIFY(db);
}

void TestCli::testDatabaseEdit()
{
    TemporaryFile firstKeyFile;
    firstKeyFile.open();
    firstKeyFile.write(QString("keyFilePassword").toLatin1());
    firstKeyFile.close();

    TemporaryFile secondKeyFile;
    secondKeyFile.open();
    secondKeyFile.write(QString("newKeyFilePassword").toLatin1());
    secondKeyFile.close();

    QScopedPointer<QTemporaryDir> testDir(new QTemporaryDir());

    DatabaseCreate createCmd;
    DatabaseEdit editCmd;
    QVERIFY(!editCmd.name.isEmpty());
    QVERIFY(editCmd.getDescriptionLine().contains(editCmd.name));

    QString dbFilename;
    dbFilename = testDir->path() + "/testDatabaseEdit.kdbx";

    // Creating a database for testing
    setInput({"a", "a"});
    execCmd(createCmd, {"db-create", dbFilename, "-p"});
    QCOMPARE(m_stdout->readLine(), QByteArray("Successfully created new database.\n"));

    // Sanity check.
    auto db = readDatabase(dbFilename, "a");
    QVERIFY(!db.isNull());

    setInput("a");
    execCmd(editCmd, {"db-edit", dbFilename, "-p", "--unset-password"});
    QCOMPARE(m_stdout->readAll(), QByteArray(""));
    m_stderr->readLine();
    QCOMPARE(m_stderr->readAll(), QByteArray("Cannot use p and unset-password at the same time.\n"));

    setInput("a");
    execCmd(editCmd, {"db-edit", dbFilename, "--set-key-file", "/key/file/path", "--unset-key-file"});
    QCOMPARE(m_stdout->readAll(), QByteArray(""));
    // Skipping the password prompt.
    m_stderr->readLine();
    QCOMPARE(m_stderr->readAll(), QByteArray("Cannot use set-key-file and unset-key-file at the same time.\n"));

    // Sanity check.
    db = readDatabase(dbFilename, "a");
    QVERIFY(!db.isNull());

    setInput({"a", "b", "b"});
    execCmd(editCmd, {"db-edit", dbFilename, "-p"});
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully edited the database.\n"));

    // Sanity check
    db = readDatabase(dbFilename, "b");
    QVERIFY(!db.isNull());

    setInput("b");
    execCmd(editCmd, {"db-edit", dbFilename, "--set-key-file", firstKeyFile.fileName()});
    // Skipping the password prompt.
    m_stderr->readLine();
    QCOMPARE(m_stderr->readAll(), QByteArray(""));
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully edited the database.\n"));

    // Sanity check
    db = readDatabase(dbFilename, "b");
    QVERIFY(db.isNull());
    db = readDatabase(dbFilename, "b", firstKeyFile.fileName());
    QVERIFY(!db.isNull());

    setInput("b");
    execCmd(editCmd,
            {"db-edit", dbFilename, "-k", firstKeyFile.fileName(), "--set-key-file", secondKeyFile.fileName()});
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully edited the database.\n"));

    // Sanity check
    db = readDatabase(dbFilename, "b", firstKeyFile.fileName());
    QVERIFY(db.isNull());
    db = readDatabase(dbFilename, "b", secondKeyFile.fileName());
    QVERIFY(!db.isNull());

    setInput("b");
    execCmd(editCmd, {"db-edit", dbFilename, "-k", secondKeyFile.fileName(), "--unset-password"});
    // Skipping the password prompt.
    m_stderr->readLine();
    QCOMPARE(m_stderr->readAll(), QByteArray(""));
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully edited the database.\n"));

    execCmd(editCmd,
            {"db-edit",
             dbFilename,
             "--no-password",
             "-k",
             secondKeyFile.fileName(),
             "--set-key-file",
             firstKeyFile.fileName()});
    // Skipping the password prompt.
    m_stderr->readLine();
    QCOMPARE(m_stderr->readAll(), QByteArray(""));
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully edited the database.\n"));

    setInput({"b", "b"});
    execCmd(editCmd, {"db-edit", dbFilename, "-k", firstKeyFile.fileName(), "--no-password", "--set-password"});
    // Skipping over the password setting prompts.
    m_stderr->readLine();
    m_stderr->readLine();
    QCOMPARE(m_stderr->readAll(), QByteArray(""));
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully edited the database.\n"));

    setInput("b");
    execCmd(editCmd, {"db-edit", dbFilename, "-k", firstKeyFile.fileName(), "--unset-key-file"});
    // Skipping the password prompt.
    m_stderr->readLine();
    QCOMPARE(m_stderr->readAll(), QByteArray(""));
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully edited the database.\n"));

    // Sanity check
    db = readDatabase(dbFilename, "b", firstKeyFile.fileName());
    QVERIFY(db.isNull());
    db = readDatabase(dbFilename, "b");
    QVERIFY(!db.isNull());

    // Trying to remove the key file when there is none set should
    // raise an error.
    setInput("b");
    execCmd(editCmd, {"db-edit", dbFilename, "-p", "--unset-key-file"});
    QCOMPARE(m_stdout->readAll(), QByteArray(""));
    m_stderr->readLine();
    QCOMPARE(m_stderr->readLine(), QByteArray("Cannot remove file key: The database does not have a file key.\n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Could not change the database key.\n"));

    setInput("b");
    execCmd(editCmd, {"db-edit", dbFilename, "--unset-password"});
    QCOMPARE(m_stdout->readAll(), QByteArray(""));
    // Skipping the password prompt.
    m_stderr->readLine();
    QCOMPARE(m_stderr->readLine(), QByteArray("Cannot remove all the keys from a database.\n"));
}

void TestCli::testInfo()
{
    DatabaseInfo infoCmd;
    QVERIFY(!infoCmd.name.isEmpty());
    QVERIFY(infoCmd.getDescriptionLine().contains(infoCmd.name));

    setInput("a");
    execCmd(infoCmd, {"db-info", m_dbFile->fileName()});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QVERIFY(m_stdout->readLine().contains(QByteArray("UUID: ")));
    QCOMPARE(m_stdout->readLine(), QByteArray("Name: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Description: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Cipher: AES 256-bit\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("KDF: AES (6000 rounds)\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Recycle bin is enabled.\n"));
    QVERIFY(m_stdout->readLine().contains(m_dbFile->fileName().toUtf8()));
    QVERIFY(m_stdout->readLine().contains(
        QByteArray("Database created: "))); // date changes often, so just test for the first part
    QVERIFY(m_stdout->readLine().contains(
        QByteArray("Last saved: "))); // date changes often, so just test for the first part
    QCOMPARE(m_stdout->readLine(), QByteArray("Unsaved changes: no\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Number of groups: 8\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Number of entries: 2\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Number of expired entries: 0\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Unique passwords: 2\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Non-unique passwords: 0\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Maximum password reuse: 1\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Number of short passwords: 0\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Number of weak passwords: 2\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Entries excluded from reports: 0\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Average password length: 11 characters\n"));

    // Test with quiet option.
    setInput("a");
    execCmd(infoCmd, {"db-info", "-q", m_dbFile->fileName()});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QVERIFY(m_stdout->readLine().contains(QByteArray("UUID: ")));
    QCOMPARE(m_stdout->readLine(), QByteArray("Name: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Description: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Cipher: AES 256-bit\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("KDF: AES (6000 rounds)\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Recycle bin is enabled.\n"));
}

void TestCli::testDiceware()
{
    Diceware dicewareCmd;
    QVERIFY(!dicewareCmd.name.isEmpty());
    QVERIFY(dicewareCmd.getDescriptionLine().contains(dicewareCmd.name));

    execCmd(dicewareCmd, {"diceware"});
    QString passphrase(m_stdout->readLine());
    QVERIFY(!passphrase.isEmpty());

    execCmd(dicewareCmd, {"diceware", "-W", "2"});
    passphrase = m_stdout->readLine();
    QCOMPARE(passphrase.split(" ").size(), 2);

    execCmd(dicewareCmd, {"diceware", "-W", "10"});
    passphrase = m_stdout->readLine();
    QCOMPARE(passphrase.split(" ").size(), 10);

    // Testing with invalid word count
    execCmd(dicewareCmd, {"diceware", "-W", "-10"});
    QCOMPARE(m_stderr->readLine(), QByteArray("Invalid word count -10\n"));

    // Testing with invalid word count format
    execCmd(dicewareCmd, {"diceware", "-W", "bleuh"});
    QCOMPARE(m_stderr->readLine(), QByteArray("Invalid word count bleuh\n"));

    TemporaryFile wordFile;
    wordFile.open();
    for (int i = 0; i < 4500; ++i) {
        wordFile.write(QString("word" + QString::number(i) + "\n").toLatin1());
    }
    wordFile.close();

    execCmd(dicewareCmd, {"diceware", "-W", "11", "-w", wordFile.fileName()});
    passphrase = m_stdout->readLine();
    const auto words = passphrase.split(" ");
    QCOMPARE(words.size(), 11);
    QRegularExpression regex("^word\\d+$");
    for (const auto& word : words) {
        QVERIFY2(regex.match(word).hasMatch(), qPrintable("Word " + word + " was not on the word list"));
    }

    TemporaryFile smallWordFile;
    smallWordFile.open();
    for (int i = 0; i < 50; ++i) {
        smallWordFile.write(QString("word" + QString::number(i) + "\n").toLatin1());
    }
    smallWordFile.close();

    execCmd(dicewareCmd, {"diceware", "-W", "11", "-w", smallWordFile.fileName()});
    QCOMPARE(m_stderr->readLine(), QByteArray("Cannot generate valid passphrases because the wordlist is too short\n"));
}

void TestCli::testEdit()
{
    Edit editCmd;
    QVERIFY(!editCmd.name.isEmpty());
    QVERIFY(editCmd.getDescriptionLine().contains(editCmd.name));

    setInput("a");
    execCmd(editCmd,
            {"edit",
             "-u",
             "newuser",
             "--url",
             "https://otherurl.example.com/",
             "--notes",
             "newnotes",
             "-t",
             "newtitle",
             m_dbFile->fileName(),
             "/Sample Entry"});
    QCOMPARE(m_stdout->readLine(), QByteArray("Successfully edited entry newtitle.\n"));

    auto db = readDatabase();
    auto* entry = db->rootGroup()->findEntryByPath("/newtitle");
    QVERIFY(entry);
    QCOMPARE(entry->username(), QString("newuser"));
    QCOMPARE(entry->url(), QString("https://otherurl.example.com/"));
    QCOMPARE(entry->password(), QString("Password"));
    QCOMPARE(entry->notes(), QString("newnotes"));

    // Quiet option
    setInput("a");
    execCmd(editCmd, {"edit", m_dbFile->fileName(), "-q", "-t", "newertitle", "/newtitle"});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray());

    setInput("a");
    execCmd(editCmd, {"edit", "-g", m_dbFile->fileName(), "/newertitle"});
    db = readDatabase();
    entry = db->rootGroup()->findEntryByPath("/newertitle");
    QVERIFY(entry);
    QCOMPARE(entry->username(), QString("newuser"));
    QCOMPARE(entry->url(), QString("https://otherurl.example.com/"));
    QVERIFY(!entry->password().isEmpty());
    QVERIFY(entry->password() != QString("Password"));

    setInput("a");
    execCmd(editCmd, {"edit", "-g", "-L", "34", "-t", "evennewertitle", m_dbFile->fileName(), "/newertitle"});
    db = readDatabase();
    entry = db->rootGroup()->findEntryByPath("/evennewertitle");
    QVERIFY(entry);
    QCOMPARE(entry->username(), QString("newuser"));
    QCOMPARE(entry->url(), QString("https://otherurl.example.com/"));
    QVERIFY(entry->password() != QString("Password"));
    QCOMPARE(entry->password().size(), 34);
    QRegularExpression defaultPasswordClassesRegex("^[a-zA-Z0-9]+$");
    QVERIFY(defaultPasswordClassesRegex.match(entry->password()).hasMatch());

    setInput("a");
    execCmd(editCmd,
            {"edit",
             "-g",
             "-L",
             "20",
             "--every-group",
             "-s",
             "-n",
             "--upper",
             "-l",
             m_dbFile->fileName(),
             "/evennewertitle"});
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully edited entry evennewertitle.\n"));

    db = readDatabase();
    entry = db->rootGroup()->findEntryByPath("/evennewertitle");
    QVERIFY(entry);
    QCOMPARE(entry->password().size(), 20);
    QVERIFY(!defaultPasswordClassesRegex.match(entry->password()).hasMatch());

    setInput({"a", "newpassword"});
    execCmd(editCmd, {"edit", "-p", m_dbFile->fileName(), "/evennewertitle"});
    db = readDatabase();
    QVERIFY(db);
    entry = db->rootGroup()->findEntryByPath("/evennewertitle");
    QVERIFY(entry);
    QCOMPARE(entry->password(), QString("newpassword"));

    // with line break in notes
    setInput("a");
    execCmd(editCmd, {"edit", m_dbFile->fileName(), "--notes", "testing\\nline breaks", "/evennewertitle"});
    db = readDatabase();
    entry = db->rootGroup()->findEntryByPath("/evennewertitle");
    QVERIFY(entry);
    QCOMPARE(entry->notes(), QString("testing\nline breaks"));
}

void TestCli::testEstimate_data()
{
    // clang-format off
    QTest::addColumn<QString>("input");
    QTest::addColumn<QStringList>("searchStrings");

    QTest::newRow("Dictionary")
        << "password"
        << QStringList{"Type: Dictionary", "\tpassword"};

    QTest::newRow("Spatial")
        << "sdfg"
        << QStringList{"Type: Spatial", "\tsdfg"};

    QTest::newRow("Spatial(Rep)")
        << "sdfgsdfg"
        << QStringList{"Type: Spatial(Rep)", "\tsdfgsdfg"};

    QTest::newRow("Dictionary / Sequence")
        << "password123"
        << QStringList{"Type: Dictionary", "Type: Sequence", "\tpassword", "\t123"};

    QTest::newRow("Dict+Leet")
        << "p455w0rd"
        << QStringList{"Type: Dict+Leet", "\tp455w0rd"};

    QTest::newRow("Dictionary(Rep)")
        << "hellohello"
        << QStringList{"Type: Dictionary(Rep)", "\thellohello"};

    QTest::newRow("Sequence(Rep) / Dictionary")
        << "456456foobar"
        << QStringList{"Type: Sequence(Rep)", "Type: Dictionary", "\t456456", "\tfoobar"};

    QTest::newRow("Bruteforce(Rep) / Bruteforce")
        << "xzxzy"
        << QStringList{"Type: Bruteforce(Rep)", "Type: Bruteforce", "\txzxz", "\ty"};

    QTest::newRow("Dictionary / Date(Rep)")
        << "pass20182018"
        << QStringList{"Type: Dictionary", "Type: Date(Rep)", "\tpass", "\t20182018"};

    QTest::newRow("Dictionary / Date / Bruteforce")
        << "mypass2018-2"
        << QStringList{"Type: Dictionary", "Type: Date", "Type: Bruteforce", "\tmypass", "\t2018", "\t-2"};

    QTest::newRow("Strong Password")
        << "E*!%.Qw{t.X,&bafw)\"Q!ah$%;U/"
        << QStringList{"Type: Bruteforce", "\tE*"};

    // TODO: detect passphrases and adjust entropy calculation accordingly (issue #2347)
    QTest::newRow("Strong Passphrase")
        << "squint wooing resupply dangle isolation axis headsman"
        << QStringList{"Type: Dictionary", "Type: Bruteforce", "Multi-word extra bits 22.0", "\tsquint", "\t ", "\twooing"};
    // clang-format on
}

void TestCli::testEstimate()
{
    QFETCH(QString, input);
    QFETCH(QStringList, searchStrings);

    // Calculate expected values since zxcvbn output can vary by platform if different wordlists are used
    const auto e = ZxcvbnMatch(input.toUtf8(), nullptr, nullptr);
    auto length = QString::number(input.length());
    auto entropy = QString("%1").arg(e, 0, 'f', 3);
    auto log10 = QString("%1").arg(e * 0.301029996, 0, 'f', 3);

    Estimate estimateCmd;
    QVERIFY(!estimateCmd.name.isEmpty());
    QVERIFY(estimateCmd.getDescriptionLine().contains(estimateCmd.name));

    setInput(input);
    execCmd(estimateCmd, {"estimate", "-a"});
    auto result = QString(m_stdout->readAll());
    QVERIFY(result.contains("Length " + length));
    QVERIFY(result.contains("Entropy " + entropy));
    QVERIFY(result.contains("Log10 " + log10));
    for (const auto& string : asConst(searchStrings)) {
        QVERIFY2(result.contains(string), qPrintable("String " + string + " missing"));
    }
}

void TestCli::testExport()
{
    Export exportCmd;
    QVERIFY(!exportCmd.name.isEmpty());
    QVERIFY(exportCmd.getDescriptionLine().contains(exportCmd.name));

    setInput("a");
    execCmd(exportCmd, {"export", m_dbFile->fileName()});

    TemporaryFile xmlOutput;
    xmlOutput.open(QIODevice::WriteOnly);
    xmlOutput.write(m_stdout->readAll());
    xmlOutput.close();

    QScopedPointer<Database> db(new Database());
    QVERIFY(db->import(xmlOutput.fileName()));

    auto* entry = db->rootGroup()->findEntryByPath("/Sample Entry");
    QVERIFY(entry);
    QCOMPARE(entry->password(), QString("Password"));

    // Quiet option
    QScopedPointer<Database> dbQuiet(new Database());
    setInput("a");
    execCmd(exportCmd, {"export", "-f", "xml", "-q", m_dbFile->fileName()});
    QCOMPARE(m_stderr->readAll(), QByteArray());

    xmlOutput.open(QIODevice::WriteOnly);
    xmlOutput.write(m_stdout->readAll());
    xmlOutput.close();

    QVERIFY(db->import(xmlOutput.fileName()));

    // CSV exporting
    setInput("a");
    execCmd(exportCmd, {"export", "-f", "csv", m_dbFile->fileName()});
    QByteArray csvHeader = m_stdout->readLine();
    QVERIFY(csvHeader.contains(QByteArray("\"Group\",\"Title\",\"Username\",\"Password\",\"URL\",\"Notes\"")));
    QByteArray csvData = m_stdout->readAll();
    QVERIFY(csvData.contains(QByteArray(
        "\"NewDatabase\",\"Sample Entry\",\"User Name\",\"Password\",\"http://www.somesite.com/\",\"Notes\"")));

    // HTML exporting
    setInput("a");
    execCmd(exportCmd, {"export", "-f", "html", m_dbFile->fileName()});
    QByteArray htmlHeader = m_stdout->readLine();
    QVERIFY(htmlHeader.contains(QByteArray("<meta charset=\"UTF-8\"><title></title>")));
    QByteArray htmlBody = m_stdout->readAll();
    QVERIFY(htmlBody.contains(QByteArray("<h2>NewDatabase</h2>")));
    QVERIFY(htmlBody.contains(QByteArray(
        "<caption>Sample Entry</caption>"
        "<tr><th>User name</th><td class=\"username\">User Name</td></tr>"
        "<tr><th>Password</th><td class=\"password\">Password</td></tr>"
        "<tr><th>URL</th><td class=\"url\"><a href=\"http://www.somesite.com/\">http://www.somesite.com/</a></td></tr>")));
    // test invalid format
    setInput("a");
    execCmd(exportCmd, {"export", "-f", "yaml", m_dbFile->fileName()});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readLine(), QByteArray("Unsupported format yaml\n"));
}

void TestCli::testGenerate_data()
{
    QTest::addColumn<QStringList>("parameters");
    QTest::addColumn<QString>("pattern");

    QTest::newRow("default") << QStringList{"generate"} << "^[^\r\n]+$";
    QTest::newRow("length") << QStringList{"generate", "-L", "13"} << "^.{13}$";
    QTest::newRow("lowercase") << QStringList{"generate", "-L", "14", "-l"} << "^[a-z]{14}$";
    QTest::newRow("uppercase") << QStringList{"generate", "-L", "15", "--upper"} << "^[A-Z]{15}$";
    QTest::newRow("numbers") << QStringList{"generate", "-L", "16", "-n"} << "^[0-9]{16}$";
    QTest::newRow("special") << QStringList{"generate", "-L", "200", "-s"}
                             << R"(^[\(\)\[\]\{\}\.\-*|\\,:;"'\/\_!+-<=>?#$%&^`@~]{200}$)";
    QTest::newRow("special (exclude)") << QStringList{"generate", "-L", "200", "-s", "-x", "+.?@&"}
                                       << R"(^[\(\)\[\]\{\}\.\-*|\\,:;"'\/\_!-<=>#$%^`~]{200}$)";
    QTest::newRow("extended") << QStringList{"generate", "-L", "50", "-e"}
                              << R"(^[^a-zA-Z0-9\(\)\[\]\{\}\.\-\*\|\\,:;"'\/\_!+-<=>?#$%&^`@~]{50}$)";
    QTest::newRow("numbers + lowercase + uppercase")
        << QStringList{"generate", "-L", "16", "-n", "--upper", "-l"} << "^[0-9a-zA-Z]{16}$";
    QTest::newRow("numbers + lowercase + uppercase (exclude)")
        << QStringList{"generate", "-L", "500", "-n", "-U", "-l", "-x", "abcdefg0123@"} << "^[^abcdefg0123@]{500}$";
    QTest::newRow("numbers + lowercase + uppercase (exclude similar)")
        << QStringList{"generate", "-L", "200", "-n", "-U", "-l", "--exclude-similar"} << "^[^l1IO0]{200}$";
    QTest::newRow("uppercase + lowercase (every)")
        << QStringList{"generate", "-L", "2", "--upper", "-l", "--every-group"} << "^[a-z][A-Z]|[A-Z][a-z]$";
    QTest::newRow("numbers + lowercase (every)")
        << QStringList{"generate", "-L", "2", "-n", "-l", "--every-group"} << "^[a-z][0-9]|[0-9][a-z]$";
    QTest::newRow("custom character set")
        << QStringList{"generate", "-L", "200", "-n", "-c", "abc"} << "^[abc0-9]{200}$";
    QTest::newRow("custom character set without extra options uses only custom chars")
        << QStringList{"generate", "-L", "200", "-c", "a"} << "^a{200}$";
}

void TestCli::testGenerate()
{
    QFETCH(QStringList, parameters);
    QFETCH(QString, pattern);

    Generate generateCmd;
    QVERIFY(!generateCmd.name.isEmpty());
    QVERIFY(generateCmd.getDescriptionLine().contains(generateCmd.name));

    for (int i = 0; i < 10; ++i) {
        execCmd(generateCmd, parameters);
        QRegularExpression regex(pattern);
#ifdef Q_OS_UNIX
        QString password = QString::fromUtf8(m_stdout->readLine());
#else
        QString password = QString::fromLatin1(m_stdout->readLine());
#endif

        QVERIFY2(regex.match(password).hasMatch(),
                 qPrintable("Password " + password + " does not match pattern " + pattern));
        QCOMPARE(m_stderr->readAll(), QByteArray());
    }

    // Testing with invalid password length
    execCmd(generateCmd, {"generate", "-L", "-10"});
    QCOMPARE(m_stderr->readLine(), QByteArray("Invalid password length -10\n"));

    execCmd(generateCmd, {"generate", "-L", "0"});
    QCOMPARE(m_stderr->readLine(), QByteArray("Invalid password length 0\n"));

    // Testing with invalid word count format
    execCmd(generateCmd, {"generate", "-L", "bleuh"});
    QCOMPARE(m_stderr->readLine(), QByteArray("Invalid password length bleuh\n"));
}

void TestCli::testImport()
{
    Import importCmd;
    QVERIFY(!importCmd.name.isEmpty());
    QVERIFY(importCmd.getDescriptionLine().contains(importCmd.name));

    QScopedPointer<QTemporaryDir> testDir(new QTemporaryDir());
    QString databaseFilename = testDir->path() + "/testImport1.kdbx";

    setInput({"a", "a"});
    execCmd(importCmd, {"import", m_xmlFile->fileName(), databaseFilename, "-p"});

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Repeat password: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Successfully imported database.\n"));

    auto db = readDatabase(databaseFilename, "a");
    QVERIFY(db);
    auto* entry = db->rootGroup()->findEntryByPath("/Sample Entry 1");
    QVERIFY(entry);
    QCOMPARE(entry->username(), QString("User Name"));

    // Should refuse to create the database if it already exists.
    execCmd(importCmd, {"import", m_xmlFile->fileName(), databaseFilename});
    // Output should be empty when there is an error.
    QCOMPARE(m_stdout->readAll(), QByteArray());
    QString errorMessage = QString("File " + databaseFilename + " already exists.\n");
    QCOMPARE(m_stderr->readAll(), errorMessage.toUtf8());

    // Testing import with non-existing keyfile
    databaseFilename = testDir->path() + "/testImport2.kdbx";
    QString keyfilePath = testDir->path() + "/keyfile.txt";
    setInput({"a", "a"});
    execCmd(importCmd, {"import", "-p", "-k", keyfilePath, m_xmlFile->fileName(), databaseFilename});

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Repeat password: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Successfully imported database.\n"));

    db = readDatabase(databaseFilename, "a", keyfilePath);
    QVERIFY(db);

    // Testing import with existing keyfile
    databaseFilename = testDir->path() + "/testImport3.kdbx";
    setInput({"a", "a"});
    execCmd(importCmd, {"import", "-p", "-k", keyfilePath, m_xmlFile->fileName(), databaseFilename});

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Repeat password: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Successfully imported database.\n"));

    db = readDatabase(databaseFilename, "a", keyfilePath);
    QVERIFY(db);

    // Invalid decryption time (format).
    databaseFilename = testDir->path() + "/testCreate_time.kdbx";
    execCmd(importCmd, {"import", "-p", "-t", "NAN", m_xmlFile->fileName(), databaseFilename});

    QCOMPARE(m_stdout->readAll(), QByteArray());
    QCOMPARE(m_stderr->readAll(), QByteArray("Invalid decryption time NAN.\n"));

    // Invalid decryption time (range).
    execCmd(importCmd, {"import", "-p", "-t", "10", m_xmlFile->fileName(), databaseFilename});

    QCOMPARE(m_stdout->readAll(), QByteArray());
    QVERIFY(m_stderr->readAll().contains(QByteArray("Target decryption time must be between")));

    int encryptionTime = 500;
    // Custom encryption time
    setInput({"a", "a"});
    int epochBefore = QDateTime::currentMSecsSinceEpoch();
    execCmd(importCmd,
            {"import", "-p", "-t", QString::number(encryptionTime), m_xmlFile->fileName(), databaseFilename});
    // Removing 100ms to make sure we account for changes in computation time.
    QVERIFY(QDateTime::currentMSecsSinceEpoch() > (epochBefore + encryptionTime - 100));

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Repeat password: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray("Benchmarking key derivation function for 500ms delay.\n"));
    QVERIFY(m_stdout->readLine().contains(QByteArray("rounds for key derivation function.\n")));

    db = readDatabase(databaseFilename, "a");
    QVERIFY(db);

    // Quiet option
    QScopedPointer<QTemporaryDir> testDirQuiet(new QTemporaryDir());
    QString databaseFilenameQuiet = testDirQuiet->path() + "/testImport2.kdbx";

    setInput({"a", "a"});
    execCmd(importCmd, {"import", "-p", "-q", m_xmlFile->fileName(), databaseFilenameQuiet});

    QCOMPARE(m_stderr->readLine(), QByteArray("Enter password to encrypt database (optional): \n"));
    QCOMPARE(m_stderr->readLine(), QByteArray("Repeat password: \n"));
    QCOMPARE(m_stdout->readLine(), QByteArray());

    db = readDatabase(databaseFilenameQuiet, "a");
    QVERIFY(db);
}

void TestCli::testKeyFileOption()
{
    List listCmd;

    QString keyFilePath(QString(KEEPASSX_TEST_DATA_DIR).append("/KeyFileProtected.key"));
    setInput("a");
    execCmd(listCmd, {"ls", "-k", keyFilePath, m_keyFileProtectedDbFile->fileName()});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray("entry1\n"
                        "entry2\n"));

    // Should raise an error with no key file.
    setInput("a");
    execCmd(listCmd, {"ls", m_keyFileProtectedDbFile->fileName()});
    QCOMPARE(m_stdout->readAll(), QByteArray());
    QVERIFY(m_stderr->readAll().contains("Invalid credentials were provided"));

    // Should raise an error if key file path is invalid.
    setInput("a");
    execCmd(listCmd, {"ls", "-k", "invalidpath", m_keyFileProtectedDbFile->fileName()});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stdout->readAll(), QByteArray());
    QCOMPARE(m_stderr->readAll().split(':').at(0), QByteArray("Failed to load key file invalidpath"));
}

void TestCli::testNoPasswordOption()
{
    List listCmd;

    QString keyFilePath(QString(KEEPASSX_TEST_DATA_DIR).append("/KeyFileProtectedNoPassword.key"));
    execCmd(listCmd, {"ls", "-k", keyFilePath, "--no-password", m_keyFileProtectedNoPasswordDbFile->fileName()});
    // Expecting no password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray("entry1\n"
                        "entry2\n"));

    // Should raise an error with no key file.
    execCmd(listCmd, {"ls", "--no-password", m_keyFileProtectedNoPasswordDbFile->fileName()});
    QCOMPARE(m_stdout->readAll(), QByteArray());
    QVERIFY(m_stderr->readAll().contains("Invalid credentials were provided"));
}

void TestCli::testList()
{
    List listCmd;
    QVERIFY(!listCmd.name.isEmpty());
    QVERIFY(listCmd.getDescriptionLine().contains(listCmd.name));

    setInput("a");
    execCmd(listCmd, {"ls", m_dbFile->fileName()});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Sample Entry\n"
                        "General/\n"
                        "Windows/\n"
                        "Network/\n"
                        "Internet/\n"
                        "eMail/\n"
                        "Homebanking/\n"));

    // Quiet option
    setInput("a");
    execCmd(listCmd, {"ls", "-q", m_dbFile->fileName()});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Sample Entry\n"
                        "General/\n"
                        "Windows/\n"
                        "Network/\n"
                        "Internet/\n"
                        "eMail/\n"
                        "Homebanking/\n"));

    setInput("a");
    execCmd(listCmd, {"ls", "-R", m_dbFile->fileName()});
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Sample Entry\n"
                        "General/\n"
                        "  [empty]\n"
                        "Windows/\n"
                        "  [empty]\n"
                        "Network/\n"
                        "  [empty]\n"
                        "Internet/\n"
                        "  [empty]\n"
                        "eMail/\n"
                        "  [empty]\n"
                        "Homebanking/\n"
                        "  Subgroup/\n"
                        "    Subgroup Entry\n"));

    setInput("a");
    execCmd(listCmd, {"ls", "-R", "-f", m_dbFile->fileName()});
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Sample Entry\n"
                        "General/\n"
                        "General/[empty]\n"
                        "Windows/\n"
                        "Windows/[empty]\n"
                        "Network/\n"
                        "Network/[empty]\n"
                        "Internet/\n"
                        "Internet/[empty]\n"
                        "eMail/\n"
                        "eMail/[empty]\n"
                        "Homebanking/\n"
                        "Homebanking/Subgroup/\n"
                        "Homebanking/Subgroup/Subgroup Entry\n"));

    setInput("a");
    execCmd(listCmd, {"ls", "-R", "-f", m_dbFile->fileName(), "/Homebanking"});
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Subgroup/\n"
                        "Subgroup/Subgroup Entry\n"));

    setInput("a");
    execCmd(listCmd, {"ls", m_dbFile->fileName(), "/General/"});
    QCOMPARE(m_stdout->readAll(), QByteArray("[empty]\n"));

    setInput("a");
    execCmd(listCmd, {"ls", m_dbFile->fileName(), "/DoesNotExist/"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Cannot find group /DoesNotExist/.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());
}

void TestCli::testMerge()
{
    Merge mergeCmd;
    QVERIFY(!mergeCmd.name.isEmpty());
    QVERIFY(mergeCmd.getDescriptionLine().contains(mergeCmd.name));

    // load test database and save copies
    auto db = readDatabase();
    QVERIFY(db);
    TemporaryFile targetFile1;
    targetFile1.open();
    targetFile1.close();

    TemporaryFile targetFile2;
    targetFile2.open();
    targetFile2.close();

    TemporaryFile targetFile3;
    targetFile3.open();
    targetFile3.close();

    db->saveAs(targetFile1.fileName());
    db->saveAs(targetFile2.fileName());

    // save another copy with a different password
    auto oldKey = db->key();
    auto key = QSharedPointer<CompositeKey>::create();
    key->addKey(QSharedPointer<PasswordKey>::create("b"));
    db->setKey(key);
    db->saveAs(targetFile3.fileName());

    // Restore the original password
    db->setKey(oldKey);

    // then add a new entry to the in-memory database and save another copy
    auto* entry = new Entry();
    entry->setUuid(QUuid::createUuid());
    entry->setTitle("Some Website");
    entry->setPassword("secretsecretsecret");
    auto* group = db->rootGroup()->findGroupByPath("/Internet/");
    QVERIFY(group);
    group->addEntry(entry);

    TemporaryFile sourceFile;
    sourceFile.open();
    sourceFile.close();
    db->saveAs(sourceFile.fileName());

    setInput("a");
    execCmd(mergeCmd, {"merge", "-s", targetFile1.fileName(), sourceFile.fileName()});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QList<QByteArray> outLines1 = m_stdout->readAll().split('\n');
    QVERIFY(outLines1.at(0).contains("Overwriting Internet"));
    QVERIFY(outLines1.at(1).contains("Creating missing Some Website"));
    QCOMPARE(outLines1.at(2),
             QString("Successfully merged %1 into %2.").arg(sourceFile.fileName(), targetFile1.fileName()).toUtf8());

    auto mergedDb = QSharedPointer<Database>::create();
    QVERIFY(mergedDb->open(targetFile1.fileName(), oldKey));

    auto* entry1 = mergedDb->rootGroup()->findEntryByPath("/Internet/Some Website");
    QVERIFY(entry1);
    QCOMPARE(entry1->title(), QString("Some Website"));
    QCOMPARE(entry1->password(), QString("secretsecretsecret"));

    // the dry run option should not modify the target database.
    setInput("a");
    execCmd(mergeCmd, {"merge", "--dry-run", "-s", targetFile2.fileName(), sourceFile.fileName()});
    QList<QByteArray> outLines2 = m_stdout->readAll().split('\n');
    QVERIFY(outLines2.at(0).contains("Overwriting Internet"));
    QVERIFY(outLines2.at(1).contains("Creating missing Some Website"));
    QCOMPARE(outLines2.at(2), QByteArray("Database was not modified by merge operation."));

    mergedDb = QSharedPointer<Database>::create();
    QVERIFY(mergedDb->open(targetFile2.fileName(), oldKey));
    entry1 = mergedDb->rootGroup()->findEntryByPath("/Internet/Some Website");
    QVERIFY(!entry1);

    // the dry run option can be used with the quiet option
    setInput("a");
    execCmd(mergeCmd, {"merge", "--dry-run", "-s", "-q", targetFile2.fileName(), sourceFile.fileName()});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray());

    mergedDb = QSharedPointer<Database>::create();
    QVERIFY(mergedDb->open(targetFile2.fileName(), oldKey));
    entry1 = mergedDb->rootGroup()->findEntryByPath("/Internet/Some Website");
    QVERIFY(!entry1);

    // try again with different passwords for both files
    setInput({"b", "a"});
    execCmd(mergeCmd, {"merge", targetFile3.fileName(), sourceFile.fileName()});
    QList<QByteArray> outLines3 = m_stdout->readAll().split('\n');
    QCOMPARE(outLines3.at(2),
             QString("Successfully merged %1 into %2.").arg(sourceFile.fileName(), targetFile3.fileName()).toUtf8());

    mergedDb = QSharedPointer<Database>::create();
    QVERIFY(mergedDb->open(targetFile3.fileName(), key));

    entry1 = mergedDb->rootGroup()->findEntryByPath("/Internet/Some Website");
    QVERIFY(entry1);
    QCOMPARE(entry1->title(), QString("Some Website"));
    QCOMPARE(entry1->password(), QString("secretsecretsecret"));

    // making sure that the message is different if the database was not
    // modified by the merge operation.
    setInput("a");
    execCmd(mergeCmd, {"merge", "-s", sourceFile.fileName(), sourceFile.fileName()});
    QCOMPARE(m_stdout->readAll(), QByteArray("Database was not modified by merge operation.\n"));

    // Quiet option
    setInput("a");
    execCmd(mergeCmd, {"merge", "-q", "-s", sourceFile.fileName(), sourceFile.fileName()});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Quiet option without the -s option
    setInput({"a", "a"});
    execCmd(mergeCmd, {"merge", "-q", sourceFile.fileName(), sourceFile.fileName()});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray());
}

void TestCli::testMergeWithKeys()
{
    DatabaseCreate createCmd;
    QVERIFY(!createCmd.name.isEmpty());
    QVERIFY(createCmd.getDescriptionLine().contains(createCmd.name));

    Merge mergeCmd;
    QVERIFY(!mergeCmd.name.isEmpty());
    QVERIFY(mergeCmd.getDescriptionLine().contains(mergeCmd.name));

    QScopedPointer<QTemporaryDir> testDir(new QTemporaryDir());

    QString sourceDatabaseFilename = testDir->path() + "/testSourceDatabase.kdbx";
    QString sourceKeyfilePath = testDir->path() + "/testSourceKeyfile.txt";

    QString targetDatabaseFilename = testDir->path() + "/testTargetDatabase.kdbx";
    QString targetKeyfilePath = testDir->path() + "/testTargetKeyfile.txt";

    setInput({"a", "a"});
    execCmd(createCmd, {"db-create", sourceDatabaseFilename, "-p", "-k", sourceKeyfilePath});

    setInput({"b", "b"});
    execCmd(createCmd, {"db-create", targetDatabaseFilename, "-p", "-k", targetKeyfilePath});

    auto sourceDatabase = readDatabase(sourceDatabaseFilename, "a", sourceKeyfilePath);
    QVERIFY(sourceDatabase);

    auto targetDatabase = readDatabase(targetDatabaseFilename, "b", targetKeyfilePath);
    QVERIFY(targetDatabase);

    auto* rootGroup = new Group();
    rootGroup->setName("root");
    rootGroup->setUuid(QUuid::createUuid());
    auto* group = new Group();
    group->setUuid(QUuid::createUuid());
    group->setParent(rootGroup);
    group->setName("Internet");

    auto* entry = new Entry();
    entry->setUuid(QUuid::createUuid());
    entry->setTitle("Some Website");
    entry->setPassword("secretsecretsecret");
    group->addEntry(entry);

    auto oldGroup = sourceDatabase->setRootGroup(rootGroup);
    delete oldGroup;

    auto* otherRootGroup = new Group();
    otherRootGroup->setName("root");
    otherRootGroup->setUuid(QUuid::createUuid());
    auto* otherGroup = new Group();
    otherGroup->setUuid(QUuid::createUuid());
    otherGroup->setParent(otherRootGroup);
    otherGroup->setName("Internet");

    auto* otherEntry = new Entry();
    otherEntry->setUuid(QUuid::createUuid());
    otherEntry->setTitle("Some Website 2");
    otherEntry->setPassword("secretsecretsecret 2");
    otherGroup->addEntry(otherEntry);

    oldGroup = targetDatabase->setRootGroup(otherRootGroup);
    delete oldGroup;

    sourceDatabase->saveAs(sourceDatabaseFilename);
    targetDatabase->saveAs(targetDatabaseFilename);

    setInput({"b", "a"});
    execCmd(mergeCmd,
            {"merge",
             "-k",
             targetKeyfilePath,
             "--key-file-from",
             sourceKeyfilePath,
             targetDatabaseFilename,
             sourceDatabaseFilename});

    QList<QByteArray> lines = m_stdout->readAll().split('\n');
    QVERIFY(lines.contains(
        QString("Successfully merged %1 into %2.").arg(sourceDatabaseFilename, targetDatabaseFilename).toUtf8()));
}

void TestCli::testMove()
{
    Move moveCmd;
    QVERIFY(!moveCmd.name.isEmpty());
    QVERIFY(moveCmd.getDescriptionLine().contains(moveCmd.name));

    setInput("a");
    execCmd(moveCmd, {"mv", m_dbFile->fileName(), "invalid_entry_path", "invalid_group_path"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readLine(), QByteArray("Could not find entry with path invalid_entry_path.\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray());

    setInput("a");
    execCmd(moveCmd, {"mv", m_dbFile->fileName(), "Sample Entry", "invalid_group_path"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readLine(), QByteArray("Could not find group with path invalid_group_path.\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray());

    setInput("a");
    execCmd(moveCmd, {"mv", m_dbFile->fileName(), "Sample Entry", "General/"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readLine(), QByteArray());
    QCOMPARE(m_stdout->readLine(), QByteArray("Successfully moved entry Sample Entry to group General/.\n"));

    auto db = readDatabase();
    auto* entry = db->rootGroup()->findEntryByPath("General/Sample Entry");
    QVERIFY(entry);

    // Test that not modified if the same group is destination.
    setInput("a");
    execCmd(moveCmd, {"mv", m_dbFile->fileName(), "General/Sample Entry", "General/"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readLine(), QByteArray("Entry is already in group General/.\n"));
    QCOMPARE(m_stdout->readLine(), QByteArray());

    // sanity check
    db = readDatabase();
    entry = db->rootGroup()->findEntryByPath("General/Sample Entry");
    QVERIFY(entry);
}

void TestCli::testRemove()
{
    Remove removeCmd;
    QVERIFY(!removeCmd.name.isEmpty());
    QVERIFY(removeCmd.getDescriptionLine().contains(removeCmd.name));

    // load test database and save a copy with disabled recycle bin
    auto db = readDatabase();
    QVERIFY(db);
    TemporaryFile fileCopy;
    fileCopy.open();
    fileCopy.close();

    db->metadata()->setRecycleBinEnabled(false);
    db->saveAs(fileCopy.fileName());

    // delete entry and verify
    setInput("a");
    execCmd(removeCmd, {"rm", m_dbFile->fileName(), "/Sample Entry"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully recycled entry Sample Entry.\n"));

    auto readBackDb = readDatabase();
    QVERIFY(readBackDb);
    QVERIFY(!readBackDb->rootGroup()->findEntryByPath("/Sample Entry"));
    QVERIFY(readBackDb->rootGroup()->findEntryByPath(QString("/%1/Sample Entry").arg(Group::tr("Recycle Bin"))));

    // try again, this time without recycle bin
    setInput("a");
    execCmd(removeCmd, {"rm", fileCopy.fileName(), "/Sample Entry"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully deleted entry Sample Entry.\n"));

    readBackDb = readDatabase(fileCopy.fileName(), "a");
    QVERIFY(readBackDb);
    QVERIFY(!readBackDb->rootGroup()->findEntryByPath("/Sample Entry"));
    QVERIFY(!readBackDb->rootGroup()->findEntryByPath(QString("/%1/Sample Entry").arg(Group::tr("Recycle Bin"))));

    // finally, try deleting a non-existent entry
    setInput("a");
    execCmd(removeCmd, {"rm", fileCopy.fileName(), "/Sample Entry"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Entry /Sample Entry not found.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // try deleting a directory, should fail
    setInput("a");
    execCmd(removeCmd, {"rm", fileCopy.fileName(), "/General"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Entry /General not found.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());
}

void TestCli::testRemoveGroup()
{
    RemoveGroup removeGroupCmd;
    QVERIFY(!removeGroupCmd.name.isEmpty());
    QVERIFY(removeGroupCmd.getDescriptionLine().contains(removeGroupCmd.name));

    // try deleting a directory, should recycle it first.
    setInput("a");
    execCmd(removeGroupCmd, {"rmdir", m_dbFile->fileName(), "/General"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully recycled group /General.\n"));

    auto db = readDatabase();
    auto* group = db->rootGroup()->findGroupByPath("General");
    QVERIFY(!group);

    // try deleting a directory again, should delete it permanently.
    setInput("a");
    execCmd(removeGroupCmd, {"rmdir", m_dbFile->fileName(), "Recycle Bin/General"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray("Successfully deleted group Recycle Bin/General.\n"));

    db = readDatabase();
    group = db->rootGroup()->findGroupByPath("Recycle Bin/General");
    QVERIFY(!group);

    // try deleting an invalid group, should fail.
    setInput("a");
    execCmd(removeGroupCmd, {"rmdir", m_dbFile->fileName(), "invalid"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Group invalid not found.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Should fail to remove the root group.
    setInput("a");
    execCmd(removeGroupCmd, {"rmdir", m_dbFile->fileName(), "/"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("Cannot remove root group from database.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());
}

void TestCli::testRemoveQuiet()
{
    Remove removeCmd;
    QVERIFY(!removeCmd.name.isEmpty());
    QVERIFY(removeCmd.getDescriptionLine().contains(removeCmd.name));

    // delete entry and verify
    setInput("a");
    execCmd(removeCmd, {"rm", "-q", m_dbFile->fileName(), "/Sample Entry"});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray());

    auto db = readDatabase();
    QVERIFY(db);

    QVERIFY(!db->rootGroup()->findEntryByPath("/Sample Entry"));
    QVERIFY(db->rootGroup()->findEntryByPath(QString("/%1/Sample Entry").arg(Group::tr("Recycle Bin"))));

    // remove the entry completely
    setInput("a");
    execCmd(removeCmd, {"rm", "-q", m_dbFile->fileName(), QString("/%1/Sample Entry").arg(Group::tr("Recycle Bin"))});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray());

    db = readDatabase();
    QVERIFY(!db->rootGroup()->findEntryByPath("/Sample Entry"));
    QVERIFY(!db->rootGroup()->findEntryByPath(QString("/%1/Sample Entry").arg(Group::tr("Recycle Bin"))));
}

void TestCli::testSearch()
{
    Search searchCmd;
    QVERIFY(!searchCmd.name.isEmpty());
    QVERIFY(searchCmd.getDescriptionLine().contains(searchCmd.name));

    setInput("a");
    execCmd(searchCmd, {"search", m_dbFile->fileName(), "Sample"});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray("/Sample Entry\n"));

    // Quiet option
    setInput("a");
    execCmd(searchCmd, {"search", m_dbFile->fileName(), "-q", "Sample"});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(), QByteArray("/Sample Entry\n"));

    setInput("a");
    execCmd(searchCmd, {"search", m_dbFile->fileName(), "Does Not Exist"});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray("No results for that search term.\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // write a modified database
    auto db = readDatabase();
    QVERIFY(db);
    auto* group = db->rootGroup()->findGroupByPath("/General/");
    QVERIFY(group);
    auto* entry = new Entry();
    entry->setUuid(QUuid::createUuid());
    entry->setTitle("New Entry");
    group->addEntry(entry);

    TemporaryFile tmpFile;
    tmpFile.open();
    tmpFile.close();
    db->saveAs(tmpFile.fileName());

    setInput("a");
    execCmd(searchCmd, {"search", tmpFile.fileName(), "title:New"});
    QCOMPARE(m_stdout->readAll(), QByteArray("/General/New Entry\n"));

    setInput("a");
    execCmd(searchCmd, {"search", tmpFile.fileName(), "title:Entry"});
    QCOMPARE(m_stdout->readAll(),
             QByteArray("/Sample Entry\n/General/New Entry\n/Homebanking/Subgroup/Subgroup Entry\n"));

    setInput("a");
    execCmd(searchCmd, {"search", tmpFile.fileName(), "group:General"});
    QCOMPARE(m_stdout->readAll(), QByteArray("/General/New Entry\n"));

    setInput("a");
    execCmd(searchCmd, {"search", tmpFile.fileName(), "group:NewDatabase"});
    QCOMPARE(m_stdout->readAll(), QByteArray("/Sample Entry\n"));

    setInput("a");
    execCmd(searchCmd, {"search", tmpFile.fileName(), "group:/NewDatabase"});
    QCOMPARE(m_stdout->readAll(),
             QByteArray("/Sample Entry\n/General/New Entry\n/Homebanking/Subgroup/Subgroup Entry\n"));

    setInput("a");
    execCmd(searchCmd, {"search", tmpFile.fileName(), "url:bank"});
    QCOMPARE(m_stdout->readAll(), QByteArray("/Homebanking/Subgroup/Subgroup Entry\n"));

    setInput("a");
    execCmd(searchCmd, {"search", tmpFile.fileName(), "u:User Name"});
    QCOMPARE(m_stdout->readAll(), QByteArray("/Sample Entry\n/Homebanking/Subgroup/Subgroup Entry\n"));
}

void TestCli::testShow()
{
    Show showCmd;
    QVERIFY(!showCmd.name.isEmpty());
    QVERIFY(showCmd.getDescriptionLine().contains(showCmd.name));

    setInput("a");
    execCmd(showCmd, {"show", m_dbFile->fileName(), "/Sample Entry"});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Title: Sample Entry\n"
                        "UserName: User Name\n"
                        "Password: PROTECTED\n"
                        "URL: http://www.somesite.com/\n"
                        "Notes: Notes\n"
                        "Uuid: {9f4544c2-ab00-c74a-8a1a-6eaf26cf57e9}\n"
                        "Tags: \n"));

    setInput("a");
    execCmd(showCmd, {"show", "-s", m_dbFile->fileName(), "/Sample Entry"});
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Title: Sample Entry\n"
                        "UserName: User Name\n"
                        "Password: Password\n"
                        "URL: http://www.somesite.com/\n"
                        "Notes: Notes\n"
                        "Uuid: {9f4544c2-ab00-c74a-8a1a-6eaf26cf57e9}\n"
                        "Tags: \n"));

    setInput("a");
    execCmd(showCmd, {"show", m_dbFile->fileName(), "-q", "/Sample Entry"});
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Title: Sample Entry\n"
                        "UserName: User Name\n"
                        "Password: PROTECTED\n"
                        "URL: http://www.somesite.com/\n"
                        "Notes: Notes\n"
                        "Uuid: {9f4544c2-ab00-c74a-8a1a-6eaf26cf57e9}\n"
                        "Tags: \n"));

    setInput("a");
    execCmd(showCmd, {"show", m_dbFile->fileName(), "--show-attachments", "/Sample Entry"});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Title: Sample Entry\n"
                        "UserName: User Name\n"
                        "Password: PROTECTED\n"
                        "URL: http://www.somesite.com/\n"
                        "Notes: Notes\n"
                        "Uuid: {9f4544c2-ab00-c74a-8a1a-6eaf26cf57e9}\n"
                        "Tags: \n"
                        "\n"
                        "Attachments:\n"
                        "  Sample attachment.txt (15 B)\n"));

    setInput("a");
    execCmd(showCmd, {"show", m_dbFile->fileName(), "--show-attachments", "/Homebanking/Subgroup/Subgroup Entry"});
    m_stderr->readLine(); // Skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Title: Subgroup Entry\n"
                        "UserName: Bank User Name\n"
                        "Password: PROTECTED\n"
                        "URL: https://www.bank.com\n"
                        "Notes: Important note\n"
                        "Uuid: {20b183fd-6878-4506-a50b-06d30792aa10}\n"
                        "Tags: \n"
                        "\n"
                        "No attachments present.\n"));

    setInput("a");
    execCmd(showCmd, {"show", "-a", "Title", m_dbFile->fileName(), "/Sample Entry"});
    QCOMPARE(m_stdout->readAll(), QByteArray("Sample Entry\n"));

    setInput("a");
    execCmd(showCmd, {"show", "-a", "Password", m_dbFile->fileName(), "/Sample Entry"});
    QCOMPARE(m_stdout->readAll(), QByteArray("Password\n"));

    setInput("a");
    execCmd(showCmd, {"show", "-a", "Uuid", m_dbFile->fileName(), "/Sample Entry"});
    QCOMPARE(m_stdout->readAll(), QByteArray("{9f4544c2-ab00-c74a-8a1a-6eaf26cf57e9}\n"));

    setInput("a");
    execCmd(showCmd, {"show", "-a", "Title", "-a", "URL", m_dbFile->fileName(), "/Sample Entry"});
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Sample Entry\n"
                        "http://www.somesite.com/\n"));

    // Test case insensitivity
    setInput("a");
    execCmd(showCmd, {"show", "-a", "TITLE", "-a", "URL", m_dbFile->fileName(), "/Sample Entry"});
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Sample Entry\n"
                        "http://www.somesite.com/\n"));

    setInput("a");
    execCmd(showCmd, {"show", "-a", "DoesNotExist", m_dbFile->fileName(), "/Sample Entry"});
    QCOMPARE(m_stdout->readAll(), QByteArray());
    QVERIFY(m_stderr->readAll().contains("ERROR: unknown attribute DoesNotExist.\n"));

    setInput("a");
    execCmd(showCmd, {"show", "-t", m_dbFile->fileName(), "/Sample Entry"});
    QVERIFY(isTotp(m_stdout->readAll()));

    setInput("a");
    execCmd(showCmd, {"show", "-a", "Title", m_dbFile->fileName(), "--totp", "/Sample Entry"});
    QCOMPARE(m_stdout->readLine(), QByteArray("Sample Entry\n"));
    QVERIFY(isTotp(m_stdout->readAll()));

    setInput("a");
    execCmd(showCmd, {"show", m_dbFile2->fileName(), "--totp", "/Sample Entry"});
    QCOMPARE(m_stdout->readAll(), QByteArray());
    QVERIFY(m_stderr->readAll().contains("Entry with path /Sample Entry has no TOTP set up.\n"));

    // Show with ambiguous attributes
    setInput("a");
    execCmd(showCmd, {"show", m_dbFile->fileName(), "-a", "Testattribute1", "/Sample Entry"});
    QCOMPARE(m_stdout->readAll(), QByteArray());
    QVERIFY(m_stderr->readAll().contains("ERROR: attribute Testattribute1 is ambiguous"));

    setInput("a");
    execCmd(showCmd, {"show", "--all", m_dbFile->fileName(), "/Sample Entry"});
    QCOMPARE(m_stdout->readAll(),
             QByteArray("Title: Sample Entry\n"
                        "UserName: User Name\n"
                        "Password: PROTECTED\n"
                        "URL: http://www.somesite.com/\n"
                        "Notes: Notes\n"
                        "Uuid: {9f4544c2-ab00-c74a-8a1a-6eaf26cf57e9}\n"
                        "Tags: \n"
                        "TOTP Seed: PROTECTED\n"
                        "TOTP Settings: 30;6\n"
                        "TestAttribute1: b\n"
                        "testattribute1: a\n"));
}

void TestCli::testInvalidDbFiles()
{
    Show showCmd;
    QString nonExistentDbPath("/foo/bar/baz");
    QString directoryName("/");

    execCmd(showCmd, {"show", nonExistentDbPath, "/Sample Entry"});
    QCOMPARE(QString(m_stderr->readAll()),
             QObject::tr("Failed to open database file %1: not found").arg(nonExistentDbPath) + "\n");
    QCOMPARE(m_stdout->readAll(), QByteArray());

    execCmd(showCmd, {"show", directoryName, "whatever"});
    QCOMPARE(QString(m_stderr->readAll()),
             QObject::tr("Failed to open database file %1: not a plain file").arg(directoryName) + "\n");

    // Create a write-only file and try to open it.
    // QFileInfo.isReadable returns 'true' on Windows, even after the call to
    // setPermissions(WriteOwner) and with NTFS permissions enabled, so this
    // check doesn't work.
#if !defined(Q_OS_WIN)
    QTemporaryFile tempFile;
    QVERIFY(tempFile.open());
    QString path = QFileInfo(tempFile).absoluteFilePath();
    QVERIFY(tempFile.setPermissions(QFileDevice::WriteOwner));
    execCmd(showCmd, {"show", path, "some entry"});
    QCOMPARE(QString(m_stderr->readAll()),
             QObject::tr("Failed to open database file %1: not readable").arg(path) + "\n");
#endif // Q_OS_WIN
}

/**
 * Secret key for the YubiKey slot used by the unit test is
 * 1c e3 0f d7 8d 20 dc fa 40 b5 0c 18 77 9a fb 0f 02 28 8d b7
 * This secret can be on either slot but must be passive.
 */
void TestCli::testYubiKeyOption()
{
    if (!YubiKey::instance()->isInitialized()) {
        QSKIP("Unable to initialize YubiKey interface.");
    }

    YubiKey::instance()->findValidKeys();

    const auto keys = YubiKey::instance()->foundKeys().keys();
    if (keys.isEmpty()) {
        QSKIP("No YubiKey devices were detected.");
    }

    bool wouldBlock = false;
    QByteArray challenge("CLITest");
    Botan::secure_vector<char> response;
    QByteArray expected("\xA2\x3B\x94\x00\xBE\x47\x9A\x30\xA9\xEB\x50\x9B\x85\x56\x5B\x6B\x30\x25\xB4\x8E", 20);

    // Find a key that as configured for this test
    YubiKeySlot pKey(0, 0);
    for (auto key : keys) {
        if (YubiKey::instance()->testChallenge(key, &wouldBlock) && !wouldBlock) {
            YubiKey::instance()->challenge(key, challenge, response);
            if (std::memcmp(response.data(), expected.data(), expected.size()) == 0) {
                pKey = key;
                break;
            }
            Tools::wait(100);
        }
    }

    if (pKey.first == 0 && pKey.second == 0) {
        QSKIP("No YubiKey is properly configured to perform this test.");
    }

    List listCmd;
    Add addCmd;

    setInput("a");
    execCmd(listCmd,
            {"ls",
             "-y",
             QString("%1:%2").arg(QString::number(pKey.second), QString::number(pKey.first)),
             m_yubiKeyProtectedDbFile->fileName()});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll(), QByteArray());
    QCOMPARE(m_stdout->readAll(),
             QByteArray("entry1\n"
                        "entry2\n"));

    // Should raise an error with no yubikey slot.
    setInput("a");
    execCmd(listCmd, {"ls", m_yubiKeyProtectedDbFile->fileName()});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readLine(),
             QByteArray("Error while reading the database: Invalid credentials were provided, please try again.\n"));
    QCOMPARE(m_stderr->readLine(),
             QByteArray("If this reoccurs, then your database file may be corrupt. (HMAC mismatch)\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Should raise an error if yubikey slot is not a string
    setInput("a");
    execCmd(listCmd, {"ls", "-y", "invalidslot", m_yubiKeyProtectedDbFile->fileName()});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll().split(':').at(0), QByteArray("Invalid YubiKey slot invalidslot\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());

    // Should raise an error if yubikey slot is invalid.
    setInput("a");
    execCmd(listCmd, {"ls", "-y", "3", m_yubiKeyProtectedDbFile->fileName()});
    m_stderr->readLine(); // skip password prompt
    QCOMPARE(m_stderr->readAll().split(':').at(0), QByteArray("Invalid YubiKey slot 3\n"));
    QCOMPARE(m_stdout->readAll(), QByteArray());
}

void TestCli::testNonAscii()
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(
        KEEPASSX_CLI_PATH,
        QStringList(
            {"show", "-a", "password", m_nonAsciiDbFile->fileName(), QString::fromUtf8("\xe7\xa7\x98\xe5\xaf\x86")}));
    process.waitForStarted();
    QCOMPARE(process.state(), QProcess::ProcessState::Running);

    // Write password.
    process.write("\xce\x94\xc3\xb6\xd8\xb6\n");
    process.closeWriteChannel();

    process.waitForFinished();

    process.readLine(); // skip password prompt
    QByteArray password = process.readLine();
    QCOMPARE(QString::fromUtf8(password).trimmed(),
             QString::fromUtf8("\xf0\x9f\x9a\x97\xf0\x9f\x90\x8e\xf0\x9f\x94\x8b\xf0\x9f\x93\x8e"));
}

void TestCli::testCommandParsing_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QStringList>("expectedOutput");

    QTest::newRow("basic") << "hello world" << QStringList({"hello", "world"});
    QTest::newRow("basic escaping") << "hello\\ world" << QStringList({"hello world"});
    QTest::newRow("quoted string") << "\"hello world\"" << QStringList({"hello world"});
    QTest::newRow("multiple params") << "show Passwords/Internet" << QStringList({"show", "Passwords/Internet"});
    QTest::newRow("quoted string inside param")
        << R"(ls foo\ bar\ baz"quoted")" << QStringList({"ls", "foo bar baz\"quoted\""});
    QTest::newRow("multiple whitespace") << "hello    world" << QStringList({"hello", "world"});
    QTest::newRow("single slash char") << "\\" << QStringList({"\\"});
    QTest::newRow("double backslash entry name") << "show foo\\\\\\\\bar" << QStringList({"show", "foo\\\\bar"});
}

void TestCli::testCommandParsing()
{
    QFETCH(QString, input);
    QFETCH(QStringList, expectedOutput);

    QStringList result = Utils::splitCommandString(input);
    QCOMPARE(result.size(), expectedOutput.size());
    for (int i = 0; i < expectedOutput.size(); ++i) {
        QCOMPARE(result[i], expectedOutput[i]);
    }
}

void TestCli::testOpen()
{
    Open openCmd;

    setInput("a");
    execCmd(openCmd, {"open", m_dbFile->fileName()});
    QVERIFY(openCmd.currentDatabase);

    List listCmd;
    // Set a current database, simulating interactive mode.
    listCmd.currentDatabase = openCmd.currentDatabase;
    execCmd(listCmd, {"ls"});
    QByteArray expectedOutput("Sample Entry\n"
                              "General/\n"
                              "Windows/\n"
                              "Network/\n"
                              "Internet/\n"
                              "eMail/\n"
                              "Homebanking/\n");
    QByteArray actualOutput = m_stdout->readAll();
    actualOutput.truncate(expectedOutput.length());
    QCOMPARE(actualOutput, expectedOutput);
}

void TestCli::testHelp()
{
    Help helpCmd;
    Commands::setupCommands(false);

    execCmd(helpCmd, {"help"});
    QVERIFY(m_stdout->readAll().contains("Available commands"));

    List listCmd;
    execCmd(helpCmd, {"help", "ls"});
    QVERIFY(m_stdout->readAll().contains(listCmd.description.toLatin1()));
}
