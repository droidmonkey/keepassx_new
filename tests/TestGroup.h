/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_TESTGROUP_H
#define KEEPASSX_TESTGROUP_H

#include "core/Database.h"

class TestGroup : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void testParenting();
    void testSignals();
    void testEntries();
    void testDeleteSignals();
    void testCopyCustomIcon();
    void testClone();
    void testCopyCustomIcons();
    void testFindEntry();
    void testFindGroupByPath();
    void testPrint();
    void testAddEntryWithPath();
    void testIsRecycled();
    void testCopyDataFrom();
    void testEquals();
    void testChildrenSort();
    void testHierarchy();
    void testApplyGroupIconRecursively();
    void testUsernamesRecursive();
    void testMoveUpDown();
    void testPreviousParentGroup();
    void testAutoTypeState();
    void testTimeinfoChanges();
    void testWalk();
};

#endif // KEEPASSX_TESTGROUP_H
