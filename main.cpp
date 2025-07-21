#define STB_IMAGE_IMPLEMENTATION
#include "externalHeaders/stb_image.h"
#include "bezierPawn/bezierCurvesPawn.h"
#include "textures/generateTexture.h"
#include "textures/loadTextures.h"
#include "textures/marble.h"

#include "shaders/shaders.h"
#include "glfw/setupGLFW.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <chrono>


class DrawScene {
    public:
        glfwObject pawn{};

        GLuint textureMarble{}, textureBase{}, textureFloor{};

        explicit DrawScene() {
            // Load textures
            loadTextureFromMemory(marble_jpg, marble_jpg_len, textureMarble, "marble.h");
            generateTexture(textureBase, true);
            generateTexture(textureFloor, false);

            // Create the Pawn and send vertices and indices to GPU
            generatePawnMesh(pawn.vertices, pawn.indices);
            pawn.uploadToGPU();
            pawn.textures.push_back(textureMarble);
            pawn.textures.push_back(textureBase);

            // Setup two spots on the scene
            setLighting();
        }

        void draw() {
            pawn = updateMovementAndMatrices(pawn);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, pawn.textures[0]);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, pawn.textures[1]);

            glBindVertexArray(pawn.VAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(pawn.indices.size()), GL_UNSIGNED_INT, nullptr);
        }
};

int main() {
    GLFWmonitor* monitor = nullptr;
    const GLFWvidmode* mode = nullptr;
    GLFWwindow* window = initWindow(&monitor, &mode);

    setupShaders();
    DrawScene scene{};

    while (!glfwWindowShouldClose(window)) {
        // Check for fullscreen or exit program keys
        checkKeyBoard(window, monitor, mode);

        // Draw the scene
        scene.draw();

        // Swap buffers and poll for events
        glfwSwapBuffers(window);
        glfwPollEvents();

        // Throttle the framerate
        limitFrameRate();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

