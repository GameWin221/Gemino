cmake_minimum_required(VERSION 3.14)
project(Gemino LANGUAGES CXX C)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)

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
add_subdirectory(external/imgui)
add_subdirectory(external/implot)
add_subdirectory(external/meshoptimizer)
add_subdirectory(external/VulkanMemoryAllocator)

file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.inl"
)

add_executable(${PROJECT_NAME} ${SRC_FILES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/external/stb ${CMAKE_CURRENT_LIST_DIR}/external/LegitProfiler)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw Vulkan::Vulkan glm::glm VulkanMemoryAllocator tinygltf ImGui ImPlot meshoptimizer)

if(NOT MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE -static)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE "_CRT_SECURE_NO_WARNINGS")
endif()

set(GLSL_FILES_DIR "${CMAKE_CURRENT_LIST_DIR}/src/renderer/shaders")

file(GLOB_RECURSE GLSL_FILES
    "${GLSL_FILES_DIR}/*.vert"
    "${GLSL_FILES_DIR}/*.frag"
    "${GLSL_FILES_DIR}/*.comp"
    "${GLSL_FILES_DIR}/*.tesc"
    "${GLSL_FILES_DIR}/*.tese"
    "${GLSL_FILES_DIR}/*.geom"
    "${GLSL_FILES_DIR}/*.rgen"
    "${GLSL_FILES_DIR}/*.rint"
    "${GLSL_FILES_DIR}/*.rahit"
    "${GLSL_FILES_DIR}/*.rchit"
    "${GLSL_FILES_DIR}/*.rmiss"
    "${GLSL_FILES_DIR}/*.rcall"
)

add_custom_command(TARGET ${PROJECT_NAME} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/res
)

foreach(FILE ${GLSL_FILES})
    file(RELATIVE_PATH GLSL_FILE_NAME ${GLSL_FILES_DIR} ${FILE})
    set(SPIRV_FILE "${PROJECT_BINARY_DIR}/shaders/${GLSL_FILE_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV_FILE}
        DEPENDS ${FILE}
        COMMAND $ENV{VULKAN_SDK}/bin/glslc ${FILE} -o ${SPIRV_FILE} "-g" "--target-env=vulkan1.2"
    )
    list(APPEND SPIRV_FILES ${SPIRV_FILE})
endforeach()

add_custom_target(compile_glsl DEPENDS ${SPIRV_FILES})
add_dependencies(${PROJECT_NAME} compile_glsl)