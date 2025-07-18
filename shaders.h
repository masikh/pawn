//
// Created by Robert Nagtegaal on 18/07/2025.
//

#ifndef SHADERS_H
#define SHADERS_H
#include <GL/glew.h>

GLuint compileShader(GLenum type, const char* source);
GLuint createShaderProgram();
#endif //SHADERS_H
