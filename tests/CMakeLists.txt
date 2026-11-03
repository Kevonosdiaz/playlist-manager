find_package(Qt6 REQUIRED COMPONENTS Test)

# Adds one Qt Test executable per source file and registers it with CTest.
function(playlist_manager_add_test name)
    qt_add_executable(${name} ${name}.cpp testutils.h)
    target_link_libraries(${name} PRIVATE playlist-manager-core Qt::Test)
    target_compile_options(${name} PRIVATE ${PLAYLIST_MANAGER_WARNING_FLAGS})
    add_test(NAME ${name} COMMAND ${name})
    # Run headless: QPixmap/QIcon need a QGuiApplication but no real display.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

playlist_manager_add_test(tst_playlist)
playlist_manager_add_test(tst_songtablemodel)
playlist_manager_add_test(tst_tag)
