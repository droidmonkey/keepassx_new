#  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

set(keeshare_SOURCES
    SettingsPageKeeShare.cpp
    SettingsWidgetKeeShare.cpp
    DatabaseSettingsWidgetKeeShare.cpp
    group/EditGroupWidgetKeeShare.cpp
    group/EditGroupPageKeeShare.cpp
    KeeShare.cpp
    KeeShareSettings.cpp
    ShareImport.cpp
    ShareExport.cpp
    ShareObserver.cpp
    )

add_library(keeshare STATIC ${keeshare_SOURCES})
target_link_libraries(keeshare PUBLIC Qt6::Core Qt6::Widgets ${BOTAN_LIBRARIES} ${ZLIB_LIBRARIES} PRIVATE ${MINIZIP_LIBRARIES})
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
