cmake_minimum_required(VERSION 3.24)
project(smstrikers-port CXX C)
include(cmake/StrikersVersion.cmake)

if(VITA)
    set(STRIKERS_VITA ON)
else()
    set(STRIKERS_VITA OFF)
endif()

# Super Mario Strikers, native port Targets macOS (arm64 and x86_64) first, but nothing here is
# Darwin-specific: the platform surface is confined to src/platform/ and to Aurora, both of which
# already cover Windows and Linux.

if(STRIKERS_VITA)
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 11)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Toolchain contract: clang on desktop; VitaSDK and devkitA64 use GCC.
if(NOT STRIKERS_VITA AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang"
   AND NOT (NX AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
    message(FATAL_ERROR
        "This port requires clang (found ${CMAKE_CXX_COMPILER_ID}). "
        "See the toolchain contract in CMakeLists.txt.")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/Game/main.cpp")
    message(FATAL_ERROR
        "src/Game/main.cpp is missing. The game sources are committed in this "
        "tree, so this checkout is incomplete.")
endif()

# Build options. Declared ahead of the first line that reads them; the order matters.

# Heap bugs in ported code tend to surface far from their cause, a stray write through a truncated
# pointer shows up as a crash inside malloc much later.
option(STRIKERS_ASAN "Build with AddressSanitizer" OFF)
option(STRIKERS_VITA_NO_LOGS "Build the Vita port without runtime log files" OFF)
option(STRIKERS_VITA_AVC_CONTINUITY_PROBE "Build the local-only frozen AVC diagnostic probe" OFF)
option(STRIKERS_VITA_HOT_LTO "Use LTO/fast math only on Vita render/matrix hot paths" ON)
option(STRIKERS_VITA_NATIVE_LTO "Use LTO across native Vita game/platform sources" OFF)
set(STRIKERS_VITA_LTO_JOBS "auto" CACHE STRING
    "Vita LTO parallelism: auto or a positive GCC -flto job count")
option(STRIKERS_VITA_AUDIO_THREAD "Run the Vita MusyX software mixer on a dedicated worker" ON)
option(STRIKERS_VITA_VITAGL "Use the Strikers VitaGL graphics backend" OFF)
option(STRIKERS_VITA_PROFILER "Build VitaDebugger's user-mode profiler into the Vita port" OFF)
option(STRIKERS_VITA_PROFILE_AUTOSTART
    "Start the Vita profiler automatically at boot instead of requiring an environment variable"
    OFF)
set(STRIKERS_VITADEBUGGER_ROOT "" CACHE PATH
    "Path to a VitaDebugger checkout when STRIKERS_VITA_PROFILER is enabled")

# Aurora is a drop-in replacement for the Dolphin SDK: it defines the real GX/VI/PAD/DVD/CARD/SI
# symbols over WebGPU and SDL3.
option(STRIKERS_AURORA "Build against Aurora (real GX/PAD/CARD/DVD)" ON)
# Vita is a native application. Aurora is a desktop dependency only.
if(STRIKERS_VITA)
    set(STRIKERS_AURORA OFF CACHE BOOL "Build against Aurora" FORCE)
    set(STRIKERS_VITA_VITAGL ON CACHE BOOL "VitaGL renderer" FORCE)
    list(APPEND PORT_DEFINES STRIKERS_VITA_NATIVE_RUNTIME=1)
endif()


# include/msl links only the Metrowerks headers whose names do not collide with host ones, so
# <stdio.h>, <math.h> and the rest resolve to the host.
set(PORT_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/msl"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/include"
)
if(STRIKERS_AURORA AND NOT STRIKERS_VITA)
    list(PREPEND PORT_INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/include/port/aurora-overlay"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/aurora/include")
endif()
if(STRIKERS_VITA)
    list(PREPEND PORT_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/include/port/native-sdk")
endif()
if(STRIKERS_VITA AND STRIKERS_VITA_PROFILER)
    if(NOT STRIKERS_VITADEBUGGER_ROOT)
        message(FATAL_ERROR
            "STRIKERS_VITA_PROFILER requires STRIKERS_VITADEBUGGER_ROOT=<VitaDebugger checkout>")
    endif()
    set(_strikers_vitaprofiler_dir "${STRIKERS_VITADEBUGGER_ROOT}/profiler")
    if(NOT EXISTS "${_strikers_vitaprofiler_dir}/include/vitaprofiler.h"
       OR NOT EXISTS "${_strikers_vitaprofiler_dir}/src/vitaprofiler.c")
        message(FATAL_ERROR
            "STRIKERS_VITADEBUGGER_ROOT does not contain a usable profiler checkout: "
            "${STRIKERS_VITADEBUGGER_ROOT}")
    endif()
    list(APPEND PORT_INCLUDES "${_strikers_vitaprofiler_dir}/include")
endif()

# The build type, as a string, so a measurement can say which build produced it.
if(CMAKE_BUILD_TYPE)
    set(PORT_BUILD_TYPE_STR "${CMAKE_BUILD_TYPE}")
else()
    set(PORT_BUILD_TYPE_STR "unspecified")
endif()

set(PORT_DEFINES
    TARGET_PC=1
    $<$<BOOL:${STRIKERS_VITA}>:STRIKERS_VITA_NATIVE_RUNTIME=1>
    $<$<BOOL:${STRIKERS_VITA}>:STRIKERS_VITA_VITAGL=1>
    PORT_BUILD_TYPE="${PORT_BUILD_TYPE_STR}"
    # MSVC's <math.h> defines M_PI and friends only when this is set; every other libc defines them
    # unconditionally, so nlMath.cpp's `M_PI / 2.0f` compiled everywhere except Windows.
    _USE_MATH_DEFINES
    # No VERSION_G4Q* define: the region is resolved at runtime from the disc id.
    dSINGLE                    # ODE single precision
    # MusyX picks its ABI with `#if MUSY_TARGET == MUSY_TARGET_DOLPHIN`, which is evaluated in
    # headers reached before musyx/version.h defines either name, both expand to 0 and the console
    # branch wins by accident.
    MUSY_TARGET_PC=0
    MUSY_TARGET_DOLPHIN=1
    MUSY_TARGET=0
    # The MusyX the game shipped with: the decomp builds game objects at 2.0.3 while musyx/version.h
    # defaults to 2.0.0, so the two would disagree on version-gated layouts and lose the group-stack
    # API.
    MUSY_VERSION_MAJOR=2
    MUSY_VERSION_MINOR=0
    MUSY_VERSION_PATCH=3
)
if(STRIKERS_AURORA)
    list(APPEND PORT_DEFINES PORT_USE_AURORA=1)
endif()
if(STRIKERS_VITA)
    list(APPEND PORT_DEFINES STRIKERS_VITA=1 PORT_VITA=1)
endif()
if(STRIKERS_VITA AND STRIKERS_VITA_AUDIO_THREAD)
    list(APPEND PORT_DEFINES STRIKERS_VITA_AUDIO_THREAD=1)
endif()
if(STRIKERS_VITA AND STRIKERS_VITA_AVC_CONTINUITY_PROBE)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/native_avc_continuity_inputs.inc")
        message(FATAL_ERROR "AVC probe requires local frozen inputs; they are not distributed")
    endif()
    list(APPEND PORT_DEFINES STRIKERS_VITA_AVC_CONTINUITY_PROBE=1)
endif()
if(STRIKERS_VITA AND STRIKERS_VITA_PROFILER)
    list(APPEND PORT_DEFINES STRIKERS_VITA_PROFILER=1)
endif()
if(STRIKERS_VITA AND STRIKERS_VITA_PROFILER AND STRIKERS_VITA_PROFILE_AUTOSTART)
    list(APPEND PORT_DEFINES STRIKERS_VITA_PROFILE_AUTOSTART=1)
endif()

set(STRIKERS_MAIN_COMPILE_DEFINITIONS)
if(STRIKERS_VITA AND STRIKERS_VITA_NO_LOGS)
    list(APPEND STRIKERS_MAIN_COMPILE_DEFINITIONS STRIKERS_VITA_NO_LOGS=1)
endif()
if(STRIKERS_MAIN_COMPILE_DEFINITIONS)
    # These switches only affect backend setup in main.cpp. Keep them source-local
    # so A/B performance builds do not invalidate every game object file.
    set_source_files_properties(src/Game/main.cpp PROPERTIES
        COMPILE_DEFINITIONS "${STRIKERS_MAIN_COMPILE_DEFINITIONS}")
endif()
if(MSVC AND STRIKERS_AURORA)
    # Aurora's Windows Dawn package contains only a Release archive, so its C++ ABI is /MT with
    # iterator debugging disabled even when this project is a Debug build.
    add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

if(STRIKERS_AURORA AND NOT STRIKERS_VITA)
    # OFF: src/platform/dvd.c provides DVD, reading an extracted disc directory rather than a disc
    # image.
    set(AURORA_ENABLE_DVD   OFF CACHE BOOL "" FORCE)
    if(NX)
        # The shader cache inside the .nro is stored zstd-compressed.
        set(AURORA_CACHE_USE_ZSTD ON CACHE BOOL "" FORCE)
    endif()
    set(AURORA_ENABLE_CARD  ON  CACHE BOOL "" FORCE)
    set(AURORA_ENABLE_RMLUI OFF CACHE BOOL "" FORCE)

    # std::jthread is probed rather than version-checked; an older libc++ has it only under
    # -fexperimental-library.
    include(CheckCXXSourceCompiles)
    set(_jthread_src [[
#include <thread>
#include <stop_token>
int main() { std::jthread t([](std::stop_token) {}); }
]])
    set(_saved_cxx_standard "${CMAKE_CXX_STANDARD}")
    set(_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
    set(CMAKE_CXX_STANDARD 20)
    check_cxx_source_compiles("${_jthread_src}" PORT_JTHREAD_PLAIN)
    if(NOT PORT_JTHREAD_PLAIN)
        set(CMAKE_REQUIRED_FLAGS
            "${_saved_required_flags} -fexperimental-library")
        check_cxx_source_compiles("${_jthread_src}" PORT_JTHREAD_EXPERIMENTAL)
        if(NOT PORT_JTHREAD_EXPERIMENTAL)
            message(FATAL_ERROR
                "Aurora needs std::jthread and std::stop_token, and this "
                "standard library has them neither plainly nor under "
                "-fexperimental-library. A newer libc++ (Apple clang 17, or "
                "Xcode 17) is required.")
        endif()
        message(STATUS "strikers: libc++ needs -fexperimental-library for std::jthread")
        add_compile_options(
            "$<$<COMPILE_LANGUAGE:CXX>:-fexperimental-library>")
        add_link_options(
            "$<$<LINK_LANGUAGE:CXX>:-fexperimental-library>")
    endif()
    set(CMAKE_REQUIRED_FLAGS "${_saved_required_flags}")
    unset(_saved_required_flags)
    set(CMAKE_CXX_STANDARD "${_saved_cxx_standard}")

    include(FetchContent)
    # Freetype from upstream's own mirrors: Aurora fetches it from a private one that now answers
    # 403.
    FetchContent_Declare(Freetype
        URL https://downloads.sourceforge.net/project/freetype/freetype2/2.14.3/freetype-2.14.3.tar.gz
            https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.gz
        URL_HASH SHA256=e61b31ab26358b946e767ed7eb7f4bb2e507da1cfefeb7a8861ace7fd5c899a1
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE
        EXCLUDE_FROM_ALL
    )
    # Freetype's bzip2 support decompresses bzip2-compressed PCF fonts and nothing else.
    set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)

    # Overriding an upstream URL is only right while that URL is still the one upstream uses.
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/extern/aurora/extern/CMakeLists.txt"
         _aurora_extern)
    string(FIND "${_aurora_extern}" "files.twilitrealm.dev/freetype" _ft_anchor)
    unset(_aurora_extern)
    if(_ft_anchor EQUAL -1)
        message(FATAL_ERROR
            "Aurora no longer fetches Freetype from files.twilitrealm.dev, so "
            "the FetchContent_Declare(Freetype) override above is stale; "
            "re-review it. If upstream's own URL works now, delete it.")
    endif()

    if(NX)
        set(STRIKERS_SWITCH_DAWN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/dawn-switch" CACHE PATH
            "Dawn source tree with the Switch changes, from tools/fetch-switch-deps.sh")
        set(STRIKERS_SWITCH_NVK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/nvk-switch" CACHE PATH
            "NVK for Switch: lib/libvulkan.a and include/vulkan")
        foreach(_nx_dep "${STRIKERS_SWITCH_DAWN_DIR}/CMakeLists.txt" "${STRIKERS_SWITCH_NVK_DIR}/lib/libvulkan.a")
            if(NOT EXISTS "${_nx_dep}")
                message(FATAL_ERROR "${_nx_dep} is missing; run tools/fetch-switch-deps.sh and tools/switch/build-nvk.sh.")
            endif()
        endforeach()

        # devkitPro ships only SDL2, which sdl3on2 exposes as SDL3. Listed in SDL2's link order.
        set(SDL2_LIBRARIES
            "${DEVKITPRO}/portlibs/switch/lib/libSDL2.a"
            "${DEVKITPRO}/portlibs/switch/lib/libEGL.a"
            "${DEVKITPRO}/portlibs/switch/lib/libglapi.a"
            "${DEVKITPRO}/portlibs/switch/lib/libdrm_nouveau.a"
            "${DEVKITPRO}/portlibs/switch/lib/libGLESv2.a"
            nx stdc++ m)
        FetchContent_Declare(sdl3on2
            GIT_REPOSITORY https://github.com/new-coke/sdl3on2.git
            GIT_TAG main
            GIT_SHALLOW TRUE
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(sdl3on2)
        add_library(SDL3::SDL3-static ALIAS sdl3on2)
        set(SDL3_FOUND TRUE)
        set(AURORA_SDL3_PROVIDER system CACHE STRING "" FORCE)
        set(AURORA_SDL3_LINKAGE static CACHE STRING "" FORCE)

        # Point Aurora's vendored Dawn build at the patched local tree.
        set(AURORA_DAWN_PROVIDER vendor CACHE STRING "" FORCE)
        set(AURORA_DAWN_LINKAGE static CACHE STRING "" FORCE)
        set(FETCHCONTENT_SOURCE_DIR_DAWN "${STRIKERS_SWITCH_DAWN_DIR}" CACHE PATH "" FORCE)
        set(DAWN_PLATFORM_SWITCH ON CACHE BOOL "" FORCE)
        foreach(_nx_off DAWN_ENABLE_D3D11 DAWN_ENABLE_D3D12 DAWN_ENABLE_METAL DAWN_ENABLE_DESKTOP_GL
                        DAWN_USE_WAYLAND DAWN_USE_X11 DAWN_BUILD_TESTS
                        # Tint's IR binary format needs a host protoc when cross-compiling.
                        DAWN_BUILD_PROTOBUF)
            set(${_nx_off} OFF CACHE BOOL "" FORCE)
        endforeach()
        set(DAWN_ENABLE_VULKAN ON CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_OPENGLES OFF CACHE BOOL "" FORCE)
        set(DAWN_ENABLE_NULL ON CACHE BOOL "" FORCE)

        # Defined before Aurora so it does not fetch Tracy's client.
        add_library(TracyClient INTERFACE)
        target_include_directories(TracyClient INTERFACE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/switch/tracy")
    endif()

    add_subdirectory(extern/aurora EXCLUDE_FROM_ALL)

    # The Switch has no Discord client.
    if(NOT NX)
        add_subdirectory(extern/borealis EXCLUDE_FROM_ALL)
    endif()

    if(NX AND TARGET sqlite3)
        # The Horizon VFS has no shared memory, mmap, dynamic loading or temporary files.
        target_compile_definitions(sqlite3 PRIVATE
            SQLITE_OMIT_WAL=1 SQLITE_MAX_MMAP_SIZE=0 SQLITE_OMIT_LOAD_EXTENSION=1
            SQLITE_TEMP_STORE=3)
    endif()
endif()


add_library(port_flags INTERFACE)
target_include_directories(port_flags INTERFACE ${PORT_INCLUDES})
target_compile_definitions(port_flags INTERFACE ${PORT_DEFINES})
# The compile contract, in both of clang's command-line dialects.
if(STRIKERS_VITA)
    target_compile_options(port_flags INTERFACE
        -fshort-wchar
        # ARM EABI otherwise uses the smallest integer container for enums.
        # The original GameCube ABI stores enums as 32-bit values throughout
        # asset/runtime structs (for example GXTextureHeader::format).  A short
        # enum moves every following field and silently corrupts on-disc layout.
        -fno-short-enums
        -include "${CMAKE_CURRENT_SOURCE_DIR}/include/port/prelude.h"
        -fno-strict-aliasing
        -fsigned-char
        $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
        -ffunction-sections
        -fdata-sections
        -Wno-invalid-offsetof)
elseif(MSVC)
    target_compile_options(port_flags INTERFACE
        /clang:-fdeclspec
        /clang:-fshort-wchar
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/include/port/prelude.h"
        /clang:-fno-strict-aliasing
        /clang:-fsigned-char
        -Wno-invalid-offsetof
        # A pointer truncated into a smaller integer is a build error here, as it already is
        # everywhere else.
        /clang:-Werror=pointer-to-int-cast
        /clang:-Werror=void-pointer-to-int-cast
        $<$<BOOL:${STRIKERS_ASAN}>:-fsanitize=address>
    )
else()
    target_compile_options(port_flags INTERFACE
        $<$<NOT:$<BOOL:${NX}>>:-fdeclspec>
        -fshort-wchar
        -include "${CMAKE_CURRENT_SOURCE_DIR}/include/port/prelude.h"
        -fno-strict-aliasing
        -fsigned-char
        -Wno-invalid-offsetof
        # GCC rejects a member named after its own type, which the decompiled structs use.
        $<$<AND:$<BOOL:${NX}>,$<COMPILE_LANGUAGE:CXX>>:-Wno-changes-meaning>
        $<$<BOOL:${STRIKERS_ASAN}>:-fsanitize=address>
        $<$<BOOL:${STRIKERS_ASAN}>:-fno-omit-frame-pointer>
    )
endif()
if(STRIKERS_ASAN)
    target_link_options(port_flags INTERFACE -fsanitize=address)
endif()
if(STRIKERS_VITA)
    # The decomp uses the GameCube/Metrowerks 16-bit wchar_t ABI. Passing the
    # flag to the link driver as well keeps the final ELF EABI attribute aligned
    # with every translation unit instead of advertising VitaSDK's default 32-bit wchar_t.
    target_link_options(port_flags INTERFACE -fshort-wchar -fno-short-enums)
endif()

if(STRIKERS_AURORA AND NOT STRIKERS_VITA)
    # aurora/event.h exposes SDL_Event, so the game needs SDL3 on its include path.
    target_include_directories(port_flags INTERFACE
        $<TARGET_PROPERTY:aurora::core,INTERFACE_INCLUDE_DIRECTORIES>)
endif()

# Source sets src/Dolphin, src/PowerPC_EABI_Support and src/SDK are vendored but not built: Aurora
# supplies GX/PAD/DVD/CARD and the host supplies libc/libc++.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/build-sources.txt" _built
     REGEX "^[^#]")
function(collect_sources out_var)
    set(_all "")
    foreach(_rel ${_built})
        foreach(_pre ${ARGN})
            if(_rel MATCHES "^${_pre}/")
                list(APPEND _all "${CMAKE_CURRENT_SOURCE_DIR}/src/${_rel}")
            endif()
        endforeach()
    endforeach()
    set(${out_var} ${_all} PARENT_SCOPE)
endfunction()

collect_sources(GAME_SOURCES Game NL)

# src/NL/MemAlloc.cpp is replaced by src/platform/memalloc.cpp: its free-block allocator is 32-bit
# PowerPC in its on-heap layout and has no reason to survive the port.
list(REMOVE_ITEM GAME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/NL/MemAlloc.cpp")
collect_sources(ODE_SOURCES  ode)

# ODE pieces the decomp does not build but the port needs to link: matrix.cpp calls dDot/dSolveL1
# and ode.cpp calls dRand, all of which live in files outside configure.py's object list (the retail
# build presumably got them from a prebuilt library).
set(ODE_EXTRA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ode/fastdot.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ode/fastlsolve.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ode/fastltsolve.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ode/fastldlt.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ode/misc.cpp"
    # step.cpp holds dInternalStepIsland, the constraint solver the world step calls into.
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ode/step.cpp"
)
set_source_files_properties(${ODE_EXTRA_SOURCES} PROPERTIES LANGUAGE CXX)
list(APPEND ODE_SOURCES ${ODE_EXTRA_SOURCES})
file(GLOB_RECURSE PLATFORM_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/*.c")
if(STRIKERS_VITA)
    list(REMOVE_ITEM PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/aurora_compat.c")
endif()

# Camera and world chunks are decoded immutably by their runtime loaders. The
# endian C files below are now malformed-buffer validators used only by tests.
list(REMOVE_ITEM PLATFORM_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/cam_endian.c"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/wld_endian.c")

# src/platform/operator_new.cpp is the one file in the tree that is not C++11.
list(REMOVE_ITEM PLATFORM_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/operator_new.cpp")
add_library(strikers_operators OBJECT
     "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/operator_new.cpp")
set_target_properties(strikers_operators PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

# borealis headers need C++20, so discord.cpp is a target of its own.
list(REMOVE_ITEM PLATFORM_SOURCES
     "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/discord.cpp")
if(STRIKERS_AURORA AND NOT NX AND NOT STRIKERS_VITA)
    add_library(strikers_discord OBJECT
         "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/discord.cpp")
    set_target_properties(strikers_discord PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON)
    target_include_directories(strikers_discord PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(strikers_discord PRIVATE borealis::discord)
endif()

# The debug overlay is ImGui, and ImGui arrives with Aurora and only with Aurora: with
# STRIKERS_AURORA=OFF none of overlay.cpp's includes are on the path, so it leaves the source set
# rather than becoming an empty translation unit.
if(NOT STRIKERS_AURORA OR STRIKERS_VITA)
    list(REMOVE_ITEM PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/overlay.cpp")
endif()

if(NX)
    list(REMOVE_ITEM PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/overlay.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/host.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/stubs_generated.c")
    # The tree's stubs use host symbol names; the Switch rebuild writes ELF stubs here.
    set(_nx_stubs "${CMAKE_CURRENT_BINARY_DIR}/stubs_generated.c")
    if(NOT EXISTS "${_nx_stubs}")
        file(WRITE "${_nx_stubs}" "// Filled by tools/switch/rebuild.sh.\n")
    endif()
    list(APPEND PLATFORM_SOURCES "${_nx_stubs}")
else()
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "/src/platform/switch/")
endif()

if(STRIKERS_AURORA AND NOT STRIKERS_VITA)
    # Aurora implements PAD (SDL_Gamepad, GC adapters, rumble, remapping) far better than the
    # placeholder here, so pad.c goes.
    list(REMOVE_ITEM PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/pad.c")
endif()

if(STRIKERS_VITA)
    list(REMOVE_ITEM PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/audio_out.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/launch.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/morphwatch.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/crashlog.c")
    # Vita, like Switch, has no Discord client. Reuse the platform-neutral
    # no-op presence shim so gameplay can call PortDiscordSetMatch unconditionally.
    list(APPEND PLATFORM_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/switch/discord.c")
endif()

# Portable slices of the Dolphin SDK: MTX, QUAT and VEC are pure computation Aurora does not
# provide,; the paired-single entry points forward to the C_ twins in the same files.
set(SDK_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Dolphin/mtx/mtx.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Dolphin/mtx/mtx44.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Dolphin/mtx/quat.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Dolphin/mtx/vec.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Dolphin/mtx/mtxvec.c"
)
add_library(strikers_sdk      OBJECT ${SDK_SOURCES})

if(STRIKERS_VITA AND STRIKERS_VITA_HOT_LTO)
    # Match the proven ACGC strategy without changing gameplay/physics FP
    # semantics globally: optimize only render submission and pure matrix math.
    set(_strikers_vita_hot_sources
        "${CMAKE_CURRENT_SOURCE_DIR}/src/NL/glx/glxSend.cpp"
        ${SDK_SOURCES})
    set_source_files_properties(${_strikers_vita_hot_sources} PROPERTIES
        COMPILE_OPTIONS
            "-O3;-flto=${STRIKERS_VITA_LTO_JOBS};-ffat-lto-objects;-funroll-loops;-ftree-vectorize;-fno-math-errno;-funsafe-math-optimizations;-fno-signed-zeros;-ffp-contract=fast;-mfpu=neon")
endif()

# MusyX, from the decomp submodule. It is self-contained and must NOT get the port prelude: musyx.h
# typedefs `bool` itself under C99, which collides with the <stdbool.h> the prelude pulls in for C
# translation units.
set(MUSYX_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/hw_pc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/CheapReverb/creverb.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/CheapReverb/creverb_fx.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/Chorus/chorus_fx.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/Delay/delay_fx.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/StdReverb/reverb.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/StdReverb/reverb_fx.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/dsp_import.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/hardware.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/hw_aramdma.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/hw_dspctrl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/hw_lib_dolphin.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/hw_memory.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/hw_volconv.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/s_data.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/seq.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/seq_api.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/snd3d.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/snd_init.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/snd_math.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/snd_midictrl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/snd_service.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/snd_synthapi.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/stream.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/synth.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/synth_ac.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/synth_adsr.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/synth_dbtab.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/synth_vsamples.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/synthdata.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/synthmacros.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/src/musyx/runtime/synthvoice.c"
)
add_library(strikers_musyx OBJECT ${MUSYX_SOURCES})
target_include_directories(strikers_musyx PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/musyx/include")
# Version gates real API: sndStackGetSize/sndStackAdd and friends sit behind `#if MUSY_VERSION >=
# MUSY_VERSION_CHECK(2, 0, 2)`, and the header defaults to 2.0.0.
target_compile_definitions(strikers_musyx PRIVATE
    MUSY_TARGET=0
    _USE_MATH_DEFINES
    MUSY_VERSION_MAJOR=2
    MUSY_VERSION_MINOR=0
    MUSY_VERSION_PATCH=3
    # MusyX does not link port_flags and so does not get PORT_DEFINES, which is where
    # _USE_MATH_DEFINES lives; and hardware.c's DPL2 coefficient uses M_PI.
    _USE_MATH_DEFINES)
if(STRIKERS_VITA)
    # MusyX is intentionally kept off port_flags, but it still exchanges enums
    # and enum-containing structures with the game runtime.  Match the 32-bit
    # GameCube enum ABI used by every other Vita object.
    if(STRIKERS_VITA_AUDIO_THREAD)
        target_compile_definitions(strikers_musyx PRIVATE MUSYX_THREADED_AUDIO=1)
    endif()
    target_compile_options(strikers_musyx PRIVATE -w -std=gnu11 -fshort-wchar -fno-short-enums)
elseif(NX)
    target_compile_options(strikers_musyx PRIVATE
        -w -std=gnu11
        # GCC 14 makes mismatched C pointer arguments an error; clang only warns, and -w hides that.
        -Wno-error=incompatible-pointer-types)
else()
    target_compile_options(strikers_musyx PRIVATE -fdeclspec -w -std=gnu11)
endif()

add_library(strikers_ode      OBJECT ${ODE_SOURCES})
add_library(strikers_game     OBJECT ${GAME_SOURCES})
add_library(strikers_platform OBJECT ${PLATFORM_SOURCES})
if(STRIKERS_VITA AND STRIKERS_VITA_NATIVE_LTO)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR "STRIKERS_VITA_NATIVE_LTO requires the VitaSDK GCC toolchain")
    endif()
    # Expose game/platform calls to the linker without extending the hot-path
    # fast-math policy to gameplay, validation or resource management.
    foreach(_t strikers_game strikers_platform)
        target_compile_options(${_t} PRIVATE
            "-flto=${STRIKERS_VITA_LTO_JOBS}" -ffat-lto-objects)
    endforeach()
    # These boundaries protect virtual event dispatch, disabled audio TLS and
    # strict DOF math from interprocedural optimization across their callers.
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/event_data.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/audio_out_native.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/NL/glx/glxTarget.cpp"
        APPEND PROPERTY COMPILE_OPTIONS -fno-lto)
endif()
if(NOT STRIKERS_VITA)
    # The shared native file reader uses bounded C++20 spans. Keep this
    # requirement local instead of changing the legacy desktop game's dialect.
    set_source_files_properties(
        src/NL/glx/glxLoadModel.cpp src/platform/native_graphics.cpp
        PROPERTIES COMPILE_OPTIONS
        "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-std=gnu++20;-Wno-register>;$<$<CXX_COMPILER_ID:MSVC>:/std:c++20>")
endif()
# The crash log stamps the build into every file it writes.
target_include_directories(strikers_platform PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/version")

if(STRIKERS_VITA AND STRIKERS_VITA_PROFILER)
    # Compile only the allocation-free user-mode core needed by Strikers. TCP,
    # PMU and debugger/kernel pieces stay out of the game binary.
    add_library(strikers_vitaprofiler STATIC
        "${_strikers_vitaprofiler_dir}/src/vitaprofiler.c"
        "${_strikers_vitaprofiler_dir}/src/vitaprofiler_vita.c"
        "${_strikers_vitaprofiler_dir}/src/vitaprofiler_names.c"
        "${_strikers_vitaprofiler_dir}/src/vitaprofiler_stream.c")
    set_target_properties(strikers_vitaprofiler PROPERTIES
        C_STANDARD 11
        C_STANDARD_REQUIRED ON)
    target_include_directories(strikers_vitaprofiler PUBLIC
        "${_strikers_vitaprofiler_dir}/include")
    target_compile_options(strikers_vitaprofiler PRIVATE -O2 -Wall -Wextra)
endif()

foreach(_t strikers_ode strikers_game strikers_platform strikers_sdk
          strikers_operators)
    target_link_libraries(${_t} PRIVATE port_flags)
endforeach()

# strikers_scan is the cheap per-push check: every in-scope translation unit compiled, nothing
# linked.
add_custom_target(strikers_scan
    DEPENDS strikers_ode strikers_game strikers_platform strikers_sdk
            strikers_musyx strikers_operators)
if(TARGET strikers_discord)
    add_dependencies(strikers_scan strikers_discord)
endif()

# The link test. Aurora, MusyX and the THP decoder are not wired up, so tools/genstubs.py supplies
# an abort-on-call definition for each symbol they would provide.
add_executable(strikers
    $<TARGET_OBJECTS:strikers_game>
    $<TARGET_OBJECTS:strikers_ode>
    $<TARGET_OBJECTS:strikers_sdk>
    $<TARGET_OBJECTS:strikers_platform>
    $<TARGET_OBJECTS:strikers_operators>
    $<TARGET_OBJECTS:strikers_musyx>
)
if(STRIKERS_VITA AND (STRIKERS_VITA_HOT_LTO OR STRIKERS_VITA_NATIVE_LTO))
    target_link_options(strikers PRIVATE "-flto=${STRIKERS_VITA_LTO_JOBS}")
endif()
target_link_libraries(strikers PRIVATE port_flags)
if(TARGET strikers_discord)
    target_sources(strikers PRIVATE $<TARGET_OBJECTS:strikers_discord>)
    target_link_libraries(strikers PRIVATE borealis::discord)
endif()

# The executable's icon on Windows: the game's memory card icon, from assets/icon. The files are
# checked in; the build only compiles the resource.
if(WIN32)
    target_sources(strikers PRIVATE assets/icon/strikers.rc)
    set_source_files_properties(assets/icon/strikers.rc PROPERTIES
        INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/version"
        OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/version/strikers_version.h")

    # /ENTRY:mainCRTStartup keeps main() as the entry point of a GUI-subsystem executable.
    set_target_properties(strikers PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MSVC)
        target_link_options(strikers PRIVATE /ENTRY:mainCRTStartup)
    endif()
endif()

# The name macOS shows in the menu bar, from assets/macos/Info.plist.
if(APPLE)
    configure_file(assets/macos/Info.plist "${CMAKE_CURRENT_BINARY_DIR}/Info.plist" @ONLY)
    target_link_options(strikers PRIVATE
        "-Wl,-sectcreate,__TEXT,__info_plist,${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
    # Relink when the plist changes; a link option is not a dependency.
    set_property(TARGET strikers APPEND PROPERTY
        LINK_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/Info.plist")
endif()

# ffmpeg, for THP video: keep it disabled on Vita. Switch prefers the THP-only
# build over devkitPro's portlib; desktop keeps the upstream default.
if(STRIKERS_VITA)
    set(_strikers_ffmpeg_default OFF)
elseif(NX AND NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/ffmpeg-switch/lib/libavcodec.a"
       AND NOT EXISTS "$ENV{DEVKITPRO}/portlibs/switch/lib/libavcodec.a")
    set(_strikers_ffmpeg_default OFF)
    message(STATUS "THP movies: off; `tools/build-ffmpeg.sh --switch` builds the decoder")
else()
    set(_strikers_ffmpeg_default ON)
endif()
set(_ff_switch "${CMAKE_CURRENT_SOURCE_DIR}/extern/ffmpeg-switch")
option(STRIKERS_FFMPEG "Decode THP movies with libavcodec" ${_strikers_ffmpeg_default})
set(STRIKERS_FFMPEG_ROOT "" CACHE PATH
    "An unpacked ffmpeg (include/, lib/, and bin/ on Windows), for hosts with no pkg-config")
set(_ff_static "${CMAKE_CURRENT_SOURCE_DIR}/extern/ffmpeg-static")

if(STRIKERS_FFMPEG)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND AND NOT STRIKERS_FFMPEG_ROOT AND NOT DEFINED ENV{STRIKERS_FFMPEG_ROOT}
       AND NOT EXISTS "${_ff_static}/lib/libavcodec.a" AND NOT EXISTS "${_ff_static}/lib/avcodec.lib"
       AND NOT (NX AND EXISTS "${_ff_switch}/lib/libavcodec.a"))
        if(APPLE AND EXISTS "/opt/homebrew/opt/ffmpeg/lib/pkgconfig")
            set(ENV{PKG_CONFIG_PATH}
                "/opt/homebrew/opt/ffmpeg/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
        endif()
        pkg_check_modules(FFMPEG QUIET IMPORTED_TARGET libavcodec libavutil)
    endif()
endif()

if(STRIKERS_FFMPEG AND FFMPEG_FOUND AND NX)
    # The portlib is static, so its own dependencies come from pkg-config's Libs.private.
    target_compile_definitions(strikers_platform PRIVATE STRIKERS_FFMPEG=1)
    target_include_directories(strikers_platform PRIVATE ${FFMPEG_STATIC_INCLUDE_DIRS})
    target_link_libraries(strikers PRIVATE ${FFMPEG_STATIC_LDFLAGS})
    message(STATUS "THP movies: libavcodec ${FFMPEG_libavcodec_VERSION} (devkitPro portlib)")
elseif(STRIKERS_FFMPEG AND FFMPEG_FOUND)
    target_compile_definitions(strikers_platform PRIVATE STRIKERS_FFMPEG=1)
    target_include_directories(strikers_platform PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(strikers PRIVATE PkgConfig::FFMPEG)
    message(STATUS "THP movies: libavcodec ${FFMPEG_libavcodec_VERSION} (pkg-config)")
elseif(STRIKERS_FFMPEG)
    # Two passes: the named roots first, taken literally (NO_DEFAULT_PATH, and
    # NO_CMAKE_FIND_ROOT_PATH so a cross build does not search inside the Windows SDK), then an
    # ordinary search that cannot pick up the host's own libavcodec.
    set(_ff_roots "")
    if(STRIKERS_FFMPEG_ROOT)
        list(APPEND _ff_roots "${STRIKERS_FFMPEG_ROOT}")
    endif()
    if(DEFINED ENV{STRIKERS_FFMPEG_ROOT})
        list(APPEND _ff_roots "$ENV{STRIKERS_FFMPEG_ROOT}")
    endif()
    if(NX)
        list(APPEND _ff_roots "${_ff_switch}")
    endif()
    list(APPEND _ff_roots "${_ff_static}")
    list(APPEND _ff_roots "${CMAKE_CURRENT_SOURCE_DIR}/extern/ffmpeg-windows-x86_64")
    if(APPLE)
        list(APPEND _ff_roots "/opt/homebrew/opt/ffmpeg")
    endif()

    find_path(STRIKERS_FFMPEG_INCLUDE_DIR
        NAMES libavcodec/avcodec.h
        PATHS ${_ff_roots} PATH_SUFFIXES include
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    find_library(STRIKERS_FFMPEG_AVCODEC
        NAMES avcodec
        PATHS ${_ff_roots} PATH_SUFFIXES lib
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    find_library(STRIKERS_FFMPEG_AVUTIL
        NAMES avutil
        PATHS ${_ff_roots} PATH_SUFFIXES lib
        NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)

    # A no-op when the pass above answered: find_path and find_library return immediately on a cache
    # entry that is set.
    find_path(STRIKERS_FFMPEG_INCLUDE_DIR NAMES libavcodec/avcodec.h)
    find_library(STRIKERS_FFMPEG_AVCODEC NAMES avcodec)
    find_library(STRIKERS_FFMPEG_AVUTIL NAMES avutil)

    if(STRIKERS_FFMPEG_INCLUDE_DIR AND STRIKERS_FFMPEG_AVCODEC AND STRIKERS_FFMPEG_AVUTIL)
        # The version out of the header, not the file name: a rolling asset is called "latest" and
        # says nothing.
        set(_ff_version "unknown")
        if(EXISTS "${STRIKERS_FFMPEG_INCLUDE_DIR}/libavcodec/version_major.h")
            file(STRINGS "${STRIKERS_FFMPEG_INCLUDE_DIR}/libavcodec/version_major.h"
                 _ff_major REGEX "LIBAVCODEC_VERSION_MAJOR +[0-9]+")
        endif()
        if(EXISTS "${STRIKERS_FFMPEG_INCLUDE_DIR}/libavcodec/version.h")
            file(STRINGS "${STRIKERS_FFMPEG_INCLUDE_DIR}/libavcodec/version.h"
                 _ff_minor REGEX "LIBAVCODEC_VERSION_MINOR +[0-9]+")
        endif()
        string(REGEX MATCH "[0-9]+" _ff_major "${_ff_major}")
        string(REGEX MATCH "[0-9]+" _ff_minor "${_ff_minor}")
        if(_ff_major)
            set(_ff_version "${_ff_major}.${_ff_minor}")
        endif()

        target_compile_definitions(strikers_platform PRIVATE STRIKERS_FFMPEG=1)
        target_include_directories(strikers_platform PRIVATE "${STRIKERS_FFMPEG_INCLUDE_DIR}")
        target_link_libraries(strikers PRIVATE
            "${STRIKERS_FFMPEG_AVCODEC}" "${STRIKERS_FFMPEG_AVUTIL}")
        string(FIND "${STRIKERS_FFMPEG_AVCODEC}" "${_ff_static}/" _ff_static_at)
        if(STRIKERS_FFMPEG_AVCODEC MATCHES "\\.a$" OR _ff_static_at EQUAL 0)
            set(_ff_is_static TRUE)
        else()
            set(_ff_is_static FALSE)
        endif()
        if(_ff_is_static AND WIN32)
            target_link_libraries(strikers PRIVATE bcrypt)
        elseif(_ff_is_static)
            # A static libavcodec leaves its libm and thread references for the executable to resolve.
            find_package(Threads REQUIRED)
            target_link_libraries(strikers PRIVATE m Threads::Threads)
        endif()
        message(STATUS "THP movies: libavcodec ${_ff_version} (${STRIKERS_FFMPEG_AVCODEC})")

        if(WIN32 AND NOT _ff_is_static)
            # An import library is not a runtime: linking avcodec.lib without avcodec-NN.dll beside
            # it does not start. Copied at build time, and package.sh takes the same files. These
            # three, from avcodec's import table.
            get_filename_component(_ff_libdir "${STRIKERS_FFMPEG_AVCODEC}" DIRECTORY)
            get_filename_component(_ff_bindir "${_ff_libdir}/../bin" ABSOLUTE)
            file(GLOB _ff_dlls
                "${_ff_bindir}/avcodec-*.dll"
                "${_ff_bindir}/avutil-*.dll"
                "${_ff_bindir}/swresample-*.dll")
            if(_ff_dlls)
                add_custom_command(TARGET strikers POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                            ${_ff_dlls} "$<TARGET_FILE_DIR:strikers>"
                    COMMENT "ffmpeg runtime DLLs -> $<TARGET_FILE_DIR:strikers>"
                    VERBATIM)
            else()
                message(WARNING "THP movies: linking ${STRIKERS_FFMPEG_AVCODEC} but found no "
                                "avcodec/avutil/swresample DLLs in ${_ff_bindir}; the build "
                                "will not start until they are beside strikers.exe.")
            endif()
        endif()
    else()
        # A warning, so a build that drops the movies is seen before a player finds it.
        message(WARNING "THP movies: STRIKERS_FFMPEG is ON but no libavcodec/libavutil was "
                        "found, by pkg-config or otherwise; the game will build without "
                        "movies, and without their audio. Install ffmpeg's development "
                        "files (brew install ffmpeg, apt install libavcodec-dev "
                        "libavutil-dev), or run tools/build-ffmpeg.sh or "
                        "tools/fetch-ffmpeg-windows.sh, or point "
                        "-DSTRIKERS_FFMPEG_ROOT at an unpacked one, or pass "
                        "-DSTRIKERS_FFMPEG=OFF to say this is intended.")
    endif()
endif()

# zlib, for .gcz disc images and compressed PSARC blocks. The asset preparation
# tool writes stored PSARC blocks, which need no decompressor at runtime.
if(NOT WIN32 AND NOT STRIKERS_VITA AND NOT NX)
    find_package(Threads REQUIRED)
    target_link_libraries(strikers_platform PRIVATE Threads::Threads)
endif()
if(NOT TARGET ZLIB::ZLIB)
    find_package(ZLIB QUIET)
endif()
if(TARGET ZLIB::ZLIB)
    target_compile_definitions(strikers_platform PRIVATE STRIKERS_ZLIB=1)
    target_link_libraries(strikers_platform PRIVATE ZLIB::ZLIB)
    target_link_libraries(strikers PRIVATE ZLIB::ZLIB)
    message(STATUS "Disc images: .iso/.gcm, .ciso and .gcz (zlib)")
else()
    message(STATUS "Disc images: .iso/.gcm and .ciso; no zlib, so no .gcz")
endif()

# Dead-strip the debug classes (StatsGatherer, cFuzzyDebugger) whose vtables reference methods
# defined only in files the decomp excludes, and which nothing ever constructs.
if(STRIKERS_VITA)
    target_compile_options(port_flags INTERFACE -ffunction-sections -fdata-sections)
    target_link_options(strikers PRIVATE -Wl,--gc-sections -Wl,-q)
elseif(APPLE)
    target_link_options(strikers PRIVATE -dead_strip)
elseif(MSVC)
    # clang-cl sets CMake's MSVC, and link.exe/lld-link take neither spelling. /OPT:REF is the
    # equivalent; no compile-side flag is needed because the MSVC-style frontend already emits one
    # COMDAT per function.
    target_link_options(strikers PRIVATE /OPT:REF /OPT:ICF)
else()
    target_compile_options(port_flags INTERFACE -ffunction-sections -fdata-sections)
    target_link_options(strikers PRIVATE -Wl,--gc-sections)
endif()

# operator new/delete are weak, coalesced symbols, so dyld merges them process-wide and
# SDL3/Dawn/AppKit would otherwise bind to the game's.
if(APPLE)
    foreach(_sym __Znwm __Znam __ZdlPv __ZdaPv)
        target_link_options(strikers PRIVATE "-Wl,-unexported_symbol,${_sym}")
    endforeach()
elseif(NOT MSVC AND NOT STRIKERS_VITA AND NOT NX)
    # An ELF executable exports a definition a shared library needs, so libc++ would bind to the
    # game's operators exactly as dyld does.
    set(_local_ops "${CMAKE_CURRENT_BINARY_DIR}/local-operators.map")
    file(WRITE "${_local_ops}" "{ local: _Znw*; _Zna*; _ZdlPv*; _ZdaPv*; };\n")
    target_link_options(strikers PRIVATE "-Wl,--version-script=${_local_ops}")
    # A bundled libstdc++ breaks Mesa's LLVM, so it is linked in; glibc 2.41 refuses executable stacks.
    target_link_options(strikers PRIVATE -static-libstdc++ -static-libgcc -Wl,-z,noexecstack)
endif()

if(STRIKERS_AURORA AND NOT STRIKERS_VITA)
    # Deliberately NOT aurora::os, Aurora's OS has no threads, and ours is already complete.
    target_link_libraries(strikers PRIVATE
        aurora::core aurora::gx aurora::gd aurora::vi aurora::pad aurora::si
        aurora::card aurora::main)

    # Frame capture lives inside Aurora now (lib/aurora.cpp, namespace capture).

endif()

if(STRIKERS_VITA)
    # Build the checked-in provider, with one renderer owner and no background splash.
    file(GLOB _strikers_vitagl_sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/vitaGL/source/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/vitaGL/source/utils/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/vitaGL/source/utils/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/vitaGL/source/utils/preprocessor/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/extern/vitaGL/source/utils/preprocessor/*.cpp")
    add_library(strikers_vitagl STATIC ${_strikers_vitagl_sources})
    target_include_directories(strikers_platform BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern/vitaGL/source")
    target_include_directories(strikers_vitagl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern/vitaGL/source")
    target_compile_definitions(strikers_vitagl PRIVATE SKIP_SPLASHSCREEN=1 HAVE_SCRATCH_MEMORY=1
        VGL_GIT_HASH="strikers-source-snapshot" HAVE_SHADER_CACHE=1
        READBACKS_SPEEDHACK=1 DRAW_SPEEDHACK=1 NO_DEBUG=1 SKIP_ERROR_HANDLING=1
        # Main-scene depth must survive the intermediate framebuffer copies
        # before particles. Keep the requested speed flags enabled.
        STORE_DEPTH_STENCIL=1)
    target_compile_options(strikers_vitagl PRIVATE -O3 -ffast-math -fshort-wchar -fno-short-enums -mtune=cortex-a9 -mfpu=neon
        -mfp16-format=ieee -Wno-incompatible-pointer-types -Wno-stringop-overflow)
endif()

if(STRIKERS_VITA)
    # AVNR record hashes use xxHash independently of Aurora. The version and
    # digest match the existing asset-file ABI; a fresh build fetches it itself.
    include(FetchContent)
    FetchContent_Declare(xxhash
        URL "https://github.com/Cyan4973/xxHash/archive/refs/tags/v0.8.3.tar.gz"
        URL_HASH SHA256=aae608dfe8213dfd05d909a57718ef82f30722c392344583d3f39050c7f29a80
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        SOURCE_SUBDIR strikers_headers_only)
    FetchContent_MakeAvailable(xxhash)
    target_include_directories(strikers_platform PRIVATE "${xxhash_SOURCE_DIR}")
    target_link_options(strikers PRIVATE -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/strikers.map)
    target_link_libraries(strikers PRIVATE
        strikers_vitagl vitashark SceShaccCgExt taihen_stub SceShaccCg_stub mathneon
        SceCommonDialog_stub SceKernelDmacMgr_stub SceKernelThreadMgr_stub SceSysmem_stub SceProcessmgr_stub
        SceCtrl_stub SceAudio_stub SceAudioIn_stub ScePower_stub SceRtc_stub
        SceIofilemgr_stub SceAppMgr_stub SceDisplay_stub SceGxm_stub
        SceSysmodule_stub SceVideodec_stub SceAvPlayer_stub SceSblSsMgr_stub SceLibKernel_stub pthread m)
    if(STRIKERS_VITA_PROFILER)
        target_link_libraries(strikers PRIVATE
            strikers_vitaprofiler SceKernelThreadMgr_stub SceProcessmgr_stub SceSysmem_stub)
    endif()

    include("${VITASDK}/share/vita.cmake")
    # Request the extended user-memory mode in param.sfo. vita-mksfoex uses
    # -d for DWORD entries; ATTRIBUTE2=12 selects the larger app memory budget.
    set(VITA_MKSFOEX_FLAGS "${VITA_MKSFOEX_FLAGS} -d ATTRIBUTE2=12")
    # Packaging the images alone does not assign their LiveArea roles. Keep
    # the background and launch artwork attached to the supported a1 layout.
    set(_strikers_livearea_template
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/vita/sce_sys/livearea/contents/template.xml")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${_strikers_livearea_template}")
    file(READ "${_strikers_livearea_template}" _strikers_livearea_xml)
    if(NOT _strikers_livearea_xml MATCHES "<livearea[ \t\r\n]+style=\"a1\"" OR
       NOT _strikers_livearea_xml MATCHES
           "<livearea-background>[ \t\r\n]*<image>bg\\.png</image>[ \t\r\n]*</livearea-background>" OR
       NOT _strikers_livearea_xml MATCHES
           "<gate>[ \t\r\n]*<startup-image>startup\\.png</startup-image>[ \t\r\n]*</gate>")
        message(FATAL_ERROR
            "LiveArea must use the a1 layout, bg.png background and startup.png launch gate")
    endif()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    execute_process(COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_vita_package.py"
        RESULT_VARIABLE _strikers_assets_result)
    if(NOT _strikers_assets_result EQUAL 0)
        message(FATAL_ERROR "Invalid Vita LiveArea assets; see validate_vita_package.py diagnostics")
    endif()
    vita_create_self(strikers_vita.self strikers UNSAFE)
    vita_create_vpk(strikers_vita.vpk SMSVITA01 strikers_vita.self
        VERSION 01.00 NAME "Super Mario Strikers Vita"
        FILE assets/vita/sce_sys/icon0.png sce_sys/icon0.png
             assets/vita/sce_sys/pic0.png sce_sys/pic0.png
             assets/vita/sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
             assets/vita/sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
             assets/vita/sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png)
    add_custom_command(TARGET strikers_vita.vpk-vpk POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_vita_package.py"
            --vpk "${CMAKE_CURRENT_BINARY_DIR}/strikers_vita.vpk"
            --self "${CMAKE_CURRENT_BINARY_DIR}/strikers_vita.self"
        COMMENT "Checking VPK executable and all LiveArea asset paths/bytes"
        VERBATIM)
elseif(NX)
    # Horizon has no dynamic loader, so the Vulkan driver is linked statically.
    target_link_libraries(strikers PRIVATE
        "${STRIKERS_SWITCH_NVK_DIR}/lib/libvulkan.a"
        "${DEVKITPRO}/portlibs/switch/lib/libdrm_nouveau.a")
    # Dawn references the entry point but links after the driver, so force it in.
    target_link_options(strikers PRIVATE "-Wl,--undefined=vk_icdGetInstanceProcAddr")
    target_link_libraries(strikers_platform PRIVATE sqlite3)
    target_link_libraries(strikers PRIVATE nx m)
    # Increase default pthread stacks for Tint and Dawn.
    target_link_options(strikers PRIVATE -Wl,--wrap=pthread_create)
    nx_generate_nacp(strikers.nacp
        NAME "Strikers, a Super Mario Strikers port"
        AUTHOR "new-coke"
        VERSION "${STRIKERS_VERSION}")
    # romfs holds the shader seeds, when present, and the Switch prompt art.
    set(_strikers_nx_romfs "${CMAKE_CURRENT_BINARY_DIR}/romfs")
    file(REMOVE_RECURSE "${_strikers_nx_romfs}")
    set(_strikers_nx_seeds "${CMAKE_CURRENT_SOURCE_DIR}/assets/switch/romfs/initial_pipeline_cache.db")
    set(STRIKERS_SWITCH_DAWN_SEED "${CMAKE_CURRENT_SOURCE_DIR}/assets/switch/romfs/initial_dawn_cache.db"
        CACHE FILEPATH "Compiled shader seed; packed when its .stamp names this build's Dawn and Mesa")
    set(_strikers_nx_dawn_seed OFF)
    if(EXISTS "${STRIKERS_SWITCH_DAWN_SEED}")
        # Its blobs load only under the Dawn commit and Mesa ref in its .stamp, "<Dawn> <Mesa>".
        set(_seed_refs "")
        set(_build_refs "")
        foreach(_stamp "${STRIKERS_SWITCH_DAWN_SEED}.stamp" "${STRIKERS_SWITCH_DAWN_DIR}/.strikers-dawn"
                "${STRIKERS_SWITCH_NVK_DIR}/.strikers-nvk")
            set(_line "")
            if(EXISTS "${_stamp}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_stamp}")
                file(STRINGS "${_stamp}" _line LIMIT_COUNT 1)
            endif()
            if(_stamp STREQUAL "${STRIKERS_SWITCH_DAWN_SEED}.stamp")
                set(_seed_refs "${_line}")
            else()
                string(REGEX MATCH "^[0-9a-f]*" _line "${_line}")
                string(STRIP "${_build_refs} ${_line}" _build_refs)
            endif()
        endforeach()
        if(_seed_refs STREQUAL _build_refs)
            set(_strikers_nx_dawn_seed ON)
            configure_file("${STRIKERS_SWITCH_DAWN_SEED}" "${_strikers_nx_romfs}/initial_dawn_cache.db" COPYONLY)
        else()
            message(WARNING "Not packing ${STRIKERS_SWITCH_DAWN_SEED}: compiled for Dawn and Mesa "
                "'${_seed_refs}', this build has '${_build_refs}'.")
        endif()
    endif()
    # Only Switch prompt glyphs; no other controller family appears on the console.
    file(GLOB _strikers_nx_prompts CONFIGURE_DEPENDS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/input-prompts/kenney/Nintendo Switch/*.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/input-prompts/LICENSE-Kenney.txt")
    set(_strikers_nx_romfs_files "")
    if(_strikers_nx_dawn_seed)
        list(APPEND _strikers_nx_romfs_files "${_strikers_nx_romfs}/initial_dawn_cache.db")
    endif()
    foreach(_src IN LISTS _strikers_nx_seeds)
        get_filename_component(_name "${_src}" NAME)
        configure_file("${_src}" "${_strikers_nx_romfs}/${_name}" COPYONLY)
        list(APPEND _strikers_nx_romfs_files "${_strikers_nx_romfs}/${_name}")
    endforeach()
    foreach(_rel IN LISTS _strikers_nx_prompts)
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/assets/${_rel}" "${_strikers_nx_romfs}/${_rel}" COPYONLY)
        list(APPEND _strikers_nx_romfs_files "${_strikers_nx_romfs}/${_rel}")
    endforeach()
    include(dkp-asset-folder)
    dkp_add_asset_target(strikers_romfs "${_strikers_nx_romfs}")
    set_target_properties(strikers_romfs PROPERTIES DKP_ASSET_FILES "${_strikers_nx_romfs_files}")
    nx_create_nro(strikers
        ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/switch/icon.jpg"
        NACP strikers.nacp
        ROMFS strikers_romfs)
endif()

# Windows: what a runnable build needs that a linkable one does not.
if(MSVC)
    # 1. dbghelp, for src/platform/crashlog.c.
    target_link_libraries(strikers PRIVATE dbghelp)

    # 2. Debug info, in Release as well. /Z7 puts it in the objects and /DEBUG makes lld-link emit
    # the PDB.
    target_compile_options(port_flags INTERFACE /Z7)
    target_link_options(strikers PRIVATE /DEBUG)
endif()

if(MSVC AND STRIKERS_AURORA)
    # 3. The prebuilt DLLs, beside the executable.
    include("${CMAKE_CURRENT_SOURCE_DIR}/extern/aurora/cmake/AuroraCopyRuntimeDLLs.cmake")
    aurora_copy_runtime_dlls(strikers)
endif()

# CMAKE_MSVC_RUNTIME_LIBRARY is MultiThreaded at the top level, but Abseil replaces it in its own
# directory with MultiThreadedDebug for Debug builds.
if(MSVC AND STRIKERS_AURORA)
    function(port_force_windows_runtime directory)
        get_property(_targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_target IN LISTS _targets)
            get_target_property(_imported "${_target}" IMPORTED)
            get_target_property(_type "${_target}" TYPE)
            if(NOT _imported AND _type MATCHES
                    "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
                set_property(TARGET "${_target}" PROPERTY
                    MSVC_RUNTIME_LIBRARY MultiThreaded)
            endif()
        endforeach()

        get_property(_subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
        foreach(_subdirectory IN LISTS _subdirectories)
            port_force_windows_runtime("${_subdirectory}")
        endforeach()
    endfunction()
    port_force_windows_runtime("${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Tests, reachable with `ctest` from the build directory. Aurora registers its own under
# extern/aurora/tests, and without enable_testing() here the root CTest file listed nothing.
if(NOT CMAKE_CROSSCOMPILING)
    enable_testing()
    add_subdirectory(tests)
endif()

list(LENGTH GAME_SOURCES _n_game)
list(LENGTH ODE_SOURCES  _n_ode)
message(STATUS "in scope: ${_n_game} game/engine + ${_n_ode} ODE translation units")
