cmake_minimum_required(VERSION 3.20)
project(Gemino LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_EXE_LINKER_FLAGS "-static")

set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
set(GLFW_INSTALL OFF)

set(GLM_TEST_ENABLE OFF)

set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF)
set(TINYGLTF_BUILD_GL_EXAMPLES OFF)
set(TINYGLTF_BUILD_VALIDATOR_EXAMPLE OFF)
set(TINYGLTF_BUILD_BUILDER_EXAMPLE OFF)
set(TINYGLTF_HEADER_ONLY ON)
set(TINYGLTF_INSTALL OFF)

set(VMA_BUILD_DOCUMENTATION OFF)
set(VMA_BUILD_SAMPLES OFF)

find_package(Vulkan REQUIRED)

add_subdirectory(external/tinygltf)
add_subdirectory(external/glfw)
add_subdirectory(external/glm)
add_subdirectory(external/VulkanMemoryAllocator)

file(GLOB_RECURSE GEMINO_SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.hpp"
)

add_executable(${PROJECT_NAME} ${GEMINO_SRC_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${GLFW_INCLUDE_DIRS} ${GLM_INCLUDE_DIRS} ${VULKAN_MEMORY_ALOCATOR_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/external/stb)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw Vulkan::Vulkan glm::glm VulkanMemoryAllocator tinygltf)

file(GLOB_RECURSE GEMINO_SHADER_SRC_FILES
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

if(CMAKE_BUILD_TYPE MATCHES "Debug" OR CMAKE_BUILD_TYPE MATCHES "RelWithDebInfo")
    foreach(file ${GEMINO_SHADER_SRC_FILES})
        add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND $ENV{VULKAN_SDK}/bin/glslc ${file} -o ${file}.spv -g
        )
    endforeach()
    message(STATUS "${PROJECT_NAME}: Compiled shaders with debug info.")
else()
    foreach(file ${GEMINO_SHADER_SRC_FILES})
        add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND $ENV{VULKAN_SDK}/bin/glslc ${file} -o ${file}.spv
        )
    endforeach()
    message(STATUS "${PROJECT_NAME}: Compiled shaders without debug info.")
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/res/ ${PROJECT_BINARY_DIR}/res/
)
message(STATUS "${PROJECT_NAME}: Copied ${CMAKE_CURRENT_LIST_DIR}/res/ to ${PROJECT_BINARY_DIR}/res/")