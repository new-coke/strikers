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

# Host-only validation of the direct-native GXM upload contract. Invalid
# geometry or missing material coverage must be rejected, not GX-fallbacked.
if(NOT STRIKERS_VITA)
    if(UNIX)
        add_executable(native_render_limiter_test native_render_limiter_test.c ../src/platform/vi.c)
        target_compile_definitions(native_render_limiter_test PRIVATE TARGET_PC=1)
        target_include_directories(native_render_limiter_test PRIVATE ../include)
        target_compile_options(native_render_limiter_test PRIVATE -UNDEBUG)
        target_link_libraries(native_render_limiter_test PRIVATE m)
        add_test(NAME native_render_limiter COMMAND native_render_limiter_test)
    endif()
    if(UNIX AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_executable(simulation_task_clock_test simulation_task_clock_test.cpp ../src/NL/nlTask.cpp)
        target_compile_features(simulation_task_clock_test PRIVATE cxx_std_20)
        target_compile_definitions(simulation_task_clock_test PRIVATE TARGET_PC=1 STRIKERS_VITA_NATIVE_RUNTIME=1)
        target_include_directories(simulation_task_clock_test PRIVATE ../include)
        target_compile_options(simulation_task_clock_test PRIVATE -UNDEBUG -fdeclspec -fshort-wchar
            -Wno-register -Wno-ignored-attributes -include "${CMAKE_CURRENT_SOURCE_DIR}/../include/port/prelude.h")
        add_test(NAME simulation_task_clock COMMAND simulation_task_clock_test)
    endif()
    add_executable(nis_render_state_test nis_render_state_test.cpp)
    target_include_directories(nis_render_state_test PRIVATE ../include)
    target_compile_options(nis_render_state_test PRIVATE -UNDEBUG)
    add_test(NAME nis_render_state COMMAND nis_render_state_test)
    add_executable(vitagl_circular_pool_test vitagl_circular_pool_test.cpp)
    target_include_directories(vitagl_circular_pool_test PRIVATE ../extern/vitaGL/source)
    target_compile_options(vitagl_circular_pool_test PRIVATE -UNDEBUG)
    add_test(NAME vitagl_circular_pool COMMAND vitagl_circular_pool_test)
    find_path(STRIKERS_NATIVE_XXHASH_INCLUDE_DIR xxhash.h HINTS
        "${CMAKE_BINARY_DIR}/_deps/xxhash-src"
        "${CMAKE_CURRENT_SOURCE_DIR}/../build-vita-gxm-latest/_deps/xxhash-src")
    if(STRIKERS_NATIVE_XXHASH_INCLUDE_DIR)
        if(UNIX AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_executable(native_match_runtime_driver_test native_match_runtime_driver_test.cpp
                ../src/platform/native_match_runtime.cpp ../src/platform/native_match_draw.cpp
                ../src/platform/native_match_geometry.cpp ../src/platform/native_match_material.cpp
                ../src/platform/native_match_resources.cpp ../src/platform/native_match_diagnostics.cpp
                ../src/platform/native_texture_record.cpp)
            target_compile_features(native_match_runtime_driver_test PRIVATE cxx_std_20)
            target_compile_definitions(native_match_runtime_driver_test PRIVATE
                STRIKERS_VITA_NATIVE_RUNTIME=1 STRIKERS_NATIVE_TEXTURE_TEST=1)
            target_compile_options(native_match_runtime_driver_test PRIVATE -UNDEBUG -Wno-register)
            target_include_directories(native_match_runtime_driver_test PRIVATE "${STRIKERS_NATIVE_XXHASH_INCLUDE_DIR}")
            target_link_libraries(native_match_runtime_driver_test PRIVATE port_flags)
            add_test(NAME native_match_runtime_driver COMMAND native_match_runtime_driver_test)
        endif()
        add_executable(strikers_compile_native_graphics ../tools/compile_native_graphics.cpp
            ../src/platform/native_graphics.cpp ../src/platform/native_texture_record.cpp)
        target_compile_features(strikers_compile_native_graphics PRIVATE cxx_std_20)
        target_compile_definitions(strikers_compile_native_graphics PRIVATE STRIKERS_NATIVE_TEXTURE_TEST=1)
        target_include_directories(strikers_compile_native_graphics PRIVATE ../include "${STRIKERS_NATIVE_XXHASH_INCLUDE_DIR}")
        add_executable(native_graphics_test native_graphics_test.cpp ../src/platform/native_graphics.cpp
            ../src/platform/native_texture_record.cpp ../src/platform/asset_archive.c)
        target_compile_features(native_graphics_test PRIVATE cxx_std_20)
        target_compile_definitions(native_graphics_test PRIVATE STRIKERS_NATIVE_TEXTURE_TEST=1)
        target_compile_options(native_graphics_test PRIVATE -UNDEBUG)
        target_include_directories(native_graphics_test PRIVATE ../include "${STRIKERS_NATIVE_XXHASH_INCLUDE_DIR}")
        find_package(ZLIB QUIET)
        if(TARGET ZLIB::ZLIB)
            target_compile_definitions(native_graphics_test PRIVATE STRIKERS_ZLIB=1)
            target_link_libraries(native_graphics_test PRIVATE ZLIB::ZLIB)
        endif()
        add_test(NAME native_graphics COMMAND native_graphics_test)
        add_test(NAME tool_native_graphics COMMAND ${CMAKE_COMMAND} -E env
            "STRIKERS_GRAPHICS_COMPILER=$<TARGET_FILE:strikers_compile_native_graphics>"
            python3 ${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_native_graphics.py)
        add_executable(native_texture_record_test native_texture_record_test.cpp
            ../src/platform/native_texture_record.cpp)
        target_compile_features(native_texture_record_test PRIVATE cxx_std_20)
        target_compile_definitions(native_texture_record_test PRIVATE STRIKERS_NATIVE_TEXTURE_TEST=1)
        target_include_directories(native_texture_record_test PRIVATE ../include
            "${STRIKERS_NATIVE_XXHASH_INCLUDE_DIR}")
        if(NOT MSVC)
            target_compile_options(native_texture_record_test PRIVATE -UNDEBUG)
        endif()
        add_test(NAME native_texture_record COMMAND native_texture_record_test)
        add_executable(vitagl_texture_test vitagl_texture_test.cpp ../src/platform/native_texture_record.cpp ../src/platform/asset_archive.c)
        target_compile_features(vitagl_texture_test PRIVATE cxx_std_20)
        target_compile_definitions(vitagl_texture_test PRIVATE STRIKERS_NATIVE_TEXTURE_TEST=1)
        target_include_directories(vitagl_texture_test PRIVATE ../include "${STRIKERS_NATIVE_XXHASH_INCLUDE_DIR}")
        target_compile_options(vitagl_texture_test PRIVATE -UNDEBUG)
        find_package(ZLIB QUIET)
        if(TARGET ZLIB::ZLIB)
            target_compile_definitions(vitagl_texture_test PRIVATE STRIKERS_ZLIB=1)
            target_link_libraries(vitagl_texture_test PRIVATE ZLIB::ZLIB)
        endif()
        add_test(NAME vitagl_texture COMMAND vitagl_texture_test)

    endif()

    add_executable(native_match_transform_test
        native_match_transform_test.cpp
        ../src/NL/plat/platvmath.cpp
        ../src/NL/glx/glxMatrix.cpp
        ../src/Dolphin/mtx/mtx44.c
        ../src/Dolphin/mtx/vec.c)
    target_compile_features(native_match_transform_test PRIVATE cxx_std_20)
    target_compile_options(native_match_transform_test PRIVATE -UNDEBUG
        $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>)
    target_link_libraries(native_match_transform_test PRIVATE port_flags m)
    add_test(NAME native_match_transform COMMAND native_match_transform_test)


    # Exercise the production renderer against a mock GPU/display driver with
    # the actual SDK declarations. The SDK's libc must follow the host libc.
    find_path(STRIKERS_VITA_SDK_INCLUDE_DIR psp2/gxm.h
        HINTS "$ENV{VITASDK}/arm-vita-eabi/include"
              "/usr/local/vitasdk/arm-vita-eabi/include")
    if(UNIX AND STRIKERS_VITA_SDK_INCLUDE_DIR AND
       CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        add_executable(native_audio_driver_test native_audio_driver_test.cpp ../src/platform/native_audio.cpp)
        target_compile_features(native_audio_driver_test PRIVATE cxx_std_20)
        target_compile_definitions(native_audio_driver_test PRIVATE PORT_VITA=1)
        target_include_directories(native_audio_driver_test PRIVATE ../include)
        target_compile_options(native_audio_driver_test PRIVATE -UNDEBUG)
        find_package(Threads REQUIRED)
        target_link_libraries(native_audio_driver_test PRIVATE Threads::Threads)
        add_test(NAME native_audio_driver COMMAND native_audio_driver_test)
        add_executable(native_boot_screen_test native_boot_screen_test.cpp ../src/platform/native_boot_screen.cpp)
        target_compile_features(native_boot_screen_test PRIVATE cxx_std_20)
        target_compile_definitions(native_boot_screen_test PRIVATE STRIKERS_BOOT_SCREEN_TEST=1)
        target_include_directories(native_boot_screen_test PRIVATE ../include)
        target_compile_options(native_boot_screen_test PRIVATE -UNDEBUG -idirafter "${STRIKERS_VITA_SDK_INCLUDE_DIR}")
        add_test(NAME native_boot_screen COMMAND native_boot_screen_test)
        add_executable(native_pack_verify_test native_pack_verify_test.cpp ../src/platform/native_pack_verify.cpp)
        target_compile_features(native_pack_verify_test PRIVATE cxx_std_20)
        target_compile_definitions(native_pack_verify_test PRIVATE STRIKERS_VITA_NATIVE_RUNTIME=1)
        target_include_directories(native_pack_verify_test PRIVATE ../include)
        target_compile_options(native_pack_verify_test PRIVATE -UNDEBUG -idirafter "${STRIKERS_VITA_SDK_INCLUDE_DIR}")
        add_test(NAME native_pack_verify COMMAND native_pack_verify_test)
        add_executable(deferred_renderer_driver_test deferred_renderer_driver_test.cpp ../src/platform/deferred_renderer.cpp)
        target_compile_features(deferred_renderer_driver_test PRIVATE cxx_std_20)
        target_compile_definitions(deferred_renderer_driver_test PRIVATE STRIKERS_VITA_VITAGL=1)
        target_include_directories(deferred_renderer_driver_test PRIVATE ../include)
        target_compile_options(deferred_renderer_driver_test PRIVATE -UNDEBUG -idirafter "${STRIKERS_VITA_SDK_INCLUDE_DIR}")
        find_package(Threads REQUIRED)
        target_link_libraries(deferred_renderer_driver_test PRIVATE Threads::Threads)
        add_test(NAME deferred_renderer_driver COMMAND deferred_renderer_driver_test)
        add_executable(audio_out_native_driver_test audio_out_native_driver_test.cpp ../src/platform/audio_out_native.cpp)
        target_compile_features(audio_out_native_driver_test PRIVATE cxx_std_20)
        target_compile_definitions(audio_out_native_driver_test PRIVATE STRIKERS_VITA_NATIVE_RUNTIME=1)
        target_include_directories(audio_out_native_driver_test PRIVATE ../include)
        target_compile_options(audio_out_native_driver_test PRIVATE -UNDEBUG -idirafter "${STRIKERS_VITA_SDK_INCLUDE_DIR}")
        target_link_libraries(audio_out_native_driver_test PRIVATE Threads::Threads)
        add_test(NAME audio_out_native_driver COMMAND audio_out_native_driver_test)
        add_executable(native_jobs_driver_test native_jobs_driver_test.cpp ../src/platform/native_jobs.cpp)
        target_compile_features(native_jobs_driver_test PRIVATE cxx_std_20)
        target_compile_definitions(native_jobs_driver_test PRIVATE STRIKERS_VITA_NATIVE_RUNTIME=1)
        target_include_directories(native_jobs_driver_test PRIVATE ../include)
        target_compile_options(native_jobs_driver_test PRIVATE -UNDEBUG -idirafter "${STRIKERS_VITA_SDK_INCLUDE_DIR}")
        find_package(Threads REQUIRED)
        target_link_libraries(native_jobs_driver_test PRIVATE Threads::Threads)
        add_test(NAME native_jobs_driver COMMAND native_jobs_driver_test)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../extern/vitaGL/source/vitaGL.h")
            add_executable(vitagl_renderer_driver_test vitagl_renderer_driver_test.cpp ../src/platform/vitagl_renderer.cpp)
            target_compile_features(vitagl_renderer_driver_test PRIVATE cxx_std_20)
            target_compile_definitions(vitagl_renderer_driver_test PRIVATE PORT_VITA=1 STRIKERS_VITA_VITAGL=1)
            target_include_directories(vitagl_renderer_driver_test PRIVATE ../include ../extern/vitaGL/source)
            target_compile_options(vitagl_renderer_driver_test PRIVATE -UNDEBUG -idirafter "${STRIKERS_VITA_SDK_INCLUDE_DIR}")
            add_test(NAME vitagl_renderer_driver COMMAND vitagl_renderer_driver_test)
        endif()

    endif()


    add_executable(native_model_vertices_test native_model_vertices_test.cpp)
    target_compile_features(native_model_vertices_test PRIVATE cxx_std_20)
    target_include_directories(native_model_vertices_test PRIVATE ../include)
    if(NOT MSVC)
        target_compile_options(native_model_vertices_test PRIVATE -UNDEBUG)
    endif()
    add_test(NAME native_model_vertices COMMAND native_model_vertices_test)
    add_executable(native_model_sources_test native_model_sources_test.cpp)
    target_compile_features(native_model_sources_test PRIVATE cxx_std_20)
    target_include_directories(native_model_sources_test PRIVATE ../include)
    if(NOT MSVC)
        target_compile_options(native_model_sources_test PRIVATE -UNDEBUG)
    endif()
    add_test(NAME native_model_sources COMMAND native_model_sources_test)

endif()

# The two tool tests, pure Python, so they are one `ctest` away.
add_test(NAME tool_genstubs
         COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_genstubs.py")
add_test(NAME tool_extract_disc
         COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_extract_disc.py")
add_test(NAME tool_vita_cpu_trace
         COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_analyze_vita_cpu_trace.py")
add_test(NAME tool_vita_frames
         COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_analyze_vita_frames.py")
add_test(NAME tool_vita_asset_io
         COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_analyze_vita_asset_io.py")
add_test(NAME tool_vita_performance
         COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_vita_performance_analysis.py")

if(UNIX)
    add_executable(glx_display_list_test
        glx_display_list_test.cpp
        ../src/NL/glx/glxDisplayList.cpp)
    set_target_properties(glx_display_list_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_compile_options(glx_display_list_test PRIVATE -UNDEBUG -Wno-register)
    target_link_libraries(glx_display_list_test PRIVATE port_flags)
    add_test(NAME glx_display_list COMMAND glx_display_list_test)
    add_executable(cpu_trace_hotpaths_test
        cpu_trace_hotpaths_test.cpp
        ../src/NL/plat/platvmath.cpp
        ../src/Dolphin/mtx/mtx44.c
        ../src/Dolphin/mtx/vec.c)
    set_target_properties(cpu_trace_hotpaths_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_compile_options(cpu_trace_hotpaths_test PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>)
    target_link_libraries(cpu_trace_hotpaths_test PRIVATE port_flags m)
    add_test(NAME cpu_trace_hotpaths COMMAND cpu_trace_hotpaths_test)
    add_executable(diagnostics_config_test diagnostics_config_test.c ../src/platform/config.c)
    target_include_directories(diagnostics_config_test PRIVATE ../include)
    add_test(NAME diagnostics_config COMMAND diagnostics_config_test)

    add_executable(vitagl_default_config_test vitagl_default_config_test.c ../src/platform/config.c)
    target_include_directories(vitagl_default_config_test PRIVATE ../include)
    target_compile_options(vitagl_default_config_test PRIVATE -UNDEBUG)
    target_compile_definitions(vitagl_default_config_test PRIVATE PORT_VITA=1 STRIKERS_VITA_VITAGL=1)
    add_test(NAME vitagl_default_config COMMAND vitagl_default_config_test)

    add_executable(benchmark_fps_test benchmark_fps_test.c ../src/platform/benchmark.c)
    target_include_directories(benchmark_fps_test PRIVATE ../include)
    target_link_libraries(benchmark_fps_test PRIVATE m)
    add_test(NAME benchmark_fps COMMAND benchmark_fps_test)
    add_executable(frame_capture_test frame_capture_test.c ../src/platform/benchmark.c)
    target_include_directories(frame_capture_test PRIVATE ../include)
    add_test(NAME frame_capture COMMAND frame_capture_test)
    add_executable(frame_capture_io_test frame_capture_io_test.c)
    target_include_directories(frame_capture_io_test PRIVATE ../include)
    add_test(NAME frame_capture_io COMMAND frame_capture_io_test)
    find_package(Threads REQUIRED)
    add_executable(skin_matrix_packets_test
        skin_matrix_packets_test.cpp
        ../src/Dolphin/mtx/mtx.c
        ../src/Dolphin/mtx/vec.c)
    set_target_properties(skin_matrix_packets_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_compile_options(skin_matrix_packets_test PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-register>)
    target_link_libraries(skin_matrix_packets_test PRIVATE port_flags Threads::Threads m)
    add_test(NAME skin_matrix_packets COMMAND skin_matrix_packets_test)
    add_executable(profile_output_test profile_output_test.cpp)
    target_include_directories(profile_output_test PRIVATE ../include)
    add_test(NAME profile_output COMMAND profile_output_test)
    add_executable(switch_dvd_test switch_dvd_test.c ../src/platform/asset_archive.c)
    target_include_directories(switch_dvd_test PRIVATE ../include)
    target_link_libraries(switch_dvd_test PRIVATE Threads::Threads)
    add_test(NAME switch_dvd COMMAND switch_dvd_test)
    add_executable(vita_dvd_test vita_dvd_test.c ../src/platform/asset_archive.c)
    target_include_directories(vita_dvd_test PRIVATE ../include)
    target_link_libraries(vita_dvd_test PRIVATE Threads::Threads)
    add_test(NAME vita_dvd COMMAND vita_dvd_test)
    add_executable(asset_archive_probe asset_archive_probe.c ../src/platform/asset_archive.c)
    target_include_directories(asset_archive_probe PRIVATE ../include)
    target_link_libraries(asset_archive_probe PRIVATE Threads::Threads)
    find_package(ZLIB QUIET)
    if(TARGET ZLIB::ZLIB)
        target_compile_definitions(asset_archive_probe PRIVATE STRIKERS_ZLIB=1)
        target_link_libraries(asset_archive_probe PRIVATE ZLIB::ZLIB)
    endif()
    add_test(NAME tool_vita_assets
             COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_vita_assets.py"
                     --probe $<TARGET_FILE:asset_archive_probe>)
    add_executable(vita_pad_test vita_pad_test.c)
    target_include_directories(vita_pad_test PRIVATE ../include)
    target_link_libraries(vita_pad_test PRIVATE m)
    add_test(NAME vita_pad COMMAND vita_pad_test)
    add_test(NAME tool_switch_rebuild
             COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_switch_rebuild.py")
    add_test(NAME tool_vita_delay_contract
             COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/vita_delay_contract_test.py")
endif()

add_test(NAME tool_native_asset_adapter
         COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_native_asset_adapter.py")

if(UNIX)
    add_executable(strikers_compile_native_audio ../tools/compile_native_audio.cpp)
    target_include_directories(strikers_compile_native_audio PRIVATE ../include)
    set_target_properties(strikers_compile_native_audio PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    add_executable(native_media_record_test native_media_record_test.cpp)
    target_include_directories(native_media_record_test PRIVATE ../include)
    set_target_properties(native_media_record_test PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON)
    target_compile_options(native_media_record_test PRIVATE -UNDEBUG)
    add_test(NAME native_media_record COMMAND native_media_record_test)
    add_test(NAME tool_native_media COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_native_media.py"
        --audio-compiler $<TARGET_FILE:strikers_compile_native_audio>)
endif()

add_test(NAME tool_vita_dump_identity COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_vita_dump_identity.py)
