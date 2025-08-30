//
// Created by Robert Nagtegaal on 18/07/2025.
//
#include <GL/glew.h>
#include <iostream>
#include "shaders.h"

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

const char* fragmentShaderSourcePawn = R"(
    #version 330 core
    in vec2 TexCoord;
    in float TexID;
    in vec3 WorldPos;
    in vec3 Normal;

    out vec4 FragColor;

    uniform sampler2D texture0; // marble
    uniform sampler2D texture1; // base

    uniform float uFadeFactor;
    uniform vec3 lightPos1;
    uniform vec3 lightPos2;
    uniform vec3 lightDir3;
    uniform vec3 lightColor;
    uniform vec3 viewPos;

    void main() {
        // Apply animated UVs with per-object variation

        vec4 baseColor;

        if (TexID == 0.0) {
            baseColor = texture(texture0, TexCoord);
        } else if (TexID == 1.0) {
            // Animate base texture with circular mask
            vec2 centeredUV = TexCoord - vec2(0.5);
            float dist = length(centeredUV);

            float radius = 0.298;
            float edgeWidth = 0.001;

            float alpha = 1.0 - smoothstep(radius - edgeWidth, radius + edgeWidth, dist);
            baseColor = texture(texture1, TexCoord) * vec4(1.0, 1.0, 1.0, alpha);

            if (alpha < 0.01)
                discard;
        }

        // Lighting calculations
        vec3 norm = normalize(Normal);
        vec3 viewDir = normalize(viewPos - WorldPos);

        vec3 lightDir1 = normalize(lightPos1 - WorldPos);
        vec3 lightDir2 = normalize(lightPos2 - WorldPos);
        vec3 lightDir3Norm = normalize(-lightDir3);

        float diff1 = max(dot(norm, lightDir1), 0.0);
        float diff2 = max(dot(norm, lightDir2), 0.0);
        float diff3 = max(dot(norm, lightDir3Norm), 0.0) * 0.3;

        float brightnessFactor = 2.2;
        float diffuseLighting = ((diff1 + diff2) * 0.5 + diff3) * brightnessFactor;

        // Default specular properties
        float shininess = 128.0;
        float specularStrength = 0.3;

        // Remove specular for fabric (TexID == 1)
        if (TexID == 1.0) {
            specularStrength = 0.0;
            brightnessFactor = 0.4;
            shininess = 8.0;
        }

        vec3 reflectDir1 = reflect(-lightDir1, norm);
        vec3 reflectDir2 = reflect(-lightDir2, norm);

        float spec1 = pow(max(dot(viewDir, reflectDir1), 0.0), shininess);
        float spec2 = pow(max(dot(viewDir, reflectDir2), 0.0), shininess);

        float specularLighting = ((spec1 + spec2) * 0.5) * brightnessFactor;
        vec3 specular = specularStrength * lightColor * specularLighting;

        vec3 litColor = baseColor.rgb * lightColor * diffuseLighting + specular;
        FragColor = vec4(litColor * uFadeFactor, baseColor.a * uFadeFactor);
    }
)";

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
    GLuint vs = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    GLuint fs;
    fs = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSourcePawn);
    const GLuint program = glCreateProgram();
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