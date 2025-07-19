//
// Created by Robert Nagtegaal on 18/07/2025.
//

#include <vector>
#include <cmath>
#include <glm/glm.hpp>
#include "bezierCurvesPawn.h"


// ---- Step 1: Evaluate a single Bézier curve ----
curvePoint evaluateBezier(const Curve& c, float t) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    curvePoint p{};
    p.x = uuu * c.P0.x + 3 * uu * t * c.C1.x + 3 * u * tt * c.C2.x + ttt * c.P3.x;
    p.y = uuu * c.P0.y + 3 * uu * t * c.C1.y + 3 * u * tt * c.C2.y + ttt * c.P3.y;
    return p;
}

// Derivative of Bézier (for gradient)
curvePoint evaluateBezierDerivative(const Curve& c, float t) {
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;

    curvePoint d{};
    d.x = 3 * uu * (c.C1.x - c.P0.x) + 6 * u * t * (c.C2.x - c.C1.x) + 3 * tt * (c.P3.x - c.C2.x);
    d.y = 3 * uu * (c.C1.y - c.P0.y) + 6 * u * t * (c.C2.y - c.C1.y) + 3 * tt * (c.P3.y - c.C2.y);
    return d;
}

// ---- Step 2–3: Generate mesh from curves ----
void generatePawnMesh(
    std::vector<Vertex>& outVertices,
    std::vector<unsigned int>& outIndices,
    int curveResolution, // number of points sampled along each Bézier curve segment.
    int radialDivisions  // number of rotational steps around the Y-axis to create the 3D mesh
) {
    std::vector<curvePoint> profilePoints;
    std::vector<curvePoint> profileDerivatives;
    std::vector<float> tValues;

    // Sample all curves into a single profile
    for (const auto& curve : curves) {
        for (int i = 0; i <= curveResolution; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(curveResolution);
            profilePoints.push_back(evaluateBezier(curve, t));
            profileDerivatives.push_back(evaluateBezierDerivative(curve, t));
            tValues.push_back(t);
        }
    }

    // ---- Step 2: Revolve to 3D ----
    outVertices.clear();
    outIndices.clear();

    int rows = static_cast<int>(profilePoints.size());

    // Compute min and max Y from profilePoints to determine total vertical range
    float minY = FLT_MAX, maxY = -FLT_MAX;
    for (const auto& pt : profilePoints) {
        if (pt.y < minY) minY = pt.y;
        if (pt.y > maxY) maxY = pt.y;
    }
    float totalHeight = maxY - minY;

    // Build the mesh by revolving the profile
    for (int i = 0; i < rows; ++i) {
        const auto& p = profilePoints[i];
        const auto& dp = profileDerivatives[i];

        // Compute v based on actual height
        float v = (p.y - minY) / totalHeight;

        // Remap v: restart the texture after a certain vertical point
        float vAdjusted;
        float controlPoint = 0.24089038672798702f;  // from bezierCurvesPawn.h
        if (v <= controlPoint) {
            vAdjusted = v / controlPoint; // map [0, 0.0575] → [0, 1]
        } else {
            vAdjusted = (v - controlPoint) / (1.0f - controlPoint); // map [0.0575, 1.0] → [0, 1]
        }

        float texID = 0.0f; // Always use texture1

        for (int j = 0; j <= radialDivisions; ++j) {
            float u = static_cast<float>(j) / static_cast<float>(radialDivisions);
            float theta = 2.0f * static_cast<float>(M_PI) * u;

            // Position
            float x = p.x * cos(theta);
            float y = p.y;
            float z = p.x * sin(theta);

            // Tangent w.r.t t (profile direction)
            glm::vec3 dt(
                dp.x * cos(theta),
                dp.y,
                dp.x * sin(theta)
            );

            // Tangent w.r.t θ (rotation direction)
            glm::vec3 dtheta(
                -p.x * sin(theta),
                0.0f,
                p.x * cos(theta)
            );

            // Normal = cross product
            glm::vec3 normal = glm::normalize(glm::cross(dtheta, dt));

            Vertex vert{};
            vert.x = x;
            vert.y = y;
            vert.z = z;
            vert.u = u;
            vert.v = vAdjusted;
            vert.texID = texID;
            vert.nx = normal.x;
            vert.ny = normal.y;
            vert.nz = normal.z;

            outVertices.push_back(vert);
        }
    }

    // ---- Step 3: Build triangle indices ----
    for (int i = 0; i < rows - 1; ++i) {
        for (int j = 0; j < radialDivisions; ++j) {
            int curr = i * (radialDivisions + 1) + j;
            int next = (i + 1) * (radialDivisions + 1) + j;

            outIndices.push_back(curr);
            outIndices.push_back(next);
            outIndices.push_back(curr + 1);

            outIndices.push_back(curr + 1);
            outIndices.push_back(next);
            outIndices.push_back(next + 1);
        }
    }
}
