cmake_minimum_required(VERSION 3.25)
include(FetchContent)

find_package(PkgConfig REQUIRED)

set(RES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/res)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -Wall -Wextra -Wpedantic -O0 -g2 -ggdb -Wno-psabi")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -Wall -Wextra -Wpedantic -O3 -Wno-psabi")
set(BUILD_SHARED_LIBS OFF)

project(kappa CXX C)
set(FETCHCONTENT_QUIET FALSE)

set(LIB_INCLUDE)
set(LIB_LINK)

# libfmt
find_package(fmt)
list(APPEND LIB_INCLUDE ${FMT_INCLUDE_DIRES})
list(APPEND LIB_LINK fmt::fmt)

# vulkan
find_package(VulkanHeaders REQUIRED)
#list(APPEND LIB_INCLUDE ${VULKANHEADERS_INCLUDE_DIRS})
list(APPEND LIB_LINK vulkan)

# vulkan allocator
add_subdirectory("lib/VulkanMemoryAllocator")
#list(APPEND LIB_INCLUDE GPUOpen::VulkanMemoryAllocator)
list(APPEND LIB_LINK GPUOpen::VulkanMemoryAllocator)

# ranmath
add_subdirectory("lib/ranmath")
list(APPEND LIB_LINK ranmath::ranmath)

# glfw
pkg_search_module(GLFW REQUIRED glfw3)
list(APPEND LIB_INCLUDE ${GLFW_INCLUDE_DIRS})
list(APPEND LIB_LINK glfw)

# imgui
file(GLOB IMGUI_SOURCES "lib/imgui/*.cpp")
list(APPEND IMGUI_SOURCES "lib/imgui/backends/imgui_impl_glfw.cpp")
list(APPEND IMGUI_SOURCES "lib/imgui/backends/imgui_impl_vulkan.cpp")
list(APPEND LIB_INCLUDE "lib/imgui" "lib/imgui/backends")
list(APPEND SOURCE_FILES ${IMGUI_SOURCES})

# chimatools
add_subdirectory("lib/chimatools")
list(APPEND LIB_LINK chimatools)

# assimp
set(ASSIMP_NO_EXPORT ON)
set(ASSIMP_INSTALL OFF)
set(ASSIMP_BUILD_TESTS OFF)
add_subdirectory("lib/assimp")
list(APPEND LIB_LINK assimp)

# Shaders
file(GLOB_RECURSE GLSL_SOURCES
	"${RES_DIR}/shaders/*.frag"
  "${RES_DIR}/shaders/*.vert"
  "${RES_DIR}/shaders/*.comp"
)

find_program(GLSL_VALIDATOR glslangValidator
  HINTS /usr/bin/ /usr/local/bin/ $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/
)

foreach(GLSL ${GLSL_SOURCES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
	set(SPIRV "${RES_DIR}/shaders/${FILE_NAME}.spv")
	message(STATUS "kappa: Shader registered ${GLSL} -> ${SPIRV}")
	add_custom_command(
	  OUTPUT ${SPIRV}
		COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
		DEPENDS ${GLSL}
  )
	list(APPEND SPIRV_BIN ${SPIRV})
endforeach(GLSL)

add_custom_target(kappa_shaders
  DEPENDS ${SPIRV_BIN}
)

# Project files
file(GLOB_RECURSE SOURCE_FILES "src/**.cpp")
file(GLOB_RECURSE HEADER_FILES "src/**.hpp")

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES})
add_dependencies(${PROJECT_NAME} kappa_shaders)
target_include_directories(${PROJECT_NAME} PUBLIC src ${LIB_INCLUDE})
set_target_properties(${PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX CXX_STANDARD 20)
target_link_libraries(${PROJECT_NAME} ${LIB_LINK})
