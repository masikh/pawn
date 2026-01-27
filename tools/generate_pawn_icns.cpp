#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <zlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "externalHeaders/stb_image.h"

#include "bezierPawn/bezierCurvesPawn.h"
#include "textures/marble.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace {

struct Rgba {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct Image {
    int w = 0;
    int h = 0;
    std::vector<Rgba> px;

    Image() = default;
    Image(int width, int height) : w(width), h(height), px(static_cast<size_t>(w) * static_cast<size_t>(h)) {}

    Rgba& at(int x, int y) { return px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]; }
    const Rgba& at(int x, int y) const { return px[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]; }
};

static inline float clamp01(float x) { return std::min(1.0f, std::max(0.0f, x)); }

static inline float smoothstep(float e0, float e1, float x) {
    if (e0 == e1) return 0.0f;
    float t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0f - 2.0f * t);
}

static inline uint8_t to_u8(float x) { return static_cast<uint8_t>(std::lround(clamp01(x) * 255.0f)); }

static inline Rgba lerp(const Rgba& a, const Rgba& b, float t) {
    float ft = clamp01(t);
    return {
        static_cast<uint8_t>(std::lround(a.r + (b.r - a.r) * ft)),
        static_cast<uint8_t>(std::lround(a.g + (b.g - a.g) * ft)),
        static_cast<uint8_t>(std::lround(a.b + (b.b - a.b) * ft)),
        static_cast<uint8_t>(std::lround(a.a + (b.a - a.a) * ft)),
    };
}

static inline uint32_t pack_rgba(const Rgba& c) {
    return (static_cast<uint32_t>(c.r) << 24) | (static_cast<uint32_t>(c.g) << 16) | (static_cast<uint32_t>(c.b) << 8) |
           static_cast<uint32_t>(c.a);
}

static inline glm::vec3 unpack_rgb(const Rgba& c) {
    return glm::vec3(float(c.r), float(c.g), float(c.b)) / 255.0f;
}

struct MarbleTex {
    int w = 0;
    int h = 0;
    int comp = 0;
    std::vector<uint8_t> data;

    glm::vec3 sample(float u, float v) const {
        if (w <= 0 || h <= 0 || data.empty()) return glm::vec3(0.05f);

        u = u - std::floor(u);
        v = v - std::floor(v);

        float x = u * float(w - 1);
        float y = (1.0f - v) * float(h - 1);
        int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, w - 1);
        int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, h - 1);
        int x1 = std::min(x0 + 1, w - 1);
        int y1 = std::min(y0 + 1, h - 1);
        float tx = x - float(x0);
        float ty = y - float(y0);

        auto px_at = [&](int xi, int yi) -> glm::vec3 {
            const size_t idx = (static_cast<size_t>(yi) * static_cast<size_t>(w) + static_cast<size_t>(xi)) * static_cast<size_t>(comp);
            float r = float(data[idx + 0]) / 255.0f;
            float g = float(data[idx + 1]) / 255.0f;
            float b = float(data[idx + 2]) / 255.0f;
            return glm::vec3(r, g, b);
        };

        glm::vec3 c00 = px_at(x0, y0);
        glm::vec3 c10 = px_at(x1, y0);
        glm::vec3 c01 = px_at(x0, y1);
        glm::vec3 c11 = px_at(x1, y1);

        glm::vec3 cx0 = glm::mix(c00, c10, tx);
        glm::vec3 cx1 = glm::mix(c01, c11, tx);
        return glm::mix(cx0, cx1, ty);
    }
};

struct VertexOut {
    glm::vec4 clip;
    glm::vec3 worldN;
    glm::vec2 uv;
};

static void draw_background(Image& img) {
    const Rgba top{245, 245, 245, 255};
    const Rgba bottom{220, 220, 220, 255};

    for (int y = 0; y < img.h; ++y) {
        float fy = float(y) / float(std::max(1, img.h - 1));
        Rgba row = lerp(top, bottom, fy);
        for (int x = 0; x < img.w; ++x) {
            img.at(x, y) = row;
        }
    }
}

static inline float edge_function(const glm::vec2& a, const glm::vec2& b, const glm::vec2& c) {
    return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
}

static Image render_pawn(int size, const MarbleTex& marble) {
    Image img(size, size);
    draw_background(img);

    std::vector<glfwObject::Vertex> vertices;
    std::vector<unsigned int> indices;

    generatePawnMesh(vertices, indices, 140, 96);

    glm::vec3 minV(1e9f), maxV(-1e9f);
    for (const auto& v : vertices) {
        minV.x = std::min(minV.x, v.x);
        minV.y = std::min(minV.y, v.y);
        minV.z = std::min(minV.z, v.z);
        maxV.x = std::max(maxV.x, v.x);
        maxV.y = std::max(maxV.y, v.y);
        maxV.z = std::max(maxV.z, v.z);
    }

    glm::vec3 center = (minV + maxV) * 0.5f;
    float extent = std::max({maxV.x - minV.x, maxV.y - minV.y, maxV.z - minV.z});

    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3(0.0f, -center.y, 0.0f));
    model = glm::scale(model, glm::vec3((1.0f / extent) * (0.82f / 1.56f)));
    model = glm::translate(model, glm::vec3(0.0f, 0.62f, 0.0f));

    glm::mat4 view(1.0f);
    view = glm::lookAt(glm::vec3(0.0f, 0.10f, 1.55f), glm::vec3(0.0f, 0.10f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 proj = glm::perspective(glm::radians(22.0f), 1.0f, 0.1f, 10.0f);

    glm::mat4 mvp = proj * view * model;

    std::vector<VertexOut> vout(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const auto& v = vertices[i];
        glm::vec4 wp = model * glm::vec4(v.x, v.y, v.z, 1.0f);
        glm::vec3 wn = glm::normalize(glm::mat3(model) * glm::vec3(v.nx, v.ny, v.nz));
        vout[i].clip = mvp * glm::vec4(v.x, v.y, v.z, 1.0f);
        vout[i].worldN = wn;
        vout[i].uv = glm::vec2(v.u, v.v);
    }

    std::vector<float> zbuf(static_cast<size_t>(size) * static_cast<size_t>(size), 1e9f);

    glm::vec3 lightDir = glm::normalize(glm::vec3(-0.45f, 0.85f, 0.35f));

    auto to_screen = [&](const glm::vec4& clip) -> glm::vec3 {
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        float sx = ((-ndc.x) * 0.5f + 0.5f) * float(size - 1);
        float sy = (1.0f - ((-ndc.y) * 0.5f + 0.5f)) * float(size - 1);
        float sz = ndc.z;
        return glm::vec3(sx, sy, sz);
    };

    for (size_t t = 0; t + 2 < indices.size(); t += 3) {
        uint32_t i0 = indices[t + 0];
        uint32_t i1 = indices[t + 1];
        uint32_t i2 = indices[t + 2];

        if (vertices[i0].texID > 0.5f || vertices[i1].texID > 0.5f || vertices[i2].texID > 0.5f) {
            continue;
        }

        const VertexOut& a = vout[i0];
        const VertexOut& b = vout[i1];
        const VertexOut& c = vout[i2];

        if (a.clip.w <= 0.0f || b.clip.w <= 0.0f || c.clip.w <= 0.0f) continue;

        glm::vec3 sa = to_screen(a.clip);
        glm::vec3 sb = to_screen(b.clip);
        glm::vec3 sc = to_screen(c.clip);

        glm::vec2 p0(sa.x, sa.y);
        glm::vec2 p1(sb.x, sb.y);
        glm::vec2 p2(sc.x, sc.y);

        float area = edge_function(p0, p1, p2);
        if (area == 0.0f) continue;

        int minx = std::max(0, static_cast<int>(std::floor(std::min({p0.x, p1.x, p2.x}))));
        int maxx = std::min(size - 1, static_cast<int>(std::ceil(std::max({p0.x, p1.x, p2.x}))));
        int miny = std::max(0, static_cast<int>(std::floor(std::min({p0.y, p1.y, p2.y}))));
        int maxy = std::min(size - 1, static_cast<int>(std::ceil(std::max({p0.y, p1.y, p2.y}))));

        float invW0 = 1.0f / a.clip.w;
        float invW1 = 1.0f / b.clip.w;
        float invW2 = 1.0f / c.clip.w;

        for (int y = miny; y <= maxy; ++y) {
            for (int x = minx; x <= maxx; ++x) {
                glm::vec2 p(float(x) + 0.5f, float(y) + 0.5f);

                float w0 = edge_function(p1, p2, p);
                float w1 = edge_function(p2, p0, p);
                float w2 = edge_function(p0, p1, p);

                if ((w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) && (w0 > 0.0f || w1 > 0.0f || w2 > 0.0f)) {
                    continue;
                }

                w0 /= area;
                w1 /= area;
                w2 /= area;

                float invW = w0 * invW0 + w1 * invW1 + w2 * invW2;
                if (invW <= 0.0f) continue;

                float z = (w0 * sa.z * invW0 + w1 * sb.z * invW1 + w2 * sc.z * invW2) / invW;

                const size_t idx = static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x);
                if (z >= zbuf[idx]) continue;
                zbuf[idx] = z;

                glm::vec2 uv = (w0 * a.uv * invW0 + w1 * b.uv * invW1 + w2 * c.uv * invW2) / invW;
                glm::vec3 n = glm::normalize((w0 * a.worldN * invW0 + w1 * b.worldN * invW1 + w2 * c.worldN * invW2) / invW);

                glm::vec3 tex = marble.sample(uv.x * 1.2f, uv.y * 1.1f);

                float diff = std::max(0.0f, glm::dot(n, lightDir));
                float ambient = 0.18f;
                float shade = ambient + diff * 0.92f;

                glm::vec3 col = tex * shade;

                glm::vec3 viewDir = glm::normalize(glm::vec3(0.0f, 0.15f, 1.0f));
                glm::vec3 h = glm::normalize(lightDir + viewDir);
                float spec = std::pow(std::max(0.0f, glm::dot(n, h)), 52.0f) * 0.25f;
                col += glm::vec3(spec);

                float rim = std::pow(1.0f - std::max(0.0f, glm::dot(n, viewDir)), 2.2f) * 0.14f;
                col += glm::vec3(rim) * glm::vec3(0.45f, 0.25f, 0.65f);

                Rgba out{to_u8(col.r), to_u8(col.g), to_u8(col.b), 255};
                img.at(x, y) = out;
            }
        }
    }

    return img;
}

static Image downscale_box(const Image& src, int newW, int newH) {
    Image dst(newW, newH);

    float sx = float(src.w) / float(newW);
    float sy = float(src.h) / float(newH);

    for (int y = 0; y < newH; ++y) {
        for (int x = 0; x < newW; ++x) {
            int x0 = int(std::floor(x * sx));
            int y0 = int(std::floor(y * sy));
            int x1 = int(std::floor((x + 1) * sx));
            int y1 = int(std::floor((y + 1) * sy));
            x1 = std::max(x1, x0 + 1);
            y1 = std::max(y1, y0 + 1);
            x0 = std::clamp(x0, 0, src.w - 1);
            y0 = std::clamp(y0, 0, src.h - 1);
            x1 = std::clamp(x1, 1, src.w);
            y1 = std::clamp(y1, 1, src.h);

            uint32_t accR = 0, accG = 0, accB = 0, accA = 0;
            uint32_t count = 0;
            for (int yy = y0; yy < y1; ++yy) {
                for (int xx = x0; xx < x1; ++xx) {
                    const auto& p = src.at(xx, yy);
                    accR += p.r;
                    accG += p.g;
                    accB += p.b;
                    accA += p.a;
                    ++count;
                }
            }

            if (count == 0) count = 1;
            dst.at(x, y) = {
                static_cast<uint8_t>(accR / count),
                static_cast<uint8_t>(accG / count),
                static_cast<uint8_t>(accB / count),
                static_cast<uint8_t>(accA / count),
            };
        }
    }

    return dst;
}

static void append_be32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(uint8_t((v >> 24) & 0xFF));
    out.push_back(uint8_t((v >> 16) & 0xFF));
    out.push_back(uint8_t((v >> 8) & 0xFF));
    out.push_back(uint8_t(v & 0xFF));
}

static void append_bytes(std::vector<uint8_t>& out, const void* data, size_t len) {
    const auto* b = static_cast<const uint8_t*>(data);
    out.insert(out.end(), b, b + len);
}

static void png_chunk(std::vector<uint8_t>& out, const char type[4], const std::vector<uint8_t>& data) {
    append_be32(out, static_cast<uint32_t>(data.size()));
    append_bytes(out, type, 4);
    append_bytes(out, data.data(), data.size());

    uint32_t crc = crc32(0, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(type), 4);
    if (!data.empty()) {
        crc = crc32(crc, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size()));
    }
    append_be32(out, crc);
}

static std::vector<uint8_t> png_from_rgba(const Image& img) {
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(img.w) * static_cast<size_t>(img.h) * 4 + static_cast<size_t>(img.h));

    for (int y = 0; y < img.h; ++y) {
        raw.push_back(0);
        for (int x = 0; x < img.w; ++x) {
            const auto& p = img.at(x, y);
            raw.push_back(p.r);
            raw.push_back(p.g);
            raw.push_back(p.b);
            raw.push_back(p.a);
        }
    }

    uLongf compBound = compressBound(static_cast<uLong>(raw.size()));
    std::vector<uint8_t> compressed(compBound);
    uLongf compSize = compBound;

    int zerr = compress2(reinterpret_cast<Bytef*>(compressed.data()), &compSize, reinterpret_cast<const Bytef*>(raw.data()),
                         static_cast<uLong>(raw.size()), 9);
    if (zerr != Z_OK) {
        throw std::runtime_error("zlib compress2 failed");
    }
    compressed.resize(static_cast<size_t>(compSize));

    std::vector<uint8_t> out;
    const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    append_bytes(out, sig, 8);

    std::vector<uint8_t> ihdr;
    ihdr.reserve(13);
    append_be32(ihdr, static_cast<uint32_t>(img.w));
    append_be32(ihdr, static_cast<uint32_t>(img.h));
    ihdr.push_back(8);
    ihdr.push_back(6);
    ihdr.push_back(0);
    ihdr.push_back(0);
    ihdr.push_back(0);

    png_chunk(out, "IHDR", ihdr);
    png_chunk(out, "IDAT", compressed);
    png_chunk(out, "IEND", {});

    return out;
}

static void write_icns(const std::string& path, const std::vector<std::pair<std::array<char, 4>, std::vector<uint8_t>>>& entries) {
    std::vector<uint8_t> body;

    for (const auto& e : entries) {
        const auto& type = e.first;
        const auto& data = e.second;
        uint32_t elemSize = 8 + static_cast<uint32_t>(data.size());

        body.insert(body.end(), type.begin(), type.end());
        append_be32(body, elemSize);
        body.insert(body.end(), data.begin(), data.end());
    }

    uint32_t totalSize = 8 + static_cast<uint32_t>(body.size());

    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("failed to open output icns");

    f.write("icns", 4);
    uint8_t beSize[4] = {uint8_t((totalSize >> 24) & 0xFF), uint8_t((totalSize >> 16) & 0xFF), uint8_t((totalSize >> 8) & 0xFF),
                         uint8_t(totalSize & 0xFF)};
    f.write(reinterpret_cast<const char*>(beSize), 4);
    f.write(reinterpret_cast<const char*>(body.data()), static_cast<std::streamsize>(body.size()));
}

static MarbleTex load_marble() {
    MarbleTex tex;

    int w = 0, h = 0, n = 0;
    unsigned char* img = stbi_load_from_memory(marble_jpg, static_cast<int>(marble_jpg_len), &w, &h, &n, 3);
    if (!img) {
        throw std::runtime_error("stbi_load_from_memory failed for marble_jpg");
    }

    tex.w = w;
    tex.h = h;
    tex.comp = 3;
    tex.data.assign(img, img + static_cast<size_t>(w) * static_cast<size_t>(h) * 3);

    stbi_image_free(img);
    return tex;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;

    try {
        std::string outPath = argv[1];
        MarbleTex marble = load_marble();

        Image base = render_pawn(1024, marble);

        std::vector<std::pair<std::array<char, 4>, std::vector<uint8_t>>> entries;

        const std::vector<std::pair<int, std::array<char, 4>>> sizes = {
            {16, {'i', 'c', 'p', '4'}},
            {32, {'i', 'c', 'p', '5'}},
            {64, {'i', 'c', 'p', '6'}},
            {128, {'i', 'c', '0', '7'}},
            {256, {'i', 'c', '0', '8'}},
            {512, {'i', 'c', '0', '9'}},
            {1024, {'i', 'c', '1', '0'}},
        };

        for (const auto& [sz, type] : sizes) {
            Image img = (sz == 1024) ? base : downscale_box(base, sz, sz);
            entries.emplace_back(type, png_from_rgba(img));
        }

        write_icns(outPath, entries);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Icon generation failed: " << e.what() << "\n";
        return 1;
    }
}
