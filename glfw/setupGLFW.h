//
// Created by Robert Nagtegaal on 18/07/2025.
//

#ifndef SETUPGLFW_H
#define SETUPGLFW_H
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
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

// Movement and lights
struct positionXYZ {
    float x, y, z, speed;
};
positionXYZ updateMovementAndMatrices(positionXYZ positionXYZ);
void setLighting();

#endif //SETUPGLFW_H
