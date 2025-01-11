include_directories(SYSTEM ${X11_X11_INCLUDE_PATH})

set(autotype_XCB_SOURCES AutoTypeXCB.cpp)

add_library(keepassxc-autotype-xcb MODULE ${autotype_XCB_SOURCES})
target_link_libraries(keepassxc-autotype-xcb keepassxc_gui Qt6::Core Qt6::Widgets Qt6::X11Extras ${X11_X11_LIB} ${X11_Xi_LIB} ${X11_XTest_LIB})
install(TARGETS keepassxc-autotype-xcb
        BUNDLE DESTINATION . COMPONENT Runtime
        LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR} COMPONENT Runtime)
