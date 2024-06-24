set(keeshare_SOURCES
    SettingsPageKeeShare.cpp
    SettingsWidgetKeeShare.cpp
    DatabaseSettingsWidgetKeeShare.cpp
    group/EditGroupWidgetKeeShare.cpp
    group/EditGroupPageKeeShare.cpp
    KeeShare.cpp
    KeeShareSettings.cpp
    ShareImport.cpp
    ShareExport.cpp
    ShareObserver.cpp
    )

add_library(keeshare STATIC ${keeshare_SOURCES})
target_link_libraries(keeshare PUBLIC Qt5::Core Qt5::Widgets ${BOTAN_LIBRARIES} ${ZLIB_LIBRARIES} PRIVATE ${MINIZIP_LIBRARIES})
include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})
