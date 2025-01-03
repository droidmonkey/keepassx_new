#  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 or (at your option)
#  version 3 of the License.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

add_unit_test(NAME testgui SOURCES TestGui.cpp ../util/TemporaryFile.cpp ../mock/MockRemoteProcess.cpp LIBS ${TEST_LIBRARIES})
add_unit_test(NAME testguipixmaps SOURCES TestGuiPixmaps.cpp LIBS ${TEST_LIBRARIES})

if(KPXC_FEATURE_BROWSER)
    add_unit_test(NAME testguibrowser SOURCES TestGuiBrowser.cpp ../util/TemporaryFile.cpp LIBS ${TEST_LIBRARIES})
endif()

if(KPXC_FEATURE_FDOSECRETS)
    add_unit_test(NAME testguifdosecrets
                  SOURCES TestGuiFdoSecrets.cpp ../util/TemporaryFile.cpp ../util/FdoSecretsProxy.cpp
                  LIBS ${TEST_LIBRARIES}
                  # The following doesn't work because dbus-run-session expects execname to be in PATH
                  # dbus-run-session -- execname
                  LAUNCHER dbus-run-session --config-file ${CMAKE_CURRENT_SOURCE_DIR}/../data/dbus/session.conf -- sh -c "exec ./$0")
endif()
