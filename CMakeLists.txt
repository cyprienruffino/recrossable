cmake_minimum_required(VERSION 3.16)
project(recrossable LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# dlib is compiled into the backend, the device doesn't ship it
set(DLIB_SOURCE_DIR "" CACHE PATH "Path to an extracted dlib source tree")
if(NOT DLIB_SOURCE_DIR)
    include(FetchContent)
    FetchContent_Declare(dlib
        URL https://github.com/davisking/dlib/archive/refs/tags/v20.0.tar.gz
        URL_HASH SHA512=a4bcb2d013bd2b0000530d684c9c4b9f047f9fa6216174b3cb26d96f66c4a302d0bd1733d0ba35626d57133d9159f90114ab51a3af8fb9c493ff3e74dcc73911
    )
    FetchContent_Populate(dlib)
    set(DLIB_SOURCE_DIR ${dlib_SOURCE_DIR})
endif()

add_executable(entry
    backend/main.cpp
    backend/puzzle.cpp
    backend/recognizer.cpp
    cwc/cwc.cc
    cwc/dict.cc
    cwc/grid.cc
    cwc/letterdict.cc
    cwc/symbol.cc
    cwc/timer.cc
    cwc/wordlist.cc
    ${DLIB_SOURCE_DIR}/dlib/all/source.cpp
)
target_include_directories(entry PRIVATE ${CMAKE_SOURCE_DIR} ${DLIB_SOURCE_DIR})
target_compile_definitions(entry PRIVATE DLIB_NO_GUI_SUPPORT DLIB_DISABLE_ASSERTS)
find_package(Threads REQUIRED)
target_link_libraries(entry PRIVATE Threads::Threads -static-libstdc++ -static-libgcc)

# The frontend is packed into resources.rcc with the host's rcc
if(NOT RCC_EXECUTABLE AND DEFINED ENV{RCC_EXECUTABLE})
    set(RCC_EXECUTABLE $ENV{RCC_EXECUTABLE})
endif()
find_program(RCC_EXECUTABLE NAMES rcc PATHS /usr/lib/qt6/libexec /usr/lib/qt6/bin)
if(NOT RCC_EXECUTABLE)
    message(FATAL_ERROR "rcc not found, set RCC_EXECUTABLE")
endif()
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/resources.rcc
    COMMAND ${RCC_EXECUTABLE} --binary -o ${CMAKE_BINARY_DIR}/resources.rcc ${CMAKE_SOURCE_DIR}/application.qrc
    DEPENDS ${CMAKE_SOURCE_DIR}/application.qrc ${CMAKE_SOURCE_DIR}/ui/main.qml
)
add_custom_target(resources ALL DEPENDS ${CMAKE_BINARY_DIR}/resources.rcc)

# Installs the AppLoad application directory: cmake --install build --prefix <appload dir>/recrossable
install(TARGETS entry DESTINATION backend)
install(FILES manifest.json icon.png ${CMAKE_BINARY_DIR}/resources.rcc DESTINATION .)
install(FILES nyt.tsv net.dat DESTINATION data)
install(DIRECTORY patterns/ DESTINATION data/patterns PATTERN gmon.out EXCLUDE)
