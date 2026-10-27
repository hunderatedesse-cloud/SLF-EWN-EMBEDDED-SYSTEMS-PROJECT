cmake_minimum_required(VERSION 3.16)
project(SlfEwnDashboard VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick QuickControls2 Qml SerialPort QuickDialogs2 Sql Network)

qt_add_executable(SlfEwnDashboard
    src/main.cpp
    src/core/Crc8.cpp
    src/core/PacketParser.cpp
    src/model/Database.cpp
    src/model/SerialPortReader.cpp
    src/model/SimulatedTelemetrySource.cpp
    src/model/NodeListModel.cpp
    src/model/AlertListModel.cpp
    src/controller/NodeManager.cpp
    src/controller/AuthController.cpp
    src/controller/AppController.cpp
    qml.qrc
)

target_include_directories(SlfEwnDashboard PRIVATE src)

target_link_libraries(SlfEwnDashboard PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Qml
    Qt6::SerialPort
    Qt6::QuickDialogs2
    Qt6::Sql
    Qt6::Network
)

set_target_properties(SlfEwnDashboard PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "SLF-EWN"
)

if (MSVC)
    target_compile_options(SlfEwnDashboard PRIVATE /W4 /permissive- /utf-8)
else()
    target_compile_options(SlfEwnDashboard PRIVATE -Wall -Wextra -Wpedantic)
endif()
