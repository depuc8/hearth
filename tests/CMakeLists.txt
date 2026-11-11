include(FetchContent)

FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
  URL_HASH
    SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c)
set(gtest_force_shared_crt
    ON
    CACHE BOOL "" FORCE)
set(INSTALL_GTEST
    OFF
    CACHE BOOL "" FORCE)
set(BUILD_GMOCK
    OFF
    CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(
  hearth_tests
  version_test.cpp
  net_test.cpp
  http_test.cpp
  parser_test.cpp)
target_link_libraries(hearth_tests PRIVATE hearth_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(hearth_tests DISCOVERY_TIMEOUT 60)
