/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *  Copyright (C) 2012 Intel Corporation
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

#ifndef KEEPASSX_GLOBAL_H
#define KEEPASSX_GLOBAL_H

#include <QString>
#include <QTextStream>

#if defined(Q_OS_WIN)
#if defined(KPXC_BUILDING_CORE)
#define KEEPASSXC_EXPORT Q_DECL_IMPORT
#else
#define KEEPASSXC_EXPORT Q_DECL_EXPORT
#endif
#else
#define KEEPASSXC_EXPORT Q_DECL_EXPORT
#endif

#ifndef QUINT32_MAX
#define QUINT32_MAX 4294967295U
#endif

#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
#define FILE_CASE_SENSITIVE Qt::CaseInsensitive
#else
#define FILE_CASE_SENSITIVE Qt::CaseSensitive
#endif

#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
// "Backport" a few things to the 'Qt' namespace as required for older Qt
// versions.
namespace Qt
{
    const QString::SplitBehavior SkipEmptyParts = QString::SkipEmptyParts;
    inline QTextStream& endl(QTextStream& s)
    {
        return ::endl(s);
    }
    inline QTextStream& flush(QTextStream& s)
    {
        return ::flush(s);
    }
} // namespace Qt
#endif

static const auto TRUE_STR = QStringLiteral("true");
static const auto FALSE_STR = QStringLiteral("false");

enum class AuthDecision
{
    Undecided,
    Allowed,
    AllowedOnce,
    Denied,
    DeniedOnce,
};

template <typename T> struct AddConst
{
    typedef const T Type;
};

// this adds const to non-const objects (like std::as_const)
template <typename T> constexpr typename AddConst<T>::Type& asConst(T& t) noexcept
{
    return t;
}
// prevent rvalue arguments:
template <typename T> void asConst(const T&&) = delete;

#endif // KEEPASSX_GLOBAL_H
