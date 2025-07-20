//
// Created by Robert Nagtegaal on 18/07/2025.
//

#include "setupGLFW.h"
#include "../shaders/shaders.h"

bool isKeyPressed(GLFWwindow* window, std::initializer_list<int> keys) {
    for (int key : keys) {
        if (glfwGetKey(window, key) == GLFW_PRESS)
            return true;
    }
    return false;
}

void checkKeyBoard(GLFWwindow* window, GLFWmonitor* monitor, const GLFWvidmode* mode) {
    if (isKeyPressed(window, {GLFW_KEY_F})) {
        if (!fWasPressed) {
            toggleFullscreen(window, monitor, mode, isFullscreen);
            fWasPressed = !fWasPressed;
        }
    } else {
        fWasPressed = false;
    }

    if (isKeyPressed(window, {GLFW_KEY_ESCAPE, GLFW_KEY_Q})) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
}

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

void setLighting() {
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUniform3f(lightPos1Loc, -10.0f,  0.0f, 0.0f);
    glUniform3f(lightPos2Loc,   0.0f, 10.0f, 0.0f);

    float brightness = 2.0f;
    glUniform3f(lightColorLoc,
        std::min(1.0f, brightness * 1.0f),
        std::min(1.0f, brightness * 1.0f),
        std::min(1.0f, brightness * 1.0f));
}

positionXYZ updateMovementAndMatrices(positionXYZ positionXYZ) {
    static double lastRawTime = glfwGetTime();
    static float smoothTime = 0.0f;

    double currentRawTime = glfwGetTime();
    float deltaTime = static_cast<float>(currentRawTime - lastRawTime);
    lastRawTime = currentRawTime;

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // --- Smooth, fluctuating speed ---
    // Faster-changing speed
    float baseSpeed = 1.8f
                    + 0.6f * sin(currentRawTime * 0.8f)     // was 0.15f
                    + 0.2f * sin(currentRawTime * 3.5f + 1.0f); // was 0.5f
    baseSpeed = glm::clamp(baseSpeed, 1.8f, 4.5f);
    positionXYZ.speed = baseSpeed;

    // --- Accumulate smoothTime with fluctuating speed ---
    smoothTime += deltaTime * baseSpeed;
    float t = smoothTime;

    // Move towards us or back
    float z_mod = 2.5f + 1.5f * sin((t * 0.4f) + M_PI);

    // --- Non-predictable smooth x movement (using integrated time) ---
    // --- Adaptive horizontal movement based on depth (no clamp) ---
    float maxXAmplitude = 2.2f * glm::smoothstep(1.0f, 4.0f, z_mod); // 0 → 1 as z increases
    float raw_x =
        sin(t * 0.25f) * 0.3f +
        sin(t * 0.12f + 1.0f) * 0.6f +
        sin(t * 0.05f - 1.0f) * 0.5f;

    // Estimate and subtract average offset
    float bias = 0.0f;  // fine-tuning
    positionXYZ.x = (raw_x - bias) * maxXAmplitude;

    // Smooth y wiggle
    float maxYAmplitude = 0.5f * glm::smoothstep(1.0f, 4.0f, z_mod); // scales with depth

    float raw_y =
        sin(t * 0.35f) * 0.5f +
        sin(t * 0.13f + 0.8f) * 0.3f +
        sin(t * 0.07f - 0.5f) * 0.2f;

    float y_bias = 0.0f;  // fine-tuning
    float y_mod = (raw_y - y_bias) * maxYAmplitude;

    y_mod = glm::clamp(y_mod, 0.0f, 0.5f);
    z_mod = glm::clamp(z_mod, 1.0f, 4.0f);

    // --- Smooth rotations using t ---
    double angle_x = 180.0 + 50.0 * sin(t * 0.3);
    double angle_y = 60.0 + 60.0 * sin(t * 0.5 + sin(t * 0.07));
    double angle_z = 90.0 + 90.0 * cos(t * 0.25 + cos(t * 0.05));

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(positionXYZ.x, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(static_cast<float>(angle_x)), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(static_cast<float>(angle_y)), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(static_cast<float>(angle_z)), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::vec3 cameraPos(0.0f, y_mod, z_mod);
    glUniform3f(viewPosLoc, cameraPos.x, cameraPos.y, cameraPos.z);

    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, y_mod, -z_mod));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.f / 600.f, 0.1f, 100.0f);
    glm::mat4 mvp = projection * view * model;

    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    return positionXYZ;
}

void limitFrameRate() {
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> frameTime = now - lastFrameTime;

    if (frameTime.count() < FRAME_DURATION) {
        double sleepTime = FRAME_DURATION - frameTime.count();
        std::this_thread::sleep_for(std::chrono::duration<double>(sleepTime));
    } else if (frameTime.count() > FRAME_DURATION + 0.001) { // Allow small margin
        int actualFPS = static_cast<int>(1.0 / frameTime.count());
        std::cout << "[Warning] Frame drop detected! FPS: " << actualFPS << "\n";
    }

    lastFrameTime = std::chrono::high_resolution_clock::now(); // Reset for next frame
}