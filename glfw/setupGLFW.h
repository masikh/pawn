//
// Created by Robert Nagtegaal on 18/07/2025.
//
#pragma once

#ifndef SETUPGLFW_H
#define SETUPGLFW_H
#include <chrono>
#include <functional>
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
void limitFrameRate(std::chrono::high_resolution_clock::time_point &lastFrame,
                    std::chrono::high_resolution_clock::time_point &lastFrameDrop,
                    bool &frameRateRestored, double keyPressedTime);

// Set window
GLFWwindow* initWindow(GLFWmonitor** outMonitor, const GLFWvidmode** outMode);
void toggleFullscreen(GLFWwindow* window, GLFWmonitor* monitor, const GLFWvidmode* mode, bool& isFullscreen);

// Keyboard handler
inline static bool keyWasPressed = false;
bool isKeyPressed(GLFWwindow* window, std::initializer_list<int> keys);
struct keyboardResult {
    int key;
};
keyboardResult checkKeyBoard(GLFWwindow* window, keyboardResult &keyboard);


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
    GLuint VAO = 0, VBO = 0, EBO = 0;

    // Current object position and speed
    float positionX = 0.0f, positionY = 0.0f, positionZ = 0.0f;
    float speed = 0.0f, angle = 0.0f;

    // Shader program (to be implemented...)
    GLuint shaderProgram = 0;
    float uFadeFactor = 1.0f;

    // Upload geometry to GPU
    void uploadToGPU() {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(3 * sizeof(float))); // Tex coords
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(5 * sizeof(float))); // Tex ID
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(6 * sizeof(float))); // Normal
        glEnableVertexAttribArray(3);

        glBindVertexArray(0);
    }
};

void updatePawnMovement(glfwObject &object, float aspectRatio, bool pawnRotate); // movement of the pawn object.

// lights
void setLighting(GLuint shaderProgram);

// Resize handler
class ResizeHandler {
    public:
        using ResizeCallback = std::function<void(int width, int height)>;

        explicit ResizeHandler(GLFWwindow* window, ResizeCallback callback, int debounceMs = 200)
            : window(window), userCallback(std::move(callback)), debounceDelay(debounceMs)
        {
            glfwSetWindowUserPointer(window, this);
            glfwSetFramebufferSizeCallback(window, ResizeHandler::framebuffer_size_callback);
        }

        // Call this once per frame in the main loop
        void update() {
            if (resizePending) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastResizeTime).count();
                if (elapsed > debounceDelay) {
                    int width, height;
                    glfwGetFramebufferSize(window, &width, &height);
                    userCallback(width, height);
                    resizePending = false;
                }
            }
        }

    private:
        GLFWwindow* window{};
        ResizeCallback userCallback;
        int debounceDelay; // ms debounce
        bool resizePending = false;
        std::chrono::steady_clock::time_point lastResizeTime;

        static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
            if (height == 0) return; // avoid divide-by-zero
            auto* handler = static_cast<ResizeHandler*>(glfwGetWindowUserPointer(window));
            handler->resizePending = true;
            handler->lastResizeTime = std::chrono::steady_clock::now();
        }
};
#endif //SETUPGLFW_H
