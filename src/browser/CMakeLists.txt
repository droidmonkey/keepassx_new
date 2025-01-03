#  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

if(KPXC_FEATURE_BROWSER)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

    set(browser_SOURCES
            BrowserAccessControlDialog.cpp
            BrowserAction.cpp
            BrowserCbor.cpp
            BrowserEntryConfig.cpp
            BrowserEntrySaveDialog.cpp
            BrowserHost.cpp
            BrowserMessageBuilder.cpp
            BrowserPasskeys.cpp
            BrowserPasskeysClient.cpp
            BrowserPasskeysConfirmationDialog.cpp
            BrowserSettingsPage.cpp
            BrowserSettingsWidget.cpp
            BrowserService.cpp
            BrowserSettings.cpp
            BrowserShared.cpp
            CustomTableWidget.cpp
            NativeMessageInstaller.cpp
            PasskeyUtils.cpp)

    add_library(browser STATIC ${browser_SOURCES})
    target_link_libraries(browser Qt5::Core Qt5::Concurrent Qt5::Widgets Qt5::Network ${BOTAN_LIBRARIES})
endif()
