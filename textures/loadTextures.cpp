//
// Created by Robert Nagtegaal on 20/07/2025.
//

#include "textures/stb_image.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>


void loadTextureFromMemory(const unsigned char* data, size_t len, GLuint& textureID, const std::string& name) {
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
