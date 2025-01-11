/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2016 Enrico Mariotti <enricomariotti@yahoo.it>
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

#include "CsvParser.h"

#include <QFile>

#include "core/Tools.h"

CsvParser::CsvParser()
    : m_comment('#')
    , m_isBackslashSyntax(false)
    , m_isFileLoaded(false)
    , m_qualifier('"')
    , m_separator(',')
{
    reset();
    m_csv.setBuffer(&m_array);
    m_ts.setDevice(&m_csv);
    m_csv.open(QIODevice::ReadOnly);
}

CsvParser::~CsvParser()
{
    m_csv.close();
}

bool CsvParser::isFileLoaded()
{
    return m_isFileLoaded;
}

bool CsvParser::reparse()
{
    reset();
    return parseFile();
}

bool CsvParser::parse(QFile* device)
{
    clear();
    if (!device) {
        appendStatusMsg(QObject::tr("NULL device"), true);
        return false;
    }
    if (!readFile(device)) {
        return false;
    }
    return parseFile();
}

bool CsvParser::readFile(QFile* device)
{
    if (device->isOpen()) {
        device->close();
    }

    device->open(QIODevice::ReadOnly);
    if (!Tools::readAllFromDevice(device, m_array)) {
        appendStatusMsg(QObject::tr("error reading from device"), true);
        m_isFileLoaded = false;
    } else {
        device->close();

        m_array.replace("\r\n", "\n");
        m_array.replace("\r", "\n");
        if (m_array.isEmpty()) {
            appendStatusMsg(QObject::tr("file empty").append("\n"));
        }
        m_isFileLoaded = true;
    }
    return m_isFileLoaded;
}

void CsvParser::reset()
{
    m_ch = QChar(0);
    m_currCol = 1;
    m_currRow = 1;
    m_isEof = false;
    m_isGood = true;
    m_lastPos = -1;
    m_maxCols = 0;
    m_statusMsg.clear();
    m_ts.seek(0);
    m_table.clear();
    // the following can be overridden by the user
    // m_comment = '#';
    // m_backslashSyntax = false;
    // m_comment = '#';
    // m_qualifier = '"';
    // m_separator = ',';
}

void CsvParser::clear()
{
    reset();
    m_isFileLoaded = false;
    m_array.clear();
}

bool CsvParser::parseFile()
{
    parseRecord();
    while (!m_isEof) {
        if (!skipEndline()) {
            appendStatusMsg(QObject::tr("malformed string"), true);
        }
        m_currRow++;
        m_currCol = 1;
        parseRecord();
    }
    fillColumns();
    return m_isGood;
}

void CsvParser::parseRecord()
{
    CsvRow row;
    if (isComment()) {
        skipLine();
        return;
    }
    do {
        parseField(row);
        getChar(m_ch);
    } while (m_ch == m_separator && !m_isEof);

    if (!m_isEof) {
        ungetChar();
    }
    if (isEmptyRow(row)) {
        row.clear();
        return;
    }
    m_table.push_back(row);
    if (m_maxCols < row.size()) {
        m_maxCols = row.size();
    }
    m_currCol++;
}

void CsvParser::parseField(CsvRow& row)
{
    QString field;
    peek(m_ch);
    if (m_ch != m_separator && m_ch != '\n' && m_ch != '\r') {
        if (isQualifier(m_ch)) {
            parseQuoted(field);
        } else {
            parseSimple(field);
        }
    }
    row.push_back(field);
}

void CsvParser::parseSimple(QString& s)
{
    QChar c;
    getChar(c);
    while (c != '\n' && c != m_separator && !m_isEof) {
        s.append(c);
        getChar(c);
    }
    if (!m_isEof) {
        ungetChar();
    }
}

void CsvParser::parseQuoted(QString& s)
{
    // read and discard initial qualifier (e.g. quote)
    getChar(m_ch);
    parseEscaped(s);
    if (!isQualifier(m_ch)) {
        appendStatusMsg(QObject::tr("missing closing quote"), true);
    }
}

void CsvParser::parseEscaped(QString& s)
{
    parseEscapedText(s);
    while (processEscapeMark(s, m_ch)) {
        parseEscapedText(s);
    }
    if (!m_isEof) {
        ungetChar();
    }
}

void CsvParser::parseEscapedText(QString& s)
{
    getChar(m_ch);
    while (!isQualifier(m_ch) && !m_isEof) {
        s.append(m_ch);
        getChar(m_ch);
    }
}

bool CsvParser::processEscapeMark(QString& s, QChar c)
{
    QChar c2;
    peek(c2);
    if (m_isBackslashSyntax) {
        // escape-character syntax, e.g. \"
        if (c != '\\') {
            return false;
        }
        // consume (and append) second qualifier
        getChar(c2);
        if (m_isEof) {
            c2 = '\\';
            s.append('\\');
            return false;
        }
        s.append(c2);
        return true;
    }

    // double quote syntax, e.g. ""
    if (!isQualifier(c)) {
        return false;
    }
    peek(c2);
    if (!m_isEof) { // not EOF, can read one char
        if (isQualifier(c2)) {
            s.append(c2);
            getChar(c2);
            return true;
        }
    }
    return false;
}

void CsvParser::fillColumns()
{
    // fill shorter rows with empty placeholder columns
    for (int i = 0; i < m_table.size(); ++i) {
        int gap = m_maxCols - m_table.at(i).size();
        if (gap > 0) {
            CsvRow r = m_table.at(i);
            for (int j = 0; j < gap; ++j) {
                r.append(QString(""));
            }
            m_table.replace(i, r);
        }
    }
}

void CsvParser::skipLine()
{
    m_ts.readLine();
    m_ts.seek(m_ts.pos() - 1);
}

bool CsvParser::skipEndline()
{
    getChar(m_ch);
    return m_ch == '\n';
}

void CsvParser::getChar(QChar& c)
{
    m_isEof = m_ts.atEnd();
    if (!m_isEof) {
        m_lastPos = m_ts.pos();
        m_ts >> c;
    }
}

void CsvParser::ungetChar()
{
    if (!m_ts.seek(m_lastPos)) {
        qWarning("CSV Parser: unget lower bound exceeded");
        m_isGood = false;
    }
}

void CsvParser::peek(QChar& c)
{
    getChar(c);
    if (!m_isEof) {
        ungetChar();
    }
}

bool CsvParser::isQualifier(const QChar& c) const
{
    if (m_isBackslashSyntax && c != m_qualifier) {
        return c == '\\';
    }
    return c == m_qualifier;
}

bool CsvParser::isComment()
{
    bool result = false;
    QChar c2;
    qint64 pos = m_ts.pos();

    do {
        getChar(c2);
    } while ((c2 == ' ' || c2 == '\t') && !m_isEof);

    if (c2 == m_comment) {
        result = true;
    }
    m_ts.seek(pos);
    return result;
}

bool CsvParser::isEmptyRow(const CsvRow& row) const
{
    for (auto it = row.constBegin(); it != row.constEnd(); ++it) {
        if (*it != "\n" && *it != "") {
            return false;
        }
    }
    return true;
}

void CsvParser::setBackslashSyntax(bool set)
{
    m_isBackslashSyntax = set;
}

void CsvParser::setComment(const QChar& c)
{
    m_comment = c.unicode();
}

void CsvParser::setCodec(const QString& s)
{
    if (s.toLocal8Bit().compare(QByteArray("UTF-8"), Qt::CaseInsensitive) == 0) {
        m_ts.setEncoding(QStringConverter::Utf8);
    } else if (s.toLocal8Bit().compare(QByteArray("Windows-1252"), Qt::CaseInsensitive) == 0) {
        m_ts.setEncoding(QStringConverter::System);
    } else if (s.toLocal8Bit().compare(QByteArray("UTF-16"), Qt::CaseInsensitive) == 0) {
        m_ts.setEncoding(QStringConverter::Utf16);
    } else if (s.toLocal8Bit().compare(QByteArray("UTF-16LE"), Qt::CaseInsensitive) == 0) {
        m_ts.setEncoding(QStringConverter::Utf16LE);
    }
}

void CsvParser::setFieldSeparator(const QChar& c)
{
    m_separator = c.unicode();
}

void CsvParser::setTextQualifier(const QChar& c)
{
    m_qualifier = c.unicode();
}

int CsvParser::getFileSize() const
{
    return m_csv.size();
}

CsvTable CsvParser::getCsvTable() const
{
    return m_table;
}

QString CsvParser::getStatus() const
{
    return m_statusMsg;
}

int CsvParser::getCsvCols() const
{
    if (!m_table.isEmpty() && !m_table.at(0).isEmpty()) {
        return m_table.at(0).size();
    }
    return 0;
}

int CsvParser::getCsvRows() const
{
    return m_table.size();
}

void CsvParser::appendStatusMsg(const QString& s, bool isCritical)
{
    m_statusMsg += QObject::tr("%1: (row, col) %2,%3").arg(s, m_currRow, QChar(m_currCol)).append("\n");
    m_isGood = !isCritical;
}
