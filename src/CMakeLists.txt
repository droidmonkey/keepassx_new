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

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

add_feature_info(Auto-Type WITH_XC_AUTOTYPE "Automatic password typing")
add_feature_info(Networking WITH_XC_NETWORKING "Compile KeePassXC with network access code (e.g. for downloading website icons)")
add_feature_info(KeePassXC-Browser WITH_XC_BROWSER "Browser integration with KeePassXC-Browser")
add_feature_info(Passkeys WITH_XC_BROWSER_PASSKEYS "Passkeys support for browser integration")
add_feature_info(SSHAgent WITH_XC_SSHAGENT "SSH agent integration compatible with KeeAgent")
add_feature_info(KeeShare WITH_XC_KEESHARE "Sharing integration with KeeShare")
add_feature_info(YubiKey WITH_XC_YUBIKEY "YubiKey HMAC-SHA1 challenge-response")
add_feature_info(UpdateCheck WITH_XC_UPDATECHECK "Automatic update checking")
if(UNIX AND NOT APPLE)
    add_feature_info(FdoSecrets WITH_XC_FDOSECRETS "Implement freedesktop.org Secret Storage Spec server side API.")
endif()

set(core_SOURCES
        core/Alloc.cpp
        core/AutoTypeAssociations.cpp
        core/Base32.cpp
        core/Bootstrap.cpp
        core/Clock.cpp
        core/Config.cpp
        core/CustomData.cpp
        core/Database.cpp
        core/DatabaseStats.cpp
        core/Entry.cpp
        core/EntryAttachments.cpp
        core/EntryAttributes.cpp
        core/EntrySearcher.cpp
        core/FileWatcher.cpp
        core/Group.cpp
        core/HibpOffline.cpp
        core/InactivityTimer.cpp
        core/Merger.cpp
        core/Metadata.cpp
        core/ModifiableObject.cpp
        core/PasswordGenerator.cpp
        core/PasswordHealth.cpp
        core/PassphraseGenerator.cpp
        core/Resources.cpp
        core/SignalMultiplexer.cpp
        core/TimeDelta.cpp
        core/TimeInfo.cpp
        core/Tools.cpp
        core/Totp.cpp
        core/Translator.cpp
        cli/Utils.cpp
        cli/TextStream.cpp
        crypto/Crypto.cpp
        crypto/CryptoHash.cpp
        crypto/Random.cpp
        crypto/SymmetricCipher.cpp
        crypto/kdf/Kdf.cpp
        crypto/kdf/AesKdf.cpp
        crypto/kdf/Argon2Kdf.cpp
        format/BitwardenReader.cpp
        format/CsvExporter.cpp
        format/CsvParser.cpp
        format/KeePass1Reader.cpp
        format/KeePass2.cpp
        format/KeePass2RandomStream.cpp
        format/KdbxReader.cpp
        format/KdbxWriter.cpp
        format/KdbxXmlReader.cpp
        format/KeePass2Reader.cpp
        format/KeePass2Writer.cpp
        format/Kdbx3Reader.cpp
        format/Kdbx3Writer.cpp
        format/Kdbx4Reader.cpp
        format/Kdbx4Writer.cpp
        format/KdbxXmlWriter.cpp
        format/OpData01.cpp
        format/OPUXReader.cpp
        format/OpVaultReader.cpp
        format/OpVaultReaderAttachments.cpp
        format/OpVaultReaderBandEntry.cpp
        format/OpVaultReaderSections.cpp
        format/ProtonPassReader.cpp
        keys/CompositeKey.cpp
        keys/FileKey.cpp
        keys/PasswordKey.cpp
        keys/ChallengeResponseKey.cpp
        streams/HashedBlockStream.cpp
        streams/HashingStream.cpp
        streams/HmacBlockStream.cpp
        streams/LayeredStream.cpp
        streams/qtiocompressor.cpp
        streams/StoreDataStream.cpp
        streams/SymmetricCipherStream.cpp)

set(gui_SOURCES
        gui/styles/styles.qrc
        gui/styles/StateColorPalette.cpp
        gui/styles/base/phantomcolor.cpp
        gui/styles/base/BaseStyle.cpp
        gui/styles/dark/DarkStyle.cpp
        gui/styles/light/LightStyle.cpp
        gui/AboutDialog.cpp
        gui/ActionCollection.cpp
        gui/Application.cpp
        gui/CategoryListWidget.cpp
        gui/Clipboard.cpp
        gui/CloneDialog.cpp
        gui/DatabaseIcons.cpp
        gui/DatabaseOpenWidget.cpp
        gui/DatabaseTabWidget.cpp
        gui/DatabaseWidget.cpp
        gui/DatabaseWidgetStateSync.cpp
        gui/EntryPreviewWidget.cpp
        gui/DialogyWidget.cpp
        gui/EditWidget.cpp
        gui/EditWidgetIcons.cpp
        gui/EditWidgetProperties.cpp
        gui/FileDialog.cpp
        gui/Font.cpp
        gui/GuiTools.cpp
        gui/HtmlExporter.cpp
        gui/IconModels.cpp
        gui/KMessageWidget.cpp
        gui/MainWindow.cpp
        gui/MessageBox.cpp
        gui/MessageWidget.cpp
        gui/PasswordWidget.cpp
        gui/PasswordGeneratorWidget.cpp
        gui/ApplicationSettingsWidget.cpp
        gui/Icons.cpp
        gui/SearchWidget.cpp
        gui/SettingsWidget.cpp
        gui/SortFilterHideProxyModel.cpp
        gui/SquareSvgWidget.cpp
        gui/ShortcutSettingsPage.cpp
        gui/TotpSetupDialog.cpp
        gui/TotpDialog.cpp
        gui/TotpExportSettingsDialog.cpp
        gui/DatabaseOpenDialog.cpp
        gui/URLEdit.cpp
        gui/UrlTools.cpp
        gui/WelcomeWidget.cpp
        gui/csvImport/CsvImportWidget.cpp
        gui/csvImport/CsvParserModel.cpp
        gui/entry/AutoTypeAssociationsModel.cpp
        gui/entry/EditEntryWidget.cpp
        gui/entry/EntryAttachmentsModel.cpp
        gui/entry/EntryAttachmentsWidget.cpp
        gui/entry/EntryAttributesModel.cpp
        gui/entry/NewEntryAttachmentsDialog.cpp
        gui/entry/PreviewEntryAttachmentsDialog.cpp
        gui/entry/EntryHistoryModel.cpp
        gui/entry/EntryModel.cpp
        gui/entry/EntryView.cpp
        gui/export/ExportDialog.cpp
        gui/group/EditGroupWidget.cpp
        gui/group/GroupModel.cpp
        gui/group/GroupView.cpp
        gui/tag/TagModel.cpp
        gui/tag/TagView.cpp
        gui/tag/TagsEdit.cpp
        gui/databasekey/KeyComponentWidget.cpp
        gui/databasekey/PasswordEditWidget.cpp
        gui/databasekey/YubiKeyEditWidget.cpp
        gui/databasekey/KeyFileEditWidget.cpp
        gui/dbsettings/DatabaseSettingsWidget.cpp
        gui/dbsettings/DatabaseSettingsDialog.cpp
        gui/dbsettings/DatabaseSettingsWidgetGeneral.cpp
        gui/dbsettings/DatabaseSettingsWidgetMaintenance.cpp
        gui/dbsettings/DatabaseSettingsWidgetMetaDataSimple.cpp
        gui/dbsettings/DatabaseSettingsWidgetEncryption.cpp
        gui/dbsettings/DatabaseSettingsWidgetDatabaseKey.cpp
        gui/remote/DatabaseSettingsWidgetRemote.cpp
        gui/remote/RemoteHandler.cpp
        gui/remote/RemoteProcess.cpp
        gui/remote/RemoteSettings.cpp
        gui/reports/ReportsWidget.cpp
        gui/reports/ReportsDialog.cpp
        gui/reports/ReportsWidgetHealthcheck.cpp
        gui/reports/ReportsPageHealthcheck.cpp
        gui/reports/ReportsWidgetHibp.cpp
        gui/reports/ReportsPageHibp.cpp
        gui/reports/ReportsWidgetStatistics.cpp
        gui/reports/ReportsPageStatistics.cpp
        gui/osutils/OSUtilsBase.cpp
        gui/osutils/ScreenLockListener.cpp
        gui/osutils/ScreenLockListenerPrivate.cpp
        gui/widgets/ElidedLabel.cpp
        gui/widgets/KPToolBar.cpp
        gui/widgets/PopupHelpWidget.cpp
        gui/widgets/ShortcutWidget.cpp
        gui/wizard/ImportWizard.cpp
        gui/wizard/ImportWizardPageReview.cpp
        gui/wizard/ImportWizardPageSelect.cpp
        gui/wizard/NewDatabaseWizard.cpp
        gui/wizard/NewDatabaseWizardPage.cpp
        gui/wizard/NewDatabaseWizardPageMetaData.cpp
        gui/wizard/NewDatabaseWizardPageEncryption.cpp
        gui/wizard/NewDatabaseWizardPageDatabaseKey.cpp
        quickunlock/QuickUnlockInterface.cpp
        ../share/icons/icons.qrc
        ../share/wizard/wizard.qrc)

if(APPLE)
    list(APPEND gui_SOURCES
            gui/osutils/macutils/MacPasteboard.cpp
            gui/osutils/macutils/MacUtils.cpp
            gui/osutils/macutils/ScreenLockListenerMac.cpp
            gui/osutils/macutils/AppKitImpl.mm
            gui/osutils/macutils/AppKit.h
            quickunlock/TouchID.mm)

    # TODO: Remove -Wno-error once deprecation warnings have been resolved.
    set_source_files_properties(quickunlock/TouchID.mm PROPERTY COMPILE_FLAGS "-Wno-old-style-cast")
endif()

if(UNIX AND NOT APPLE)
    list(APPEND gui_SOURCES
            gui/osutils/nixutils/ScreenLockListenerDBus.cpp
            gui/osutils/nixutils/NixUtils.cpp)
    if("${CMAKE_SYSTEM}" MATCHES "Linux")
        list(APPEND core_SOURCES
                quickunlock/Polkit.cpp
                quickunlock/PolkitDbusTypes.cpp)
    endif()
    if(WITH_XC_X11)
        list(APPEND gui_SOURCES
            gui/osutils/nixutils/X11Funcs.cpp)
    endif()
    qt5_add_dbus_adaptor(gui_SOURCES
            gui/org.keepassxc.KeePassXC.MainWindow.xml
            gui/MainWindow.h
            MainWindow)

    set_source_files_properties(
        quickunlock/dbus/org.freedesktop.PolicyKit1.Authority.xml
        PROPERTIES
        INCLUDE "quickunlock/PolkitDbusTypes.h"
        )
    qt5_add_dbus_interface(core_SOURCES
        quickunlock/dbus/org.freedesktop.PolicyKit1.Authority.xml
        polkit_dbus
    )

    find_library(KEYUTILS_LIBRARIES NAMES keyutils)
    if(NOT KEYUTILS_LIBRARIES)
		message(FATAL_ERROR "Could not find libkeyutils")
    endif()
endif()

if(WIN32)
    list(APPEND gui_SOURCES
            gui/osutils/winutils/ScreenLockListenerWin.cpp
            gui/osutils/winutils/WinUtils.cpp)
    if (MSVC)
        list(APPEND gui_SOURCES quickunlock/WindowsHello.cpp)
    endif()
endif()

if(WITH_XC_YUBIKEY)
    list(APPEND gui_SOURCES gui/osutils/DeviceListener.cpp)
    if(APPLE)
        list(APPEND gui_SOURCES gui/osutils/macutils/DeviceListenerMac.cpp)
    elseif(UNIX)
        list(APPEND gui_SOURCES gui/osutils/nixutils/DeviceListenerLibUsb.cpp)
    elseif(WIN32)
        list(APPEND gui_SOURCES gui/osutils/winutils/DeviceListenerWin.cpp)
    endif()
endif()

add_subdirectory(browser)
add_subdirectory(proxy)
if(WITH_XC_BROWSER)
    set(browser_LIB browser)
    list(APPEND gui_SOURCES
            gui/dbsettings/DatabaseSettingsWidgetBrowser.cpp
            gui/entry/EntryURLModel.cpp
            gui/reports/ReportsWidgetBrowserStatistics.cpp
            gui/reports/ReportsPageBrowserStatistics.cpp)
endif()

if(WITH_XC_BROWSER_PASSKEYS)
    list(APPEND gui_SOURCES
            gui/reports/ReportsWidgetPasskeys.cpp
            gui/reports/ReportsPagePasskeys.cpp
            gui/passkeys/PasskeyExporter.cpp
            gui/passkeys/PasskeyExportDialog.cpp
            gui/passkeys/PasskeyImporter.cpp
            gui/passkeys/PasskeyImportDialog.cpp)
endif()

add_subdirectory(autotype)
add_subdirectory(cli)
add_subdirectory(qrcode)
set(qrcode_LIB qrcode)

add_subdirectory(keeshare)
if(WITH_XC_KEESHARE)
    set(keeshare_LIB keeshare)
endif()

add_subdirectory(sshagent)
if(WITH_XC_SSHAGENT)
    set(sshagent_LIB sshagent)
endif()

add_subdirectory(fdosecrets)
if(WITH_XC_FDOSECRETS)
    set(fdosecrets_LIB fdosecrets)
endif()

add_subdirectory(thirdparty)

set(autotype_SOURCES
        autotype/AutoType.cpp
        autotype/AutoTypeAction.cpp
        autotype/AutoTypeMatchModel.cpp
        autotype/AutoTypeMatchView.cpp
        autotype/AutoTypeSelectDialog.cpp
        autotype/PickcharsDialog.cpp
        autotype/WindowSelectComboBox.cpp)

add_library(autotype STATIC ${autotype_SOURCES})
target_link_libraries(autotype Qt5::Core Qt5::Widgets)

if(WITH_XC_YUBIKEY)
    list(APPEND core_SOURCES
        keys/drivers/YubiKey.h
        keys/drivers/YubiKey.cpp
        keys/drivers/YubiKeyInterface.cpp
        keys/drivers/YubiKeyInterfaceUSB.cpp
        keys/drivers/YubiKeyInterfacePCSC.cpp)
else()
    list(APPEND core_SOURCES
        keys/drivers/YubiKey.h
        keys/drivers/YubiKeyStub.cpp)
endif()

if(WITH_XC_NETWORKING)
    list(APPEND gui_SOURCES
            networking/HibpDownloader.cpp
            networking/NetworkManager.cpp
            networking/UpdateChecker.cpp
            gui/UpdateCheckDialog.cpp
            gui/IconDownloader.cpp
            gui/IconDownloaderDialog.cpp)
endif()

configure_file(config-keepassx.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-keepassx.h)
configure_file(git-info.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/git-info.h)

# Core Library Definition
add_library(keepassxc_core STATIC ${core_SOURCES})
set_target_properties(keepassxc_core PROPERTIES COMPILE_DEFINITIONS KEEPASSX_BUILDING_CORE)
target_link_libraries(keepassxc_core 
        ${qrcode_LIB}
        Qt5::Core
        Qt5::Concurrent
        ${BOTAN_LIBRARIES}
        ${PCSC_LIBRARIES}
        ${ZXCVBN_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${MINIZIP_LIBRARIES}
        ${ARGON2_LIBRARIES}
        ${KEYUTILS_LIBRARIES}
        ${thirdparty_LIBRARIES})

# GUI Library Definition
add_library(keepassxc_gui STATIC ${gui_SOURCES})
set_target_properties(keepassxc_gui PROPERTIES COMPILE_DEFINITIONS KEEPASSX_BUILDING_CORE)
target_link_libraries(keepassxc_gui
        keepassxc_core
        Qt5::Network
        Qt5::Widgets
        autotype
        ${browser_LIB}
        ${fdosecrets_LIB}
        ${keeshare_LIB}
        ${sshagent_LIB})

if(APPLE)
    target_link_libraries(keepassxc_gui "-framework Foundation -framework AppKit -framework Carbon -framework Security -framework LocalAuthentication -framework ScreenCaptureKit")
    if(Qt5MacExtras_FOUND)
        target_link_libraries(keepassxc_gui Qt5::MacExtras)
    endif()
endif()
if(HAIKU)
    target_link_libraries(keepassxc_gui network)
endif()
if(UNIX AND NOT APPLE)
    target_link_libraries(keepassxc_core Qt5::DBus ${LIBUSB_LIBRARIES})
    if(WITH_XC_X11)
        target_link_libraries(keepassxc_gui Qt5::X11Extras X11)
    endif()
    include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})
endif()
if(WIN32)
    target_link_libraries(keepassxc_gui Wtsapi32.lib Ws2_32.lib)
    if (MSVC)
        target_link_libraries(keepassxc_gui WindowsApp.lib)
    endif()
endif()

# Main Executable Definition
if(WIN32)
    include(GenerateProductVersion)
    generate_product_version(
            WIN32_ResourceFiles
            NAME "KeePassXC"
            COMPANY_NAME "KeePassXC Team"
            VERSION_MAJOR ${KEEPASSXC_VERSION_MAJOR}
            VERSION_MINOR ${KEEPASSXC_VERSION_MINOR}
            VERSION_PATCH ${KEEPASSXC_VERSION_PATCH}
    )
    list(APPEND WIN32_ResourceFiles "${CMAKE_SOURCE_DIR}/share/windows/icon.rc")
endif()

add_executable(${PROGNAME} WIN32 main.cpp ${WIN32_ResourceFiles})
target_link_libraries(${PROGNAME} keepassxc_gui)
set_target_properties(${PROGNAME} PROPERTIES ENABLE_EXPORTS ON)

# macOS App Bundle
if(APPLE AND WITH_APP_BUNDLE)
    install(FILES ${CMAKE_SOURCE_DIR}/share/macosx/embedded.provisionprofile DESTINATION ${BUNDLE_INSTALL_DIR})
    configure_file(${CMAKE_SOURCE_DIR}/share/macosx/Info.plist.cmake ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)
    set_target_properties(${PROGNAME} PROPERTIES
            MACOSX_BUNDLE ON
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
            CPACK_BUNDLE_APPLE_ENTITLEMENTS "${CMAKE_SOURCE_DIR}/share/macosx/keepassxc.entitlements")

    if(QT_MAC_USE_COCOA AND EXISTS "${QT_LIBRARY_DIR}/Resources/qt_menu.nib")
        install(DIRECTORY "${QT_LIBRARY_DIR}/Resources/qt_menu.nib"
                DESTINATION "${DATA_INSTALL_DIR}")
    endif()

    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_DMG_DS_STORE "${CMAKE_SOURCE_DIR}/share/macosx/DS_Store.in")
    set(CPACK_DMG_BACKGROUND_IMAGE "${CMAKE_SOURCE_DIR}/share/macosx/background.tiff")
    set(CPACK_DMG_VOLUME_NAME "${PROGNAME}")
    set(CPACK_SYSTEM_NAME "OSX")
    set(CPACK_STRIP_FILES ON)
    set(CPACK_PACKAGE_FILE_NAME "${PROGNAME}-${KEEPASSXC_VERSION}")
    include(CPack)
endif()

install(TARGETS ${PROGNAME}
        BUNDLE DESTINATION . COMPONENT Runtime
        RUNTIME DESTINATION ${BIN_INSTALL_DIR} COMPONENT Runtime)

# Windows Installer Definition
if(WIN32)
    if(${CMAKE_SIZEOF_VOID_P} EQUAL "8")
        set(OUTPUT_FILE_POSTFIX "Win64")
    else()
        set(OUTPUT_FILE_POSTFIX "Win32")
    endif()

    # We have to copy the license file in the configuration phase.
    # CMake checks that CPACK_RESOURCE_FILE_LICENSE actually exists and
    # we have to copy it because WiX needs it to have a .txt extension.
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/LICENSE.GPL-2"
            "${CMAKE_CURRENT_BINARY_DIR}/INSTALLER_LICENSE.txt")

    # Prepare portal zip file
    set(CPACK_INSTALL_SCRIPTS "${CMAKE_SOURCE_DIR}/cmake/MakePortableZip.cmake")

    string(REGEX REPLACE "-.*$" "" KEEPASSXC_VERSION_CLEAN ${KEEPASSXC_VERSION})

    set(CPACK_GENERATOR "ZIP;WIX")
    set(CPACK_COMPONENTS_ALL "")
    set(CPACK_STRIP_FILES OFF)
    set(CPACK_PACKAGE_FILE_NAME "${PROGNAME}-${KEEPASSXC_VERSION}-${OUTPUT_FILE_POSTFIX}")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY ${PROGNAME})
    set(CPACK_PACKAGE_VERSION ${KEEPASSXC_VERSION_CLEAN})
    set(CPACK_PACKAGE_VENDOR "${PROGNAME} Team")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_BINARY_DIR}/INSTALLER_LICENSE.txt")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/share/windows/keepassxc.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CPACK_NSIS_MUI_ICON}")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "\\\\${PROGNAME}.exe")
    set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}/share/windows/installer-wizard.png")
    set(CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP "${CMAKE_SOURCE_DIR}/share/windows/installer-wizard.png")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\${PROGNAME}.lnk' '$INSTDIR\\\\${PROGNAME}.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA "Delete '$SMPROGRAMS\\\\$START_MENU\\\\${PROGNAME}.lnk'")
    set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS "ExecWait 'Taskkill /IM KeePassXC.exe'")
    set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS "${CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS}\nExecWait 'Taskkill /IM keepassxc-proxy.exe /F'")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "ExecWait 'Taskkill /IM KeePassXC.exe'")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "${CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS}\nExecWait 'Taskkill /IM keepassxc-proxy.exe /F'")
    set(CPACK_NSIS_URL_INFO_ABOUT "https://keepassxc.org")
    set(CPACK_NSIS_DISPLAY_NAME ${PROGNAME})
    set(CPACK_NSIS_PACKAGE_NAME "${PROGNAME} v${KEEPASSXC_VERSION}")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "../${PROGNAME}.exe")
    set(CPACK_WIX_UPGRADE_GUID 88785A72-3EAE-4F29-89E3-BC6B19BA9A5B)
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_SOURCE_DIR}/share/windows/keepassxc.ico")
    set(CPACK_WIX_UI_BANNER "${CMAKE_SOURCE_DIR}/share/windows/installer-banner.png")
    set(CPACK_WIX_UI_DIALOG "${CMAKE_SOURCE_DIR}/share/windows/installer-wizard.png")
    set(CPACK_WIX_UI_REF "KPXC_InstallDir")
    set(CPACK_WIX_TEMPLATE "${CMAKE_SOURCE_DIR}/share/windows/wix-template.xml")
    set(CPACK_WIX_PATCH_FILE "${CMAKE_SOURCE_DIR}/share/windows/wix-patch.xml")
    set(CPACK_WIX_EXTRA_SOURCES
            "${CMAKE_SOURCE_DIR}/share/windows/KPXC_InstallDir.wxs"
            "${CMAKE_SOURCE_DIR}/share/windows/KPXC_InstallDirDlg.wxs"
            "${CMAKE_SOURCE_DIR}/share/windows/KPXC_ExitDlg.wxs")
    set(CPACK_WIX_PROPERTY_ARPURLINFOABOUT  "https://keepassxc.org")
    set(CPACK_WIX_EXTENSIONS "WixUtilExtension.dll")
    include(CPack)

    if(NOT VCPKG_INSTALLED_DIR)
        install(CODE "set(gp_tool \"objdump\")" COMPONENT Runtime)

        # Deploy all 3rd party library dependencies first
        install(CODE "include(BundleUtilities)
                      fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/${PROGNAME}.exe\" \"\" \"\")"
                COMPONENT Runtime)

        # Use windeployqt.exe to setup Qt dependencies
        if(Qt5Core_VERSION VERSION_LESS "5.14.1")
            set(WINDEPLOYQT_MODE "--release")
            if(CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
                set(WINDEPLOYQT_MODE "--debug")
            endif()
        endif()

        install(CODE "execute_process(COMMAND ${WINDEPLOYQT_EXE} ${PROGNAME}.exe ${WINDEPLOYQT_MODE} WORKING_DIRECTORY \${CMAKE_INSTALL_PREFIX} OUTPUT_QUIET)"
                COMPONENT Runtime)

        # install OpenSSL library
        if(WITH_XC_NETWORKING)
            find_file(OPENSSL_DLL
                    NAMES libssl-3.dll libssl-3-x64.dll
                    HINTS "${OPENSSL_ROOT_DIR}/bin"
                    )
            if (NOT OPENSSL_DLL)
                message(FATAL_ERROR "Cannot find libssl dll, ensure openssl is properly installed.")
            endif()

            find_file(CRYPTO_DLL
                    NAMES libcrypto-3.dll libcrypto-3-x64.dll
                    HINTS "${OPENSSL_ROOT_DIR}/bin"
                    )
            if (NOT CRYPTO_DLL)
                message(FATAL_ERROR "Cannot find libcrypto dll, ensure openssl is properly installed.")
            endif()

            install(FILES ${OPENSSL_DLL} ${CRYPTO_DLL} DESTINATION ".")
        endif()
    endif()

    # install CA cert chains
    find_file(SSL_CA_BUNDLE ca-bundle.crt PATHS "${Qt5_PREFIX}/ssl/certs")
    if(SSL_CA_BUNDLE)
        install(FILES ${SSL_CA_BUNDLE} DESTINATION "ssl/certs")
    else()
        file(DOWNLOAD "https://curl.se/ca/cacert.pem" "${CMAKE_BINARY_DIR}/ca-bundle.crt" TLS_VERIFY ON)
        install(FILES "${CMAKE_BINARY_DIR}/ca-bundle.crt" DESTINATION "ssl/certs")
    endif()

endif()

# The install commands in this subdirectory will be executed after all the install commands in the
# current scope are ran. It is required for correct functioning of macdeployqt.
add_subdirectory(post_install)
