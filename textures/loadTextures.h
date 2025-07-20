//
// Created by Robert Nagtegaal on 20/07/2025.
//

#ifndef LOADTEXTURES_H
#define LOADTEXTURES_H
#include <iostream>
#include <GL/glew.h>

void loadTextureFromMemory(const unsigned char* data, size_t len, GLuint& textureID, const std::string& name);
void loadGeneratedTexture(GLuint& textureID, const unsigned char* pixelData, int width, int height);
#endif //LOADTEXTURES_H
