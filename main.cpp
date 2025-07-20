#include "bezierPawn/bezierCurvesPawn.h"
#include "textures/createTextureBase.h"
#include "textures/marble.h"
#include "textures/loadTextures.h"

#include "shaders/shaders.h"
#include "glfw/setupGLFW.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <chrono>


class DrawScene {
    public:
        std::vector<Vertex> verticesPawn;
        std::vector<unsigned int> indicesPawn;
        GLuint textureMarble{}, textureBase{}, textureFloor{};
        GLuint VAO{}, VBO{}, EBO{};

        unsigned char* pixelBufBase = nullptr;
        unsigned char* pixelBufFloor = nullptr;

        int generatedTextureWidth = 0, generatedTextureHeight = 0, generatedTextureChannels = 0;
        positionXYZ positionXYZ = {0.0f, 0.0f, 0.0f};

        explicit DrawScene() {
            // Create the Pawn and send vertices and indices to GPU
            generatePawnMesh(verticesPawn, indicesPawn);
            pawnMeshToGPU();

            // Load textures
            loadTextureFromMemory(marble_jpg, marble_jpg_len, textureMarble, "marble.h");
            createTexture(pixelBufBase, generatedTextureWidth, generatedTextureHeight, generatedTextureChannels, true);
            createTexture(pixelBufFloor, generatedTextureWidth, generatedTextureHeight, generatedTextureChannels, false);
            loadGeneratedTexture(textureBase, pixelBufBase, generatedTextureWidth, generatedTextureHeight);
            loadGeneratedTexture(textureFloor, pixelBufFloor, generatedTextureWidth, generatedTextureHeight);

            // Setup two spots on the scene
            setLighting();
        }

        void updatePawnMeshPositionXYZ() {
            positionXYZ = updateMovementAndMatrices(positionXYZ);
        }

        void draw() {
            updatePawnMeshPositionXYZ();

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textureMarble);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, textureBase);

            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indicesPawn.size()), GL_UNSIGNED_INT, nullptr);
        }

    private:
        void pawnMeshToGPU() {
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verticesPawn.size() * sizeof(Vertex)), verticesPawn.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indicesPawn.size() * sizeof(unsigned int)), indicesPawn.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));                      // aPos
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(3 * sizeof(float)));     // aTexCoord
            glEnableVertexAttribArray(1);

            glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(5 * sizeof(float)));     // aTexID
            glEnableVertexAttribArray(2);

            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(6 * sizeof(float)));     // aNormal
            glEnableVertexAttribArray(3);

            glBindVertexArray(0);
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

