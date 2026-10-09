# chime: runs a command and chimes when it ends.
# Copyright (C) 2026 Daher Alfawares
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.25)
project(chime VERSION 0.1.0 LANGUAGES CXX
        DESCRIPTION "Run a command, and chime when it ends"
        HOMEPAGE_URL "https://github.com/da0x/chime")

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(chime chime.cpp)
target_compile_definitions(chime PRIVATE CHIME_VERSION="${PROJECT_VERSION}")
target_compile_options(chime PRIVATE -Wall -Wextra -Wpedantic)

install(TARGETS chime RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES chime.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

include(CTest)
if(BUILD_TESTING)
  add_test(NAME chime COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run.sh $<TARGET_FILE:chime>)
endif()
