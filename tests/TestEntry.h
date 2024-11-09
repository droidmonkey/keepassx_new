/*
 *  Copyright (C) 2013 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_TESTENTRY_H
#define KEEPASSX_TESTENTRY_H

#include <QObject>

class Entry;

class TestEntry : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testHistoryItemDeletion();
    void testCopyDataFrom();
    void testClone();
    void testResolveUrl();
    void testResolveUrlPlaceholders();
    void testResolveRecursivePlaceholders();
    void testResolveReferencePlaceholders();
    void testResolveNonIdPlaceholdersToUuid();
    void testResolveConversionPlaceholders();
    void testResolveReplacePlaceholders();
    void testResolveClonedEntry();
    void testIsRecycled();
    void testMoveUpDown();
    void testPreviousParentGroup();
};

#endif // KEEPASSX_TESTENTRY_H
