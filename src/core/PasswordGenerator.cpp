/*
 *  Copyright (C) 2013 Felix Geyer <debfx@fobos.de>
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

#include "PasswordGenerator.h"

#include "crypto/Random.h"

const int PasswordGenerator::DefaultLength = 32;
const char* PasswordGenerator::DefaultCustomCharacterSet = "";
const char* PasswordGenerator::DefaultExcludedChars = "";

PasswordGenerator::PasswordGenerator()
    : m_length(PasswordGenerator::DefaultLength)
    , m_classes(PasswordGenerator::CharClass::DefaultCharset)
    , m_flags(PasswordGenerator::GeneratorFlag::DefaultFlags)
    , m_custom(PasswordGenerator::DefaultCustomCharacterSet)
    , m_excluded(PasswordGenerator::DefaultExcludedChars)
{
}

void PasswordGenerator::setLength(int length)
{
    m_length = length;
}

void PasswordGenerator::setCharClasses(const PasswordGenerator::CharClasses& classes)
{
    m_classes = classes;
}

void PasswordGenerator::setCustomCharacterSet(const QString& customCharacterSet)
{
    m_custom = customCharacterSet;
}
void PasswordGenerator::setExcludedCharacterSet(const QString& excludedCharacterSet)
{
    m_excluded = excludedCharacterSet;
}

void PasswordGenerator::setFlags(const GeneratorFlags& flags)
{
    m_flags = flags;
}

QString PasswordGenerator::generatePassword() const
{
    Q_ASSERT(isValid());

    const QVector<PasswordGroup> groups = passwordGroups();

    QVector<QChar> passwordChars;
    for (const PasswordGroup& group : groups) {
        for (QChar ch : group) {
            passwordChars.append(ch);
        }
    }

    QString password;

    if (m_flags & CharFromEveryGroup) {
        for (const auto& group : groups) {
            int pos = randomGen()->randomUInt(static_cast<quint32>(group.size()));

            password.append(group[pos]);
        }

        for (int i = groups.size(); i < m_length; i++) {
            int pos = randomGen()->randomUInt(static_cast<quint32>(passwordChars.size()));

            password.append(passwordChars[pos]);
        }

        // shuffle chars
        for (int i = (password.size() - 1); i >= 1; i--) {
            int j = randomGen()->randomUInt(static_cast<quint32>(i + 1));

            QChar tmp = password[i];
            password[i] = password[j];
            password[j] = tmp;
        }
    } else {
        for (int i = 0; i < m_length; i++) {
            int pos = randomGen()->randomUInt(static_cast<quint32>(passwordChars.size()));

            password.append(passwordChars[pos]);
        }
    }

    return password;
}

bool PasswordGenerator::isValid() const
{
    if (m_classes == CharClass::NoClass && m_custom.isEmpty()) {
        return false;
    } else if (m_length <= 0) {
        return false;
    }

    if ((m_flags & CharFromEveryGroup) && (m_length < numCharClasses())) {
        return false;
    }

    return !passwordGroups().isEmpty();
}

QVector<PasswordGroup> PasswordGenerator::passwordGroups() const
{
    QVector<PasswordGroup> passwordGroups;

    if (m_classes & LowerLetters) {
        PasswordGroup group;

        for (int i = 97; i <= (97 + 25); i++) {

            if ((m_flags & ExcludeLookAlike) && (i == 108)) { // "l"
                continue;
            }

            group.append(QChar(i));
        }

        passwordGroups.append(group);
    }
    if (m_classes & UpperLetters) {
        PasswordGroup group;

        for (int i = 65; i <= (65 + 25); i++) {

            if ((m_flags & ExcludeLookAlike) && (i == 66 || i == 71 || i == 73 || i == 79)) { //"B", "G", "I" and "O"
                continue;
            }

            group.append(QChar(i));
        }

        passwordGroups.append(group);
    }
    if (m_classes & Numbers) {
        PasswordGroup group;

        for (int i = 48; i < (48 + 10); i++) {
            if ((m_flags & ExcludeLookAlike) && (i == 48 || i == 49 || i == 54 || i == 56)) { // "0", "1", "6", and "8"
                continue;
            }

            group.append(QChar(i));
        }

        passwordGroups.append(group);
    }
    if (m_classes & Braces) {
        PasswordGroup group;

        // ()[]{}
        group.append(QChar(40));
        group.append(QChar(41));
        group.append(QChar(91));
        group.append(QChar(93));
        group.append(QChar(123));
        group.append(QChar(125));

        passwordGroups.append(group);
    }
    if (m_classes & Punctuation) {
        PasswordGroup group;

        // .,:;
        group.append(QChar(44));
        group.append(QChar(46));
        group.append(QChar(58));
        group.append(QChar(59));

        passwordGroups.append(group);
    }
    if (m_classes & Quotes) {
        PasswordGroup group;

        // "'
        group.append(QChar(34));
        group.append(QChar(39));

        passwordGroups.append(group);
    }
    if (m_classes & Dashes) {
        PasswordGroup group;

        // -/\_|
        group.append(QChar(45));
        group.append(QChar(47));
        group.append(QChar(92));
        group.append(QChar(95));
        if (!(m_flags & ExcludeLookAlike)) {
            group.append(QChar(124)); // "|"
        }

        passwordGroups.append(group);
    }
    if (m_classes & Math) {
        PasswordGroup group;

        // !*+<=>?
        group.append(QChar(33));
        group.append(QChar(42));
        group.append(QChar(43));
        group.append(QChar(60));
        group.append(QChar(61));
        group.append(QChar(62));
        group.append(QChar(63));

        passwordGroups.append(group);
    }
    if (m_classes & Logograms) {
        PasswordGroup group;

        // #$%&
        for (int i = 35; i <= 38; i++) {
            group.append(QChar(i));
        }
        // @^`~
        group.append(QChar(64));
        group.append(QChar(94));
        group.append(QChar(96));
        group.append(QChar(126));

        passwordGroups.append(group);
    }
    if (m_classes & EASCII) {
        PasswordGroup group;

        // [U+0080, U+009F] are C1 control characters,
        // U+00A0 is non-breaking space
        for (int i = 161; i <= 172; i++) {
            group.append(QChar(i));
        }
        // U+00AD is soft hyphen (format character)
        for (int i = 174; i <= 255; i++) {
            if ((m_flags & ExcludeLookAlike) && (i == 249)) { // "﹒"
                continue;
            }
            group.append(QChar(i));
        }

        passwordGroups.append(group);
    }
    if (!m_custom.isEmpty()) {
        PasswordGroup group;

        for (const auto& ch : m_custom) {
            if (!group.contains(ch)) {
                group.append(ch);
            }
        }

        passwordGroups.append(group);
    }

    // Loop over character groups and remove excluded characters from them;
    // remove empty groups
    int i = 0;
    while (i != passwordGroups.size()) {
        PasswordGroup group = passwordGroups[i];

        for (QChar ch : m_excluded) {
            int j = group.indexOf(ch);
            while (j != -1) {
                group.remove(j);
                j = group.indexOf(ch);
            }
        }
        if (!group.isEmpty()) {
            passwordGroups.replace(i, group);
            ++i;
        } else {
            passwordGroups.remove(i);
        }
    }

    return passwordGroups;
}

int PasswordGenerator::numCharClasses() const
{
    // Actually compute the non empty password groups
    auto non_empty_groups = passwordGroups();
    return non_empty_groups.size();
}

int PasswordGenerator::getMinLength() const
{
    if ((m_flags & CharFromEveryGroup)) {
        return numCharClasses();
    }
    return 1;
}
void PasswordGenerator::reset()
{
    m_classes = CharClass::DefaultCharset;
    m_flags = GeneratorFlag::DefaultFlags;
    m_custom = DefaultCustomCharacterSet;
    m_excluded = DefaultExcludedChars;
    m_length = DefaultLength;
}
int PasswordGenerator::getLength() const
{
    return m_length;
}
const PasswordGenerator::GeneratorFlags& PasswordGenerator::getFlags() const
{
    return m_flags;
}
const PasswordGenerator::CharClasses& PasswordGenerator::getActiveClasses() const
{
    return m_classes;
}
const QString& PasswordGenerator::getCustomCharacterSet() const
{
    return m_custom;
}
const QString& PasswordGenerator::getExcludedCharacterSet() const
{
    return m_excluded;
}
