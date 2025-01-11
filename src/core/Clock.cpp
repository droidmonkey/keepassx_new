/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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
#include "Clock.h"

#include <QLocale>

QSharedPointer<Clock> Clock::m_instance;

QDateTime Clock::currentDateTimeUtc()
{
    return instance().currentDateTimeUtcImpl();
}

QDateTime Clock::currentDateTime()
{
    return instance().currentDateTimeImpl();
}

uint Clock::currentSecondsSinceEpoch()
{
    return instance().currentDateTimeImpl().toSecsSinceEpoch();
}

qint64 Clock::currentMilliSecondsSinceEpoch()
{
    return instance().currentDateTimeImpl().toMSecsSinceEpoch();
}

QDateTime Clock::serialized(const QDateTime& dateTime)
{
    auto time = dateTime.time();
    if (time.isValid() && time.msec() != 0) {
        return dateTime.addMSecs(-time.msec());
    }
    return dateTime;
}

QDateTime Clock::datetimeUtc(int year, int month, int day, int hour, int min, int second)
{
    return {QDate(year, month, day), QTime(hour, min, second), Qt::UTC};
}

QDateTime Clock::datetime(int year, int month, int day, int hour, int min, int second)
{
    return {QDate(year, month, day), QTime(hour, min, second), Qt::LocalTime};
}

QDateTime Clock::datetimeUtc(qint64 msecSinceEpoch)
{
    return QDateTime::fromMSecsSinceEpoch(msecSinceEpoch, Qt::UTC);
}

QDateTime Clock::datetime(qint64 msecSinceEpoch)
{
    return QDateTime::fromMSecsSinceEpoch(msecSinceEpoch, Qt::LocalTime);
}

QDateTime Clock::parse(const QString& text, Qt::DateFormat format)
{
    return QDateTime::fromString(text, format);
}

QDateTime Clock::parse(const QString& text, const QString& format)
{
    return QDateTime::fromString(text, format);
}

QString Clock::toString(const QDateTime& dateTime)
{
    static QLocale locale;
    return locale.toString(dateTime, QLocale::ShortFormat);
}

Clock::~Clock() = default;

Clock::Clock() = default;

QDateTime Clock::currentDateTimeUtcImpl() const
{
    return QDateTime::currentDateTimeUtc();
}

QDateTime Clock::currentDateTimeImpl() const
{
    return QDateTime::currentDateTime();
}

void Clock::resetInstance()
{
    m_instance.reset();
}

void Clock::setInstance(Clock* clock)
{
    m_instance = QSharedPointer<Clock>(clock);
}

const Clock& Clock::instance()
{
    if (!m_instance) {
        m_instance = QSharedPointer<Clock>(new Clock());
    }
    return *m_instance;
}
