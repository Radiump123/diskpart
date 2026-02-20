cmake_minimum_required(VERSION 3.16)
project(diskpart C)

if(UNIX AND NOT WIN32)
    include(GNUInstallDirs)

    add_executable(diskpart diskpart/linux_main.c)
    target_compile_features(diskpart PRIVATE c_std_99)

    # Keep Linux binary at build/diskpart for a predictable run path.
    set_target_properties(diskpart PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

    install(TARGETS diskpart
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
else()
    add_subdirectory(diskpart)
endif()
