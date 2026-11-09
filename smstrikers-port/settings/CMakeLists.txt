cmake_minimum_required(VERSION 3.21)

# The settings application is a standalone project on purpose: it depends on Qt and the game does
# not, so building the port must never require Qt to be installed.
project(strikers-settings
        DESCRIPTION "Settings front end for the Super Mario Strikers native port"
        LANGUAGES C CXX)
include(../cmake/StrikersVersion.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# QUIET, and not a required component, because lrelease and lupdate are not part of Qt Widgets:
# Debian and Ubuntu put them in qt6-tools-dev-tools, Fedora in qt6-linguist, Arch in qt6-tools, and
# aqt/install-qt-action needs `qttools` asked for by name.
find_package(Qt6 QUIET COMPONENTS LinguistTools)

qt_standard_project_setup(I18N_SOURCE_LANGUAGE en I18N_TRANSLATED_LANGUAGES de es fr)

if(MSVC)
  # /utf-8, because the sources hold non-ASCII narrow literals; without it MSVC reads the file in
  # the system code page and emits the execution string in it too.
  add_compile_options(/W4 $<$<COMPILE_LANGUAGE:CXX>:/permissive-> /utf-8)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Everything that is not the window lives in a library so the CTest round-trip test can link the
# same parser the application uses rather than a copy of it.
add_library(strikers-settings-core STATIC
  src/apppaths.cpp
  src/apppaths.h
  src/discimage.cpp
  src/discimage.h
  src/helptext.cpp
  src/helptext.h
  src/inifile.cpp
  src/inifile.h
  src/keynames.cpp
  src/keynames.h
  src/schema.cpp
  src/schema.h
  src/texturepacks.cpp
  src/texturepacks.h
)
target_include_directories(strikers-settings-core PUBLIC src)
target_link_libraries(strikers-settings-core PUBLIC Qt6::Core)

# Only these port headers go on the include path: ../include's malloc.h and types.h would shadow the system's.
set(STRIKERS_PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/..")
foreach(header aspect.h disc.h disc_reader.h host.h steamdeck.h)
  configure_file("${STRIKERS_PORT_DIR}/include/port/${header}"
                 "${CMAKE_CURRENT_BINARY_DIR}/portinc/port/${header}" COPYONLY)
endforeach()
target_sources(strikers-settings-core PRIVATE
  "${STRIKERS_PORT_DIR}/src/platform/aspect.c"
  "${STRIKERS_PORT_DIR}/src/platform/disc.c"
  "${STRIKERS_PORT_DIR}/src/platform/disc_reader.cpp"
  "${STRIKERS_PORT_DIR}/src/platform/steamdeck.c")
target_include_directories(strikers-settings-core PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/portinc")
target_compile_definitions(strikers-settings-core PRIVATE PORT_DISC_INFLATE_HOOK=1)
if(MSVC)
  target_compile_definitions(strikers-settings-core PRIVATE strcmpi=_stricmp _CRT_SECURE_NO_WARNINGS)
else()
  target_compile_definitions(strikers-settings-core PRIVATE strcmpi=strcasecmp)
endif()

qt_add_executable(strikers-settings
  src/main.cpp
  src/mainwindow.cpp
  src/mainwindow.h
  src/infobutton.cpp
  src/infobutton.h
  src/keycapturebutton.cpp
  src/keycapturebutton.h
  src/settingspage.cpp
  src/settingspage.h
  src/windowlanguage.cpp
  src/windowlanguage.h
)
target_link_libraries(strikers-settings PRIVATE strikers-settings-core Qt6::Widgets)

# A plain executable by default, so it sits beside strikers when run from a build tree.
option(STRIKERS_SETTINGS_BUNDLE "Build strikers-settings as a macOS .app bundle" OFF)
set_target_properties(strikers-settings PROPERTIES
  WIN32_EXECUTABLE ON
  MACOSX_BUNDLE ${STRIKERS_SETTINGS_BUNDLE}
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.smstrikers.settings"
  # package.sh and apppaths.cpp find the app by its target name, so only the bundle name changes.
  MACOSX_BUNDLE_BUNDLE_NAME "Super Mario Strikers Settings"
  MACOSX_BUNDLE_ICON_FILE settings.icns
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${STRIKERS_VERSION}"
  MACOSX_BUNDLE_BUNDLE_VERSION "${STRIKERS_VERSION}"
)

# The game's memory card icon with a cog on it, checked in under ./assets/icon: the bundle's icon
# on macOS, the exe resource on Windows.
set(STRIKERS_ICON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon")
if(APPLE AND STRIKERS_SETTINGS_BUNDLE)
  target_sources(strikers-settings PRIVATE "${STRIKERS_ICON_DIR}/settings.icns")
  set_source_files_properties("${STRIKERS_ICON_DIR}/settings.icns"
    PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
elseif(WIN32)
  target_sources(strikers-settings PRIVATE "${STRIKERS_ICON_DIR}/settings.rc")
  set_source_files_properties("${STRIKERS_ICON_DIR}/settings.rc" PROPERTIES
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/version"
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/version/strikers_version.h")
endif()

# and the same picture again as a Qt resource, for the window and the Dock.
qt_add_resources(strikers-settings "appicon"
  PREFIX "/"
  BASE "${STRIKERS_ICON_DIR}"
  FILES "${STRIKERS_ICON_DIR}/settings.png")

# German, Spanish and French, compiled into the executable rather than shipped beside it.
set(STRIKERS_SETTINGS_LANGUAGES de es fr)
set(STRIKERS_SETTINGS_TS "")
set(STRIKERS_SETTINGS_QM "")
foreach(lang IN LISTS STRIKERS_SETTINGS_LANGUAGES)
  list(APPEND STRIKERS_SETTINGS_TS "${CMAKE_CURRENT_SOURCE_DIR}/i18n/strikers-settings_${lang}.ts")
  list(APPEND STRIKERS_SETTINGS_QM "${CMAKE_CURRENT_SOURCE_DIR}/i18n/strikers-settings_${lang}.qm")
endforeach()

if(Qt6LinguistTools_FOUND)
  # Qt's own catalogues are merged into ours because no packager deploys them: windeployqt runs with
  # --no-translations, macdeployqt copies none, and the Linux archive takes whatever the distro has.
  if(NOT DEFINED STRIKERS_QT_CATALOGS)
    set(STRIKERS_QT_CATALOGS "${QT6_INSTALL_PREFIX}/${QT6_INSTALL_TRANSLATIONS}")
  endif()
  set(STRIKERS_HAVE_QT_CATALOGS TRUE)
  foreach(lang IN LISTS STRIKERS_SETTINGS_LANGUAGES)
    if(NOT EXISTS "${STRIKERS_QT_CATALOGS}/qtbase_${lang}.qm")
      set(STRIKERS_HAVE_QT_CATALOGS FALSE)
    endif()
  endforeach()

  # MERGE_QT_TRANSLATIONS arrived in Qt 6.9.
  set(STRIKERS_MERGE_ARG "")
  if(STRIKERS_HAVE_QT_CATALOGS AND Qt6_VERSION VERSION_GREATER_EQUAL 6.9)
    set(STRIKERS_MERGE_ARG MERGE_QT_TRANSLATIONS)
    set(STRIKERS_QM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/i18n")
    message(STATUS "strikers-settings: building translations into i18n/, with Qt's own merged in")
  else()
    set(STRIKERS_QM_DIR "${CMAKE_CURRENT_BINARY_DIR}/i18n")
    message(STATUS "strikers-settings: building translations into the build tree only "
                   "(no qtbase catalogues, or Qt older than 6.7). The .qm in i18n/ are "
                   "left as they are, do not commit what this build produces.")
  endif()

  # OUTPUT_LOCATION works before Qt 6.9; RESOURCE_PREFIX doubles the .qm path and no translation loads.
  set_source_files_properties(${STRIKERS_SETTINGS_TS} PROPERTIES OUTPUT_LOCATION "${STRIKERS_QM_DIR}")
  qt_add_translations(strikers-settings
    TS_FILES ${STRIKERS_SETTINGS_TS}
    SOURCE_TARGETS strikers-settings strikers-settings-core
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../src/platform/online/panel_text.cpp"
    QM_FILES_OUTPUT_VARIABLE STRIKERS_SETTINGS_QM_BUILT
    ${STRIKERS_MERGE_ARG})
  set(STRIKERS_SETTINGS_QM_EMBED ${STRIKERS_SETTINGS_QM_BUILT})
  set(STRIKERS_SETTINGS_QM_BASE "${STRIKERS_QM_DIR}")
else()
  # No lrelease. Embed what is committed, which is why it is committed.
  set(STRIKERS_SETTINGS_QM_EMBED "")
  foreach(qm IN LISTS STRIKERS_SETTINGS_QM)
    if(EXISTS "${qm}")
      list(APPEND STRIKERS_SETTINGS_QM_EMBED "${qm}")
    endif()
  endforeach()
  set(STRIKERS_SETTINGS_QM_BASE "${CMAKE_CURRENT_SOURCE_DIR}/i18n")
  if(STRIKERS_SETTINGS_QM_EMBED)
    message(STATUS "strikers-settings: no Qt Linguist tools; using the pre-built "
                   "translations in i18n/. Install them (qt6-tools-dev-tools, "
                   "qt6-linguist, qt6-tools, or aqt's qttools) to rebuild these.")
  else()
    message(STATUS "strikers-settings: no Qt Linguist tools and no pre-built "
                   "translations in i18n/; the window will be English only.")
  endif()
endif()

# One prefix, both branches: main.cpp asks for:/i18n and must not have to know which of the two put
# the file there. and the binary is told which languages it is supposed to be carrying.
set(STRIKERS_SETTINGS_TAGS "")
foreach(qm IN LISTS STRIKERS_SETTINGS_QM_EMBED)
  get_filename_component(tag "${qm}" NAME_WE)
  string(REGEX REPLACE "^strikers-settings_" "" tag "${tag}")
  list(APPEND STRIKERS_SETTINGS_TAGS "${tag}")
endforeach()
list(JOIN STRIKERS_SETTINGS_TAGS "," STRIKERS_SETTINGS_TAGLIST)
target_compile_definitions(strikers-settings
  PRIVATE STRIKERS_SETTINGS_TRANSLATIONS="${STRIKERS_SETTINGS_TAGLIST}")

if(STRIKERS_SETTINGS_QM_EMBED)
  qt_add_resources(strikers-settings "translations"
    PREFIX "/i18n"
    BASE "${STRIKERS_SETTINGS_QM_BASE}"
    FILES ${STRIKERS_SETTINGS_QM_EMBED})
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

install(TARGETS strikers-settings RUNTIME DESTINATION . BUNDLE DESTINATION .)
