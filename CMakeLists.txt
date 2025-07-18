cmake_minimum_required(VERSION 3.31)
project(Pawn)

set(CMAKE_CXX_STANDARD 20)

# Enable LTO for dead code elimination and internal compression:
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

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
add_executable(Pawn main.cpp
        createTextureBase.cpp
        createTextureBase.h
        bezierCurvesPawn.cpp
        bezierCurvesPawn.h
        shaders.cpp
        shaders.h
)

# Link libraries: GLFW, GLEW, OpenGL framework (required on macOS)
target_link_libraries(Pawn
        ${GLFW_LIBRARIES}
        ${GLEW_LIBRARIES}
        "-framework OpenGL"
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreVideo"
)

# Post-build: Strip symbols from the binary and compress
add_custom_command(TARGET Pawn POST_BUILD
        COMMAND strip -u -r $<TARGET_FILE:Pawn>
        COMMENT "Stripping and compressing binary with UPX (Release build)"
)
