#  Copyright (C) 2021 KeePassXC Team <team@keepassxc.org>
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

# This only works with CMake 3.18 and newer
if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.18")
    find_package(PkgConfig QUIET)
    pkg_check_modules(Xkbcommon xkbcommon)
endif()

if(NOT Xkbcommon_FOUND)
    find_path(Xkbcommon_INCLUDE_DIRS xkbcommon/xkbcommon.h)
    find_library(Xkbcommon_LIBRARIES xkbcommon)

    if(Xkbcommon_INCLUDE_DIRS AND Xkbcommon_LIBRARIES)
        set(Xkbcommon_FOUND TRUE)
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Xkbcommon DEFAULT_MSG Xkbcommon_LIBRARIES Xkbcommon_INCLUDE_DIRS)

mark_as_advanced(Xkbcommon_LIBRARIES Xkbcommon_INCLUDE_DIRS)
