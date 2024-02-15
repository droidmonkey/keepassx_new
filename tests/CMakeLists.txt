#  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
#  Copyright (C) 2010 Felix Geyer <debfx@fobos.de>
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

include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}/../src)

add_definitions(-DQT_TEST_LIB)

set(KEEPASSX_TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/data)
configure_file(config-keepassx-tests.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-keepassx-tests.h)

macro(parse_arguments prefix arg_names option_names)
    set(DEFAULT_ARGS)
    foreach(arg_name ${arg_names})
        set(${prefix}_${arg_name})
    endforeach(arg_name)
    foreach(option ${option_names})
        set(${prefix}_${option} FALSE)
    endforeach(option)

    set(current_arg_name DEFAULT_ARGS)
    set(current_arg_list)
    foreach(arg ${ARGN})
        set(larg_names ${arg_names})
        list(FIND larg_names "${arg}" is_arg_name)
        if(is_arg_name GREATER -1)
            set(${prefix}_${current_arg_name} ${current_arg_list})
            set(current_arg_name ${arg})
            set(current_arg_list)
        else()
            set(loption_names ${option_names})
            list(FIND loption_names "${arg}" is_option)
            if(is_option GREATER -1)
                set(${prefix}_${arg} TRUE)
            else(is_option GREATER -1)
                set(current_arg_list ${current_arg_list} ${arg})
            endif()
        endif()
    endforeach(arg)
    set(${prefix}_${current_arg_name} ${current_arg_list})
endmacro(parse_arguments)

macro(add_unit_test)
    parse_arguments(TEST "NAME;SOURCES;LIBS;LAUNCHER" "" ${ARGN})
    set(_test_NAME ${TEST_NAME})
    set(_test_LAUNCHER ${TEST_LAUNCHER})
    set(_srcList ${TEST_SOURCES})
    add_executable(${_test_NAME} ${_srcList})
    target_link_libraries(${_test_NAME} ${TEST_LIBS})

    if(NOT TEST_OUTPUT)
        set(TEST_OUTPUT plaintext)
    endif(NOT TEST_OUTPUT)
    set(TEST_OUTPUT ${TEST_OUTPUT} CACHE STRING "The output to generate when running the QTest unit tests")

    if(KDE4_TEST_OUTPUT STREQUAL "xml")
        add_test(${_test_NAME} ${_test_LAUNCHER} ${_test_NAME} -xml -o ${_test_NAME}.tml)
    else(KDE4_TEST_OUTPUT STREQUAL "xml")
        add_test(${_test_NAME} ${_test_LAUNCHER} ${_test_NAME})
    endif(KDE4_TEST_OUTPUT STREQUAL "xml")

    set_tests_properties(${_test_NAME} PROPERTIES ENVIRONMENT "LANG=en_US.UTF-8")

    if(NOT MSVC_IDE)   #not needed for the ide
        # if the tests are EXCLUDE_FROM_ALL, add a target "buildtests" to build all tests
        if(NOT WITH_TESTS)
            get_directory_property(_buildtestsAdded BUILDTESTS_ADDED)
            if(NOT _buildtestsAdded)
                add_custom_target(buildtests)
                set_directory_properties(PROPERTIES BUILDTESTS_ADDED TRUE)
            endif()
            add_dependencies(buildtests ${_test_NAME})
        endif()
    endif()
endmacro(add_unit_test)

set(TEST_LIBRARIES keepassxc_gui Qt5::Test)

set(testsupport_SOURCES
        modeltest.cpp
        FailDevice.cpp
        mock/MockClock.cpp
        util/TemporaryFile.cpp)
add_library(testsupport STATIC ${testsupport_SOURCES})
target_link_libraries(testsupport Qt5::Core Qt5::Concurrent Qt5::Widgets Qt5::Test)

add_unit_test(NAME testauthenticationfactorparsing SOURCES TestAuthenticationFactorParsing.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testgroup SOURCES TestGroup.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testkdbx2 SOURCES TestKdbx2.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testkdbx3 SOURCES TestKeePass2Format.cpp FailDevice.cpp mock/MockChallengeResponseKey.cpp TestKdbx3.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testkdbx4 SOURCES TestKeePass2Format.cpp FailDevice.cpp mock/MockChallengeResponseKey.cpp TestKdbx4.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testkeys SOURCES TestKeys.cpp mock/MockChallengeResponseKey.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testgroupmodel SOURCES TestGroupModel.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testentrymodel SOURCES TestEntryModel.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testcryptohash SOURCES TestCryptoHash.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testsymmetriccipher SOURCES TestSymmetricCipher.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testhashedblockstream SOURCES TestHashedBlockStream.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testkeepass2randomstream SOURCES TestKeePass2RandomStream.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testmodified SOURCES TestModified.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testdeletedobjects SOURCES TestDeletedObjects.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testkeepass1reader SOURCES TestKeePass1Reader.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testimports SOURCES TestImports.cpp
        LIBS ${TEST_LIBRARIES})

if(WITH_XC_NETWORKING)
    add_unit_test(NAME testupdatecheck SOURCES TestUpdateCheck.cpp
            LIBS ${TEST_LIBRARIES})

    add_unit_test(NAME testicondownloader SOURCES TestIconDownloader.cpp LIBS ${TEST_LIBRARIES})
endif()

if(WITH_XC_AUTOTYPE)
    add_unit_test(NAME testautotype SOURCES TestAutoType.cpp
            LIBS ${TEST_LIBRARIES})
    set_target_properties(testautotype PROPERTIES ENABLE_EXPORTS ON)
endif()

if(WITH_XC_SSHAGENT)
    add_unit_test(NAME testopensshkey SOURCES TestOpenSSHKey.cpp
            LIBS sshagent ${TEST_LIBRARIES})
    if(NOT WIN32)
        add_unit_test(NAME testsshagent SOURCES TestSSHAgent.cpp
                LIBS sshagent ${TEST_LIBRARIES})
    endif()
endif()

add_unit_test(NAME testentry SOURCES TestEntry.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testmerge SOURCES TestMerge.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testpasswordgenerator SOURCES TestPasswordGenerator.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testpasswordhealth SOURCES TestPasswordHealth.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testpassphrasegenerator SOURCES TestPassphraseGenerator.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testhibp SOURCES TestHibp.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testtotp SOURCES TestTotp.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testbase32 SOURCES TestBase32.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testcsvparser SOURCES TestCsvParser.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testrandomgenerator SOURCES TestRandomGenerator.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testentrysearcher SOURCES TestEntrySearcher.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testcsvexporter SOURCES TestCsvExporter.cpp
        LIBS ${TEST_LIBRARIES})

if(WITH_XC_YUBIKEY)
    add_unit_test(NAME testykchallengeresponsekey
        SOURCES TestYkChallengeResponseKey.cpp
        LIBS ${TEST_LIBRARIES})
endif()

if(WITH_XC_KEESHARE)
    add_unit_test(NAME testsharing SOURCES TestSharing.cpp
        LIBS testsupport ${TEST_LIBRARIES})
endif()

add_unit_test(NAME testdatabase SOURCES TestDatabase.cpp
        LIBS testsupport ${TEST_LIBRARIES})

add_unit_test(NAME testtools SOURCES TestTools.cpp
        LIBS ${TEST_LIBRARIES})

add_unit_test(NAME testconfig SOURCES TestConfig.cpp
        LIBS testsupport ${TEST_LIBRARIES})

if(WITH_XC_FDOSECRETS)
    add_unit_test(NAME testfdosecrets SOURCES TestFdoSecrets.cpp
        LIBS testsupport fdosecrets ${TEST_LIBRARIES})
endif()

if(WITH_XC_BROWSER)
    add_unit_test(NAME testbrowser SOURCES TestBrowser.cpp
        LIBS browser ${TEST_LIBRARIES})

    if(WITH_XC_BROWSER_PASSKEYS)
        add_unit_test(NAME testpasskeys SOURCES TestPasskeys.cpp
            LIBS browser ${TEST_LIBRARIES})
    endif()
endif()

if(WITH_XC_NETWORKING OR WITH_XC_BROWSER)
    add_unit_test(NAME testurltools SOURCES TestUrlTools.cpp LIBS ${TEST_LIBRARIES})
endif()

add_unit_test(NAME testcli SOURCES TestCli.cpp
        LIBS testsupport cli ${ZXCVBN_LIBRARIES} ${TEST_LIBRARIES})
target_compile_definitions(testcli PRIVATE KEEPASSX_CLI_PATH="$<TARGET_FILE:keepassxc-cli>")

if(WITH_GUI_TESTS)
    add_subdirectory(gui)
endif(WITH_GUI_TESTS)
