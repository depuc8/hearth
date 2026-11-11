# Intentionally racy demo; never part of the default build or test run.
add_executable(race_demo race_demo.cpp)
target_link_libraries(race_demo PRIVATE Threads::Threads)

if("thread" IN_LIST HEARTH_SANITIZE)
  # Under TSan the plain-int run must produce a data-race report and the
  # atomic run must be clean. Opt-in only, so the regular tsan preset still
  # has zero reports.
  add_test(NAME RaceDemo.PlainIntIsReportedByTSan COMMAND race_demo plain)
  set_tests_properties(
    RaceDemo.PlainIntIsReportedByTSan
    PROPERTIES PASS_REGULAR_EXPRESSION "WARNING: ThreadSanitizer: data race"
               ENVIRONMENT "TSAN_OPTIONS=halt_on_error=0:exitcode=0")
  add_test(NAME RaceDemo.AtomicIsClean COMMAND race_demo atomic)
  set_tests_properties(RaceDemo.AtomicIsClean PROPERTIES FAIL_REGULAR_EXPRESSION
                                                       "ThreadSanitizer")
endif()
