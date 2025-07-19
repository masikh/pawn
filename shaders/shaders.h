//
// Created by Robert Nagtegaal on 18/07/2025.
//

#ifndef SHADERS_H
#define SHADERS_H
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

GLuint compileShader(GLenum type, const char* source);
GLuint createShaderProgram();
void setupShaders();

inline GLuint shaderProgram;
inline GLint mvpLoc;
inline GLint modelLoc;
inline GLint lightPos1Loc;
inline GLint lightPos2Loc;
inline GLint lightDir3Loc;
inline GLint lightColorLoc;
inline GLint viewPosLoc;
inline glm::vec3 lightDir3;

#endif //SHADERS_H
