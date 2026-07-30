cmake_minimum_required(VERSION 3.16)
project(GearIniEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

add_executable(GearIniEditor
    src/main.cpp
    src/iniparser.h
    src/iniparser.cpp
    src/quicksettings.h
    src/cheats.h
    src/mainwindow.h
    src/mainwindow.cpp
)

target_link_libraries(GearIniEditor PRIVATE Qt6::Widgets)

# On Windows, avoid a console window alongside the GUI.
if(WIN32)
    set_target_properties(GearIniEditor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
