cmake_minimum_required(VERSION 3.16.0)
project(BakeForge VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions(-DNOMINMAX)

# Option to enable release mode shader handling (shaders copied next to executable)
option(RELEASE_BUILD "Build for release with shaders next to executable" OFF)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(RELEASE_BUILD ON)
endif()

# Static runtime for distribution (no MSVC redistributable needed)
if(RELEASE_BUILD AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

file(GLOB SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")


file(GLOB_RECURSE IMGUI
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include/ImGui/*.cpp"
)

file(GLOB_RECURSE DXTEX
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include/DXTEX/*.cpp"
)

file(GLOB PASSES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/passes/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/passes/*.hpp")

file(GLOB CONTENT_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/res/*.*")

file(GLOB COMMANDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/commands/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/commands/*.hpp")

file(GLOB UTILITY
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utility/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utility/*.hpp")

if(MSVC)
    add_compile_options(/MP)
    # Faster debug info format
    add_compile_options("$<$<CONFIG:Debug>:/Z7>")
endif()

add_library(IMGUI STATIC ${IMGUI})
add_library(DXTEX STATIC ${DXTEX})

add_executable(BakeForge
    ${SOURCE_FILES}
    ${PASSES}
    ${COMMANDS}
    ${UTILITY})

target_link_libraries(BakeForge
    d3d11
    dxgi
    d3dcompiler
    IMGUI
    DXTEX
)
target_include_directories(BakeForge
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${CMAKE_CURRENT_SOURCE_DIR}/src/passes
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include/ImGui
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include/DXTEX
)

# Configure shader path based on build type
if(RELEASE_BUILD)
    target_compile_definitions(BakeForge PRIVATE SHADER_PATH=L"shaders/")
    target_compile_definitions(BakeForge PRIVATE RESOURCE_PATH=L"res/")

    # Copy shaders to output directory
    file(GLOB SHADER_FILES "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.hlsl")

    add_custom_command(TARGET BakeForge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:BakeForge>/shaders"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SHADER_FILES} "$<TARGET_FILE_DIR:BakeForge>/shaders/"
        COMMENT "Copying shaders to output directory"
    )

    # Copy resources to output directory
    add_custom_command(TARGET BakeForge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/res" "$<TARGET_FILE_DIR:BakeForge>/res"
        COMMENT "Copying resources to output directory"
    )
endif()

set_target_properties(BakeForge PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
