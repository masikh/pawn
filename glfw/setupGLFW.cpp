//
// Created by Robert Nagtegaal on 18/07/2025.
//

#include "setupGLFW.h"
#include "../shaders/shaders.h"

int getPressedKey(GLFWwindow* window, std::initializer_list<int> keys) {
    for (int key : keys) {
        if (glfwGetKey(window, key) == GLFW_PRESS)
            return key;
    }
    return GLFW_KEY_UNKNOWN;
}

keyboardResult checkKeyBoard(GLFWwindow* window, keyboardResult &keyboard) {
    int key = getPressedKey(window, {GLFW_KEY_F, GLFW_KEY_Q, GLFW_KEY_ESCAPE, GLFW_KEY_R, GLFW_KEY_I, GLFW_KEY_COMMA, GLFW_KEY_PERIOD});

    switch (key) {
        case GLFW_KEY_F: // FullScreen on/off
            if (!keyWasPressed) {
                keyWasPressed = true;
                keyboard.key = GLFW_KEY_F;
                return keyboard;
            }
            break;

        case GLFW_KEY_ESCAPE:
        case GLFW_KEY_Q: // Escape program
            if (!keyWasPressed) {
                keyWasPressed = true;
                keyboard.key = GLFW_KEY_Q;
                return keyboard;
            }
            break;

        case GLFW_KEY_R: // Pawn stall rotation
            if (!keyWasPressed) {
                keyWasPressed = true;
                keyboard.key = GLFW_KEY_R;
                return keyboard;
            }
            break;

        case GLFW_KEY_I: // Pawn stall rotation
            if (!keyWasPressed) {
                keyWasPressed = true;
                keyboard.key = GLFW_KEY_I;
                return keyboard;
            }
            break;

        case GLFW_KEY_COMMA: // Decrease number of pawn radialDivisions
            if (!keyWasPressed) {
                keyWasPressed = true;
                keyboard.key = GLFW_KEY_COMMA;
                return keyboard;
            }
            break;

        case GLFW_KEY_PERIOD: // Increase number of pawn radialDivisions
            if (!keyWasPressed) {
                keyWasPressed = true;
                keyboard.key = GLFW_KEY_PERIOD;
                return keyboard;
            }
            break;

        case GLFW_KEY_UNKNOWN:
        default:
            keyWasPressed = false;
            break;
    }

    // If we reach here, no relevant key was pressed or was already held
    keyboard.key = GLFW_KEY_UNKNOWN;
    return keyboard;
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

    GLFWwindow* window = glfwCreateWindow(800, 450, "Pawn Viewer", nullptr, nullptr);
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

void setLighting(GLuint shaderProgram) {
    glUseProgram(shaderProgram);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUniform3f(glGetUniformLocation(shaderProgram, "lightPos1"), -10.0f, 0.0f, 0.0f);
    glUniform3f(glGetUniformLocation(shaderProgram, "lightPos2"), 0.0f, 10.0f, 0.0f);

    float brightness = 2.0f;
    glUniform3f(glGetUniformLocation(shaderProgram, "lightColor"),
                std::min(1.0f, brightness * 1.0f),
                std::min(1.0f, brightness * 1.0f),
                std::min(1.0f, brightness * 1.0f));

    glm::vec3 lightDir3 = glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f));
    glUniform3fv(glGetUniformLocation(shaderProgram, "lightDir3"), 1, glm::value_ptr(lightDir3));
}

void updatePawnMovement(glfwObject& object, float aspectRatio, bool pawnRotate) {
    static double lastRawTime = glfwGetTime();
    static float smoothTime = 0.0f;

    const double currentRawTime = glfwGetTime();
    float deltaTime = 0.0f;

    // Only calculate deltaTime and advance time if not paused
    if (pawnRotate) {
        deltaTime = static_cast<float>(currentRawTime - lastRawTime);
        smoothTime += deltaTime * object.speed;  // use previous speed or base speed
    }

    lastRawTime = currentRawTime;

    float t = smoothTime;

    // --- Smooth, fluctuating speed ---
    float baseSpeed = 1.8f
        + 0.6f * static_cast<float>(sin(currentRawTime * 0.8f))
        + 0.2f * static_cast<float>(sin(currentRawTime * 3.5f + 1.0f));
    baseSpeed = glm::clamp(baseSpeed, 1.8f, 4.5f);
    object.speed = baseSpeed;

    // --- Movement calculations using t ---
    float z_mod = 2.5f + 1.5f * static_cast<float>(sin((t * 0.4f) + M_PI));

    float maxXAmplitude = 3.2f * glm::smoothstep(1.0f, 4.0f, z_mod);
    float raw_x =
        sin(t * 0.25f) * 0.3f +
        sin(t * 0.12f + 1.0f) * 0.6f +
        sin(t * 0.05f - 1.0f) * 0.5f;
    object.positionX = raw_x * maxXAmplitude;

    float maxYAmplitude = 0.5f * glm::smoothstep(1.0f, 4.0f, z_mod);
    float raw_y =
        sin(t * 0.35f) * 0.5f +
        sin(t * 0.13f + 0.8f) * 0.3f +
        sin(t * 0.07f - 0.5f) * 0.2f;

    float y_mod = glm::clamp(raw_y * maxYAmplitude, 0.0f, 0.5f);
    z_mod = glm::clamp(z_mod, 0.0f, 100.0f);

    const double angle_x = 180.0 + 50.0 * sin(t * 0.3);
    const double angle_y = 60.0 + 60.0 * sin(t * 0.5 + sin(t * 0.07));
    const double angle_z = 10.0 + 90.0 * cos(t * 0.25 + cos(t * 0.05));

    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(object.positionX, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(static_cast<float>(angle_x)), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(static_cast<float>(angle_y)), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(static_cast<float>(angle_z)), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::vec3 cameraPos(0.0f, y_mod, z_mod);
    glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, y_mod, -z_mod));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    glm::mat4 mvp = projection * view * model;

    glUseProgram(object.shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(object.shaderProgram, "uMVP"), 1, GL_FALSE, glm::value_ptr(mvp));
    glUniformMatrix4fv(glGetUniformLocation(object.shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3f(glGetUniformLocation(object.shaderProgram, "viewPos"), cameraPos.x, cameraPos.y, cameraPos.z);
}


int limitFrameRate(std::chrono::high_resolution_clock::time_point &lastFrame,
                    std::chrono::high_resolution_clock::time_point &lastFrameDrop,
                    bool &frameRateRestored, double keyPressedTime) {

    // Suppress output on keyboard interaction
    if (glfwGetTime() - keyPressedTime < 2.0) {
        return -1;
    }

    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> frameTime = now - lastFrame;

    // Sleep if running faster than target
    if (frameTime.count() < FRAME_DURATION) {
        std::this_thread::sleep_for(std::chrono::duration<double>(FRAME_DURATION - frameTime.count()));
        frameTime = std::chrono::duration<double>(FRAME_DURATION);
        now = std::chrono::high_resolution_clock::now(); // Recalculate 'now' after sleep
    }

    lastFrame = now;

    // Frame duration exceeds an acceptable threshold — it's a drop
    int actualFPS = static_cast<int>(1.0 / frameTime.count());
    if (frameTime.count() > FRAME_DURATION + 0.001) {
        lastFrameDrop = now;
        if (actualFPS > 0) std::cout << "\033[2K\r❌ Frame rate " << actualFPS << "/" << TARGET_FPS << " FPS." << std::flush;
        frameRateRestored = false;
    } else {
        // If we've had good FPS for more than 1 second, announce recovery
        if (!frameRateRestored && (now - lastFrameDrop > std::chrono::seconds(1))) {
            std::cout << "\033[2K\r✅ Frame rate at " << TARGET_FPS << " FPS." << std::flush;
            frameRateRestored = true;
        }
        // Otherwise do nothing; we're still within the unstable 1s window
    }

    return actualFPS;
}