cmake_minimum_required(VERSION 3.19)
project(algo-homework LANGUAGES C CXX)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

qt_standard_project_setup()

qt_add_executable(algo-homework
    WIN32 MACOSX_BUNDLE
    src/main.cpp
    src/widget.cpp
    src/widget.h
    src/database.cpp
    src/database.h
    src/scheduler.cpp
    src/scheduler.h
)

# 添加 SQLite 库
add_library(sqlite3 STATIC
    third_party/sqlite/sqlite3.c
)

target_include_directories(sqlite3 PUBLIC
    third_party/sqlite
)

target_link_libraries(algo-homework
    PRIVATE
        Qt::Core
        Qt::Widgets
        sqlite3
)

include(GNUInstallDirs)

install(TARGETS algo-homework
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET algo-homework
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
