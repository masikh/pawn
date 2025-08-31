#ifndef PAWN_FONTLOADER_H
#define PAWN_FONTLOADER_H
#define STB_TRUETYPE_IMPLEMENTATION
#include <cstdio>
#include <iostream>
#include <GL/glew.h>
#include "externalHeaders/stb_truetype.h"


struct Font {
    GLuint textureID;
    stbtt_bakedchar cdata[96]; // ASCII 32..126
    GLuint VAO, VBO;
    int texW = 512, texH = 512;
};

inline Font loadFont(const char* filename, float pixelHeight) {
    Font font{};

    // Load TTF file
    std::FILE* fp = fopen(filename, "rb");
    if (!fp) { std::cerr << "❌ Could not open font\n"; exit(1); }
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<unsigned char> buffer(size);
    fread(buffer.data(), 1, size, fp);
    fclose(fp);

    std::vector<unsigned char> bitmap(font.texW * font.texH);
    stbtt_BakeFontBitmap(buffer.data(), 0, pixelHeight,
                         bitmap.data(), font.texW, font.texH, 32, 96, font.cdata);

    // Ensure correct alignment for 8-bit (single-channel) rows
    GLint prevUnpack = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevUnpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Create atlas texture
    glGenTextures(1, &font.textureID);
    glBindTexture(GL_TEXTURE_2D, font.textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font.texW, font.texH, 0,
                 GL_RED, GL_UNSIGNED_BYTE, bitmap.data());

    // Restore unpack alignment so other uploads (e.g., RGBA) aren't affected
    glPixelStorei(GL_UNPACK_ALIGNMENT, prevUnpack);

    // Sampling setup
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // VAO/VBO for quads
    glGenVertexArrays(1, &font.VAO);
    glGenBuffers(1, &font.VBO);
    glBindVertexArray(font.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, font.VBO);
    // 6 vertices × (2 pos + 2 tex) = 24 floats
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

    // position (x, y)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // texcoord (s, t)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Cleanup binds
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return font;
}

#endif //PAWN_FONTLOADER_H