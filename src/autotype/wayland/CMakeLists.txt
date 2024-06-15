find_package(Xkbcommon REQUIRED)

set(autotype_WAYLAND_SOURCES AutoTypeWayland.cpp)

add_library(keepassxc-autotype-wayland MODULE ${autotype_WAYLAND_SOURCES})
target_link_libraries(keepassxc-autotype-wayland keepassxc_gui Qt5::Core Qt5::Widgets Qt5::DBus ${Xkbcommon_LIBRARIES})
install(TARGETS keepassxc-autotype-wayland
        BUNDLE DESTINATION . COMPONENT Runtime
        LIBRARY DESTINATION ${PLUGIN_INSTALL_DIR} COMPONENT Runtime)
