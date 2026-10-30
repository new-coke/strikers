# The port's own tests: small, no Aurora, no game data, so they run in every configuration.
add_executable(chunk_bounds_test
    chunk_bounds_test.c
    ../src/platform/cam_endian.c
    ../src/platform/skin_endian.c
    ../src/platform/wld_endian.c
    ../src/platform/bmd_endian.c)
target_include_directories(chunk_bounds_test PRIVATE ../include)
add_test(NAME chunk_bounds COMMAND chunk_bounds_test)

add_executable(prompts_resolve_test
    prompts_resolve_test.cpp
    ../src/platform/prompts_resolve.cpp)
target_include_directories(prompts_resolve_test PRIVATE ../include)
add_test(NAME prompts_resolve COMMAND prompts_resolve_test)

add_executable(online_desc_test
    online_desc_test.cpp
    ../src/platform/online/desc.cpp)
target_include_directories(online_desc_test PRIVATE ../include)
add_test(NAME online_desc COMMAND online_desc_test)

add_executable(online_panel_test
    online_panel_test.cpp
    ../src/platform/online/panel_edit.cpp
    ../src/platform/online/panel_text.cpp)
target_include_directories(online_panel_test PRIVATE ../include ../src/platform/online)
add_test(NAME online_panel COMMAND online_panel_test)

# The online session without the game: its wire format, then two machines over a simulated network.
set(ONLINE_WIRE_SOURCES
    ../src/platform/online/desc.cpp
    ../src/platform/online/wire.cpp
    ../src/platform/online/seal.cpp
    ../src/platform/online/addr.cpp
    ../src/platform/online/stun.cpp
    ${MONOCYPHER_SRC}/monocypher.c)
set(ONLINE_SESSION_SOURCES
    ${ONLINE_WIRE_SOURCES}
    ../src/platform/online/engine.cpp
    ../src/platform/online/session.cpp
    ../src/platform/online/link.cpp
    ../src/platform/online/match.cpp)
set(ONLINE_TEST_OUT "${CMAKE_CURRENT_BINARY_DIR}/online-out")

add_executable(online_wire_test online_wire_test.cpp online_fuzz_target.cpp ${ONLINE_WIRE_SOURCES})
target_include_directories(online_wire_test PRIVATE ../include ${MONOCYPHER_SRC})
add_test(NAME online_wire COMMAND online_wire_test)

add_executable(online_lockstep_test online_lockstep_test.cpp ${ONLINE_SESSION_SOURCES})
target_include_directories(online_lockstep_test PRIVATE ../include ${MONOCYPHER_SRC})
target_compile_definitions(online_lockstep_test PRIVATE ONLINE_TEST_OUT="${ONLINE_TEST_OUT}")
add_test(NAME online_lockstep COMMAND online_lockstep_test)

# Real UDP on 127.0.0.1, through the C API and its network thread.
if(UNIX)
    find_package(Threads REQUIRED)
    add_executable(online_api_test online_api_test.cpp ../src/platform/online/api.cpp
        ../src/platform/online/udp.cpp ${ONLINE_SESSION_SOURCES})
    target_include_directories(online_api_test PRIVATE ../include ${MONOCYPHER_SRC}
        "${CMAKE_BINARY_DIR}/version")
    target_compile_definitions(online_api_test PRIVATE ONLINE_TEST_OUT="${ONLINE_TEST_OUT}")
    target_link_libraries(online_api_test PRIVATE Threads::Threads)
    add_test(NAME online_api COMMAND online_api_test)
endif()

# The two tool tests, pure Python, so they are one `ctest` away.
add_test(NAME tool_genstubs
         COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_genstubs.py")
add_test(NAME tool_extract_disc
         COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_extract_disc.py")

if(UNIX)
    add_executable(switch_dvd_test switch_dvd_test.c)
    target_include_directories(switch_dvd_test PRIVATE ../include)
    add_test(NAME switch_dvd COMMAND switch_dvd_test)
    add_test(NAME tool_switch_rebuild
             COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_switch_rebuild.py")
endif()

add_test(NAME online_translations
         COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/gen-online-translations.py" --check)
