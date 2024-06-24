if(KPXC_FEATURE_SSHAGENT)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

    set(sshagent_SOURCES
        AgentSettingsPage.cpp
        AgentSettingsWidget.cpp
        ASN1Key.cpp
        BinaryStream.cpp
        KeeAgentSettings.cpp
        OpenSSHKey.cpp
        OpenSSHKeyGen.cpp
        OpenSSHKeyGenDialog.cpp
        SSHAgent.cpp
    )

    add_library(sshagent STATIC ${sshagent_SOURCES})
    target_link_libraries(sshagent Qt5::Core Qt5::Widgets Qt5::Network)
endif()
