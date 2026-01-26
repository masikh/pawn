//
// Created by Robert Nagtegaal on 18/07/2025.
//

#ifndef SHADERS_H
#define SHADERS_H
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

GLuint compileShader(GLenum type, const char* source, bool text = false);
GLuint createShaderProgram(bool text);

inline GLuint shaderProgram;

#endif //SHADERS_H
