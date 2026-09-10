add_executable(inifile_test roundtrip.cpp)
target_link_libraries(inifile_test PRIVATE strikers-settings-core)

# The shipped example file is the fixture.
add_test(NAME inifile_roundtrip
         COMMAND inifile_test "${CMAKE_CURRENT_SOURCE_DIR}/../../strikers.ini.example")

# The translations are a resource, and a resource path is not something "it builds" or the
# round-trip test can check, that one links no resources.
add_test(NAME settings_translations
         COMMAND strikers-settings --langs)
set_tests_properties(settings_translations PROPERTIES
                     ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
