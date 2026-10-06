target_sources(
    ${CMAKE_PROJECT_NAME}
    PRIVATE LogEntryTest.cc
            LogEntryTest.h
            LogTestHelpers.h
            LogFormatterTest.cc
            LogFormatterTest.h
            LogManagerTest.cc
            LogManagerTest.h
            LogModelTest.cc
            LogModelTest.h
            LoggingCategoryModelTest.cc
            LoggingCategoryModelTest.h
            LoggingQmlBindingTest.cc
            LoggingQmlBindingTest.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_qgc_test(LogEntryTest LABELS Unit Utilities)
add_qgc_test(LogFormatterTest LABELS Unit Utilities)
# Exercise QGC_LOG_LEVEL without higher-priority CTest/host Qt logging overrides.
add_qgc_test(
    LogManagerTest
    LABELS
    Unit
    Utilities
    ENV_MODIFICATION
    "QT_LOGGING_RULES=unset:"
    "QT_LOGGING_CONF=unset:"
)
add_qgc_test(LogModelTest LABELS Unit Utilities)
add_qgc_test(LoggingCategoryModelTest LABELS Unit Utilities)
add_qgc_test(LoggingQmlBindingTest LABELS Unit Utilities)
