cmake_minimum_required (VERSION 3.20.0)

set(CMAKE_TOOLCHAIN_FILE "D:/Program Files/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET "x64-windows")

project (Transformation)

find_package(Eigen3 REQUIRED)

add_executable (${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME}  PRIVATE Eigen3::Eigen)