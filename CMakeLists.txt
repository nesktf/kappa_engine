cmake_minimum_required(VERSION 3.25)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -Wall -Wextra -Wpedantic -O0 -g2 -ggdb -Wno-psabi")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -Wall -Wextra -Wpedantic -O3 -Wno-psabi")
set(BUILD_SHARED_LIBS OFF)

project(kappa CXX C)

set(LIB_INCLUDE)
set(LIB_LINK)

# shogle
set(SHOGLE_ENABLE_IMGUI ON)
add_subdirectory("lib/shogle")
list(APPEND LIB_LINK shogle)

# chimatools
add_subdirectory("lib/chimatools")
list(APPEND LIB_LINK chimatools)

# assimp
set(ASSIMP_NO_EXPORT ON)
set(ASSIMP_INSTALL OFF)
set(ASSIMP_BUILD_TESTS OFF)
add_subdirectory("lib/assimp")
list(APPEND LIB_LINK assimp)

# Project files
file(GLOB_RECURSE SOURCE_FILES "src/**.cpp")
file(GLOB_RECURSE HEADER_FILES "src/**.hpp")
file(GLOB_RECURSE INLINE_FILES "src/**.innl")

add_executable(${PROJECT_NAME} ${SOURCE_FILES} ${HEADER_FILES} ${INLINE_FILES})
target_include_directories(${PROJECT_NAME} PUBLIC src/)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 20)
target_link_libraries(${PROJECT_NAME} ${LIB_LINK})
