cmake_minimum_required(VERSION 3.28)
project(Pawn)

set(CMAKE_CXX_STANDARD 20)

find_package(ZLIB REQUIRED)

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
        textures/generateTexture.cpp
        textures/generateTexture.h
        bezierPawn/bezierCurvesPawn.cpp
        bezierPawn/bezierCurvesPawn.h
        shaders/shaders.cpp
        shaders/shaders.h
        glfw/setupGLFW.cpp
        glfw/setupGLFW.h
        glfw/desktopPinned.mm
        textures/loadTextures.cpp
        textures/loadTextures.h
        textRendering/fontLoader.h
        textRendering/Roboto-Regular-ttf.h
        externalHeaders/stb_truetype.h
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

 if(APPLE)
         set_target_properties(Pawn PROPERTIES
                 MACOSX_BUNDLE TRUE
                 MACOSX_BUNDLE_BUNDLE_NAME "Pawn"
                 MACOSX_BUNDLE_GUI_IDENTIFIER "com.pawn.app"
                 MACOSX_BUNDLE_BUNDLE_VERSION "1"
                 MACOSX_BUNDLE_SHORT_VERSION_STRING "1.0"
                 MACOSX_BUNDLE_LONG_VERSION_STRING "1.0"
         )

         add_executable(PawnIconGen
                 tools/generate_pawn_icns.cpp
                 bezierPawn/bezierCurvesPawn.cpp
         )
         target_link_libraries(PawnIconGen PRIVATE ZLIB::ZLIB)
         target_include_directories(PawnIconGen PRIVATE
                 /opt/homebrew/include
                 ${GLFW_INCLUDE_DIRS}
                 ${GLEW_INCLUDE_DIRS}
                 ${CMAKE_CURRENT_SOURCE_DIR}
         )

         set(APP_ICON_NAME "Pawn.icns")
         set(APP_ICON_PATH "${CMAKE_CURRENT_BINARY_DIR}/${APP_ICON_NAME}")

         add_custom_command(
                 OUTPUT "${APP_ICON_PATH}"
                 COMMAND PawnIconGen "${APP_ICON_PATH}"
                 DEPENDS PawnIconGen
                 VERBATIM
         )

         add_custom_target(PawnIcon DEPENDS "${APP_ICON_PATH}")
         add_dependencies(Pawn PawnIcon)

         target_sources(Pawn PRIVATE "${APP_ICON_PATH}")
         set_source_files_properties("${APP_ICON_PATH}" PROPERTIES
                 MACOSX_PACKAGE_LOCATION "Resources"
                 GENERATED TRUE
         )
         set_target_properties(Pawn PROPERTIES
                 MACOSX_BUNDLE_ICON_FILE "${APP_ICON_NAME}"
         )
 endif()

# Post-build: Strip symbols from the binary and compress
add_custom_command(TARGET Pawn POST_BUILD
        # COMMAND strip -u -r $<TARGET_FILE:Pawn>
        COMMAND echo "Build completed"
        COMMENT "Stripping and compressing binary with UPX (Release build)"
)
