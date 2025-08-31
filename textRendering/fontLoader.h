#ifndef PAWN_FONTLOADER_H
#define PAWN_FONTLOADER_H
#define STB_TRUETYPE_IMPLEMENTATION
#include <iostream>
#include <GL/glew.h>
#include "externalHeaders/stb_truetype.h"
#include "textRendering/Roboto-Regular-ttf.h"  // embedded font

struct Font {
    GLuint textureID;
    stbtt_bakedchar cdata[96]; // ASCII 32..126
    GLuint VAO, VBO;
    int texW = 512, texH = 512;
};

inline Font loadFont(float pixelHeight) {
    Font font{};

    // Bake font into a bitmap directly from embedded-array
    std::vector<unsigned char> bitmap(font.texW * font.texH);
    int result = stbtt_BakeFontBitmap(
        Roboto_Regular_ttf, // embedded font data
        0,                  // font index in TTF (usually 0)
        pixelHeight,        // pixel height
        bitmap.data(),      // output bitmap
        font.texW, font.texH,
        32, 96,     // first char and number of chars
        font.cdata          // baked char data
    );

    if (result <= 0) {
        std::cerr << "❌ Failed to bake embedded font!\n";
        exit(1);
    }

    // Ensure correct alignment for single-channel (8-bit) bitmap
    GLint prevUnpack = 0;
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &prevUnpack);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Upload texture to GPU
    glGenTextures(1, &font.textureID);
    glBindTexture(GL_TEXTURE_2D, font.textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, font.texW, font.texH, 0,
                 GL_RED, GL_UNSIGNED_BYTE, bitmap.data());

    // Restore previous unpack alignment
    glPixelStorei(GL_UNPACK_ALIGNMENT, prevUnpack);

    // Texture sampling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // VAO/VBO for text quads
    glGenVertexArrays(1, &font.VAO);
    glGenBuffers(1, &font.VBO);
    glBindVertexArray(font.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, font.VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW); // dynamic quad buffer

    // position (x, y)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // texcoord (s, t)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Cleanup
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return font;
}

#endif //PAWN_FONTLOADER_H