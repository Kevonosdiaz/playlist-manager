cmake_minimum_required(VERSION 3.19)
project(playlist-manager LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Widgets)
find_package(TagLib REQUIRED)

set(CMAKE_AUTOUIC ON)
qt_standard_project_setup()

qt_add_executable(playlist-manager
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/mainwindow.ui
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
)

target_link_libraries(playlist-manager
    PRIVATE
        Qt::Core
        Qt::Widgets
        TagLib::tag
)

target_compile_options(playlist-manager PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -Wpedantic -Wno-missing-field-initializers)

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
