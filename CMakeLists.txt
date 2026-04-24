cmake_minimum_required(VERSION 3.10)
project(MyGame)

set(CMAKE_CXX_STANDARD 17)

# Include headers
include_directories(includes)

# Collect all cpp files
file(GLOB_RECURSE SOURCES src/*.cpp src/*.c)

# Create executable
add_executable(Finding_abnormallies ${SOURCES})

# Link libraries
target_link_libraries(Finding_abnormallies 
    ${CMAKE_SOURCE_DIR}/lib/glfw3.lib
    ${CMAKE_SOURCE_DIR}/lib/freetype.lib
    ${CMAKE_SOURCE_DIR}/lib/assimp.lib
    ${CMAKE_SOURCE_DIR}/lib/irrKlang.lib
    ${CMAKE_SOURCE_DIR}/lib/glew32s.lib
    ${CMAKE_SOURCE_DIR}/lib/SOIL.lib
    )

# Set root directory (this FIXES your error automatically)
target_compile_definitions(Finding_abnormallies PRIVATE ROOT_DIR="${CMAKE_SOURCE_DIR}")