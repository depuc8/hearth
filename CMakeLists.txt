cmake_minimum_required(VERSION 3.16)

project(
  hearth
  VERSION 0.1.0
  DESCRIPTION "A small multithreaded HTTP/1.1 server in C++17"
  LANGUAGES CXX)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE
      Release
      CACHE STRING "Build type" FORCE)
endif()

option(HEARTH_BUILD_TESTS "Build the GoogleTest suite" ON)
option(HEARTH_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(HEARTH_COVERAGE "Instrument for code coverage (GCC/Clang)" OFF)
option(HEARTH_BUILD_RACE_DEMO "Build the intentionally racy counter demo" OFF)
set(HEARTH_SANITIZE
    ""
    CACHE STRING "Sanitizers to enable, e.g. 'address;undefined' or 'thread'")

include(cmake/HearthOptions.cmake)

find_package(Threads REQUIRED)
enable_testing()

add_subdirectory(src)

if(HEARTH_BUILD_TESTS)
  add_subdirectory(tests)
endif()

if(HEARTH_BUILD_RACE_DEMO)
  add_subdirectory(docs/race-demo)
endif()

include(GNUInstallDirs)
install(TARGETS hearth RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
