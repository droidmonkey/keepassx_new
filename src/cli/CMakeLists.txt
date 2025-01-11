#  Copyright (C) 2025 KeePassXC Team
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

set(cli_SOURCES
        Add.cpp
        AddGroup.cpp
        Analyze.cpp
        AttachmentExport.cpp
        AttachmentImport.cpp
        AttachmentRemove.cpp
        Clip.cpp
        Close.cpp
        Command.cpp
        DatabaseCommand.cpp
        DatabaseCreate.cpp
        DatabaseEdit.cpp
        DatabaseInfo.cpp
        Diceware.cpp
        Edit.cpp
        Estimate.cpp
        Exit.cpp
        Export.cpp
        Generate.cpp
        Help.cpp
        Import.cpp
        List.cpp
        Merge.cpp
        Move.cpp
        Open.cpp
        Remove.cpp
        RemoveGroup.cpp
        Search.cpp
        Show.cpp)

add_library(cli STATIC ${cli_SOURCES})
target_link_libraries(cli ${ZXCVBN_LIBRARIES} Qt6::Core)

find_package(Readline)

if (READLINE_FOUND)
    target_compile_definitions(cli PUBLIC USE_READLINE)
    target_link_libraries(cli ${Readline_LIBRARY})
endif()

add_executable(keepassxc-cli keepassxc-cli.cpp)
target_link_libraries(keepassxc-cli
        ${GPGERROR_LIBRARIES}
        cli
        keepassxc_core)

install(TARGETS keepassxc-cli
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION ${CLI_INSTALL_DIR} COMPONENT Runtime)

if(WIN32)
    target_sources(keepassxc-cli
        PRIVATE keepassxc-cli.exe.manifest)

#    install(CODE "include(BundleUtilities)
#                  fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/keepassxc-cli.exe\" \"\" \"\")"
#            COMPONENT Runtime)
endif()

if(APPLE AND WITH_APP_BUNDLE)
    set(CLI_APP_DIR "${CMAKE_BINARY_DIR}/src/${CLI_INSTALL_DIR}")

    add_custom_command(TARGET keepassxc-cli
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/keepassxc-cli ${CLI_APP_DIR}/keepassxc-cli
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/src
            COMMENT "Copying keepassxc-cli into app bundle")
    set_property(GLOBAL APPEND PROPERTY
        _MACDEPLOYQT_EXTRA_BINARIES "${CLI_INSTALL_DIR}/keepassxc-cli")
endif()
