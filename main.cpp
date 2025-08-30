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
        float aspectRatio = 16.0f / 9.0f;
        bool pawnRotate = true;
        bool abort = false;
        bool shouldRun = true;
        float currentTime = 0.0f, deltaTime = 0.0f, lastFrameTime = 0.0f;
        int radialDivisions = 40;

        explicit DrawScene() {
            // Load textures
            loadTextureFromMemory(marble_jpg, marble_jpg_len, textureMarble, "marble.h");
            generateTexture(textureBase, true);
            generateTexture(textureFloor, false);

            // Create the Pawn and send vertices and indices to GPU
            setupPawn();
            pawn.textures.push_back(textureMarble);
            pawn.textures.push_back(textureBase);

            // Setup two spots on the scene
            pawn.shaderProgram = createShaderProgram();
            assert(pawn.shaderProgram != 0 && "Pawn shader creation failed");
            setLighting(pawn.shaderProgram);
            glEnable(GL_DEPTH_TEST);
        }

        void setupPawn() {
            pawn.vertices = {}; pawn.indices = {};
            generatePawnMesh(pawn.vertices, pawn.indices, 100, radialDivisions);
            pawn.uploadToGPU();
        }

        void draw() {
            // Do this only once in the program!!!
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear buffers
            glUseProgram(pawn.shaderProgram);

            // set current time and compute deltaTime
            currentTime = static_cast<float>(glfwGetTime());
            deltaTime = currentTime - lastFrameTime;
            lastFrameTime = currentTime;

            if (abort) {
                // Fade the scene to black and exit program by setting shouldRun to false.
                float targetFade = 0.0f;  // Target for fade-out
                float lerpSpeed = 1.0f;   // Controls how fast it fades (higher = faster)

                pawn.uFadeFactor = glm::mix(pawn.uFadeFactor, targetFade, lerpSpeed * deltaTime);
                if (pawn.uFadeFactor < 0.1f) shouldRun = false;  // Lerp never reaches 0 (Zeno, loves this!)
            }

            glUniform1f(glGetUniformLocation(pawn.shaderProgram, "uFadeFactor"), pawn.uFadeFactor);
            updatePawnMovement(pawn, aspectRatio, pawnRotate);

            // Assign texture names in shader texture(x), activate and bind...
            for (int i = 0; i < pawn.textures.size(); ++i) {
                std::string textureName = "texture" + std::to_string(i);
                glUniform1i(glGetUniformLocation(pawn.shaderProgram, textureName.c_str()), i);
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, pawn.textures[i]);
            }

            glBindVertexArray(pawn.VAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(pawn.indices.size()), GL_UNSIGNED_INT, nullptr);
        }

        void keyboard (const keyboardResult &keyboard) {
            if (keyboard.key == GLFW_KEY_Q) {
                abort = true;
            }
            if (keyboard.key == GLFW_KEY_1) {
                pawnRotate = !pawnRotate;
                std::cout << "\nℹ️ " << (pawnRotate ? " Starting pawn rotation." : " Stopping pawn rotation.") << std::flush;
            }
            if (keyboard.key == GLFW_KEY_COMMA) {
                if (radialDivisions > 3) radialDivisions--;
                setupPawn();
                std::cout << "\nℹ️ Radial divisions of pawn: " << radialDivisions << std::flush;
            }
            if (keyboard.key == GLFW_KEY_PERIOD) {
                if (radialDivisions < 40) radialDivisions++;
                setupPawn();
                std::cout << "\nℹ️ Radial divisions of pawn: " << radialDivisions << std::flush;
            }
        }
};


int main() {
    GLFWmonitor* monitor = nullptr;
    const GLFWvidmode* mode = nullptr;
    GLFWwindow* window = initWindow(&monitor, &mode);

    DrawScene scene{};

    // Attach resize handler to recompute aspect ratio only after resizing stops
    ResizeHandler resizeHandler(window, [&](int width, int height) {
        scene.aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        std::cout << "\n✅ Window Resized:\n";
        std::cout << "   → Dimensions: " << width << "x" << height << "\n";
        std::cout << "   → Aspect ratio: " << scene.aspectRatio << "\n";
    });

    bool frameRateRestored = true;
    std::chrono::high_resolution_clock::time_point lastFrame;
    std::chrono::high_resolution_clock::time_point lastFrameDrop;

    double keyPressedTime = glfwGetTime();
    keyboardResult keyboard{};
    std::cout << "\nControls: [Esc/q] Quit | [f] Fullscreen | [1] Toggles rotation | [<,>] Adjust radial divisions \n\n" << std::flush;

    while (scene.shouldRun) {
        // Check for fullscreen or exit program keys
        keyboard = checkKeyBoard(window, keyboard);
        if (keyboard.key == GLFW_KEY_F) {
            toggleFullscreen(window, monitor, mode, isFullscreen);
            std::cout << "\nℹ️ " << (isFullscreen ? " Switching to full screen mode.\n" : " Exiting full screen mode.\n") << std::flush;
            keyPressedTime = glfwGetTime();
        }

        // Draw the scene
        scene.keyboard(keyboard);
        scene.draw();

        // Swap buffers and poll for events
        glfwSwapBuffers(window);
        glfwPollEvents();

        // resizeHandler with debounce
        resizeHandler.update();

        // Throttle the framerate
        limitFrameRate(lastFrame, lastFrameDrop, frameRateRestored, keyPressedTime);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

