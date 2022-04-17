#  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
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

if(WITH_XC_SSHAGENT)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

    set(sshagent_SOURCES
        AgentSettingsPage.cpp
        AgentSettingsWidget.cpp
        ASN1Key.cpp
        BinaryStream.cpp
        KeeAgentSettings.cpp
        OpenSSHKey.cpp
        OpenSSHKeyGen.cpp
        OpenSSHKeyGenDialog.cpp
        SSHAgent.cpp
    )

    find_package(Qt6 REQUIRED COMPONENTS Core5Compat)
    add_library(sshagent STATIC ${sshagent_SOURCES})
    target_link_libraries(sshagent Qt6::Core Qt6::Core5Compat Qt6::Widgets Qt6::Network)
endif()
