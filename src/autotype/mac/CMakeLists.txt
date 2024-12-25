set(autotype_mac_SOURCES AutoTypeMac.cpp)

add_library(keepassxc-autotype-cocoa MODULE ${autotype_mac_SOURCES})
set_target_properties(keepassxc-autotype-cocoa PROPERTIES LINK_FLAGS "-framework Foundation -framework AppKit -framework Carbon -framework ScreenCaptureKit")
target_link_libraries(keepassxc-autotype-cocoa ${PROGNAME} Qt5::Core Qt5::Widgets)

install(TARGETS keepassxc-autotype-cocoa
        LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR} COMPONENT Runtime)

if(WITH_APP_BUNDLE)
    add_custom_command(TARGET keepassxc-autotype-cocoa
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libkeepassxc-autotype-cocoa.so ${PLUGIN_INSTALL_DIR}/libkeepassxc-autotype-cocoa.so
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/src
            COMMENT "Copying autotype plugin into app bundle")

    set_property(GLOBAL APPEND PROPERTY
        _MACDEPLOYQT_EXTRA_BINARIES "${PLUGIN_INSTALL_DIR}/libkeepassxc-autotype-cocoa.so")
endif()
