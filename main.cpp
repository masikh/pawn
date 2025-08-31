#define STB_IMAGE_IMPLEMENTATION
#include "externalHeaders/stb_image.h"
#include "bezierPawn/bezierCurvesPawn.h"
#include "textures/generateTexture.h"
#include "textures/loadTextures.h"
#include "textures/marble.h"
#include "textRendering/fontLoader.h"

#include "shaders/shaders.h"
#include "glfw/setupGLFW.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <chrono>


class DrawScene {
    public:
        glfwObject pawn{};
        GLuint gTextShader;

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
            pawn.shaderProgram = createShaderProgram(false);
            assert(pawn.shaderProgram != 0 && "Pawn shader creation failed");
            setLighting(pawn.shaderProgram);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Setup gTextShader
            gTextShader = createShaderProgram(true);
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

        void renderText(Font& font, const std::string& text,
                float x, float y, float scale, glm::vec3 color,
                int winW, int winH)
        {
            glUseProgram(gTextShader);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Set text color uniform
            glUniform3f(glGetUniformLocation(gTextShader, "textColor"),
                        color.x, color.y, color.z);

            // Setup orthographic projection (0,0) bottom-left
            glm::mat4 projection = glm::ortho(0.0f, float(winW),
                                              0.0f, float(winH));
            glUniformMatrix4fv(glGetUniformLocation(gTextShader, "projection"),
                               1, GL_FALSE, &projection[0][0]);

            // Bind font texture and VAO
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, font.textureID);
            glUniform1i(glGetUniformLocation(gTextShader, "text"), 0);
            glBindVertexArray(font.VAO);

            float origX = x;

            for (unsigned char c : text) {
                if (c < 32 || c >= 128) continue;

                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(font.cdata, font.texW, font.texH, c - 32, &x, &y, &q, 1);

                float xpos = q.x0 * scale;
                float ypos = q.y0 * scale;
                float w    = (q.x1 - q.x0) * scale;
                float h    = (q.y1 - q.y0) * scale;

                float vertices[6][4] = {
                    { xpos,     ypos + h,   q.s0, q.t1 },
                    { xpos,     ypos,       q.s0, q.t0 },
                    { xpos + w, ypos,       q.s1, q.t0 },

                    { xpos,     ypos + h,   q.s0, q.t1 },
                    { xpos + w, ypos,       q.s1, q.t0 },
                    { xpos + w, ypos + h,   q.s1, q.t1 }
                };

                glBindBuffer(GL_ARRAY_BUFFER, font.VBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glEnable(GL_DEPTH_TEST);
        }
};


int main() {
    GLFWmonitor* monitor = nullptr;
    const GLFWvidmode* mode = nullptr;
    GLFWwindow* window = initWindow(&monitor, &mode);
    Font font = loadFont("../textRendering/Roboto-Regular.ttf", 24.0f);

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
        // Query current framebuffer size and set viewport
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        glViewport(0, 0, fbW, fbH);

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

        // Throttle the framerate
        limitFrameRate(lastFrame, lastFrameDrop, frameRateRestored, keyPressedTime);
        std::stringstream ss;
        ss << "FPS: 100";
        scene.renderText(font, ss.str(), 10.0f, fbH - 30.0f, 1.0f, glm::vec3(1.0f, 1.0f, 0.0f), fbW, fbH);

        // Swap buffers and poll for events
        glfwSwapBuffers(window);
        glfwPollEvents();

        // resizeHandler with debounce
        resizeHandler.update();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

