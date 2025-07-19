//
// Created by Robert Nagtegaal on 18/07/2025.
//

#include "setupGLFW.h"
#include "shaders.h"


GLFWwindow* initWindow(GLFWmonitor** outMonitor, const GLFWvidmode** outMode) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Get monitor and video mode for fullscreen switching later
    *outMonitor = glfwGetPrimaryMonitor();
    *outMode = glfwGetVideoMode(*outMonitor);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Pawn Viewer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window.\n";
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable VSync

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        std::cerr << "Failed to initialize GLEW.\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glEnable(GL_DEPTH_TEST);
    return window;
}

void toggleFullscreen(GLFWwindow* window, GLFWmonitor* monitor, const GLFWvidmode* mode, bool& isFullscreen) {
    if (isFullscreen) {
        // Switch to windowed
        glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, 0);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else {
        // Save current windowed size and position
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

        // Switch to fullscreen
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }

    isFullscreen = !isFullscreen;
}

float rotateAndSetLights(float position_x) {
    double time = glfwGetTime();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /*
    double angle_x = 180.0 + 50.0 * sin(time * 0.3);
    double angle_y = 60.0 + 60.0 * sin(time * 0.5);
    double angle_z = 90.0 + 90.0 * cos(time * 0.25);
    */

    double angle_x = 0.0;
    double angle_y = 60.0 + 60.0 * sin(time * 0.5);
    double angle_z = 180.0;

    /*
    position_x = 1.3f * static_cast<float>(sin(time * 0.25f));
    */
    position_x = 0.0f;

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position_x, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(static_cast<float>(angle_x)), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(static_cast<float>(angle_y)), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(static_cast<float>(angle_z)), glm::vec3(0.0f, 0.0f, 1.0f));

    /*
    float wiggle_y = 0.25f + 0.25f * static_cast<float>(sin((2.0f * M_PI / 7.0f) * time));
    */
    float wiggle_y = 0.5f;

    glm::vec3 cameraPos(0.0f, wiggle_y, 2.5f);  // Matches the inverse of the view matrix
    glUniform3f(viewPosLoc, cameraPos.x, cameraPos.y, cameraPos.z);

    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, wiggle_y, -2.5f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.f / 600.f, 0.1f, 100.0f);
    glm::mat4 mvp = projection * view * model;

    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    glUniform3f(lightPos1Loc, -10.0f,  0.0f, 0.0f);
    glUniform3f(lightPos2Loc,   0.0f, 10.0f, 0.0f);

    float brightness = 2.0f; // 2x brighter
    glUniform3f(lightColorLoc,
        std::min(1.0f, brightness * 1.0f),
        std::min(1.0f, brightness * 1.0f),
        std::min(1.0f, brightness * 1.0f));

    return position_x;
}