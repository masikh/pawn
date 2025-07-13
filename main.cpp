#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "pawn_coordinates.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <iostream>
#include <vector>
#include <utility>
#include <chrono>
#include <ranges>
#include <thread>

bool isFullscreen = false;
int windowedX = 100, windowedY = 100;  // Starting position
int windowedWidth = 800, windowedHeight = 600;


class Pawn {
    public:
        struct Vertex {
            float x, y, z;
            float u, v;
            float texID; // 0.0 for marble, 1.0 for base
        };

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        GLuint textureMarble{}, textureBase{};
        GLuint VAO{}, VBO{}, EBO{};
        int profileSize = static_cast<int>(pawn_profile.size());

        explicit Pawn(const char* texturePathMarble, const char* texturePathBase)
            : coordinates(pawn_profile) {
            computePawnCoordinates();
            addFlatSquareQuad();
            loadTexture(texturePathMarble, textureMarble);
            loadTexture(texturePathBase, textureBase);
            setupPawn();
        }

        void draw() const {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textureMarble);

            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, textureBase);

            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
        }

    private:
        const std::vector<std::pair<float, float>>& coordinates;

        void computePawnCoordinates() {
            constexpr int segments = 360;
            constexpr float DEG2RAD = M_PI / 180.0f;

            float maxY = 0.0f;
            for (const auto& val : coordinates | std::views::values)
                if (val > maxY) maxY = val;

            for (int i = 0; i <= segments; ++i) {
                float theta = static_cast<float>(i) * DEG2RAD;
                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);

                for (int j_index = 0; j_index < profileSize; ++j_index) {
                    const auto& j = coordinates[j_index];
                    float r = j.first;
                    float y = j.second;

                    float x = r * cosTheta;
                    float z = r * sinTheta;
                    float u = static_cast<float>(i) / segments;
                    float v = y / maxY;

                    float texID = (j_index >= profileSize - 3) ? 1.0f : 0.0f;
                    vertices.push_back({x, y, z, u, v, texID});
                }
            }

            for (int i = 0; i < segments; ++i) {
                for (int j = 0; j < profileSize - 1; ++j) {
                    int current = i * profileSize + j;
                    int next = (i + 1) * profileSize + j;

                    indices.push_back(current);
                    indices.push_back(next);
                    indices.push_back(current + 1);

                    indices.push_back(next);
                    indices.push_back(next + 1);
                    indices.push_back(current + 1);
                }
            }
        }

        void addFlatSquareQuad() {
            auto startIndex = static_cast<unsigned int>(vertices.size());

            std::vector<Vertex> quadVerts = {
                { -0.5f, 0.995f, -0.5f, 0.0f, 0.0f, 1.0f }, // Bottom-left
                {  0.5f, 0.995f, -0.5f, 1.0f, 0.0f, 1.0f }, // Bottom-right
                {  0.5f, 0.995f,  0.5f, 1.0f, 1.0f, 1.0f }, // Top-right
                { -0.5f, 0.995f,  0.5f, 0.0f, 1.0f, 1.0f }, // Top-left
            };

            std::vector<unsigned int> quadInds = {
                startIndex,     startIndex + 1, startIndex + 2,
                startIndex + 2, startIndex + 3, startIndex
            };

            vertices.insert(vertices.end(), quadVerts.begin(), quadVerts.end());
            indices.insert(indices.end(), quadInds.begin(), quadInds.end());
        }

        void setupPawn() {
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

            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

            glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(5 * sizeof(float)));
            glEnableVertexAttribArray(2);

            glBindVertexArray(0);
        }

        static void loadTexture(const char* path, GLuint& textureID) {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            int width, height, nrChannels;
            stbi_set_flip_vertically_on_load(true);
            unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0); // Don't force 4

            if (!data) {
                std::cerr << "❌ stbi_load failed for texture: " << path << "\n";
                std::cerr << "   Reason: " << stbi_failure_reason() << "\n";
                return;
            }

            GLenum format = 0, internalFormat = 0;
            if (nrChannels == 1) {
                format = internalFormat = GL_RED;
            } else if (nrChannels == 3) {
                format = GL_RGB;
                internalFormat = GL_RGB8;
            } else if (nrChannels == 4) {
                format = GL_RGBA;
                internalFormat = GL_RGBA8;
            } else {
                std::cerr << "❌ Unsupported image channel count (" << nrChannels << "): " << path << "\n";
                stbi_image_free(data);
                return;
            }

            std::cout << "✅ Loaded texture: " << path << "\n";
            std::cout << "   → Dimensions: " << width << "x" << height << "\n";
            std::cout << "   → Channels: " << nrChannels << "\n";
            std::cout << "   → Format: " << ((format == GL_RGB) ? "RGB" :
                                             (format == GL_RGBA) ? "RGBA" :
                                             (format == GL_RED) ? "RED" : "Unknown") << "\n";

            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);

            GLenum err = glGetError();
            if (err != GL_NO_ERROR) {
                std::cerr << "⚠️ OpenGL error after glTexImage2D: 0x" << std::hex << err << std::dec << "\n";
            }

            stbi_image_free(data);
        }
};

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "❌ Shader compilation failed:\n" << infoLog << "\n";
    }

    return shader;
}

GLuint createShaderProgram() {
    const char* vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec2 aTexCoord;
        layout(location = 2) in float aTexID;

        out vec2 TexCoord;
        out float TexID;
        out vec3 WorldPos;

        uniform mat4 uMVP;
        uniform mat4 uModel;

        void main() {
            gl_Position = uMVP * vec4(aPos, 1.0);
            TexCoord = aTexCoord;
            TexID = aTexID;
            WorldPos = vec3(uModel * vec4(aPos, 1.0)); // compute world-space position
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoord;
        in float TexID;
        in vec3 WorldPos;

        out vec4 FragColor;

        uniform sampler2D texture1; // marble
        uniform sampler2D texture2; // base

        void main() {
            vec4 color;

            if (TexID < 0.5) {
                color = texture(texture1, TexCoord);
            } else {
                // Circular mask around center (0.5, 0.5)
                vec2 centeredUV = TexCoord - vec2(0.5);
                float dist = length(centeredUV);

                float radius = 0.385;
                float edgeWidth = 0.01; // smoothstep edge width

                float alpha = 1.0 - smoothstep(radius - edgeWidth, radius + edgeWidth, dist);
                color = texture(texture2, TexCoord) * vec4(1.0, 1.0, 1.0, alpha);

                if (alpha < 0.01)
                    discard;
            }

            FragColor = color;
        }
    )";

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char infoLog[512];
        glGetProgramInfoLog(program, 512, nullptr, infoLog);
        std::cerr << "❌ Shader linking failed:\n" << infoLog << "\n";
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
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

int main() {
    static bool fWasPressed = false;
    GLFWmonitor* monitor = nullptr;
    const GLFWvidmode* mode = nullptr;
    GLFWwindow* window = initWindow(&monitor, &mode);

    GLuint shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture2"), 1);

    Pawn pawn("./data/marble.jpg", "./data/base.jpg");

    GLint mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");

    GLint currentTex0, currentTex1;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTex0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &currentTex1);
    glActiveTexture(GL_TEXTURE0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::cout << "🧩 Texture bindings: GL_TEXTURE0=" << currentTex0
              << ", GL_TEXTURE1=" << currentTex1 << "\n";

    float position_x = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        // Toggle fullscreen with 'F'
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
            if (!fWasPressed) {
                toggleFullscreen(window, monitor, mode, isFullscreen);
                fWasPressed = true;
            }
        } else {
            fWasPressed = false;
        }

        float reset_limit = 1.3f;
        double time = glfwGetTime();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // SMOOTH & CONTINUOUS ROTATION using sin/cos for fluid oscillation
        double angle_x = 180.0 + 50.0 * sin(time * 0.3);  // gentle sway in x-axis
        double angle_y = 60.0 + 60.0 * sin(time * 0.5);   // oscillates smoothly between 0°-120°
        double angle_z = 90.0 + 90.0 * cos(time * 0.25);  // full rotation-style oscillation

        // SMOOTH TRANSLATION back and forth using sine
        position_x = reset_limit * static_cast<float>(sin(time * 0.25f)); // smoothly loops from -2.0 to 2.0

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position_x, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(static_cast<float>(angle_x)), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(static_cast<float>(angle_y)), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(static_cast<float>(angle_z)), glm::vec3(0.0f, 0.0f, 1.0f));

        float wiggle_y = 0.5f + 0.25f * static_cast<float> (sin((2.0f * M_PI / 7.0f) * time));
        glm::mat4 view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, wiggle_y, -2.5f));
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), 800.f / 600.f, 0.1f, 100.0f);
        glm::mat4 mvp = projection * view * model;

        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

        pawn.draw();

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01); // optional: can remove for higher framerate
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
