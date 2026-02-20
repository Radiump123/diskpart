cmake_minimum_required(VERSION 3.16)
project(diskpart C)

if(NOT UNIX OR WIN32)
    message(FATAL_ERROR "This repository is Linux-only. Build on a Linux/Unix host.")
endif()

include(GNUInstallDirs)

add_executable(diskpart diskpart/linux_main.c)
target_compile_features(diskpart PRIVATE c_std_99)

# Emit binary where users expect it.
set_target_properties(diskpart PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/diskpart")

install(TARGETS diskpart
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
