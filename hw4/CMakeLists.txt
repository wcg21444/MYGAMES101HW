cmake_minimum_required(VERSION 3.10)
set(CMAKE_TOOLCHAIN_FILE "D:/Program Files/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake")
project(BezierCurve)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 14)

add_executable(BezierCurve main.cpp)

target_link_libraries(BezierCurve ${OpenCV_LIBRARIES})
