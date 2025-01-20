if(WITH_XC_X11)
    find_package(X11 REQUIRED COMPONENTS Xi XTest)
        
    if(PRINT_SUMMARY)
        add_feature_info(libXi X11_Xi_FOUND "The X11 Xi Protocol library is required for auto-type")
        add_feature_info(libXtst X11_XTest_FOUND "The X11 XTEST Protocol library is required for auto-type")
    endif()

    include_directories(SYSTEM ${X11_X11_INCLUDE_PATH})

    set(autotype_XCB_SOURCES AutoTypeXCB.cpp)

    add_library(keepassxc-autotype-xcb MODULE ${autotype_XCB_SOURCES})
    target_link_libraries(keepassxc-autotype-xcb keepassxc_gui Qt5::Core Qt5::Widgets Qt5::X11Extras ${X11_X11_LIB} ${X11_Xi_LIB} ${X11_XTest_LIB})
    install(TARGETS keepassxc-autotype-xcb
            BUNDLE DESTINATION . COMPONENT Runtime
            LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR} COMPONENT Runtime)
endif()
