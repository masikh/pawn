find_package(ZLIB REQUIRED)
find_package(OpenGL REQUIRED)

# Enable LTO for dead code elimination and internal compression:
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

# Find packages via pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(GLEW REQUIRED glew)

find_package(glm CONFIG QUIET)
if(NOT glm_FOUND)
        find_path(GLM_INCLUDE_DIR glm/glm.hpp)
        if(NOT GLM_INCLUDE_DIR)
                message(FATAL_ERROR "GLM not found. Install it (e.g. libglm-dev) or provide GLM_INCLUDE_DIR.")
        endif()
endif()

# Add include and lib paths manually (if needed)
include_directories(
        ${GLFW_INCLUDE_DIRS}
        ${GLEW_INCLUDE_DIRS}
        ${CMAKE_CURRENT_SOURCE_DIR} # for stb_image.h if it's in the same dir
)

if(NOT glm_FOUND)
        include_directories(${GLM_INCLUDE_DIR})
endif()

link_directories(
        ${GLFW_LIBRARY_DIRS}
        ${GLEW_LIBRARY_DIRS}
)

# Create the executable
add_executable(Pawn main.cpp
        textures/generateTexture.cpp
        textures/generateTexture.h
        bezierPawn/bezierCurvesPawn.cpp
        bezierPawn/bezierCurvesPawn.h
        shaders/shaders.cpp
        shaders/shaders.h
        glfw/setupGLFW.cpp
        glfw/setupGLFW.h
        textures/loadTextures.cpp
        textures/loadTextures.h
        textRendering/fontLoader.h
        textRendering/Roboto-Regular-ttf.h
        externalHeaders/stb_truetype.h
)

if(glm_FOUND)
        target_link_libraries(Pawn PRIVATE glm::glm)
endif()

# Link libraries
target_link_libraries(Pawn
        ${GLFW_LIBRARIES}
        ${GLEW_LIBRARIES}
        OpenGL::GL
)

# Post-build
add_custom_command(TARGET Pawn POST_BUILD
        COMMAND echo "Build completed"
        COMMENT "Build"
)
