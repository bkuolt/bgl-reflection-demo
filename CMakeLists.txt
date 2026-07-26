cmake_minimum_required(VERSION 3.20)
project(bgl-reflection-demo LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glfw3 REQUIRED)
find_package(glad REQUIRED)
find_package(DevIL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(spdlog REQUIRED)
find_package(glm REQUIRED)

add_executable(bgl-reflection-demo
    src/main.cpp
    src/MD2.cpp
    src/MD2.hpp
    src/FileHeader.hpp
)

target_include_directories(bgl-reflection-demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(bgl-reflection-demo PRIVATE
    glfw
    glad::glad
    DevIL::IL
    OpenGL::GL
    spdlog::spdlog
    glm::glm
)

# Copy assets and shaders to binary execution directory
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/bin
        $<TARGET_FILE_DIR:bgl-reflection-demo>
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders
        $<TARGET_FILE_DIR:bgl-reflection-demo>/shaders
    COMMENT "Copying assets and shaders to execution directory..."
)
add_dependencies(bgl-reflection-demo copy_assets)
