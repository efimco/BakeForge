cmake_minimum_required(VERSION 3.16.0)
project(TimeToDX VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions(-DNOMINMAX)

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

if(MSVC)
	add_compile_options(/MP)
	# Faster debug info format
	add_compile_options("$<$<CONFIG:Debug>:/Z7>")
endif()

add_library(IMGUI STATIC ${IMGUI})
add_library(DXTEX STATIC ${DXTEX})

add_executable(TimeToDX
				${SOURCE_FILES}
				${PASSES}
				${COMMANDS})

target_link_libraries(TimeToDX
				d3d11
				dxgi
				d3dcompiler
				IMGUI
				DXTEX
)
target_include_directories(TimeToDX
				PRIVATE
				${CMAKE_CURRENT_SOURCE_DIR}/src/
				${CMAKE_CURRENT_SOURCE_DIR}/src/passes
				${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include
				${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include/ImGui
				${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/Include/DXTEX
)
set_target_properties(TimeToDX PROPERTIES
				WIN32_EXECUTABLE TRUE
				VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
