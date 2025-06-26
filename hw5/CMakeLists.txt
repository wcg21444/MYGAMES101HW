cmake_minimum_required(VERSION 3.10)
set(CMAKE_TOOLCHAIN_FILE "D:/Program Files/vcpkg/vcpkg/scripts/buildsystems/vcpkg.cmake")
project(RayTracing)

set(CMAKE_CXX_STANDARD 17)

add_executable(RayTracing main.cpp Object.hpp Vector.hpp Sphere.hpp global.hpp Triangle.hpp Scene.cpp Scene.hpp Light.hpp Renderer.cpp)
target_compile_options(RayTracing PUBLIC
/W4     # 相当于 -Wall
/sdl    # 启用额外安全检查
/permissive- # 标准一致性模式
)
target_compile_features(RayTracing PUBLIC cxx_std_17)
target_link_libraries(RayTracing PUBLIC -fsanitize=undefined)
