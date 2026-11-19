cmake_minimum_required(VERSION 3.21)

# CI passes the release tag (without the leading "v") so that the .deb and the
# AppImage carry the same version as the git tag they were built from.
if(DEFINED LKPLAYER_VERSION AND LKPLAYER_VERSION)
    project(LkPlayer VERSION ${LKPLAYER_VERSION} LANGUAGES CXX)
else()
    project(LkPlayer VERSION 0.1.0 LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Qml
    Quick
    QuickControls2
    QuickDialogs2
    QuickLayouts
    Svg
    Xml
    Concurrent
)

qt_standard_project_setup(REQUIRES 6.5)

set(LK_SOURCES
    src/main.cpp

    src/app/AppController.h
    src/app/AppController.cpp
    src/app/AppPaths.h
    src/app/AppPaths.cpp

    src/core/FolderNode.h
    src/core/MediaItem.h
    src/core/NfoParser.h
    src/core/NfoParser.cpp
    src/core/PathUtils.h
    src/core/PathUtils.cpp
    src/core/LibraryIndex.h
    src/core/LibraryIndex.cpp
    src/core/LibraryScanner.h
    src/core/LibraryScanner.cpp

    src/model/FolderListModel.h
    src/model/FolderListModel.cpp
    src/model/MediaFilterProxyModel.h
    src/model/MediaFilterProxyModel.cpp
    src/model/MediaItemModel.h
    src/model/MediaItemModel.cpp

    src/player/FfplayProcess.h
    src/player/FfplayProcess.cpp
    src/player/MpvEngine.h
    src/player/MpvEngine.cpp
    src/player/MpvItem.h
    src/player/MpvItem.cpp
    src/player/MpvPlayer.h
    src/player/MpvPlayer.cpp
    src/player/PlayerController.h
    src/player/PlayerController.cpp
    src/player/PlayerFactory.h
    src/player/PlayerFactory.cpp
    src/player/StandalonePlayer.h
    src/player/StandalonePlayer.cpp
    src/player/SystemPlayer.h
    src/player/SystemPlayer.cpp

    src/settings/SettingsStore.h
    src/settings/SettingsStore.cpp

    src/thumb/ThumbnailCache.h
    src/thumb/ThumbnailCache.cpp
    src/thumb/ThumbnailProvider.h
    src/thumb/ThumbnailProvider.cpp
    src/thumb/ThumbnailService.h
    src/thumb/ThumbnailService.cpp
)

set(LK_QML_FILES
    qml/Main.qml
    qml/Theme.qml
    qml/AppLogo.qml
    qml/GlassPanel.qml
    qml/PosterPlaceholder.qml
    qml/PosterImage.qml
    qml/TopBar.qml
    qml/SideBar.qml
    qml/Breadcrumb.qml
    qml/TagFilterPanel.qml
    qml/MediaCard.qml
    qml/FolderCard.qml
    qml/PlayerControls.qml
    qml/MiniPlayer.qml
    qml/VideoSurface.qml
    qml/EmptyState.qml
    qml/DetailDrawer.qml
    qml/LibraryPage.qml
    qml/PlayerPage.qml
    qml/SettingsPage.qml
)

# Keep the QML sources in qml/ on disk but expose every file at the root of the
# QML module, so each component becomes a directly usable type.
foreach(qml_file IN LISTS LK_QML_FILES)
    get_filename_component(qml_name "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_name}")
endforeach()
# Palette stays a plain component that Theme instantiates itself; reading
# properties across two QML singletons is unreliable at start-up.
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(LkPlayer WIN32 src/main.cpp)

qt_add_qml_module(LkPlayer
    URI LkPlayer
    VERSION 1.0
    SOURCES ${LK_SOURCES}
    QML_FILES ${LK_QML_FILES}
)

# The QML type registrar emits `#include <Foo.h>` guards, so every source
# directory has to be on the include path.
target_include_directories(LkPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/app
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model
    ${CMAKE_CURRENT_SOURCE_DIR}/src/player
    ${CMAKE_CURRENT_SOURCE_DIR}/src/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/src/thumb
)

target_link_libraries(LkPlayer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::QuickLayouts
    Qt6::Svg
    Qt6::Xml
    Qt6::Concurrent
)

if(WIN32)
    target_link_libraries(LkPlayer PRIVATE user32 gdi32 dwmapi shell32)
endif()

# ------------------------------------------------------------------- libmpv
# The embedded backend paints libmpv's frames straight into the Qt Quick scene
# graph through its OpenGL render API. libmpv is optional: without it the build
# still succeeds and embedded mode falls back to ffplay in its own window.
#
# Plain find_path/find_library instead of pkg-config on purpose: MSYS2's mpv.pc
# reports its prefix as "/ucrt64", which a native Windows CMake resolves to the
# root of the drive and rejects as a non-existent include directory.
find_path(MPV_INCLUDE_DIR NAMES mpv/client.h)
find_library(MPV_LIBRARY NAMES mpv libmpv)

if(MPV_INCLUDE_DIR AND MPV_LIBRARY)
    target_include_directories(LkPlayer PRIVATE ${MPV_INCLUDE_DIR})
    target_link_libraries(LkPlayer PRIVATE ${MPV_LIBRARY})
    target_compile_definitions(LkPlayer PRIVATE LKPLAYER_HAVE_MPV)
    message(STATUS "libmpv found (${MPV_LIBRARY}): in-scene embedded playback enabled")
else()
    message(WARNING
        "libmpv not found: 'embedded' playback will fall back to ffplay in its own window. "
        "Install the mpv development package (MSYS2: mingw-w64-ucrt-x86_64-mpv, Debian: libmpv-dev).")
endif()

# --------------------------------------------------------------------- install
# Linux installs follow the usual prefix layout; the Windows build is shipped as
# a portable archive assembled in CI, so it only needs the executable.
if(UNIX AND NOT APPLE)
    # Lower case binary name to match the desktop entry and distro habits.
    set_target_properties(LkPlayer PROPERTIES OUTPUT_NAME lkplayer)
endif()

install(TARGETS LkPlayer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

if(UNIX AND NOT APPLE)
    install(FILES assets/lkplayer.desktop
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES assets/lkplayer.svg
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
endif()

option(LKPLAYER_BUILD_TESTS "Build LkPlayer unit tests" ON)
if(LKPLAYER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ------------------------------------------------------------------ packaging
# Deliberately no CPack. The .deb is assembled in CI from the linuxdeploy bundle
# so it carries its own Qt runtime, QML modules and libmpv, which means it has
# to be built from a deployed tree rather than from these install() rules. Those
# rules still exist for `cmake --install`, but that tree expects the distribution
# QML modules to be present at run time.
