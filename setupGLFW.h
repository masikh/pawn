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

inline bool isFullscreen = false;
inline int windowedX = 100, windowedY = 100;  // Starting position
inline int windowedWidth = 800, windowedHeight = 600;

GLFWwindow* initWindow(GLFWmonitor** outMonitor, const GLFWvidmode** outMode);
void toggleFullscreen(GLFWwindow* window, GLFWmonitor* monitor, const GLFWvidmode* mode, bool& isFullscreen);
float rotateAndSetLights(float position_x);
#endif //SETUPGLFW_H
