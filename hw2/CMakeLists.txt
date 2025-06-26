

cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "D:/Program Files/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake")

project(Rasterizer)

find_package(OpenCV REQUIRED)

set(CMAKE_CXX_STANDARD 17)

include_directories(/usr/local/include)

add_executable(${PROJECT_NAME} main.cpp rasterizer.hpp rasterizer.cpp global.hpp Triangle.hpp Triangle.cpp)
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBRARIES})
