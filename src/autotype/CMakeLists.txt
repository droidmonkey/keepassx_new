# Main auto-type static library
set(autotype_SOURCES
        AutoType.cpp
        AutoTypeAction.cpp
        AutoTypeMatchModel.cpp
        AutoTypeMatchView.cpp
        AutoTypeSelectDialog.cpp
        PickcharsDialog.cpp
        WindowSelectComboBox.cpp)

add_library(autotype STATIC ${autotype_SOURCES})
target_link_libraries(autotype Qt5::Core Qt5::Widgets)

# Platform specific auto-type implementations
if(UNIX AND NOT APPLE AND NOT HAIKU)
    if(WITH_X11)
        find_package(X11 REQUIRED COMPONENTS Xi XTest)
        find_package(Qt5X11Extras REQUIRED)
        if(PRINT_SUMMARY)
            add_feature_info(libXi X11_Xi_FOUND "The X11 Xi Protocol library is required for auto-type")
            add_feature_info(libXtst X11_XTest_FOUND "The X11 XTEST Protocol library is required for auto-type")
            add_feature_info(Qt5X11Extras Qt5X11Extras_FOUND "The Qt5X11Extras library is required for auto-type")
        endif()

        add_subdirectory(xcb)
    endif()
elseif(APPLE)
    add_subdirectory(mac)
elseif(WIN32)
    add_subdirectory(windows)
endif()

# Auto-type tests
if(WITH_TESTS)
    add_subdirectory(test)
endif()
