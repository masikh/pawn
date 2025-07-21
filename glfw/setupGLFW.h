//
// Created by Robert Nagtegaal on 18/07/2025.
//

#ifndef SETUPGLFW_H
#define SETUPGLFW_H
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <thread>


// Window settings
inline bool isFullscreen = false;
inline int windowedX = 100, windowedY = 100;  // Starting position
inline int windowedWidth = 800, windowedHeight = 600;

// FPS guard
const double TARGET_FPS = 60.0;
const double FRAME_DURATION = 1.0 / TARGET_FPS;
inline std::chrono::high_resolution_clock::time_point lastFrameTime = std::chrono::high_resolution_clock::now();
void limitFrameRate();

// Set window
GLFWwindow* initWindow(GLFWmonitor** outMonitor, const GLFWvidmode** outMode);
void toggleFullscreen(GLFWwindow* window, GLFWmonitor* monitor, const GLFWvidmode* mode, bool& isFullscreen);

// Keyboard handler
inline static bool fWasPressed = false;
bool isKeyPressed(GLFWwindow* window, std::initializer_list<int> keys);
void checkKeyBoard(GLFWwindow* window, GLFWmonitor* monitor, const GLFWvidmode* mode);

// 3D-objects
struct glfwObject {
    /*
     * Struct to hold a 3d object and all its needs
     * like textures, shaders, etc..
     */

    struct Vertex {
        float x, y, z;
        float u, v;
        float texID;
        float nx, ny, nz;
    };

    // Vertices and indices
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Textures
    std::vector<GLuint> textures;

    // VAO, VBO and EBO
    GLuint VAO{}, VBO{}, EBO{};

    // Current object position and speed
    float positionX, positionY, positionZ, speed;

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

glfwObject updateMovementAndMatrices(glfwObject object); // movement of the pawn object.

// lights
void setLighting();

#endif //SETUPGLFW_H
