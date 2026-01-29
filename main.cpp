#define STB_IMAGE_IMPLEMENTATION
#include "externalHeaders/stb_image.h"
#include "bezierPawn/bezierCurvesPawn.h"
#include "textures/generateTexture.h"
#include "textures/loadTextures.h"
#include "textures/marble.h"
#include "textRendering/fontLoader.h"

#include "shaders/shaders.h"
#include "glfw/setupGLFW.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <vector>
#include <chrono>
#include <cassert>
#include <sstream>

namespace {
    GLuint compileRawShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char info[1024];
            glGetShaderInfoLog(shader, 1024, nullptr, info);
            std::cerr << "❌ Cloud shader compile failed:\n" << info << "\n";
        }
        return shader;
    }

    GLuint linkRawProgram(const char* vsSource, const char* fsSource) {
        const GLuint vs = compileRawShader(GL_VERTEX_SHADER, vsSource);
        const GLuint fs = compileRawShader(GL_FRAGMENT_SHADER, fsSource);
        const GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);
        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            char info[1024];
            glGetProgramInfoLog(program, 1024, nullptr, info);
            std::cerr << "❌ Cloud shader link failed:\n" << info << "\n";
        }
        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    struct CloudForeground {
        GLuint program = 0;
        GLuint vao = 0;
        GLuint vbo = 0;
        GLuint depthTex = 0;
        int depthW = 0;
        int depthH = 0;
        bool initialized = false;

        void init() {
            if (initialized) return;

            const char* vs = R"(
                #version 330 core
                layout(location = 0) in vec2 aPos;
                out vec2 vUV;
                void main() {
                    vUV = aPos * 0.5 + 0.5;
                    gl_Position = vec4(aPos, 0.0, 1.0);
                }
            )";

            const char* fs = R"(
                #version 330 core
                in vec2 vUV;
                out vec4 FragColor;

                uniform sampler2D uDepth;
                uniform float uTime;
                uniform vec2 uResolution;
                uniform float uNear;
                uniform float uFar;
                uniform vec2 uPawnScreenPos;
                uniform vec2 uPawnVelocity;
                uniform float uPawnRadius;

                float hash12(vec2 p) {
                    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
                    p3 += dot(p3, p3.yzx + 33.33);
                    return fract((p3.x + p3.y) * p3.z);
                }

                float noise(vec2 p) {
                    vec2 i = floor(p);
                    vec2 f = fract(p);
                    f = f * f * (3.0 - 2.0 * f);
                    float a = hash12(i + vec2(0.0, 0.0));
                    float b = hash12(i + vec2(1.0, 0.0));
                    float c = hash12(i + vec2(0.0, 1.0));
                    float d = hash12(i + vec2(1.0, 1.0));
                    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
                }

                float fbm(vec2 p) {
                    float v = 0.0;
                    float a = 0.55;
                    for (int i = 0; i < 5; ++i) {
                        v += a * noise(p);
                        p *= 2.02;
                        a *= 0.5;
                    }
                    return v;
                }

                float linearizeDepth(float d) {
                    float z = d * 2.0 - 1.0;
                    return (2.0 * uNear * uFar) / (uFar + uNear - z * (uFar - uNear));
                }

                void main() {
                    vec2 uv = vUV;
                    vec2 p = (uv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

                    float sceneDepth = texture(uDepth, uv).r;
                    float linDepth = linearizeDepth(sceneDepth);

                    float depthMask = smoothstep(0.5, 15.0, linDepth);
                    depthMask = mix(0.85, 1.0, depthMask);

                    vec2 toPawn = uv - uPawnScreenPos;
                    toPawn.x *= uResolution.x / uResolution.y;
                    float distToPawn = length(toPawn);

                    float pawnSpeed = length(uPawnVelocity);
                    vec2 velDir = pawnSpeed > 0.001 ? normalize(uPawnVelocity) : vec2(0.0);

                    float dynamicRadius = uPawnRadius + pawnSpeed * 0.8;
                    float pushAway = smoothstep(0.0, dynamicRadius, distToPawn);

                    vec2 pushDir = distToPawn > 0.001 ? normalize(toPawn) : vec2(0.0);
                    float velAlignment = dot(pushDir, velDir);
                    float momentumBoost = max(0.0, velAlignment) * pawnSpeed * 2.5;

                    vec2 displacement = pushDir * (1.0 - pushAway) * (0.15 + momentumBoost);
                    displacement += velDir * (1.0 - pushAway) * pawnSpeed * 1.2;

                    vec2 perpVel = vec2(-velDir.y, velDir.x);
                    float sideSign = dot(toPawn, perpVel);
                    float curlStrength = sideSign * pawnSpeed * (1.0 - pushAway) * 1.8;
                    vec2 curlDir = vec2(-pushDir.y, pushDir.x);
                    displacement += curlDir * curlStrength;

                    float t = uTime * 2.5;
                    vec2 wind1 = vec2(-0.035, 0.012) * t;
                    vec2 wind2 = vec2(0.02, -0.025) * t;
                    vec2 swirl = vec2(sin(t * 0.3 + p.y * 2.0), cos(t * 0.25 + p.x * 2.0)) * 0.02;
                    vec2 turbulence = vec2(
                        sin(t * 0.5 + p.x * 3.0) * cos(t * 0.4 + p.y * 2.5),
                        cos(t * 0.45 + p.y * 3.0) * sin(t * 0.35 + p.x * 2.0)
                    ) * 0.015;

                    vec2 pDisplaced = p + displacement + swirl + turbulence;
                    float n = fbm(pDisplaced * 1.8 + wind1);
                    float m = fbm(pDisplaced * 3.2 + wind2);
                    float detail = fbm(pDisplaced * 5.0 - wind1 * 0.5 + vec2(t * 0.01, 0.0));
                    float c = n * 0.55 + m * 0.3 + detail * 0.15;

                    float wakeBoost = (1.0 - pushAway) * pawnSpeed * 0.6;
                    float puff = smoothstep(0.45 - wakeBoost, 0.78, c);
                    float alpha = puff * depthMask * pushAway * 0.72;

                    vec3 baseCol = vec3(0.58);
                    vec3 greenTint = vec3(0.55, 0.62, 0.54);
                    vec3 purpleTint = vec3(0.60, 0.55, 0.62);
                    float tintMix = noise(p * 3.0 + wind1 * 0.5) * 2.56;
                    vec3 col = mix(baseCol, mix(greenTint, purpleTint, noise(p * 2.0 - wind2)), tintMix);
                    FragColor = vec4(col * alpha, alpha);
                }
            )";

            program = linkRawProgram(vs, fs);

            const float quad[] = {
                -1.0f, -1.0f,
                 1.0f, -1.0f,
                 1.0f,  1.0f,
                -1.0f, -1.0f,
                 1.0f,  1.0f,
                -1.0f,  1.0f,
            };

            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
            glBindVertexArray(0);

            initialized = true;
        }

        void ensureDepthTex(int w, int h) {
            if (w <= 0 || h <= 0) return;
            if (depthTex != 0 && w == depthW && h == depthH) return;

            if (depthTex == 0) glGenTextures(1, &depthTex);
            glBindTexture(GL_TEXTURE_2D, depthTex);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);

            depthW = w;
            depthH = h;
        }

        float lastPawnX = 0.5f;
        float lastPawnY = 0.5f;
        float smoothVelX = 0.0f;
        float smoothVelY = 0.0f;

        void draw(int w, int h, float t, float pawnScreenX, float pawnScreenY, float dt) {
            if (!initialized || program == 0) return;

            float velX = (pawnScreenX - lastPawnX) / std::max(dt, 0.001f);
            float velY = (pawnScreenY - lastPawnY) / std::max(dt, 0.001f);
            smoothVelX = smoothVelX * 0.85f + velX * 0.15f;
            smoothVelY = smoothVelY * 0.85f + velY * 0.15f;
            lastPawnX = pawnScreenX;
            lastPawnY = pawnScreenY;

            ensureDepthTex(w, h);
            glBindTexture(GL_TEXTURE_2D, depthTex);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, w, h);
            glBindTexture(GL_TEXTURE_2D, 0);

            glUseProgram(program);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, depthTex);
            glUniform1i(glGetUniformLocation(program, "uDepth"), 0);

            glUniform1f(glGetUniformLocation(program, "uTime"), t);
            glUniform2f(glGetUniformLocation(program, "uResolution"), float(w), float(h));
            glUniform1f(glGetUniformLocation(program, "uNear"), 0.1f);
            glUniform1f(glGetUniformLocation(program, "uFar"), 100.0f);
            glUniform2f(glGetUniformLocation(program, "uPawnScreenPos"), pawnScreenX, pawnScreenY);
            glUniform2f(glGetUniformLocation(program, "uPawnVelocity"), smoothVelX, smoothVelY);
            glUniform1f(glGetUniformLocation(program, "uPawnRadius"), 0.18f);

            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);

            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
        }
    };

    struct CloudBackground {
        GLuint program = 0;
        GLuint vao = 0;
        GLuint vbo = 0;
        bool initialized = false;

        void init() {
            if (initialized) return;

            const char* vs = R"(
                #version 330 core
                layout(location = 0) in vec2 aPos;
                out vec2 vUV;
                void main() {
                    vUV = aPos * 0.5 + 0.5;
                    gl_Position = vec4(aPos, 0.0, 1.0);
                }
            )";

            const char* fs = R"(
                #version 330 core
                in vec2 vUV;
                out vec4 FragColor;

                uniform float uTime;
                uniform vec2 uResolution;

                float hash12(vec2 p) {
                    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
                    p3 += dot(p3, p3.yzx + 33.33);
                    return fract((p3.x + p3.y) * p3.z);
                }

                float noise(vec2 p) {
                    vec2 i = floor(p);
                    vec2 f = fract(p);
                    f = f * f * (3.0 - 2.0 * f);
                    float a = hash12(i + vec2(0.0, 0.0));
                    float b = hash12(i + vec2(1.0, 0.0));
                    float c = hash12(i + vec2(0.0, 1.0));
                    float d = hash12(i + vec2(1.0, 1.0));
                    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
                }

                float fbm(vec2 p) {
                    float v = 0.0;
                    float a = 0.55;
                    for (int i = 0; i < 5; ++i) {
                        v += a * noise(p);
                        p *= 2.02;
                        a *= 0.5;
                    }
                    return v;
                }

                void main() {
                    vec2 uv = vUV;
                    vec2 p = (uv - 0.5) * vec2(uResolution.x / uResolution.y, 1.0);

                    vec2 wind = vec2(0.015, 0.0) * uTime;
                    float n = fbm(p * 1.2 + wind);
                    float m = fbm(p * 2.6 - wind * 1.7);
                    float c = n * 0.75 + m * 0.25;

                    float puff = smoothstep(0.58, 0.86, c);
                    float alpha = puff * 0.92;

                    vec3 col = vec3(0.62);
                    FragColor = vec4(col * alpha, alpha);
                }
            )";

            program = linkRawProgram(vs, fs);

            const float quad[] = {
                -1.0f, -1.0f,
                 1.0f, -1.0f,
                 1.0f,  1.0f,
                -1.0f, -1.0f,
                 1.0f,  1.0f,
                -1.0f,  1.0f,
            };

            glGenVertexArrays(1, &vao);
            glGenBuffers(1, &vbo);
            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
            glBindVertexArray(0);

            initialized = true;
        }

        void draw(int w, int h, float t) {
            if (!initialized || program == 0) return;
            glUseProgram(program);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUniform1f(glGetUniformLocation(program, "uTime"), t);
            glUniform2f(glGetUniformLocation(program, "uResolution"), float(w), float(h));
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
        }
    };
}

std::string CONTROLS = "Controls: [Q]uit | [F]ullscreen | [B]ehind icons | [R]otation | [<,>] Radial divisions | [I]nfo";

class DrawScene {
    public:
        glfwObject pawn{};
        GLuint gTextShader;

        CloudBackground clouds{};
        CloudForeground cloudsFront{};

        GLuint textureMarble{}, textureBase{}, textureFloor{};
        Font font{};
        float aspectRatio = 16.0f / 9.0f;
        int width = 800;
        int height = 450;
        bool pawnRotate = true;
        bool abort = false;
        bool shouldRun = true;
        bool showInfo = true;
        float currentTime = 0.0f, deltaTime = 0.0f, lastFrameTime = 0.0f;
        int radialDivisions = 40;
        int fps = 0;

        explicit DrawScene() {
            // Load font
            font = loadFont(24.0f);

            // Load textures
            loadTextureFromMemory(marble_jpg, marble_jpg_len, textureMarble, "marble.h");
            generateTexture(textureBase, true);
            generateTexture(textureFloor, false);

            // Create the Pawn and send vertices and indices to GPU
            setupPawn();
            pawn.textures.push_back(textureMarble);
            pawn.textures.push_back(textureBase);

            // Setup two spots on the scene
            pawn.shaderProgram = createShaderProgram(false);
            assert(pawn.shaderProgram != 0 && "Pawn shader creation failed");
            setLighting(pawn.shaderProgram);
            glEnable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Setup gTextShader
            gTextShader = createShaderProgram(true);

            clouds.init();
            cloudsFront.init();
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        }

        void setupPawn() {
            pawn.vertices = {}; pawn.indices = {};
            generatePawnMesh(pawn.vertices, pawn.indices, 100, radialDivisions);
            pawn.uploadToGPU();
        }

        void draw() {
            // Do this only once in the program!!!
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear buffers
            clouds.draw(width, height, static_cast<float>(glfwGetTime()));

            glUseProgram(pawn.shaderProgram);

            // set current-time and compute deltaTime
            currentTime = static_cast<float>(glfwGetTime());
            deltaTime = currentTime - lastFrameTime;
            lastFrameTime = currentTime;

            if (abort) {
                // disable fps counter
                showInfo = false;

                // Fade the scene to black and exit the program by setting shouldRun to false.
                float targetFade = 0.0f;  // Target for fade-out
                float lerpSpeed = 1.0f;   // Controls how fast it fades (higher = faster)

                pawn.uFadeFactor = glm::mix(pawn.uFadeFactor, targetFade, lerpSpeed * deltaTime);
                if (pawn.uFadeFactor < 0.1f) shouldRun = false;  // Lerp never reaches 0 (Zeno, loves this!)
            }

            glUniform1f(glGetUniformLocation(pawn.shaderProgram, "uFadeFactor"), pawn.uFadeFactor);
            updatePawnMovement(pawn, aspectRatio, pawnRotate);

            // Assign texture names in shader texture(x), activate and bind...
            for (int i = 0; i < pawn.textures.size(); ++i) {
                std::string textureName = "texture" + std::to_string(i);
                glUniform1i(glGetUniformLocation(pawn.shaderProgram, textureName.c_str()), i);
                glActiveTexture(GL_TEXTURE0 + i);
                glBindTexture(GL_TEXTURE_2D, pawn.textures[i]);
            }

            glBindVertexArray(pawn.VAO);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(pawn.indices.size()), GL_UNSIGNED_INT, nullptr);

            float pawnScreenX = 0.5f + (pawn.positionX / (aspectRatio * 5.0f));
            float pawnScreenY = 0.5f;
            cloudsFront.draw(width, height, static_cast<float>(glfwGetTime()), pawnScreenX, pawnScreenY, deltaTime);
        }

        void info() {
            if (!showInfo) return;

            // Show fps
            std::string sfps = "FPS: " + std::to_string(fps);
            renderText(font, sfps, 15.0f, 30.0f, 1.0f, glm::vec3(1.0f, 1.0f, 0.0f), width, height);

            // Show runtime
            std::string sradialDivisions = "Radial Divisions: " + std::to_string(radialDivisions);
            renderText(font, sradialDivisions, 15.0f, 50.0f, 1.0f, glm::vec3(1.0f, 1.0f, 0.0f), width, height);

            // Show Controls
            std::string srunTime = CONTROLS;
            renderText(font, srunTime, 15.0f, 70.0f, 1.0f, glm::vec3(1.0f, 1.0f, 0.0f), width, height);
        }

        void keyboard (const keyboardResult &keyboard) {
            if (keyboard.key == GLFW_KEY_Q) {
                abort = true;
            }
            if (keyboard.key == GLFW_KEY_R) {
                pawnRotate = !pawnRotate;
                std::cout << "\nℹ️ " << (pawnRotate ? " Starting pawn rotation." : " Stopping pawn rotation.") << std::flush;
            }
            if (keyboard.key == GLFW_KEY_I) {
                showInfo = !showInfo;
                std::cout << "\nℹ️ " << (showInfo ? " Info pane enabled" : " Info pane disabled.") << std::flush;
            }
            if (keyboard.key == GLFW_KEY_COMMA) {
                if (radialDivisions > 3) radialDivisions--;
                setupPawn();
                std::cout << "\nℹ️ Radial divisions of pawn: " << radialDivisions << std::flush;
            }
            if (keyboard.key == GLFW_KEY_PERIOD) {
                if (radialDivisions < 40) radialDivisions++;
                setupPawn();
                std::cout << "\nℹ️ Radial divisions of pawn: " << radialDivisions << std::flush;
            }
        }

        void renderText(Font& font, const std::string& text,
                float x, float y, float scale, glm::vec3 color,
                int winW, int winH)
        {
            glUseProgram(gTextShader);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            // Set text color uniform
            glUniform3f(glGetUniformLocation(gTextShader, "textColor"),
                        color.x, color.y, color.z);

            // Setup orthographic projection (0,0) top-left
            glm::mat4 projection = glm::ortho(0.0f, float(winW),
                                              float(winH), 0.0f);
            glUniformMatrix4fv(glGetUniformLocation(gTextShader, "projection"),
                               1, GL_FALSE, &projection[0][0]);


            // Bind font texture and VAO
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, font.textureID);
            glUniform1i(glGetUniformLocation(gTextShader, "text"), 0);
            glBindVertexArray(font.VAO);

            float origX = x;

            for (unsigned char c : text) {
                if (c < 32 || c >= 128) continue;

                stbtt_aligned_quad q;
                stbtt_GetBakedQuad(font.cdata, font.texW, font.texH, c - 32, &x, &y, &q, 1);

                float xpos = q.x0 * scale;
                float ypos = q.y0 * scale;
                float w    = (q.x1 - q.x0) * scale;
                float h    = (q.y1 - q.y0) * scale;

                float vertices[6][4] = {
                    { xpos,     ypos + h,   q.s0, q.t1 },
                    { xpos,     ypos,       q.s0, q.t0 },
                    { xpos + w, ypos,       q.s1, q.t0 },

                    { xpos,     ypos + h,   q.s0, q.t1 },
                    { xpos + w, ypos,       q.s1, q.t0 },
                    { xpos + w, ypos + h,   q.s1, q.t1 }
                };

                glBindBuffer(GL_ARRAY_BUFFER, font.VBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }

            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glEnable(GL_DEPTH_TEST);
        }
};


int main() {
    GLFWmonitor* monitor = nullptr;
    const GLFWvidmode* mode = nullptr;
    GLFWwindow* window = initWindow(&monitor, &mode);

    DrawScene scene{};

    // Attach resize handler to recompute the aspect ratio only after resizing stops
    ResizeHandler resizeHandler(window, [&](int width, int height) {
        scene.aspectRatio = static_cast<float>(width) / static_cast<float>(height);
        scene.width = width;
        scene.height = height;
        std::cout << "\n✅ Window Resized:\n";
        std::cout << "   → Dimensions: " << width << "x" << height << "\n";
        std::cout << "   → Aspect ratio: " << scene.aspectRatio << "\n";
    });

    std::stringstream fps;
    bool frameRateRestored = true;
    std::chrono::high_resolution_clock::time_point lastFrame;
    std::chrono::high_resolution_clock::time_point lastFrameDrop;

    double keyPressedTime = glfwGetTime();
    keyboardResult keyboard{};
    std::cout << CONTROLS << std::flush;

    while (scene.shouldRun) {
        // Query current framebuffer size and set viewport

        int fbW = 0;
        int fbH = 0;
        glfwGetFramebufferSize(window, &fbW, &fbH);
        if (fbW > 0 && fbH > 0) {
            glViewport(0, 0, fbW, fbH);
            scene.width = fbW;
            scene.height = fbH;
            scene.aspectRatio = static_cast<float>(fbW) / static_cast<float>(fbH);
        }


        // Check for fullscreen or exit program keys
        keyboard = checkKeyBoard(window, keyboard);
        if (keyboard.key == GLFW_KEY_F) {
            toggleFullscreen(window, monitor, mode, isFullscreen);
            std::cout << "\nℹ️ " << (isFullscreen ? " Switching to full screen mode.\n" : " Exiting full screen mode.\n") << std::flush;
            keyPressedTime = glfwGetTime();
        }

        if (keyboard.key == GLFW_KEY_B) {
            if (isFullscreen) {
                toggleFullscreen(window, monitor, mode, isFullscreen);
            }
            toggleDesktopPinned(window, isDesktopPinned);
            std::cout << "\nℹ️ " << (isDesktopPinned ? " Desktop pinned mode enabled.\n" : " Desktop pinned mode disabled.\n") << std::flush;
            keyPressedTime = glfwGetTime();
        }

        // Draw the scene
        scene.keyboard(keyboard);
        scene.draw();
        scene.info();

        // Throttle the framerate
        scene.fps = limitFrameRate(lastFrame, lastFrameDrop, frameRateRestored, keyPressedTime);

        // Swap buffers and poll for events
        glfwSwapBuffers(window);
        glfwPollEvents();

        // resizeHandler with debounce
        resizeHandler.update();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

