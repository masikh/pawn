#define STB_IMAGE_IMPLEMENTATION
#include "textures/stb_image.h"
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

struct glfwObject {
    /*
     * Struct to hold a 3d object and all its needs
     * like textures, shaders, etc..
     */

    // Vertices and indices
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Textures
    std::vector<GLuint> textures;

    // VAO, VBO and EBO
    GLuint VAO{}, VBO{}, EBO{};

    // Current object position and speed
    positionXYZ positionXYZ = {0.0f, 0.0f, 0.0f, 0.0f};

    // Shader program (to be implemented...)
    GLuint shaderProgram;

    void uploadToGPU() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

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

class DrawScene {
    public:
        glfwObject pawn{};

        GLuint textureMarble{}, textureBase{}, textureFloor{};

        explicit DrawScene() {
            // Load textures
            loadTextureFromMemory(marble_jpg, marble_jpg_len, textureMarble, "marble.h");
            createTexture(textureBase, generatedTextureWidth, generatedTextureHeight, generatedTextureChannels, true);
            createTexture(textureFloor,generatedTextureWidth, generatedTextureHeight, generatedTextureChannels, false);

            // Create the Pawn and send vertices and indices to GPU
            generatePawnMesh(pawn.vertices, pawn.indices);
            pawn.uploadToGPU();
            pawn.textures.push_back(textureMarble);
            pawn.textures.push_back(textureBase);

            // Setup two spots on the scene
            setLighting();
        }

        void draw() {
            pawn.positionXYZ = updateMovementAndMatrices(pawn.positionXYZ);

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

