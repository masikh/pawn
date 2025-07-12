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
#include <thread>

class Pawn {
    public:
        struct Vertex {
            float x, y, z;
            float u, v;
        };

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        GLuint texture{}, VAO{}, VBO{}, EBO{};
        int profileSize = static_cast<int>(pawn_profile.size());

        explicit Pawn(const char* texturePath) : coordinates(pawn_profile) {
            computePawnCoordinates();
            loadTexture(texturePath);
            setupPawn();
        }

        void draw() const {
            glBindVertexArray(VAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
        }

        [[nodiscard]] GLuint getTexture() const { return texture; }

    private:
        const std::vector<std::pair<float, float>>& coordinates;

        void computePawnCoordinates() {
            const int segments = 360;
            const float DEG2RAD = M_PI / 180.0f;

            float maxY = 0.0f;
            for (auto& j : coordinates)
                if (j.second > maxY) maxY = j.second;

            for (int i = 0; i <= segments; ++i) {
                float theta = (float) i * DEG2RAD;
                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);

                for (auto& j : coordinates) {
                    float r = j.first;
                    float y = j.second;

                    float x = r * cosTheta;
                    float z = r * sinTheta;

                    float u = static_cast<float>(i) / segments;
                    float v = y / maxY;

                    vertices.push_back({x, y, z, u, v});
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

        void setupPawn() {
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)nullptr);
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);

            glBindVertexArray(0);
        }

        void loadTexture(const char* path) {
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            int width, height, nrChannels;
            stbi_set_flip_vertically_on_load(true);
            unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 4);

            if (data) {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);
                std::cout << "Loaded texture: " << path << " (" << width << "x" << height << ")\n";
            } else {
                std::cerr << "Failed to load texture: " << path << "\n";
            }

            stbi_image_free(data);
        }
};

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

GLuint createShaderProgram() {
    const char* vertexShaderSource = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec2 aTexCoord;
        out vec2 TexCoord;
        uniform mat4 uMVP;
        void main() {
            gl_Position = uMVP * vec4(aPos, 1.0);
            TexCoord = aTexCoord;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoord;
        out vec4 FragColor;
        uniform sampler2D texture1;
        void main() {
            FragColor = texture(texture1, TexCoord);
        }
    )";

    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

GLFWwindow* initWindow() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW.\n";
        exit(EXIT_FAILURE);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    if (!primaryMonitor) {
        std::cerr << "Failed to get primary monitor.\n";
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    const GLFWvidmode* mode = glfwGetVideoMode(primaryMonitor);
    if (!mode) {
        std::cerr << "Failed to get video mode.\n";
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    GLFWwindow* window = glfwCreateWindow(mode->width, mode->height, "Pawn Viewer", primaryMonitor, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW fullscreen window.\n";
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    // Hide the cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

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

int main() {
    GLFWwindow* window = initWindow();
    GLuint shaderProgram = createShaderProgram();
    glUseProgram(shaderProgram);

    Pawn pawn("./data/marble.jpg");

    GLint mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);

    double angle_x = 180.0f, angle_y = 0.0f, angle_z = 25.0f;
    float position_x = 0.0f, speed = -0.01f, reset_limit = 1.3f;

    float speed_z = 0.3f, speed_y = 0.3f; // velocity for y,z-axis rotation

    while (!glfwWindowShouldClose(window)) {
        double time = glfwGetTime();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // SMOOTH & CONTINUOUS ROTATION using sin/cos for fluid oscillation
        angle_x = 180.0 + 50.0 * sin(time * 0.3);  // gentle sway in x-axis
        angle_y = 60.0 + 60.0 * sin(time * 0.5);   // oscillates smoothly between 0°-120°
        angle_z = 90.0 + 90.0 * cos(time * 0.25);  // full rotation-style oscillation

        // SMOOTH TRANSLATION back and forth using sine
        position_x = reset_limit * sin(time * 0.25); // smoothly loops from -2.0 to 2.0

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position_x, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians((float)angle_x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians((float)angle_y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians((float)angle_z), glm::vec3(0.0f, 0.0f, 1.0f));

        float wiggle_y = 0.5f + 0.25f * sin((2.0f * M_PI / 7.0f) * time);
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
