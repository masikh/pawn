#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "shaders.h"
#include "setupGLFW.h"
#include "createTextureBase.h"
#include "pawn_coordinates.h"
#include "bezierCurvesPawn.h"
#include "marble_downsized.h"

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


class Pawn {
    public:
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        GLuint textureMarble{}, textureBase{};
        GLuint VAO{}, VBO{}, EBO{};
        int profileSize = static_cast<int>(pawn_profile.size());

        unsigned char* pixelBufBase = nullptr;

        int generatedTextureWidth = 0, generatedTextureHeight = 0, generatedTextureChannels = 0;

        explicit Pawn()
            : coordinates(pawn_profile) {
            // computePawnCoordinates();
            generatePawnMesh(vertices, indices);
            addFlatSquareQuad();
            loadTextureFromMemory(marble_jpg, marble_jpg_len, textureMarble, "marble_downsized.h");
            createTextureBase(pixelBufBase, generatedTextureWidth, generatedTextureHeight, generatedTextureChannels);
            loadGeneratedTexture(textureBase, pixelBufBase, generatedTextureWidth, generatedTextureHeight);

            pawnToGPU();
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

            std::vector quadInds = {
                startIndex,     startIndex + 1, startIndex + 2,
                startIndex + 2, startIndex + 3, startIndex
            };

            vertices.insert(vertices.end(), quadVerts.begin(), quadVerts.end());
            indices.insert(indices.end(), quadInds.begin(), quadInds.end());
        }

        void pawnToGPU() {
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

        static void loadGeneratedTexture(GLuint& textureID, const unsigned char* pixelData, int width, int height) {
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
            glGenerateMipmap(GL_TEXTURE_2D);

            std::cout << "✅ Loaded generated texture:\n";
            std::cout << "   → Dimensions: " << width << "x" << height << "\n";
            std::cout << "   → Channels: 4\n";
            std::cout << "   → Format: RGBA\n";
        }
};


int main() {
    static bool fWasPressed = false;
    GLFWmonitor* monitor = nullptr;
    const GLFWvidmode* mode = nullptr;
    GLFWwindow* window = initWindow(&monitor, &mode);

    setupShaders();

    Pawn pawn{};

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

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        position_x = rotateAndSetLights(position_x);

        pawn.draw();

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

