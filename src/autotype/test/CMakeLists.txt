set(autotype_test_SOURCES AutoTypeTest.cpp)

add_library(keepassxc-autotype-test MODULE ${autotype_test_SOURCES})
target_link_libraries(keepassxc-autotype-test keepassxc_gui ${autotype_LIB} Qt6::Core Qt6::Widgets)
