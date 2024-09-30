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

# Use pkgconfig on Linux
if(NOT WIN32)
   find_package(PkgConfig QUIET)
   pkg_check_modules(PCSC libpcsclite)
endif()

if(NOT PCSC_FOUND)
   # Search for PC/SC headers on Mac and Windows

   # Additional search paths for Windows if not running in Visual Studio environment
   if (WIN32) 
      # Resolve the ambiguity of using two names for one architechture
      if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "x64")
         set(ARCH_DIR "x64")
      else()
         set(ARCH_DIR "${CMAKE_SYSTEM_PROCESSOR}")
      endif()

      # Locate Windows SDK Paths
      if (CMAKE_WINDOWS_KITS_10_DIR)
         set(WINSDKROOTC_INCLUDE "${CMAKE_WINDOWS_KITS_10_DIR}/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um")
         set(WINSDKROOTC_LIB     "${CMAKE_WINDOWS_KITS_10_DIR}/LIB/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/${ARCH_DIR}")
      else()
         set(WINSDKROOTC_INCLUDE "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/Include/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um")
         set(WINSDKROOTC_LIB     "$ENV{ProgramFiles\(x86\)}/Windows Kits/10/LIB/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/um/${ARCH_DIR}")
      endif()
   endif()

   find_path(PCSC_INCLUDE_DIRS winscard.h
      HINTS
         ${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES}
         /usr/include/PCSC
         ${WINSDKROOTC_INCLUDE}
      PATH_SUFFIXES PCSC)

   # MAC library is PCSC, Windows library is WinSCard
   find_library(PCSC_LIBRARIES NAMES pcsclite libpcsclite WinSCard PCSC
      HINTS   
         ${CMAKE_C_IMPLICIT_LINK_DIRECTORIES}
         ${WINSDKROOTC_LIB})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PCSC DEFAULT_MSG PCSC_LIBRARIES PCSC_INCLUDE_DIRS)

mark_as_advanced(PCSC_LIBRARIES PCSC_INCLUDE_DIRS)
