cmake_minimum_required(VERSION 3.20)
project(Gemino)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)

set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)

set(GLM_TEST_ENABLE OFF)

find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/VulkanMemoryAllocator)

include_directories(${OPENGL_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${VULKAN_MEMORY_ALOCATOR_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/src)

file(GLOB_RECURSE ENGINE_SRC_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
    "${PROJECT_SOURCE_DIR}/src/*.hpp"
)

add_executable(Gemino ${ENGINE_SRC_FILES})

target_link_libraries(Gemino ${OPENGL_LIBRARIES} glfw Vulkan::Vulkan glm::glm VulkanMemoryAllocator)

file(GLOB_RECURSE SHADER_SRC_FILES
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.vert"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.frag"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.comp"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.tesc"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.tese"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.geom"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.rgen"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.rint"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.rahit"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.rchit"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.rmiss"
    "${CMAKE_CURRENT_LIST_DIR}/res/shaders/*.rcall"
)

foreach(file ${SHADER_SRC_FILES})
    add_custom_command(TARGET Gemino PRE_BUILD
        COMMAND $ENV{VULKAN_SDK}/bin/glslc ${file} -o ${file}.spv
    )
endforeach()

add_custom_command(TARGET Gemino POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/res/ ${PROJECT_BINARY_DIR}/res/
)
