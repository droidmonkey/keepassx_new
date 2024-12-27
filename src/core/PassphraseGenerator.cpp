/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include "PassphraseGenerator.h"

#include <QFile>
#include <QSet>
#include <QTextStream>
#include <cmath>

#include "core/Resources.h"
#include "crypto/Random.h"

const int PassphraseGenerator::DefaultWordCount = 7;
const char* PassphraseGenerator::DefaultSeparator = " ";
const char* PassphraseGenerator::DefaultWordList = "eff_large.wordlist";

PassphraseGenerator::PassphraseGenerator()
    : m_wordCount(DefaultWordCount)
    , m_wordCase(LOWERCASE)
    , m_separator(DefaultSeparator)
{
    setDefaultWordList();
}

double PassphraseGenerator::estimateEntropy(int wordCount)
{
    if (m_wordlist.isEmpty()) {
        return 0.0;
    }
    if (wordCount < 1) {
        wordCount = m_wordCount;
    }

    return std::log2(m_wordlist.size()) * wordCount;
}

void PassphraseGenerator::setWordCount(int wordCount)
{
    m_wordCount = qMax(1, wordCount);
}

void PassphraseGenerator::setWordCase(PassphraseWordCase wordCase)
{
    m_wordCase = wordCase;
}

void PassphraseGenerator::setWordList(const QString& path)
{
    m_wordlist.clear();
    // Initially load wordlist into a set to avoid duplicates
    QSet<QString> wordset;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Couldn't load passphrase wordlist: %s", qPrintable(path));
        return;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");
    QString line = in.readLine();
    bool isSigned = line.startsWith("-----BEGIN PGP SIGNED MESSAGE-----");
    if (isSigned) {
        while (!line.isNull() && !line.trimmed().isEmpty()) {
            line = in.readLine();
        }
    }
    QRegExp rx("^[0-9]+(-[0-9]+)*\\s+([^\\s]+)$");
    while (!line.isNull()) {
        if (isSigned && line.startsWith("-----BEGIN PGP SIGNATURE-----")) {
            break;
        }
        // Handle dash-escaped lines (if the wordlist is signed)
        if (isSigned && line.startsWith("- ")) {
            line.remove(0, 2);
        }
        line = line.trimmed();
        line.replace(rx, "\\2");
        if (!line.isEmpty()) {
            wordset.insert(line);
        }
        line = in.readLine();
    }

    m_wordlist = wordset.toList();

    if (m_wordlist.size() < m_minimum_wordlist_length) {
        qWarning("Wordlist is less than minimum acceptable size: %s", qPrintable(path));
    }
}

void PassphraseGenerator::setDefaultWordList()
{
    const QString path = resources()->wordlistPath(PassphraseGenerator::DefaultWordList);
    setWordList(path);
}

void PassphraseGenerator::setWordSeparator(const QString& separator)
{
    m_separator = separator;
}

QString PassphraseGenerator::generatePassphrase() const
{
    // In case there was an error loading the wordlist
    if (!isValid() || m_wordlist.empty()) {
        return {};
    }

    QStringList words;
    int randomIndex = randomGen()->randomUInt(static_cast<quint32>(m_wordCount));
    for (int i = 0; i < m_wordCount; ++i) {
        int wordIndex = randomGen()->randomUInt(static_cast<quint32>(m_wordlist.size()));
        auto tmpWord = m_wordlist.at(wordIndex);

        // convert case
        switch (m_wordCase) {
        case UPPERCASE:
            tmpWord = tmpWord.toUpper();
            break;
        case TITLECASE:
            tmpWord = tmpWord.replace(0, 1, tmpWord.left(1).toUpper());
            break;
        case MIXEDCASE:
            tmpWord = i == randomIndex ? tmpWord.toUpper() : tmpWord.toLower();
            break;
        case LOWERCASE:
            tmpWord = tmpWord.toLower();
            break;
        }
        words.append(tmpWord);
    }

    return words.join(m_separator);
}

bool PassphraseGenerator::isValid() const
{
    return m_wordCount > 0 && m_wordlist.size() >= m_minimum_wordlist_length;
}
