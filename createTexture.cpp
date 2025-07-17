#include <iostream>
#include "carpet.h"

#include "stb_image.h"

#define NANOSVG_IMPLEMENTATION
#define NANOSVG_ALL_COLOR_KEYWORDS
#include "nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"

const int tileCountX = 120, tileCountY = 68;

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

void createTexture(unsigned char*& bigTex, int& width, int& height, int& channels) {
    /*
     * Usage:
     *     unsigned char* pixelBuf = nullptr;
     *     int width = 0, height = 0, channels = 0;
     *     createTexture(pixelBuf, width, height, channels);
     *     glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelBuf);
     */

    bigTex = stitchTextures(width, height, channels);

    if (!bigTex) {
        return;
    }

    // Rasterize SVG to fit a portion of the stitched texture
    int svgWidth = width / 2;
    int svgHeight = height / 2;

    // Copy logo_svg (assumed const char*) to mutable buffer for NanoSVG
    char* svgCopy = new char[strlen(logo_svg) + 1];
    strcpy(svgCopy, logo_svg);

    unsigned char* svgBuffer = rasterizeSVG(svgCopy, svgWidth, svgHeight);
    delete[] svgCopy;

    if (!svgBuffer) {
        delete[] bigTex;
        bigTex = nullptr;
        return;
    }

    // Blend the SVG into the center
    blendCenter(bigTex, width, height, svgBuffer, svgWidth, svgHeight);

    delete[] svgBuffer;
}
