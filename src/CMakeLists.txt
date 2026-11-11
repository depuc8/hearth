add_library(hearth_core STATIC version.cpp)

target_include_directories(hearth_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hearth_core PUBLIC hearth_options Threads::Threads)
target_compile_definitions(hearth_core
                           PUBLIC HEARTH_VERSION="${PROJECT_VERSION}")

add_executable(hearth main.cpp)
target_link_libraries(hearth PRIVATE hearth_core)
