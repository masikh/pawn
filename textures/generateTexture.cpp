#include "externalHeaders/stb_image.h"
#include <GL/glew.h>
#include <algorithm>
#include <cstring>
#include <ctime>
#include <iostream>

#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#include "../externalHeaders/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "../externalHeaders/nanosvgrast.h"

#include "generateTexture.h"
#include "carpet.h"


unsigned char* rasterizeSVG(const char* logo_svg, int targetWidth, int targetHeight) {
    char* mutableSvg = new char[logo_svg_len + 1];
    memcpy(mutableSvg, logo_svg, logo_svg_len);
    mutableSvg[logo_svg_len] = '\0';  // Ensure null-termination

    NSVGimage* svg = nsvgParse(mutableSvg, "px", 96);

    if (!svg) {
        std::cerr << "❌ Failed to parse SVG\n";
        return nullptr;
    }

    NSVGrasterizer* rast = nsvgCreateRasterizer();
    unsigned char* svgPixels = new unsigned char[targetWidth * targetHeight * 4];  // RGBA
    std::memset(svgPixels, 0, targetWidth * targetHeight * 4);  // Clear buffer

    float scale = std::min(targetWidth / svg->width, targetHeight / svg->height);
    float offsetX = (targetWidth - svg->width * scale) / 2.0f;
    float offsetY = (targetHeight - svg->height * scale) / 2.0f;

    nsvgRasterize(rast, svg, offsetX, offsetY, scale, svgPixels, targetWidth, targetHeight, targetWidth * 4);

    nsvgDelete(svg);
    nsvgDeleteRasterizer(rast);
    return svgPixels;
}

unsigned char* stitchTextures(int& outWidth, int& outHeight, int& channels) {
    srand(static_cast<unsigned>(time(nullptr)));  // Seed RNG

    int width, height;
    unsigned char* smallTex1 = stbi_load_from_memory(carpet_1_png, static_cast<signed>(carpet_1_png_len), &width, &height, &channels, 0);
    unsigned char* smallTex2 = stbi_load_from_memory(carpet_2_png, static_cast<signed>(carpet_2_png_len), &width, &height, &channels, 0);

    if (!smallTex1 || !smallTex2) {
        std::cerr << "❌ Failed to load small textures: " << stbi_failure_reason() << "\n";
        return nullptr;
    }

    outWidth = tileCountX * width;
    outHeight = tileCountY * height;
    size_t imageSize = outWidth * outHeight * channels;
    unsigned char* bigTex = new unsigned char[imageSize];

    for (int ty = 0; ty < tileCountY; ++ty) {
        for (int tx = 0; tx < tileCountX; ++tx) {
            bool useTex2 = (rand() % 10) > 1;  // ~80% chance
            unsigned char* srcTex = useTex2 ? smallTex1 : smallTex2;

            for (int y = 0; y < height; ++y) {
                memcpy(
                    bigTex + ((ty * height + y) * outWidth + tx * width) * channels,
                    srcTex + y * width * channels,
                    width * channels
                );
            }
        }
    }

    stbi_image_free(smallTex1);
    stbi_image_free(smallTex2);
    return bigTex;
}

void blendCenter(unsigned char* dst, int dstW, int dstH, unsigned char* src, int srcW, int srcH) {
    int startX = (dstW - srcW) / 2;
    int startY = (dstH - srcH) / 2;

    for (int y = 0; y < srcH; ++y) {
        for (int x = 0; x < srcW; ++x) {
            int dstIdx = ((startY + y) * dstW + (startX + x)) * 4;
            int srcIdx = (y * srcW + x) * 4;

            float alpha = src[srcIdx + 3] / 255.0f;
            for (int c = 0; c < 3; ++c) {
                dst[dstIdx + c] = static_cast<unsigned char>(
                    src[srcIdx + c] * alpha + dst[dstIdx + c] * (1.0f - alpha)
                );
            }
            dst[dstIdx + 3] = 255; // Full alpha
        }
    }
}

void loadGeneratedTexture(GLuint& textureID, const unsigned char* pixelBuf, int width, int height, bool logo) {
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelBuf);
    glGenerateMipmap(GL_TEXTURE_2D);

    std::cout << "✅ Loaded generated texture:\n";
    std::cout << "   → Logo: " << (logo ? "True" : "False") << "\n";
    std::cout << "   → Dimensions: " << width << "x" << height << "\n";
    std::cout << "   → Channels: 4\n";
    std::cout << "   → Format: RGBA\n";
}

void generateTexture(GLuint &textureId, bool logo) {
    /*
     * Usage:
     *     unsigned char* pixelBuf = nullptr;
     *     int width = 0, height = 0, channels = 0;
     *     createTexture(pixelBuf, width, height, channels);
     *     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelBuf);
     */

    pixelBuf = stitchTextures(generatedTextureWidth, generatedTextureHeight, generatedTextureChannels);

    if (!pixelBuf) {
        return;
    }

    // Rasterize SVG to fit a portion of the stitched texture
    if (logo) {
        int svgWidth = generatedTextureWidth / 2;
        int svgHeight = generatedTextureHeight / 2;

        // Copy logo_svg (assumed const char*) to mutable buffer for NanoSVG
        char* svgCopy = new char[strlen(logo_svg) + 1];
        strcpy(svgCopy, logo_svg);

        unsigned char* svgBuffer = rasterizeSVG(svgCopy, svgWidth, svgHeight);
        delete[] svgCopy;

        if (!svgBuffer) {
            delete[] pixelBuf;
            pixelBuf = nullptr;
            return;
        }

        // Blend the SVG into the center
        blendCenter(pixelBuf, generatedTextureWidth, generatedTextureHeight, svgBuffer, svgWidth, svgHeight);

        delete[] svgBuffer;
    }

    // Load texture to GPU memory
    loadGeneratedTexture(textureId, pixelBuf, generatedTextureWidth, generatedTextureHeight, logo);
}
