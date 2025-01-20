/*
 *  Copyright (C) 2019 KeePassXC Team <team@keepassxc.org>
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

#include <QString>

#include "Clock.h"
#include "Group.h"
#include "PasswordHealth.h"
#include "Tools.h"
#include "zxcvbn.h"

namespace
{
    const static int ZXCVBN_ESTIMATE_THRESHOLD = 256;
} // namespace

PasswordHealth::PasswordHealth(double entropy)
{
    init(entropy);
}

PasswordHealth::PasswordHealth(const QString& pwd)
{
    auto entropy = 0.0;
    entropy += ZxcvbnMatch(pwd.left(ZXCVBN_ESTIMATE_THRESHOLD).toUtf8(), nullptr, nullptr);
    if (pwd.length() > ZXCVBN_ESTIMATE_THRESHOLD) {
        // Add the average entropy per character for any characters above the estimate threshold
        auto average = entropy / ZXCVBN_ESTIMATE_THRESHOLD;
        entropy += average * (pwd.length() - ZXCVBN_ESTIMATE_THRESHOLD);
    }
    init(entropy);
}

void PasswordHealth::init(double entropy)
{
    m_score = m_entropy = entropy;

    switch (quality()) {
    case Quality::Bad:
    case Quality::Poor:
        m_scoreReasons << QObject::tr("Very weak password");
        m_scoreDetails << QObject::tr("Password entropy is %1 bits").arg(QString::number(m_entropy, 'f', 2));
        break;

    case Quality::Weak:
        m_scoreReasons << QObject::tr("Weak password");
        m_scoreDetails << QObject::tr("Password entropy is %1 bits").arg(QString::number(m_entropy, 'f', 2));
        break;

    default:
        // No reason or details for good and excellent passwords
        break;
    }
}

void PasswordHealth::setScore(int score)
{
    m_score = score;
}

void PasswordHealth::adjustScore(int amount)
{
    m_score += amount;
}

QString PasswordHealth::scoreReason() const
{
    return m_scoreReasons.join("\n");
}

void PasswordHealth::addScoreReason(QString reason)
{
    m_scoreReasons << reason;
}

QString PasswordHealth::scoreDetails() const
{
    return m_scoreDetails.join("\n");
}

void PasswordHealth::addScoreDetails(QString details)
{
    m_scoreDetails.append(details);
}

PasswordHealth::Quality PasswordHealth::quality() const
{
    if (m_score <= 0) {
        return Quality::Bad;
    } else if (m_score < 40) {
        return Quality::Poor;
    } else if (m_score < 75) {
        return Quality::Weak;
    } else if (m_score < 100) {
        return Quality::Good;
    }
    return Quality::Excellent;
}

/**
 * This class provides additional information about password health
 * than can be derived from the password itself (re-use, expiry).
 */
HealthChecker::HealthChecker(QSharedPointer<Database> db)
{
    // Build the cache of re-used passwords
    for (const auto* entry : db->rootGroup()->entriesRecursive()) {
        if (!entry->isRecycled() && !entry->isAttributeReference("Password")) {
            m_reuse[entry->password()]
                << QObject::tr("Used in %1/%2").arg(entry->group()->hierarchy().join('/'), entry->title());
        }
    }
}

/**
 * Call operator of the Health Checker class.
 *
 * Returns the health of the password in `entry`, considering
 * password entropy, re-use, expiration, etc.
 */
QSharedPointer<PasswordHealth> HealthChecker::evaluate(const Entry* entry) const
{
    // Pointer sanity check
    if (!entry) {
        return {};
    }

    // First analyse the password itself
    const auto pwd = entry->password();
    auto health = QSharedPointer<PasswordHealth>(new PasswordHealth(pwd));

    // Second, if the password is in the database more than once,
    // reduce the score accordingly
    const auto& used = m_reuse[pwd];
    const auto count = used.size();
    if (count > 1) {
        constexpr auto penalty = 15;
        health->adjustScore(-penalty * (count - 1));
        health->addScoreReason(QObject::tr("Password is used %1 time(s)", "", count).arg(QString::number(count)));
        // Add the first 20 uses of the password to prevent the details display from growing too large
        for (int i = 0; i < used.size(); ++i) {
            health->addScoreDetails(used[i]);
            if (i == 19) {
                health->addScoreDetails("…");
                break;
            }
        }

        // Don't allow re-used passwords to be considered "good"
        // no matter how great their entropy is.
        if (health->score() > 74) {
            health->setScore(74);
        }
    }

    // Third, if the password has already expired, reduce score to 0;
    // or, if the password is going to expire in the next 30 days,
    // reduce score by 2 points per day.
    if (entry->isExpired()) {
        health->setScore(0);
        health->addScoreReason(QObject::tr("Password has expired"));
        health->addScoreDetails(
            QObject::tr("Password expiry was %1").arg(Clock::toString(entry->timeInfo().expiryTime())));
    } else if (entry->timeInfo().expires()) {
        const int days = QDateTime::currentDateTime().daysTo(entry->timeInfo().expiryTime());
        if (days <= 30) {
            // First bring the score down into the "weak" range
            // so that the entry appears in Health Check. Then
            // reduce the score by 2 points for every day that
            // we get closer to expiry. days<=0 has already
            // been handled above ("isExpired()").
            if (health->score() > 70) {
                health->setScore(70);
            }

            health->adjustScore((30 - days) * -2);
            health->addScoreDetails(
                QObject::tr("Password expires on %1").arg(Clock::toString(entry->timeInfo().expiryTime())));
            if (days <= 2) {
                health->addScoreReason(QObject::tr("Password is about to expire"));
            } else if (days <= 10) {
                health->addScoreReason(QObject::tr("Password expires in %1 day(s)", "", days).arg(days));
            } else {
                health->addScoreReason(QObject::tr("Password will expire soon"));
            }
        }
    }

    // Fourth, add note if password is two or more years old.
    int ageInSeconds = entry->passwordAgeSeconds();
    // Unfortunately, Qt doesn't seem to have a utility for seconds->year.
    // (365 days)(24 hours/day)(3600 s/hr) is approximately a year and gets compiled away.
    if (ageInSeconds / (365 * 24 * 3600) > 1) {
        health->addScoreReason(QObject::tr("Password is %1 old").arg(Tools::humanReadableTimeDifference(ageInSeconds)));
    }

    // Return the result
    return health;
}
