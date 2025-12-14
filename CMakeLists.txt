cmake_minimum_required(VERSION 3.10.0)
project(TimeToDX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

file(GLOB SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
	
add_definitions(-DNOMINMAX)

file(GLOB_RECURSE INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include/*.cpp"
)

file(GLOB SHADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/shaders/*.hlsl")

set_source_files_properties(${SHADERS}
    PROPERTIES VS_TOOL_OVERRIDE "None")
file(GLOB PASSES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/passes/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/passes/*.hpp")

file(GLOB CONTENT_RESOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/res/*.*")

source_group(TREE "${CMAKE_SOURCE_DIR}" FILES
    ${PASSES}
    ${SHADERS}
    ${SOURCE_FILES}
    ${INCLUDES}
    ${CONTENT_RESOURCES})


add_executable(TimeToDX ${SOURCE_FILES} ${INCLUDES} ${SHADERS} ${PASSES} ${CONTENT_RESOURCES})

target_link_libraries(TimeToDX
    d3d11
    dxgi
    d3dcompiler
)
target_include_directories(TimeToDX
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/
    ${CMAKE_CURRENT_SOURCE_DIR}/src/passes
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include/ImGui
)
set_target_properties(TimeToDX PROPERTIES
    WIN32_EXECUTABLE TRUE
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:TimeToDX>"
)
