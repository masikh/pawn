#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "pawn_coordinates.h"
#include "marble_jpg.h"
#include "base_jpg.h"

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
            float texID;
            float nx, ny, nz; // NEW: normal
        };

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        GLuint textureMarble{}, textureBase{};
        GLuint VAO{}, VBO{}, EBO{};
        int profileSize = static_cast<int>(pawn_profile.size());

        explicit Pawn()
            : coordinates(pawn_profile) {
            computePawnCoordinates();
            addFlatSquareQuad();
            loadTextureFromMemory(marble_jpg, marble_jpg_len, textureMarble, "marble_jpg.h");
            loadTextureFromMemory(base_jpg, base_jpg_len, textureBase, "base_jpg.h");
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

            // Precompute profile normals for revolution (approximate derivative along profile curve)
            std::vector<glm::vec2> profileNormals(profileSize);
            for (int j = 0; j < profileSize; ++j) {
                int prev = std::max(0, j - 1);
                int next = std::min(profileSize - 1, j + 1);

                float dx = coordinates[next].first - coordinates[prev].first;
                float dy = coordinates[next].second - coordinates[prev].second;

                glm::vec2 tangent(dx, dy);
                glm::vec2 normal = glm::normalize(glm::vec2(-dy, dx));
                profileNormals[j] = normal;
            }

            for (int i = 0; i <= segments; ++i) {
                float theta = static_cast<float>(i) * DEG2RAD;
                float cosTheta = std::cos(theta);
                float sinTheta = std::sin(theta);

                for (int j = 0; j < profileSize; ++j) {
                    const auto& coord = coordinates[j];
                    float r = coord.first;
                    float y = coord.second;

                    float x = r * cosTheta;
                    float z = r * sinTheta;

                    float u = static_cast<float>(i) / segments;
                    float v = y / maxY;

                    float texID = (j >= profileSize - 3) ? 1.0f : 0.0f;

                    // Compute 3D normal from 2D profile normal rotated around Y-axis
                    const glm::vec2& n2d = profileNormals[j];
                    glm::vec3 normal(
                        n2d.x * cosTheta,
                        n2d.y,
                        n2d.x * sinTheta
                    );

                    vertices.push_back({x, y, z, u, v, texID, normal.x, normal.y, normal.z});
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

            glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);

            std::vector<Vertex> quadVerts = {
                { -0.5f, 0.995f, -0.5f, 0.0f, 0.0f, 1.0f, normal.x, normal.y, normal.z }, // Bottom-left
                {  0.5f, 0.995f, -0.5f, 1.0f, 0.0f, 1.0f, normal.x, normal.y, normal.z }, // Bottom-right
                {  0.5f, 0.995f,  0.5f, 1.0f, 1.0f, 1.0f, normal.x, normal.y, normal.z }, // Top-right
                { -0.5f, 0.995f,  0.5f, 0.0f, 1.0f, 1.0f, normal.x, normal.y, normal.z }, // Top-left
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

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(0));                      // aPos
            glEnableVertexAttribArray(0);

            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(3 * sizeof(float)));     // aTexCoord
            glEnableVertexAttribArray(1);

            glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(5 * sizeof(float)));     // aTexID
            glEnableVertexAttribArray(2);

            glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(6 * sizeof(float)));     // aNormal
            glEnableVertexAttribArray(3);

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

        static void loadTextureFromMemory(const unsigned char* data, size_t len, GLuint& textureID, std::string name) {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            int width, height, nrChannels;
            stbi_set_flip_vertically_on_load(true);
            unsigned char* imgData = stbi_load_from_memory(data, static_cast<int>(len), &width, &height, &nrChannels, 0);

            if (!imgData) {
                std::cerr << "❌ stbi_load_from_memory failed\n";
                return;
            }

            GLenum format = (nrChannels == 4) ? GL_RGBA :
                            (nrChannels == 3) ? GL_RGB :
                            (nrChannels == 1) ? GL_RED : 0;

            GLenum internalFormat = (format == GL_RGBA) ? GL_RGBA8 :
                                     (format == GL_RGB) ? GL_RGB8 :
                                     (format == GL_RED) ? GL_RED : 0;

            std::cout << "✅ Loaded texture: " << name << "\n";
            std::cout << "   → Dimensions: " << width << "x" << height << "\n";
            std::cout << "   → Channels: " << nrChannels << "\n";
            std::cout << "   → Format: " << ((format == GL_RGB) ? "RGB" :
                                             (format == GL_RGBA) ? "RGBA" :
                                             (format == GL_RED) ? "RED" : "Unknown") << "\n";

            glTexImage2D(GL_TEXTURE_2D, 0, (GLint) internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, imgData);
            glGenerateMipmap(GL_TEXTURE_2D);
            stbi_image_free(imgData);
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
        layout(location = 3) in vec3 aNormal;

        out vec2 TexCoord;
        out float TexID;
        out vec3 WorldPos;
        out vec3 Normal;

        uniform mat4 uMVP;
        uniform mat4 uModel;

        void main() {
            gl_Position = uMVP * vec4(aPos, 1.0);
            TexCoord = aTexCoord;
            TexID = aTexID;
            WorldPos = vec3(uModel * vec4(aPos, 1.0));

            // Transform normal using the inverse transpose of the model matrix
            Normal = mat3(transpose(inverse(uModel))) * aNormal;
        }
    )";

    const char* fragmentShaderSource = R"(
        #version 330 core
        in vec2 TexCoord;
        in float TexID;
        in vec3 WorldPos;
        in vec3 Normal;

        out vec4 FragColor;

        uniform sampler2D texture1; // marble
        uniform sampler2D texture2; // base

        uniform vec3 lightPos1;
        uniform vec3 lightPos2;
        uniform vec3 lightDir3;  // NEW: constant direction light
        uniform vec3 lightColor;
        uniform vec3 viewPos;  // Camera position

        void main() {
            vec4 baseColor;

            if (TexID < 0.5) {
                baseColor = texture(texture1, TexCoord);
            } else {
                // Circular mask around center (0.5, 0.5)
                vec2 centeredUV = TexCoord - vec2(0.5);
                float dist = length(centeredUV);

                float radius = 0.385;
                float edgeWidth = 0.01; // smoothstep edge width

                float alpha = 1.0 - smoothstep(radius - edgeWidth, radius + edgeWidth, dist);
                baseColor = texture(texture2, TexCoord) * vec4(1.0, 1.0, 1.0, alpha);

                if (alpha < 0.01)
                    discard;
            }

            // Normalize normal
            vec3 norm = normalize(Normal);

            // View direction
            vec3 viewDir = normalize(viewPos - WorldPos);

            // Light directions
            vec3 lightDir1 = normalize(lightPos1 - WorldPos);
            vec3 lightDir2 = normalize(lightPos2 - WorldPos);
            vec3 lightDir3Norm = normalize(-lightDir3); // assuming it's a direction, not a position

            float diff1 = max(dot(norm, lightDir1), 0.0);
            float diff2 = max(dot(norm, lightDir2), 0.0);
            float diff3 = max(dot(norm, lightDir3Norm), 0.0) * 0.3; // fill light, softer

            float brightnessFactor = 2.2;
            float diffuseLighting = ((diff1 + diff2) * 0.5 + diff3) * brightnessFactor;

            // Initialize specular
            vec3 specular = vec3(0.0);

            if (TexID < 0.5) {
                // Only apply specular to top surface (marble)
                float shininess = 128.0;
                float specularStrength = 0.3;

                vec3 reflectDir1 = reflect(-lightDir1, norm);
                vec3 reflectDir2 = reflect(-lightDir2, norm);

                float spec1 = pow(max(dot(viewDir, reflectDir1), 0.0), shininess);
                float spec2 = pow(max(dot(viewDir, reflectDir2), 0.0), shininess);

                float specularLighting = ((spec1 + spec2) * 0.5) * brightnessFactor;
                specular = specularStrength * lightColor * specularLighting;
            }

            // Combine diffuse and specular
            vec3 litColor = baseColor.rgb * lightColor * diffuseLighting + specular;

            FragColor = vec4(litColor, baseColor.a);
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

    Pawn pawn{};

    GLint mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    GLint modelLoc = glGetUniformLocation(shaderProgram, "uModel");
    GLint lightPos1Loc = glGetUniformLocation(shaderProgram, "lightPos1");
    GLint lightPos2Loc = glGetUniformLocation(shaderProgram, "lightPos2");
    GLint lightDir3Loc = glGetUniformLocation(shaderProgram, "lightDir3");
    GLint lightColorLoc = glGetUniformLocation(shaderProgram, "lightColor");
    GLint viewPosLoc = glGetUniformLocation(shaderProgram, "viewPos");

    glm::vec3 lightDir3 = glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f));  // Fill light from above-front-right
    glUniform3fv(lightDir3Loc, 1, glm::value_ptr(lightDir3));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float position_x = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
            if (!fWasPressed) {
                toggleFullscreen(window, monitor, mode, isFullscreen);
                fWasPressed = true;
            }
        } else {
            fWasPressed = false;
        }

        double time = glfwGetTime();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        double angle_x = 180.0 + 50.0 * sin(time * 0.3);
        double angle_y = 60.0 + 60.0 * sin(time * 0.5);
        double angle_z = 90.0 + 90.0 * cos(time * 0.25);

        position_x = 1.3f * static_cast<float>(sin(time * 0.25f));

        glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(position_x, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(static_cast<float>(angle_x)), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(static_cast<float>(angle_y)), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(static_cast<float>(angle_z)), glm::vec3(0.0f, 0.0f, 1.0f));

        float wiggle_y = 0.25f + 0.25f * static_cast<float>(sin((2.0f * M_PI / 7.0f) * time));

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

        pawn.draw();

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

