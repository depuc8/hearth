# Interface target carrying warning, sanitizer and coverage flags. Every
# first-party target links it; third-party code (GoogleTest) does not get
# -Werror, but does get sanitizer flags via the global options below so that
# instrumented and uninstrumented code are never mixed.

add_library(hearth_options INTERFACE)

target_compile_options(
  hearth_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow
                           $<$<BOOL:${HEARTH_WARNINGS_AS_ERRORS}>:-Werror>)

if(HEARTH_SANITIZE)
  list(JOIN HEARTH_SANITIZE "," _hearth_san)
  message(STATUS "hearth: sanitizers enabled: ${_hearth_san}")
  add_compile_options(-fsanitize=${_hearth_san} -fno-omit-frame-pointer
                      -fno-sanitize-recover=all)
  add_link_options(-fsanitize=${_hearth_san})
endif()

if(HEARTH_COVERAGE)
  message(STATUS "hearth: coverage instrumentation enabled")
  target_compile_options(hearth_options INTERFACE --coverage -O0 -g)
  target_link_options(hearth_options INTERFACE --coverage)
endif()
