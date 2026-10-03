cmake_minimum_required(VERSION 3.19)
project(playlist-manager LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(TagLib REQUIRED)

set(CMAKE_AUTOUIC ON)
qt_standard_project_setup()

qt_add_executable(playlist-manager
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/mainwindow.ui
    src/tag.h
    src/tag.cpp
)

target_link_libraries(playlist-manager
    PRIVATE
        Qt::Core
        Qt::Widgets
        TagLib::tag
)

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
