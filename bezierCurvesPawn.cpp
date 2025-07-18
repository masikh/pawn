//
// Created by Robert Nagtegaal on 18/07/2025.
//

#include "bezierCurvesPawn.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <array>
#include <cmath>


glm::vec2 bezier(float t, const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    return uuu * p0 + 3 * uu * t * p1 + 3 * u * tt * p2 + ttt * p3;
}

// Takes a Bezier profile of N segments (each with 4 control points), returns sampled points
std::vector<std::pair<float, float>> sampleProfile(const std::vector<std::array<float, 8>>& profile, int samplesPerCurve) {
    std::vector<std::pair<float, float>> sampledPoints;

    for (const auto& segment : profile) {
        for (int i = 0; i < samplesPerCurve; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(samplesPerCurve - 1);
            sampledPoints.push_back(bezierPoint(segment, t));
        }
    }

    return sampledPoints;
}

std::vector<Vertex> generateRevolvedMesh(const std::vector<glm::vec2>& profile, int segments = 64) {
    std::vector<Vertex> vertices;
    float twoPi = 2.0f * M_PI;

    for (int i = 0; i < segments; ++i) {
        float theta = static_cast<float>(i) / static_cast<float>(segments) * twoPi;

        for (const auto& pt : profile) {
            float x = pt.x * cos(theta);
            float z = pt.x * sin(theta);
            float y = pt.y;

            glm::vec3 pos(x, y, z);
            glm::vec3 norm(cos(theta), 0.0f, sin(theta));  // Simplified normal

            vertices.push_back({ pos, glm::normalize(norm) });
        }
    }

    return vertices;
}

std::vector<unsigned int> generateIndices(int profilePoints, int segments) {
    std::vector<unsigned int> indices;

    for (int i = 0; i < segments; ++i) {
        int ringStart = i * profilePoints;
        int nextRingStart = ((i + 1) % segments) * profilePoints;

        for (int j = 0; j < profilePoints - 1; ++j) {
            indices.push_back(ringStart + j);
            indices.push_back(nextRingStart + j);
            indices.push_back(ringStart + j + 1);

            indices.push_back(ringStart + j + 1);
            indices.push_back(nextRingStart + j);
            indices.push_back(nextRingStart + j + 1);
        }
    }

    return indices;
}

void uploadToGPU() {
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void render() {
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
}

void generatePawnGeometry(const std::vector<std::array<float, 8>>& bezierSegments,
                          std::vector<Vertex>& outVertices,
                          std::vector<unsigned int>& outIndices,
                          int samplesPerCurve,
                          int segments) {
    auto profile = sampleProfile(bezierSegments, samplesPerCurve);
    std::vector<glm::vec2> profileVec2;
    for (const auto& [x, y] : profile)
        profileVec2.emplace_back(x, y);

    auto revolvedVertices = generateRevolvedMesh(profileVec2, segments);
    auto revolvedIndices = generateIndices(static_cast<int>(profile.size()), segments);

    // Append revolved geometry
    size_t startVertexIndex = outVertices.size();
    outVertices.insert(outVertices.end(), revolvedVertices.begin(), revolvedVertices.end());

    // Adjust indices and insert
    for (auto& idx : revolvedIndices)
        outIndices.push_back(static_cast<unsigned int>(idx + startVertexIndex));
}
