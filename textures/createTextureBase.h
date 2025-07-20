//
// Created by Robert Nagtegaal on 14/07/2025.
//

#ifndef CREATETEXTURE_H
#define CREATETEXTURE_H
#include <GL/glew.h>

inline int generatedTextureWidth = 0, generatedTextureHeight = 0, generatedTextureChannels = 0;
inline const int tileCountX = 120, tileCountY = 68;
inline unsigned char* pixelBuf = nullptr;

void createTexture(GLuint &textureId, int &width, int &height, int &channels, bool logo);
#endif //CREATETEXTURE_H
