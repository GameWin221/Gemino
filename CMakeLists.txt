cmake_minimum_required(VERSION 3.1)
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

add_custom_command(TARGET Gemino POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/res/ ${PROJECT_BINARY_DIR}/res/
    COMMENT "Copied resources from ${CMAKE_CURRENT_LIST_DIR} to ${CMAKE_BINARY_DIR}"
)
