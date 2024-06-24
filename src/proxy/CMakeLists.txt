#  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
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
    set(proxy_SOURCES
        ../browser/BrowserShared.cpp
        keepassxc-proxy.cpp
        NativeMessagingProxy.cpp)

    # Alloc must be defined in a static library to prevent clashing with clang ASAN definitions
    add_library(proxy_alloc STATIC ../core/Alloc.cpp)
    target_link_libraries(proxy_alloc PRIVATE Qt5::Core ${BOTAN_LIBRARIES})

    add_executable(keepassxc-proxy ${proxy_SOURCES})
    target_link_libraries(keepassxc-proxy proxy_alloc Qt5::Core Qt5::Network)
    install(TARGETS keepassxc-proxy
            BUNDLE DESTINATION . COMPONENT Runtime
            RUNTIME DESTINATION ${PROXY_INSTALL_DIR} COMPONENT Runtime)

    if(APPLE AND WITH_APP_BUNDLE)
        set(PROXY_APP_DIR "${CMAKE_BINARY_DIR}/src/${PROXY_INSTALL_DIR}")
        add_custom_command(TARGET keepassxc-proxy
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/keepassxc-proxy ${PROXY_APP_DIR}/keepassxc-proxy
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/src
                COMMENT "Copying keepassxc-proxy into app bundle")

        set_property(GLOBAL APPEND PROPERTY
            _MACDEPLOYQT_EXTRA_BINARIES "${PROXY_INSTALL_DIR}/keepassxc-proxy")
    endif()

    if(WIN32)
      target_link_libraries(keepassxc-proxy Wtsapi32.lib Ws2_32.lib)
    endif()
endif()
