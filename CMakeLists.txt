cmake_minimum_required(VERSION 2.8.6)

project(LoveBit)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_AUTOMOC TRUE)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(Qt4 COMPONENTS QtCore QtGui QtNetwork REQUIRED)
include(${QT_USE_FILE})
add_definitions(${QT_DEFINITIONS})

find_package(OpenCL REQUIRED)
include_directories(${OPENCL_INCLUDE_DIRS})

add_subdirectory(src)
