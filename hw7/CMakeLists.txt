cmake_minimum_required(VERSION 3.12)

project(RayTracing)

set(CMAKE_CXX_STANDARD 17)

set(DLL_FILES "${CMAKE_CURRENT_SOURCE_DIR}/jsoncpp.dll")


add_executable(RayTracing main.cpp Object.hpp Vector.cpp Vector.hpp Sphere.hpp global.hpp Triangle.hpp Scene.cpp
        Scene.hpp Light.hpp AreaLight.hpp BVH.cpp BVH.hpp Bounds3.hpp Ray.hpp Material.hpp Intersection.hpp
        Renderer.cpp Renderer.hpp Settings.hpp)

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD  
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${DLL_FILES}
        $<TARGET_FILE_DIR:${PROJECT_NAME}> 
    COMMENT "Copying DLLs to build directory..."
)
