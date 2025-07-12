cmake_minimum_required(VERSION 3.31)
project(Pawn)

set(CMAKE_CXX_STANDARD 20)

# Find packages via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(GLEW REQUIRED glew)

# Add include and lib paths manually (if needed)
include_directories(
        /opt/homebrew/include
        ${GLFW_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR} # for stb_image.h if it's in the same dir
)

link_directories(
        /opt/homebrew/lib
        ${GLFW_LIBRARY_DIRS}
        ${GLEW_LIBRARY_DIRS}
)

# Create the executable
add_executable(Pawn main.cpp)

# Link libraries: GLFW, GLEW, OpenGL framework (required on macOS)
target_link_libraries(Pawn
        ${GLFW_LIBRARIES}
        ${GLEW_LIBRARIES}
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
)

# Copy texture file to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/data/marble.jpg
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/data)
