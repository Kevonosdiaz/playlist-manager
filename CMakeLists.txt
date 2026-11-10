cmake_minimum_required(VERSION 3.19)
project(playlist-manager LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PLAYLIST_MANAGER_BUILD_TESTS "Build the Qt Test unit tests" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Widgets Concurrent)
find_package(TagLib REQUIRED)

set(CMAKE_AUTOUIC ON)
qt_standard_project_setup()

set(PLAYLIST_MANAGER_WARNING_FLAGS
    -Wall -Wextra -Werror -Wconversion -Wshadow -Wpedantic -Wno-missing-field-initializers)

# Non-UI logic (parsing, tagging, models) lives in a static library so the
# application and the unit tests link the same compiled code.
qt_add_library(playlist-manager-core STATIC
    src/media/tag.h
    src/media/tag.cpp
    src/media/song.h
    src/media/songtablemodel.h
    src/media/songtablemodel.cpp
    src/media/playlist.h
    src/media/playlist.cpp
    src/utils/logger.h
    src/utils/logger.cpp
    src/utils/constants.h
    src/utils/tooltiplabel.h
    src/utils/tooltiplabel.cpp
)
target_include_directories(playlist-manager-core PUBLIC src)
target_link_libraries(playlist-manager-core
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        TagLib::tag
)
target_compile_options(playlist-manager-core PRIVATE ${PLAYLIST_MANAGER_WARNING_FLAGS})

qt_add_executable(playlist-manager
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/mainwindow.ui
)

target_link_libraries(playlist-manager
    PRIVATE
        playlist-manager-core
        Qt::Widgets
        Qt::Concurrent
)

target_compile_options(playlist-manager PRIVATE ${PLAYLIST_MANAGER_WARNING_FLAGS})

if(PLAYLIST_MANAGER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

install(TARGETS playlist-manager
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET playlist-manager
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
