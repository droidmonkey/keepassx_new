if(WITH_XC_FDOSECRETS)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

    add_library(fdosecrets STATIC
        # app settings page
        FdoSecretsPlugin.cpp
        widgets/SettingsModels.cpp
        widgets/SettingsWidgetFdoSecrets.cpp
        widgets/RowButtonHelper.cpp

        # per database settings page
        widgets/DatabaseSettingsWidgetFdoSecrets.cpp

        # prompt dialog
        widgets/AccessControlDialog.cpp

        # setting storage
        FdoSecretsSettings.cpp

        # dbus objects
        dbus/DBusClient.cpp
        dbus/DBusMgr.cpp
        dbus/DBusDispatch.cpp
        dbus/DBusObject.cpp
        objects/Service.cpp
        objects/Session.cpp
        objects/SessionCipher.cpp
        objects/Collection.cpp
        objects/Item.cpp
        objects/Prompt.cpp
        dbus/DBusTypes.cpp
    )
    target_link_libraries(fdosecrets Qt6::Core Qt6::Widgets Qt6::DBus ${BOTAN_LIBRARIES})
endif()
