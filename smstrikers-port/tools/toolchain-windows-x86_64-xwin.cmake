# Build the Windows x86_64 target from a Linux or macOS host, for the two bugs that are properties
# of the target: `unsigned long` at 4 bytes, and MSVC ABI mangling. It cannot run the result.
#
#   xwin --accept-license --arch x86_64 splat --output ~/xwin
#   XWIN_DIR=~/xwin CMAKE_TOOLCHAIN_FILE=$PWD/tools/toolchain-windows-x86_64-xwin.cmake \
#       ./tools/configure.sh build-ci Debug
#
# Below: the target triple and the paths a Windows host would supply, with no compile-time
# differences from the native configure. clang-cl is clang under another argv[0]; the ABI comes from
# the triple.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
# AMD64, MSVC's spelling, and not x86_64: Aurora's cmake/AuroraTargetPlatform reads
# CMAKE_SYSTEM_PROCESSOR straight out of this variable and compares it against "AMD64" to decide
# there is a prebuilt Dawn, SDL3 and nod for this target.
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# The MSVC CRT and Windows SDK, splatted by xwin (https://github.com/Jake-Shadle/xwin).
if(NOT XWIN_DIR)
    if(DEFINED ENV{XWIN_DIR})
        set(XWIN_DIR "$ENV{XWIN_DIR}")
    else()
        set(XWIN_DIR "$ENV{HOME}/xwin")
    endif()
endif()
set(XWIN_DIR "${XWIN_DIR}" CACHE PATH "Root of an `xwin splat` output tree")
if(NOT EXISTS "${XWIN_DIR}/crt/include" OR NOT EXISTS "${XWIN_DIR}/sdk/include")
    message(FATAL_ERROR
        "XWIN_DIR=${XWIN_DIR} is not an xwin splat tree (no crt/include and "
        "sdk/include under it).\n"
        "  xwin --accept-license --arch x86_64 splat --output ${XWIN_DIR}")
endif()

set(CMAKE_C_COMPILER   clang-cl)
set(CMAKE_CXX_COMPILER clang-cl)
set(CMAKE_C_COMPILER_TARGET   x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

# The rest of the MSVC toolchain, in its LLVM spellings. lld-link is not a preference here the way
# it is on Windows: link.exe does not exist on this host, and neither do lib.exe, rc.exe or mt.exe.
set(CMAKE_LINKER      lld-link)
set(CMAKE_AR          llvm-lib)
set(CMAKE_RC_COMPILER llvm-rc)
set(CMAKE_MT          llvm-mt)

# The include and library paths a Windows host would supply through the registry and the VS
# environment.
set(_xwin_includes
    "/imsvc${XWIN_DIR}/crt/include"
    "/imsvc${XWIN_DIR}/sdk/include/ucrt"
    "/imsvc${XWIN_DIR}/sdk/include/um"
    "/imsvc${XWIN_DIR}/sdk/include/shared"
    "/imsvc${XWIN_DIR}/sdk/include/winrt")
list(JOIN _xwin_includes " " _xwin_include_flags)

# -Wno-unused-command-line-argument: CMake passes the compile flags to the link step too, and /imsvc
# means nothing to a link.
set(CMAKE_C_FLAGS_INIT   "${_xwin_include_flags} -Wno-unused-command-line-argument")
set(CMAKE_CXX_FLAGS_INIT "${_xwin_include_flags} -Wno-unused-command-line-argument")
set(CMAKE_RC_FLAGS_INIT  "-I${XWIN_DIR}/sdk/include/um -I${XWIN_DIR}/sdk/include/shared")

set(_xwin_libpaths
    "/libpath:${XWIN_DIR}/crt/lib/x86_64"
    "/libpath:${XWIN_DIR}/sdk/lib/ucrt/x86_64"
    "/libpath:${XWIN_DIR}/sdk/lib/um/x86_64")
list(JOIN _xwin_libpaths " " _xwin_libpath_flags)
foreach(_kind EXE SHARED MODULE)
    set(CMAKE_${_kind}_LINKER_FLAGS_INIT "${_xwin_libpath_flags}")
endforeach()

# Look for headers and libraries in the splat tree, never in the host's /usr.
set(CMAKE_FIND_ROOT_PATH "${XWIN_DIR}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BEFORE)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
