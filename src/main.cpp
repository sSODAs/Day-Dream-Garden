#undef GLFW_DLL
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Libs/Anim.h"
#include "Libs/Shader.h"
#include "Libs/Window.h"
#include "Libs/stb_image.h"

static const GLint WIDTH = 1280;
static const GLint HEIGHT = 800;
static const float PI = 3.14159265359f;

static const float YARD_HALF_W = 4.05f;
static const float YARD_HALF_D = 2.55f;
static const float YARD_Z = 1.55f;

static const glm::vec3 DEFAULT_CAMERA_POS(0.0f, 3.51f, 7.74f);
static const glm::vec3 DEFAULT_CAMERA_TARGET(0.0f, 3.24f, 6.78f);
static const float CAMERA_FOV_DEG = 45.0f;

static const glm::vec3 BACKDROP_POS(0.0f, 2.08f, -1.35f);
static const float BACKDROP_HALF_W = 4.05f;
static const float BACKDROP_HALF_H = 3.38f;
static const float FISH_PATH_X_RADIUS = 3.75f;
static const float FISH_PATH_Z_RADIUS = 1.85f;
static const float FISH_PATH_Y = 1.18f;
static const float FISH_PATH_Z_CENTER_OFFSET = -0.72f;

struct MeshGL
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLsizei indexCount = 0;

    void destroy()
    {
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        VAO = VBO = EBO = 0;
        indexCount = 0;
    }
};

struct ColorMeshGL
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLsizei indexCount = 0;

    void destroy()
    {
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        VAO = VBO = EBO = 0;
        indexCount = 0;
    }
};

struct HudTextureGL
{
    GLuint texture = 0;
    int width = 0;
    int height = 0;

    void init(int w, int h)
    {
        width = w;
        height = h;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void upload(const std::vector<unsigned char>& rgba)
    {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void destroy()
    {
        if (texture) glDeleteTextures(1, &texture);
        texture = 0;
        width = height = 0;
    }
};

struct FloatingFishGL
{
    ColorMeshGL body;
    ColorMeshGL tail;
    ColorMeshGL fin;
    ColorMeshGL eye;

    void destroy()
    {
        body.destroy();
        tail.destroy();
        fin.destroy();
        eye.destroy();
    }
};

struct DynamicLineGL
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLsizei vertCount = 0;
    int capacity = 0;

    void init(int maxVerts)
    {
        capacity = maxVerts;
        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, maxVerts * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    void upload(const std::vector<float>& data)
    {
        int count = std::min((int)data.size() / 6, capacity);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, count * 6 * sizeof(float), data.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        vertCount = static_cast<GLsizei>(count);
    }

    void destroy()
    {
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        VAO = VBO = 0;
        vertCount = 0;
        capacity = 0;
    }
};

struct GrassInstance
{
    float x = 0.0f;
    float z = 0.0f;
    float height = 0.20f;
    float width = 0.018f;
    float yaw = 0.0f;
    float phase = 0.0f;
    float stiffness = 1.0f;
    float colorVar = 0.5f;
};

struct FishInstance
{
    glm::vec3 offset = glm::vec3(0.0f);
    float phase = 0.0f;
    float scale = 1.0f;
    float speed = 1.0f;
    float colorVar = 0.5f;
    float lane = 0.0f;
};

struct InstancedGrassGL
{
    GLuint VAO = 0;
    GLuint bladeVBO = 0;
    GLuint instanceVBO = 0;
    GLsizei bladeVertexCount = 0;
    GLsizei instanceCount = 0;

    void init(const std::vector<float>& bladeVertices8, const std::vector<float>& instanceFloats8)
    {
        bladeVertexCount = static_cast<GLsizei>(bladeVertices8.size() / 8);
        instanceCount = static_cast<GLsizei>(instanceFloats8.size() / 8);

        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glGenBuffers(1, &bladeVBO);
        glBindBuffer(GL_ARRAY_BUFFER, bladeVBO);
        glBufferData(GL_ARRAY_BUFFER, bladeVertices8.size() * sizeof(float), bladeVertices8.data(), GL_STATIC_DRAW);

        const GLsizei bladeStride = 8 * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, bladeStride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, bladeStride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, bladeStride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(2);

        glGenBuffers(1, &instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, instanceFloats8.size() * sizeof(float), instanceFloats8.data(), GL_STATIC_DRAW);

        const GLsizei instanceStride = 8 * sizeof(float);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, instanceStride, (void*)0);
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(3, 1);

        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, instanceStride, (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(4);
        glVertexAttribDivisor(4, 1);

        glBindVertexArray(0);
    }

    void destroy()
    {
        if (instanceVBO) glDeleteBuffers(1, &instanceVBO);
        if (bladeVBO) glDeleteBuffers(1, &bladeVBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        VAO = bladeVBO = instanceVBO = 0;
        bladeVertexCount = instanceCount = 0;
    }
};

struct InstancedFishGL
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;
    GLuint instanceVBO = 0;
    GLsizei indexCount = 0;
    GLsizei instanceCount = 0;

    void init(const std::vector<float>& vertices8,
              const std::vector<unsigned int>& indices,
              const std::vector<float>& instanceFloats8)
    {
        indexCount = static_cast<GLsizei>(indices.size());
        instanceCount = static_cast<GLsizei>(instanceFloats8.size() / 8);

        glGenVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glGenBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices8.size() * sizeof(float), vertices8.data(), GL_STATIC_DRAW);

        glGenBuffers(1, &EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        const GLsizei vertexStride = 8 * sizeof(float);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexStride, (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, vertexStride, (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, vertexStride, (void*)(6 * sizeof(float)));
        glEnableVertexAttribArray(4);

        glGenBuffers(1, &instanceVBO);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
        glBufferData(GL_ARRAY_BUFFER, instanceFloats8.size() * sizeof(float), instanceFloats8.data(), GL_STATIC_DRAW);

        const GLsizei instanceStride = 8 * sizeof(float);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, instanceStride, (void*)0);
        glEnableVertexAttribArray(2);
        glVertexAttribDivisor(2, 1);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, instanceStride, (void*)(4 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribDivisor(3, 1);

        glBindVertexArray(0);
    }

    void destroy()
    {
        if (instanceVBO) glDeleteBuffers(1, &instanceVBO);
        if (EBO) glDeleteBuffers(1, &EBO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (VAO) glDeleteVertexArrays(1, &VAO);
        VAO = VBO = EBO = instanceVBO = 0;
        indexCount = instanceCount = 0;
    }
};

static float hash01(int n)
{
    float s = std::sin(float(n) * 12.9898f + 78.233f) * 43758.5453f;
    return s - std::floor(s);
}

static glm::vec3 safeNormal(glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glm::vec3 n = glm::cross(b - a, c - a);
    float len = glm::length(n);
    if (len < 1e-6f) return glm::vec3(0, 1, 0);
    return n / len;
}

static glm::vec2 safeNormalize2(glm::vec2 v, glm::vec2 fallback)
{
    float len = glm::length(v);
    if (len < 1e-5f) return fallback;
    return v / len;
}

static glm::vec3 safeNormalize3(glm::vec3 v, glm::vec3 fallback)
{
    float len = glm::length(v);
    if (len < 1e-5f) return fallback;
    return v / len;
}

static glm::vec3 cameraForward(float yawDeg, float pitchDeg)
{
    float yaw = glm::radians(yawDeg);
    float pitch = glm::radians(pitchDeg);
    glm::vec3 f;
    f.x = std::cos(yaw) * std::cos(pitch);
    f.y = std::sin(pitch);
    f.z = std::sin(yaw) * std::cos(pitch);
    return safeNormalize3(f, glm::vec3(0.0f, 0.0f, -1.0f));
}

static void pushVertex(std::vector<float>& v, glm::vec3 p, glm::vec3 n, glm::vec2 uv)
{
    v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
    v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
    v.push_back(uv.x); v.push_back(uv.y);
}

static void pushLine(std::vector<float>& data, glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    data.push_back(a.x); data.push_back(a.y); data.push_back(a.z);
    data.push_back(c.r); data.push_back(c.g); data.push_back(c.b);
    data.push_back(b.x); data.push_back(b.y); data.push_back(b.z);
    data.push_back(c.r); data.push_back(c.g); data.push_back(c.b);
}

static std::array<unsigned char, 7> glyph5x7(char ch)
{
    switch (std::toupper(static_cast<unsigned char>(ch)))
    {
    case 'A': return { 0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
    case 'B': return { 0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E };
    case 'C': return { 0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E };
    case 'D': return { 0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E };
    case 'E': return { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F };
    case 'F': return { 0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10 };
    case 'G': return { 0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F };
    case 'H': return { 0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11 };
    case 'I': return { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F };
    case 'J': return { 0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E };
    case 'K': return { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 };
    case 'L': return { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F };
    case 'M': return { 0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11 };
    case 'N': return { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 };
    case 'O': return { 0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
    case 'P': return { 0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10 };
    case 'Q': return { 0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D };
    case 'R': return { 0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11 };
    case 'S': return { 0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E };
    case 'T': return { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 };
    case 'U': return { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E };
    case 'V': return { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 };
    case 'W': return { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A };
    case 'X': return { 0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11 };
    case 'Y': return { 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04 };
    case 'Z': return { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F };
    case '0': return { 0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E };
    case '1': return { 0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E };
    case '2': return { 0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F };
    case '3': return { 0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E };
    case '4': return { 0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02 };
    case '5': return { 0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E };
    case '6': return { 0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E };
    case '7': return { 0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 };
    case '8': return { 0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E };
    case '9': return { 0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E };
    case '.': return { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C };
    case ':': return { 0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00 };
    case '-': return { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00 };
    case '/': return { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 };
    case '>': return { 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10 };
    default: return { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    }
}

static void pushHudText(std::vector<float>& data, float x, float y, float scale,
                        const std::string& text, glm::vec3 color)
{
    float startX = x;
    for (char ch : text)
    {
        if (ch == '\n')
        {
            x = startX;
            y += scale * 9.0f;
            continue;
        }

        auto rows = glyph5x7(ch);
        for (int row = 0; row < 7; ++row)
        {
            for (int col = 0; col < 5; ++col)
            {
                if (rows[row] & (1 << (4 - col)))
                {
                    float px = x + col * scale;
                    float py = y + row * scale;
                    pushLine(data, glm::vec3(px, py, 0.0f),
                             glm::vec3(px + scale * 0.82f, py, 0.0f), color);
                }
            }
        }

        x += scale * 6.0f;
    }
}

static void pushHudBox(std::vector<float>& data, float x, float y, float w, float h, glm::vec3 color)
{
    pushLine(data, glm::vec3(x, y, 0), glm::vec3(x + w, y, 0), color);
    pushLine(data, glm::vec3(x + w, y, 0), glm::vec3(x + w, y + h, 0), color);
    pushLine(data, glm::vec3(x + w, y + h, 0), glm::vec3(x, y + h, 0), color);
    pushLine(data, glm::vec3(x, y + h, 0), glm::vec3(x, y, 0), color);
}

static MeshGL buildIndexedMesh(const std::vector<float>& vertices8,
                               const std::vector<unsigned int>& indices)
{
    MeshGL m;
    glGenVertexArrays(1, &m.VAO);
    glBindVertexArray(m.VAO);

    glGenBuffers(1, &m.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices8.size() * sizeof(float), vertices8.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    m.indexCount = static_cast<GLsizei>(indices.size());
    return m;
}

static ColorMeshGL buildColorMesh(const std::vector<float>& vertices6,
                                  const std::vector<unsigned int>& indices)
{
    ColorMeshGL m;
    glGenVertexArrays(1, &m.VAO);
    glBindVertexArray(m.VAO);

    glGenBuffers(1, &m.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, m.VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices6.size() * sizeof(float), vertices6.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &m.EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = 6 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    m.indexCount = static_cast<GLsizei>(indices.size());
    return m;
}

static ColorMeshGL makeColorCube(glm::vec3 color)
{
    std::vector<float> v;
    auto push = [&](float x, float y, float z)
    {
        v.push_back(x); v.push_back(y); v.push_back(z);
        v.push_back(color.r); v.push_back(color.g); v.push_back(color.b);
    };

    push(-0.5f, -0.5f, -0.5f);
    push( 0.5f, -0.5f, -0.5f);
    push( 0.5f,  0.5f, -0.5f);
    push(-0.5f,  0.5f, -0.5f);
    push(-0.5f, -0.5f,  0.5f);
    push( 0.5f, -0.5f,  0.5f);
    push( 0.5f,  0.5f,  0.5f);
    push(-0.5f,  0.5f,  0.5f);

    std::vector<unsigned int> idx = {
        0, 1, 2, 0, 2, 3,
        4, 6, 5, 4, 7, 6,
        0, 4, 5, 0, 5, 1,
        3, 2, 6, 3, 6, 7,
        1, 5, 6, 1, 6, 2,
        0, 3, 7, 0, 7, 4
    };

    return buildColorMesh(v, idx);
}

#if 0
// Disabled old handmade fish pieces. The scene now uses Assets/goldfish.obj.
// Keeping this block only as reference, so it is not compiled or used.
static ColorMeshGL makeFishCardMesh(glm::vec3 color)
{
    std::vector<float> v;
    auto push = [&](glm::vec3 p, glm::vec3 c)
    {
        v.insert(v.end(), { p.x, p.y, p.z, c.r, c.g, c.b });
    };

    glm::vec3 tip = glm::min(color * 1.24f, glm::vec3(1.0f));
    glm::vec3 base = color * 0.82f;
    push(glm::vec3(-0.5f, -0.18f, 0.0f), base);
    push(glm::vec3( 0.5f, -0.14f, 0.0f), color);
    push(glm::vec3( 0.5f,  0.14f, 0.0f), tip);
    push(glm::vec3(-0.5f,  0.18f, 0.0f), color);

    std::vector<unsigned int> idx = { 0, 1, 2, 0, 2, 3 };
    return buildColorMesh(v, idx);
}

static ColorMeshGL makeColorSpheroid(glm::vec3 color, int slices = 14, int stacks = 8)
{
    std::vector<float> v;
    std::vector<unsigned int> idx;

    for (int iy = 0; iy <= stacks; ++iy)
    {
        float vf = float(iy) / float(stacks);
        float theta = -PI * 0.5f + vf * PI;
        float cy = std::cos(theta);
        float sy = std::sin(theta);

        for (int ix = 0; ix <= slices; ++ix)
        {
            float uf = float(ix) / float(slices);
            float phi = uf * PI * 2.0f;
            glm::vec3 p(std::cos(phi) * cy, sy, std::sin(phi) * cy);
            float warm = 0.88f + 0.18f * uf + 0.10f * vf;
            glm::vec3 c = color * warm;
            v.insert(v.end(), { p.x, p.y, p.z, c.r, c.g, c.b });
        }
    }

    int row = slices + 1;
    for (int y = 0; y < stacks; ++y)
    {
        for (int x = 0; x < slices; ++x)
        {
            unsigned int a = y * row + x;
            unsigned int b = y * row + x + 1;
            unsigned int c = (y + 1) * row + x + 1;
            unsigned int d = (y + 1) * row + x;
            idx.insert(idx.end(), { a, b, c, a, c, d });
        }
    }

    return buildColorMesh(v, idx);
}

static ColorMeshGL makeFishTailMesh(glm::vec3 color)
{
    std::vector<float> v;
    auto push = [&](glm::vec3 p)
    {
        v.insert(v.end(), { p.x, p.y, p.z, color.r, color.g, color.b });
    };

    push(glm::vec3(0.0f,  0.00f,  0.00f));
    push(glm::vec3(-0.34f, 0.24f,  0.06f));
    push(glm::vec3(-0.24f, 0.00f,  0.00f));
    push(glm::vec3(-0.34f,-0.24f, -0.06f));

    std::vector<unsigned int> idx = { 0, 1, 2, 0, 2, 3 };
    return buildColorMesh(v, idx);
}

static ColorMeshGL makeFishFinMesh(glm::vec3 color)
{
    std::vector<float> v;
    auto push = [&](glm::vec3 p)
    {
        v.insert(v.end(), { p.x, p.y, p.z, color.r, color.g, color.b });
    };

    push(glm::vec3(0.12f, 0.0f, 0.0f));
    push(glm::vec3(-0.10f, 0.0f, 0.0f));
    push(glm::vec3(-0.02f, -0.20f, 0.06f));

    std::vector<unsigned int> idx = { 0, 1, 2 };
    return buildColorMesh(v, idx);
}

static FloatingFishGL makeFloatingFish()
{
    FloatingFishGL fish;
    fish.body = makeFishCardMesh(glm::vec3(1.00f, 0.52f, 0.18f));
    return fish;
}
#endif

struct FishMeshCpu
{
    // Vertex layout: position.xyz, fallbackColor.rgb, uv.xy.
    // The fragment shader uses the UV to sample Assets/goldfish.png.
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

struct ObjFaceVertex
{
    int position = -1;
    int texcoord = -1;
};

static int parseObjIndex(const std::string& text, int count)
{
    char* end = nullptr;
    long idx = std::strtol(text.c_str(), &end, 10);
    if (idx == 0) return -1;
    if (idx < 0) idx = count + idx;
    else idx -= 1;
    if (idx < 0 || idx >= count) return -1;
    return static_cast<int>(idx);
}

static ObjFaceVertex parseObjFaceVertex(const std::string& token, int positionCount, int texcoordCount)
{
    ObjFaceVertex result;
    size_t firstSlash = token.find('/');
    size_t secondSlash = firstSlash == std::string::npos ? std::string::npos : token.find('/', firstSlash + 1);

    std::string posText = firstSlash == std::string::npos ? token : token.substr(0, firstSlash);
    result.position = parseObjIndex(posText, positionCount);

    if (firstSlash != std::string::npos)
    {
        std::string uvText = secondSlash == std::string::npos
            ? token.substr(firstSlash + 1)
            : token.substr(firstSlash + 1, secondSlash - firstSlash - 1);
        if (!uvText.empty())
        {
            result.texcoord = parseObjIndex(uvText, texcoordCount);
        }
    }

    return result;
}

static FishMeshCpu loadFishObjMeshData(const std::string& path)
{
    FishMeshCpu mesh;
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cerr << "Could not open fish obj: " << path << "\n";
        return mesh;
    }

    std::vector<glm::vec3> objPositions;
    std::vector<glm::vec2> objTexcoords;
    std::vector<std::vector<ObjFaceVertex>> faces;
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v")
        {
            glm::vec3 p;
            ss >> p.x >> p.y >> p.z;
            objPositions.push_back(p);
        }
        else if (tag == "vt")
        {
            glm::vec2 uv;
            ss >> uv.x >> uv.y;
            objTexcoords.push_back(uv);
        }
        else if (tag == "f")
        {
            std::vector<ObjFaceVertex> face;
            std::string token;
            while (ss >> token)
            {
                ObjFaceVertex corner = parseObjFaceVertex(token,
                                                          static_cast<int>(objPositions.size()),
                                                          static_cast<int>(objTexcoords.size()));
                if (corner.position >= 0) face.push_back(corner);
            }
            if (face.size() >= 3) faces.push_back(face);
        }
    }

    if (objPositions.empty() || faces.empty())
    {
        std::cerr << "Fish obj has no usable geometry: " << path << "\n";
        mesh.vertices.clear();
        mesh.indices.clear();
        return mesh;
    }

    glm::vec3 minP = objPositions[0];
    glm::vec3 maxP = objPositions[0];
    for (const glm::vec3& p : objPositions)
    {
        minP = glm::min(minP, p);
        maxP = glm::max(maxP, p);
    }

    const float targetMinX = -0.82f;
    const float targetMaxX = 0.60f;
    const float targetHeight = 0.50f;
    const float targetWidth = 0.38f;
    const float objForwardSign = 1.0f; // Change to -1.0 if the imported fish swims backward.
    glm::vec3 span = glm::max(maxP - minP, glm::vec3(0.0001f));
    glm::vec3 center = (minP + maxP) * 0.5f;

    auto pushObjCorner = [&](const ObjFaceVertex& corner)
    {
        const glm::vec3& p = objPositions[corner.position];
        // The Meshy export is longest on OBJ Z. We remap that axis to local X,
        // because the fish shader expects tail-to-head direction along local X.
        float body01 = (p.z - minP.z) / span.z;
        if (objForwardSign < 0.0f) body01 = 1.0f - body01;
        float x = targetMinX + body01 * (targetMaxX - targetMinX);
        float y = (p.y - center.y) * (targetHeight / span.y);
        float z = (p.x - center.x) * (targetWidth / span.x);

        glm::vec2 uv(body01, 0.5f);
        if (corner.texcoord >= 0)
        {
            uv = objTexcoords[corner.texcoord];
        }

        unsigned int index = static_cast<unsigned int>(mesh.vertices.size() / 8);
        mesh.vertices.insert(mesh.vertices.end(), { x, y, z, 1.0f, 1.0f, 1.0f, uv.x, uv.y });
        mesh.indices.push_back(index);
    };

    mesh.vertices.reserve(faces.size() * 3 * 8);
    mesh.indices.reserve(faces.size() * 3);
    for (const std::vector<ObjFaceVertex>& face : faces)
    {
        for (size_t i = 1; i + 1 < face.size(); ++i)
        {
            pushObjCorner(face[0]);
            pushObjCorner(face[i]);
            pushObjCorner(face[i + 1]);
        }
    }

    return mesh;
}

#if 0
// Disabled old generated low-poly fish body. This was the temporary placeholder
// before importing Assets/goldfish.obj, so it is commented out to avoid confusion.
static FishMeshCpu makeFishBodyMeshData()
{
    FishMeshCpu mesh;

    auto pushVertex = [&](glm::vec3 p, glm::vec3 c)
    {
        mesh.vertices.insert(mesh.vertices.end(), { p.x, p.y, p.z, c.r, c.g, c.b });
    };

    auto appendQuad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 color)
    {
        unsigned int base = static_cast<unsigned int>(mesh.vertices.size() / 6);
        pushVertex(a, color);
        pushVertex(b, color);
        pushVertex(c, color);
        pushVertex(d, color);
        mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    };

    auto appendTri = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 color)
    {
        unsigned int base = static_cast<unsigned int>(mesh.vertices.size() / 6);
        pushVertex(a, color);
        pushVertex(b, color);
        pushVertex(c, color);
        mesh.indices.insert(mesh.indices.end(), { base, base + 1, base + 2 });
    };

    struct Section
    {
        float x;
        float halfHeight;
        float halfThickness;
    };

    const std::array<Section, 4> sections = {{
        { -0.62f, 0.07f, 0.045f },
        { -0.34f, 0.19f, 0.145f },
        {  0.38f, 0.22f, 0.170f },
        {  0.60f, 0.12f, 0.095f }
    }};

    auto sectionPoint = [&](const Section& s, int side)
    {
        switch (side)
        {
        case 0: return glm::vec3(s.x,  s.halfHeight, 0.0f);
        case 1: return glm::vec3(s.x,  0.0f,         s.halfThickness);
        case 2: return glm::vec3(s.x, -s.halfHeight, 0.0f);
        default: return glm::vec3(s.x, 0.0f,        -s.halfThickness);
        }
    };

    const glm::vec3 topRight(1.00f, 0.66f, 0.22f);
    const glm::vec3 bottomRight(0.90f, 0.36f, 0.13f);
    const glm::vec3 bottomLeft(0.80f, 0.26f, 0.10f);
    const glm::vec3 topLeft(1.00f, 0.50f, 0.16f);

    for (size_t i = 0; i + 1 < sections.size(); ++i)
    {
        const Section& a = sections[i];
        const Section& b = sections[i + 1];
        appendQuad(sectionPoint(a, 0), sectionPoint(b, 0), sectionPoint(b, 1), sectionPoint(a, 1), topRight);
        appendQuad(sectionPoint(a, 1), sectionPoint(b, 1), sectionPoint(b, 2), sectionPoint(a, 2), bottomRight);
        appendQuad(sectionPoint(a, 2), sectionPoint(b, 2), sectionPoint(b, 3), sectionPoint(a, 3), bottomLeft);
        appendQuad(sectionPoint(a, 3), sectionPoint(b, 3), sectionPoint(b, 0), sectionPoint(a, 0), topLeft);
    }

    appendQuad(sectionPoint(sections.front(), 0),
               sectionPoint(sections.front(), 1),
               sectionPoint(sections.front(), 2),
               sectionPoint(sections.front(), 3),
               glm::vec3(0.76f, 0.24f, 0.10f));
    appendQuad(sectionPoint(sections.back(), 0),
               sectionPoint(sections.back(), 3),
               sectionPoint(sections.back(), 2),
               sectionPoint(sections.back(), 1),
               glm::vec3(1.00f, 0.74f, 0.26f));

    const glm::vec3 tailRoot(sections.front().x, 0.0f, 0.0f);
    appendTri(tailRoot,
              glm::vec3(-0.82f,  0.16f, 0.0f),
              glm::vec3(-0.76f,  0.00f, 0.115f),
              glm::vec3(1.00f, 0.46f, 0.15f));
    appendTri(tailRoot,
              glm::vec3(-0.76f,  0.00f, 0.115f),
              glm::vec3(-0.82f, -0.16f, 0.0f),
              glm::vec3(0.88f, 0.30f, 0.12f));
    appendTri(tailRoot,
              glm::vec3(-0.82f, -0.16f, 0.0f),
              glm::vec3(-0.76f,  0.00f, -0.115f),
              glm::vec3(0.78f, 0.24f, 0.10f));
    appendTri(tailRoot,
              glm::vec3(-0.76f,  0.00f, -0.115f),
              glm::vec3(-0.82f,  0.16f, 0.0f),
              glm::vec3(0.96f, 0.42f, 0.14f));

    return mesh;
}

static std::vector<float> makeFishCardVertices()
{
    return makeFishBodyMeshData().vertices;
}

static std::vector<unsigned int> makeFishCardIndices()
{
    return makeFishBodyMeshData().indices;
}
#endif

static std::vector<FishInstance> makeFishInstances(int count)
{
    std::vector<FishInstance> fish;
    fish.reserve(count);
    for (int i = 0; i < count; ++i)
    {
        float sideJitter = (hash01(i * 31 + 4) - 0.5f) * 0.045f;
        float heightJitter = (hash01(i * 47 + 8) - 0.5f) * 0.075f;
        float forwardJitter = (hash01(i * 59 + 12) - 0.5f) * 0.050f;
        int formationIndex = std::max(0, i - 1);
        int row = formationIndex / 6;
        int slot = formationIndex % 6;
        int pair = slot / 2;
        float sideSign = (slot % 2 == 0) ? -1.0f : 1.0f;
        float lateral = sideSign * (0.18f + 0.11f * (float)pair);

        FishInstance f;
        f.offset = glm::vec3(-0.12f * (float)row + forwardJitter,
                             ((row % 3) - 1) * 0.050f + heightJitter,
                             lateral + sideJitter);
        f.phase = hash01(i * 73 + 2) * PI * 2.0f;
        f.scale = 0.41f + hash01(i * 83 + 15) * 0.10f;
        f.speed = 0.90f + hash01(i * 97 + 20) * 0.18f;
        f.colorVar = hash01(i * 109 + 3);
        f.lane = (float)i * 0.065f + (float)row * 0.018f;
        if (i == 0)
        {
            f.offset = glm::vec3(0.0f);
            f.phase = 0.0f;
            f.scale = 0.50f;
            f.speed = 1.0f;
            f.colorVar = 0.65f;
            f.lane = 0.0f;  
        }
        fish.push_back(f);
    }
    return fish;
}

static std::vector<float> packFishInstances(const std::vector<FishInstance>& fish)
{
    std::vector<float> data;
    data.reserve(fish.size() * 8);
    for (const FishInstance& f : fish)
    {
        data.push_back(f.offset.x);
        data.push_back(f.offset.y);
        data.push_back(f.offset.z);
        data.push_back(f.phase);
        data.push_back(f.scale);
        data.push_back(f.speed);
        data.push_back(f.colorVar);
        data.push_back(f.lane);
    }
    return data;
}

static MeshGL makePlaneXY(float sx, float sy)
{
    std::vector<float> v;
    std::vector<unsigned int> idx = { 0, 1, 2, 0, 2, 3 };
    glm::vec3 n(0, 0, 1);
    pushVertex(v, glm::vec3(-sx, -sy, 0), n, glm::vec2(0, 0));
    pushVertex(v, glm::vec3( sx, -sy, 0), n, glm::vec2(1, 0));
    pushVertex(v, glm::vec3( sx,  sy, 0), n, glm::vec2(1, 1));
    pushVertex(v, glm::vec3(-sx,  sy, 0), n, glm::vec2(0, 1));
    return buildIndexedMesh(v, idx);
}

static MeshGL makeGridXZ(float sx, float sz, int nx, int nz)
{
    std::vector<float> v;
    std::vector<unsigned int> idx;
    glm::vec3 n(0, 1, 0);

    for (int z = 0; z <= nz; ++z)
    {
        float fz = float(z) / float(nz);
        for (int x = 0; x <= nx; ++x)
        {
            float fx = float(x) / float(nx);
            glm::vec3 p(-sx + fx * sx * 2.0f, 0.0f, -sz + fz * sz * 2.0f);
            pushVertex(v, p, n, glm::vec2(fx, fz));
        }
    }

    int row = nx + 1;
    for (int z = 0; z < nz; ++z)
    {
        for (int x = 0; x < nx; ++x)
        {
            unsigned int a = z * row + x;
            unsigned int b = z * row + x + 1;
            unsigned int c = (z + 1) * row + x + 1;
            unsigned int d = (z + 1) * row + x;
            idx.insert(idx.end(), { a, b, c, a, c, d });
        }
    }

    return buildIndexedMesh(v, idx);
}

static GLuint loadTexture2D(const char* path, bool repeat)
{
    int w = 0, h = 0, n = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &n, 4);
    if (!data)
    {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    return tex;
}

static GLuint makeSolidTexture(glm::vec3 color)
{
    unsigned char px[4] = {
        static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, color.r)) * 255.0f),
        static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, color.g)) * 255.0f),
        static_cast<unsigned char>(std::max(0.0f, std::min(1.0f, color.b)) * 255.0f),
        255
    };

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

struct HudLine
{
    std::string text;
    glm::vec3 color;
};

static std::vector<unsigned char> buildHudBitmap(int w, int h, const std::vector<HudLine>& lines)
{
    std::vector<unsigned char> rgba(w * h * 4, 0);

#ifdef _WIN32
    const unsigned char bgR = 9;
    const unsigned char bgG = 13;
    const unsigned char bgB = 18;
    const unsigned char bgA = 118;

    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(dc, bmp);

    HBRUSH bgBrush = CreateSolidBrush(RGB(bgR, bgG, bgB));
    RECT fullRect{ 0, 0, w, h };
    FillRect(dc, &fullRect, bgBrush);
    DeleteObject(bgBrush);

    HBRUSH lineBrush = CreateSolidBrush(RGB(196, 154, 45));
    RECT topLine{ 0, 0, w, 2 };
    FillRect(dc, &topLine, lineBrush);
    DeleteObject(lineBrush);

    SetBkMode(dc, TRANSPARENT);
    HFONT font = CreateFontA(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    HGDIOBJ oldFont = SelectObject(dc, font);

    int y = 9;
    for (const HudLine& line : lines)
    {
        COLORREF c = RGB(
            int(std::max(0.0f, std::min(1.0f, line.color.r)) * 255.0f),
            int(std::max(0.0f, std::min(1.0f, line.color.g)) * 255.0f),
            int(std::max(0.0f, std::min(1.0f, line.color.b)) * 255.0f));
        SetTextColor(dc, c);
        TextOutA(dc, 8, y, line.text.c_str(), static_cast<int>(line.text.size()));
        y += 15;
    }

    unsigned char* bgra = static_cast<unsigned char*>(bits);
    for (int i = 0; i < w * h; ++i)
    {
        unsigned char b = bgra[i * 4 + 0];
        unsigned char g = bgra[i * 4 + 1];
        unsigned char r = bgra[i * 4 + 2];
        int diff = std::abs(int(r) - int(bgR)) +
                   std::abs(int(g) - int(bgG)) +
                   std::abs(int(b) - int(bgB));

        rgba[i * 4 + 0] = r;
        rgba[i * 4 + 1] = g;
        rgba[i * 4 + 2] = b;
        rgba[i * 4 + 3] = static_cast<unsigned char>(diff < 8 ? bgA : std::min(255, 150 + diff * 2));
    }

    SelectObject(dc, oldFont);
    DeleteObject(font);
    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
#else
    for (int i = 0; i < w * h; ++i)
    {
        rgba[i * 4 + 0] = 9;
        rgba[i * 4 + 1] = 13;
        rgba[i * 4 + 2] = 18;
        rgba[i * 4 + 3] = 118;
    }
#endif

    return rgba;
}

static std::vector<float> makeBladeVertices()
{
    std::vector<float> v;
    auto addQuad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d)
    {
        glm::vec3 n = safeNormal(a, b, c);
        pushVertex(v, a, n, glm::vec2(0.0f, 0.0f));
        pushVertex(v, b, n, glm::vec2(1.0f, 0.0f));
        pushVertex(v, c, n, glm::vec2(1.0f, 1.0f));
        pushVertex(v, a, n, glm::vec2(0.0f, 0.0f));
        pushVertex(v, c, n, glm::vec2(1.0f, 1.0f));
        pushVertex(v, d, n, glm::vec2(0.0f, 1.0f));
    };

    addQuad(glm::vec3(-0.50f, 0.0f, 0.0f),
            glm::vec3( 0.50f, 0.0f, 0.0f),
            glm::vec3( 0.50f, 1.0f, 0.0f),
            glm::vec3(-0.50f, 1.0f, 0.0f));

    addQuad(glm::vec3(0.0f, 0.0f, -0.50f),
            glm::vec3(0.0f, 0.0f,  0.50f),
            glm::vec3(0.0f, 1.0f,  0.50f),
            glm::vec3(0.0f, 1.0f, -0.50f));

    return v;
}

static std::vector<GrassInstance> makeGrassInstances(int count)
{
    std::vector<GrassInstance> grass;
    grass.reserve(count);

    for (int i = 0; i < count; ++i)
    {
        float rx = hash01(i * 17 + 3);
        float rz = hash01(i * 23 + 7);
        float z = -YARD_HALF_D + rz * YARD_HALF_D * 2.0f;
        float depthMix = glm::smoothstep(-YARD_HALF_D, YARD_HALF_D, z);

        GrassInstance g;
        g.x = -YARD_HALF_W + rx * YARD_HALF_W * 2.0f;
        g.z = YARD_Z + z;
        g.height = (0.09f + 0.20f * hash01(i * 29 + 5)) * (0.86f + 0.14f * depthMix);
        g.width = g.height * (0.13f + 0.08f * hash01(i * 31 + 9));
        g.yaw = hash01(i * 37 + 11) * 2.0f * PI;
        g.phase = hash01(i * 41 + 13) * 2.0f * PI;
        g.stiffness = 0.65f + 0.75f * hash01(i * 43 + 17);
        g.colorVar = hash01(i * 47 + 19);
        grass.push_back(g);
    }

    return grass;
}

static std::vector<float> packGrassInstances(const std::vector<GrassInstance>& grass)
{
    std::vector<float> data;
    data.reserve(grass.size() * 8);
    for (const auto& g : grass)
    {
        data.push_back(g.x);
        data.push_back(g.z);
        data.push_back(g.height);
        data.push_back(g.width);
        data.push_back(g.yaw);
        data.push_back(g.phase);
        data.push_back(g.stiffness);
        data.push_back(g.colorVar);
    }
    return data;
}

static glm::vec3 windAt(glm::vec2 p, float t, float windStrength, float windSpeed, glm::vec3 windVector, int windMode)
{
    glm::vec3 wind3 = safeNormalize3(windVector, glm::vec3(1.0f, 0.0f, 0.32f));
    glm::vec2 windDir = safeNormalize2(glm::vec2(wind3.x, wind3.z), glm::vec2(1.0f, 0.32f));
    glm::vec2 side(-windDir.y, windDir.x);
    float along = glm::dot(p, windDir);

    if (windMode == 1)
    {
        float travel = t * windSpeed;
        float broadWave = std::sin(along * 1.35f - travel * 1.55f) * 0.5f + 0.5f;
        float softNoise = std::sin((p.x * 0.72f - windDir.x * travel * 0.38f) * 1.7f +
                                   (p.y * 0.72f - windDir.y * travel * 0.38f) * 1.1f) * 0.5f + 0.5f;
        float gustField = glm::smoothstep(0.34f, 0.96f, broadWave * 0.32f + softNoise * 0.68f);
        return wind3 * ((0.32f + gustField * 0.82f) * windStrength);
    }

    float cross = glm::dot(p, side);
    float wave = 0.65f * std::sin(along * 3.0f + t * windSpeed) +
                 0.35f * std::sin(cross * 4.7f + t * windSpeed * 1.45f);
    float gust = 0.55f + 0.45f * std::sin(t * 0.55f + along * 0.7f);
    return wind3 * (wave * gust * windStrength);
}

static glm::vec3 fishPathAt(float phase)
{
    float s = std::sin(phase);
    float c = std::cos(phase);
    return glm::vec3(s * FISH_PATH_X_RADIUS,
                     FISH_PATH_Y,
                     YARD_Z + FISH_PATH_Z_CENTER_OFFSET + s * c * FISH_PATH_Z_RADIUS);
}

int main()
{
    Window mainWindow(WIDTH, HEIGHT, 3, 3);
    if (mainWindow.initialise() != 0)
    {
        std::cerr << "Failed to initialize window.\n";
        return 1;
    }

    GLFWwindow* w = mainWindow.getWindow();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.06f, 0.10f, 0.08f, 1.0f);

    Shader texSh;
    texSh.CreateFromFiles("Shaders/Yard/texture.vert", "Shaders/Yard/texture.frag");

    Shader terrainSh;
    terrainSh.CreateFromFiles("Shaders/Yard/grass.vert", "Shaders/Yard/grass.frag");

    Shader bladeSh;
    bladeSh.CreateFromFiles("Shaders/Yard/instanced_grass.vert", "Shaders/Yard/instanced_grass.frag");

    Shader fishSh;
    fishSh.CreateFromFiles("Shaders/Yard/kinematic_fish.vert", "Shaders/Yard/kinematic_fish.frag");

    Shader lineSh;
    lineSh.CreateFromFiles("Shaders/Lab3/line.vert", "Shaders/Lab3/line.frag");

    MeshGL backdrop = makePlaneXY(1.0f, 1.0f);
    MeshGL ground = makeGridXZ(YARD_HALF_W, YARD_HALF_D, 96, 72);
    MeshGL hudPanel = makePlaneXY(1.0f, 1.0f);
    ColorMeshGL windTargetBox = makeColorCube(glm::vec3(0.05f, 0.32f, 1.0f));

    GLuint backdropTex = loadTexture2D("Assets/backdrop.jpg", false);
    GLuint grassTex = loadTexture2D("Assets/grass_patch.jpg", true);
    GLuint fishTex = loadTexture2D("Assets/goldfish.png", false);
    GLuint hudPanelTex = makeSolidTexture(glm::vec3(0.02f, 0.07f, 0.06f));

    const int MAX_GRASS = 16000;
    std::vector<GrassInstance> grassInstances = makeGrassInstances(MAX_GRASS);
    std::vector<float> grassInstanceFloats = packGrassInstances(grassInstances);
    InstancedGrassGL instancedGrass;
    instancedGrass.init(makeBladeVertices(), grassInstanceFloats);

    const int MAX_FISH = 96;
    std::vector<FishInstance> fishInstances = makeFishInstances(MAX_FISH);
    std::vector<float> fishInstanceFloats = packFishInstances(fishInstances);
    FishMeshCpu fishMesh = loadFishObjMeshData("Assets/goldfish.obj");
    if (fishMesh.vertices.empty() || fishMesh.indices.empty())
    {
        std::cerr << "Failed to load required fish model Assets/goldfish.obj\n";
        return 1;
    }
    InstancedFishGL fishSchool;
    fishSchool.init(fishMesh.vertices, fishMesh.indices, fishInstanceFloats);

    DynamicLineGL debugLines;
    debugLines.init(4096);
    HudTextureGL hudTexture;
    hudTexture.init(500, 170);

    GLint uModel_t = (GLint)texSh.GetUniformLocation("uModel");
    GLint uView_t = (GLint)texSh.GetUniformLocation("uView");
    GLint uProj_t = (GLint)texSh.GetUniformLocation("uProj");
    GLint uTexture_t = (GLint)texSh.GetUniformLocation("uTexture");
    GLint uAlpha_t = (GLint)texSh.GetUniformLocation("uAlpha");

    GLint uModel_g = (GLint)terrainSh.GetUniformLocation("uModel");
    GLint uView_g = (GLint)terrainSh.GetUniformLocation("uView");
    GLint uProj_g = (GLint)terrainSh.GetUniformLocation("uProj");
    GLint uTime_g = (GLint)terrainSh.GetUniformLocation("uTime");
    GLint uWindStrength_g = (GLint)terrainSh.GetUniformLocation("uWindStrength");
    GLint uWindSpeed_g = (GLint)terrainSh.GetUniformLocation("uWindSpeed");
    GLint uWindDir_g = (GLint)terrainSh.GetUniformLocation("uWindDir");
    GLint uGrassTexture_g = (GLint)terrainSh.GetUniformLocation("uGrassTexture");
    GLint uDebugMode_g = (GLint)terrainSh.GetUniformLocation("uDebugMode");
    GLint uWindMode_g = (GLint)terrainSh.GetUniformLocation("uWindMode");

    GLint uModel_b = (GLint)bladeSh.GetUniformLocation("uModel");
    GLint uView_b = (GLint)bladeSh.GetUniformLocation("uView");
    GLint uProj_b = (GLint)bladeSh.GetUniformLocation("uProj");
    GLint uTime_b = (GLint)bladeSh.GetUniformLocation("uTime");
    GLint uWindStrength_b = (GLint)bladeSh.GetUniformLocation("uWindStrength");
    GLint uWindSpeed_b = (GLint)bladeSh.GetUniformLocation("uWindSpeed");
    GLint uWindVector_b = (GLint)bladeSh.GetUniformLocation("uWindVector");
    GLint uWindMode_b = (GLint)bladeSh.GetUniformLocation("uWindMode");
    GLint uYardCenter_b = (GLint)bladeSh.GetUniformLocation("uYardCenter");
    GLint uYardSize_b = (GLint)bladeSh.GetUniformLocation("uYardSize");
    GLint uDebugMode_b = (GLint)bladeSh.GetUniformLocation("uDebugMode");
    GLint uGrassTexture_b = (GLint)bladeSh.GetUniformLocation("uGrassTexture");

    GLint uModel_f = (GLint)fishSh.GetUniformLocation("uModel");
    GLint uView_f = (GLint)fishSh.GetUniformLocation("uView");
    GLint uProj_f = (GLint)fishSh.GetUniformLocation("uProj");
    GLint uTime_f = (GLint)fishSh.GetUniformLocation("uTime");
    GLint uYardZ_f = (GLint)fishSh.GetUniformLocation("uYardZ");
    GLint uFishTexture_f = (GLint)fishSh.GetUniformLocation("uFishTexture");

    GLint uModel_l = (GLint)lineSh.GetUniformLocation("uModel");
    GLint uView_l = (GLint)lineSh.GetUniformLocation("uView");
    GLint uProj_l = (GLint)lineSh.GetUniformLocation("uProj");

    AnimClock clock;
    clock.mode = TimingMode::SemiFixedDt;
    clock.fixedDt = 1.0f / 60.0f;
    clock.dtMax = 0.05f;
    clock.maxSubsteps = 8;

    const glm::vec3 windOrigin(0.0f, 0.42f, YARD_Z);
    glm::vec3 windTarget = windOrigin + safeNormalize3(glm::vec3(1.0f, 0.0f, 0.32f), glm::vec3(1.0f, 0.0f, 0.32f)) * 1.55f;
    glm::vec3 windVector = safeNormalize3(windTarget - windOrigin, glm::vec3(1.0f, 0.0f, 0.32f));
    float windStrength = 0.85f;
    float windSpeed = 0.38f;
    int windMode = 0;
    int activeGrass = 12000;
    int activeFish = 1;
    bool showWindDebug = false;
    bool showGrassDebug = false;
    bool wireframe = false;
    bool flyCameraMode = false;
    bool showDebugLog = false;
    int debugMode = 0;

    glm::vec3 sceneCamPos = DEFAULT_CAMERA_POS;
    glm::vec3 sceneCamTarget = DEFAULT_CAMERA_TARGET;
    glm::vec3 flyCamPos = sceneCamPos;
    glm::vec3 initialForward = safeNormalize3(sceneCamTarget - sceneCamPos, glm::vec3(0.0f, 0.0f, -1.0f));
    float flyYawDeg = std::atan2(initialForward.z, initialForward.x) * 180.0f / PI;
    float flyPitchDeg = std::asin(std::max(-0.98f, std::min(0.98f, initialForward.y))) * 180.0f / PI;

    float lastTime = (float)glfwGetTime();
    static int prev[GLFW_KEY_LAST + 1] = { 0 };
    auto pressedOnce = [&](int key) -> bool
    {
        int cur = glfwGetKey(w, key);
        bool fired = (cur == GLFW_PRESS && prev[key] != GLFW_PRESS);
        prev[key] = cur;
        return fired;
    };

    while (!glfwWindowShouldClose(w))
    {
        glfwPollEvents();

        float now = (float)glfwGetTime();
        float realDt = now - lastTime;
        lastTime = now;

        if (pressedOnce(GLFW_KEY_ESCAPE)) glfwSetWindowShouldClose(w, GLFW_TRUE);
        bool spacePressed = pressedOnce(GLFW_KEY_SPACE);
        if (!flyCameraMode && spacePressed) clock.playing = !clock.playing;
        if (pressedOnce(GLFW_KEY_R)) clock.reset();
        if (pressedOnce(GLFW_KEY_V)) showWindDebug = !showWindDebug;
        if (pressedOnce(GLFW_KEY_G)) showGrassDebug = !showGrassDebug;
        if (pressedOnce(GLFW_KEY_F)) wireframe = !wireframe;
        if (pressedOnce(GLFW_KEY_N)) debugMode = (debugMode + 1) % 2;
        if (pressedOnce(GLFW_KEY_M)) windMode = (windMode + 1) % 2;
        if (pressedOnce(GLFW_KEY_1)) showDebugLog = !showDebugLog;
        if (pressedOnce(GLFW_KEY_TAB))
        {
            flyCameraMode = !flyCameraMode;
            if (flyCameraMode)
            {
                flyCamPos = sceneCamPos;
                glm::vec3 sceneForward = safeNormalize3(sceneCamTarget - sceneCamPos, glm::vec3(0.0f, 0.0f, -1.0f));
                flyYawDeg = std::atan2(sceneForward.z, sceneForward.x) * 180.0f / PI;
                flyPitchDeg = std::asin(std::max(-0.98f, std::min(0.98f, sceneForward.y))) * 180.0f / PI;
            }
        }
        if (pressedOnce(GLFW_KEY_HOME))
        {
            sceneCamPos = DEFAULT_CAMERA_POS;
            sceneCamTarget = DEFAULT_CAMERA_TARGET;
            flyCamPos = sceneCamPos;
            glm::vec3 sceneForward = safeNormalize3(sceneCamTarget - sceneCamPos, glm::vec3(0.0f, 0.0f, -1.0f));
            flyYawDeg = std::atan2(sceneForward.z, sceneForward.x) * 180.0f / PI;
            flyPitchDeg = std::asin(std::max(-0.98f, std::min(0.98f, sceneForward.y))) * 180.0f / PI;
        }

        if (glfwGetKey(w, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS)
            windStrength = std::max(0.0f, windStrength - realDt * 0.65f);
        if (glfwGetKey(w, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS)
            windStrength = std::min(2.0f, windStrength + realDt * 0.65f);
        if (glfwGetKey(w, GLFW_KEY_MINUS) == GLFW_PRESS)
            windSpeed = std::max(0.15f, windSpeed - realDt * 0.9f);
        if (glfwGetKey(w, GLFW_KEY_EQUAL) == GLFW_PRESS)
            windSpeed = std::min(4.0f, windSpeed + realDt * 0.9f);
        if (glfwGetKey(w, GLFW_KEY_COMMA) == GLFW_PRESS)
            activeGrass = std::max(0, activeGrass - int(realDt * 4200.0f));
        if (glfwGetKey(w, GLFW_KEY_PERIOD) == GLFW_PRESS)
            activeGrass = std::min(MAX_GRASS, activeGrass + int(realDt * 4200.0f));
        if (pressedOnce(GLFW_KEY_2))
            activeFish = std::max(1, activeFish - 1);
        if (pressedOnce(GLFW_KEY_3))
            activeFish = std::min(MAX_FISH, activeFish + 1);

        float targetStep = realDt * 1.15f;
        if (glfwGetKey(w, GLFW_KEY_I) == GLFW_PRESS)
            windTarget.z -= targetStep;
        if (glfwGetKey(w, GLFW_KEY_K) == GLFW_PRESS)
            windTarget.z += targetStep;
        if (glfwGetKey(w, GLFW_KEY_J) == GLFW_PRESS)
            windTarget.x -= targetStep;
        if (glfwGetKey(w, GLFW_KEY_L) == GLFW_PRESS)
            windTarget.x += targetStep;
        if (glfwGetKey(w, GLFW_KEY_U) == GLFW_PRESS)
            windTarget.y -= targetStep;
        if (glfwGetKey(w, GLFW_KEY_O) == GLFW_PRESS)
            windTarget.y += targetStep;
        windTarget.x = std::max(-YARD_HALF_W, std::min(YARD_HALF_W, windTarget.x));
        windTarget.y = std::max(0.22f, std::min(1.55f, windTarget.y));
        windTarget.z = std::max(YARD_Z - YARD_HALF_D, std::min(YARD_Z + YARD_HALF_D, windTarget.z));
        windVector = safeNormalize3(windTarget - windOrigin, glm::vec3(1.0f, 0.0f, 0.32f));

        glm::vec3 flyForward = cameraForward(flyYawDeg, flyPitchDeg);
        if (flyCameraMode)
        {
            float lookSpeed = 72.0f * realDt;
            if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS)
                flyYawDeg -= lookSpeed;
            if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS)
                flyYawDeg += lookSpeed;
            if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS)
                flyPitchDeg += lookSpeed;
            if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS)
                flyPitchDeg -= lookSpeed;
            flyPitchDeg = std::max(-84.0f, std::min(84.0f, flyPitchDeg));

            flyForward = cameraForward(flyYawDeg, flyPitchDeg);
            glm::vec3 flyRight = safeNormalize3(glm::cross(flyForward, glm::vec3(0, 1, 0)), glm::vec3(1, 0, 0));
            float moveSpeed = (glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ? 4.2f : 1.65f) * realDt;
            if (glfwGetKey(w, GLFW_KEY_W) == GLFW_PRESS)
                flyCamPos += flyForward * moveSpeed;
            if (glfwGetKey(w, GLFW_KEY_S) == GLFW_PRESS)
                flyCamPos -= flyForward * moveSpeed;
            if (glfwGetKey(w, GLFW_KEY_A) == GLFW_PRESS)
                flyCamPos -= flyRight * moveSpeed;
            if (glfwGetKey(w, GLFW_KEY_D) == GLFW_PRESS)
                flyCamPos += flyRight * moveSpeed;
            if (glfwGetKey(w, GLFW_KEY_SPACE) == GLFW_PRESS)
                flyCamPos.y += moveSpeed;
            if (glfwGetKey(w, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
                flyCamPos.y -= moveSpeed;
        }

        clock.beginFrame(realDt);
        float simDt = 0.0f;
        while (clock.nextStep(simDt)) {}

        float t = clock.simTime;
        glm::vec2 windDirXZ = safeNormalize2(glm::vec2(windVector.x, windVector.z), glm::normalize(glm::vec2(1.0f, 0.32f)));

        std::vector<float> debugData;
        if (showWindDebug)
        {
            pushLine(debugData, windOrigin, windTarget, glm::vec3(0.08f, 0.40f, 1.0f));
            const int fishPathSegments = 96;
            glm::vec3 prevPath = fishPathAt(0.0f);
            for (int i = 1; i <= fishPathSegments; ++i)
            {
                float phase = (float)i / (float)fishPathSegments * PI * 2.0f;
                glm::vec3 nextPath = fishPathAt(phase);
                pushLine(debugData, prevPath, nextPath, glm::vec3(0.12f, 0.82f, 1.0f));
                prevPath = nextPath;
            }

            for (int ix = 0; ix <= 8; ++ix)
            {
                for (int iz = 0; iz <= 6; ++iz)
                {
                    glm::vec2 p(-YARD_HALF_W + ix * (YARD_HALF_W * 2.0f / 8.0f),
                                YARD_Z - YARD_HALF_D + iz * (YARD_HALF_D * 2.0f / 6.0f));
                    glm::vec3 a(p.x, 0.08f, p.y);
                    glm::vec3 wv = windAt(p, t, windStrength, windSpeed, windVector, windMode);
                    pushLine(debugData, a, a + wv * 0.42f + glm::vec3(0, 0.035f, 0), glm::vec3(1.0f, 0.78f, 0.12f));
                }
            }
        }
        if (showGrassDebug)
        {
            int step = std::max(1, activeGrass / 80);
            for (int i = 0; i < activeGrass; i += step)
            {
                const GrassInstance& g = grassInstances[i];
                glm::vec3 base(g.x, 0.045f, g.z);
                glm::vec3 top = base + glm::vec3(0, g.height, 0) +
                    windAt(glm::vec2(g.x, g.z), t + g.phase, windStrength, windSpeed, windVector, windMode) * g.height * 0.30f;
                pushLine(debugData, base, top, glm::vec3(0.10f, 0.30f, 1.0f));
            }
        }
        debugLines.upload(debugData);

        if (showDebugLog)
        {
            char line[256];
            std::vector<HudLine> hudLinesData;
            glm::vec3 hudCamPos = flyCameraMode ? flyCamPos : sceneCamPos;
            glm::vec3 hudCamTarget = flyCameraMode ? flyCamPos + flyForward : sceneCamTarget;
            hudLinesData.push_back({ "Tap [1] close, Tab fly, V vectors/path, M wind, 2/3 fish", glm::vec3(0.96f, 0.83f, 0.36f) });
            hudLinesData.push_back({ flyCameraMode ? "[CATEGORY: FlyCamera]" : "[CATEGORY: SceneCamera]", glm::vec3(0.18f, 1.0f, 0.28f) });
            std::snprintf(line, sizeof(line), "Pos: %.2f, %.2f, %.2f   Target: %.2f, %.2f, %.2f",
                          hudCamPos.x, hudCamPos.y, hudCamPos.z,
                          hudCamTarget.x, hudCamTarget.y, hudCamTarget.z);
            hudLinesData.push_back({ line, glm::vec3(0.86f, 0.88f, 0.91f) });
            std::snprintf(line, sizeof(line), "Yaw: %.1f  Pitch: %.1f   Blades: %d   Fish: %d", flyYawDeg, flyPitchDeg, activeGrass, activeFish);
            hudLinesData.push_back({ line, glm::vec3(0.64f, 0.78f, 1.0f) });
            hudLinesData.push_back({ "[CATEGORY: WindField]", glm::vec3(0.18f, 1.0f, 0.28f) });
            std::snprintf(line, sizeof(line), "Mode: %s   Strength: %.2f   Speed: %.2f", windMode == 1 ? "Wave" : "Flutter", windStrength, windSpeed);
            hudLinesData.push_back({ line, glm::vec3(0.86f, 0.88f, 0.91f) });
            std::snprintf(line, sizeof(line), "Target: %.2f, %.2f, %.2f   Keys: IJKL / UO", windTarget.x, windTarget.y, windTarget.z);
            hudLinesData.push_back({ line, glm::vec3(0.91f, 0.86f, 0.54f) });
            hudLinesData.push_back({ "[CATEGORY: FishSystem]", glm::vec3(0.18f, 1.0f, 0.28f) });
            float fishScale = fishInstances.empty() ? 0.0f : fishInstances[0].scale;
            float minFishScale = fishScale;
            float maxFishScale = fishScale;
            int scaleCount = std::min(activeFish, static_cast<int>(fishInstances.size()));
            for (int i = 0; i < scaleCount; ++i)
            {
                minFishScale = std::min(minFishScale, fishInstances[i].scale);
                maxFishScale = std::max(maxFishScale, fishInstances[i].scale);
            }
            std::snprintf(line, sizeof(line), "Active: %d/%d   Main Scale: %.2f   Range: %.2f-%.2f",
                          activeFish, MAX_FISH, fishScale, minFishScale, maxFishScale);
            hudLinesData.push_back({ line, glm::vec3(0.86f, 0.88f, 0.91f) });
            std::snprintf(line, sizeof(line), "Path X: %.2f   Z: %.2f   Y: %.2f   Shape: figure-8",
                          FISH_PATH_X_RADIUS, FISH_PATH_Z_RADIUS, FISH_PATH_Y);
            hudLinesData.push_back({ line, glm::vec3(0.64f, 0.78f, 1.0f) });
            hudLinesData.push_back({ "Mesh: Assets/goldfish.obj   Texture: Assets/goldfish.png", glm::vec3(0.91f, 0.86f, 0.54f) });

            hudTexture.upload(buildHudBitmap(hudTexture.width, hudTexture.height, hudLinesData));
        }

        char title[512];
        if (flyCameraMode)
        {
            std::snprintf(title, sizeof(title),
                "Living Yard | FLY cam pos %.2f %.2f %.2f yaw %.1f pitch %.1f fish %d | 1 log Home reset | WASD Space Ctrl arrows",
                flyCamPos.x, flyCamPos.y, flyCamPos.z, flyYawDeg, flyPitchDeg, activeFish);
        }
        else
        {
            std::snprintf(title, sizeof(title),
                "Living Yard | SCENE cam %.2f %.2f %.2f -> %.2f %.2f %.2f | %s wind %.2f speed %.2f fish %d | Tab fly 1 log",
                sceneCamPos.x, sceneCamPos.y, sceneCamPos.z,
                sceneCamTarget.x, sceneCamTarget.y, sceneCamTarget.z,
                windMode == 1 ? "WAVE" : "FLUTTER",
                windStrength, windSpeed, activeFish);
        }
        glfwSetWindowTitle(w, title);

        glm::vec3 camPos = flyCameraMode ? flyCamPos : sceneCamPos;
        glm::vec3 camTarget = flyCameraMode ? flyCamPos + flyForward : sceneCamTarget;
        glm::mat4 view = glm::lookAt(camPos, camTarget, glm::vec3(0, 1, 0));
        glm::mat4 proj = glm::perspective(glm::radians(CAMERA_FOV_DEG), float(WIDTH) / float(HEIGHT), 0.05f, 100.0f);
        glm::mat4 identity(1.0f);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);

        if (backdropTex)
        {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            texSh.UseShader();
            glm::mat4 backdropM =
                glm::translate(identity, BACKDROP_POS) *
                glm::scale(identity, glm::vec3(BACKDROP_HALF_W, BACKDROP_HALF_H, 1.0f));
            glUniformMatrix4fv(uModel_t, 1, GL_FALSE, glm::value_ptr(backdropM));
            glUniformMatrix4fv(uView_t, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(uProj_t, 1, GL_FALSE, glm::value_ptr(proj));
            glUniform1f(uAlpha_t, 1.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, backdropTex);
            glUniform1i(uTexture_t, 0);
            glBindVertexArray(backdrop.VAO);
            glDrawElements(GL_TRIANGLES, backdrop.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);
        }

        // Draw a guaranteed visible grass-texture base first; procedural shader and blades sit on top.
        if (grassTex)
        {
            texSh.UseShader();
            glm::mat4 groundTexM = glm::translate(identity, glm::vec3(0.0f, -0.032f, YARD_Z));
            glUniformMatrix4fv(uModel_t, 1, GL_FALSE, glm::value_ptr(groundTexM));
            glUniformMatrix4fv(uView_t, 1, GL_FALSE, glm::value_ptr(view));
            glUniformMatrix4fv(uProj_t, 1, GL_FALSE, glm::value_ptr(proj));
            glUniform1f(uAlpha_t, 1.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, grassTex);
            glUniform1i(uTexture_t, 0);
            glBindVertexArray(ground.VAO);
            glDrawElements(GL_TRIANGLES, ground.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

        terrainSh.UseShader();
        glm::mat4 groundM = glm::translate(identity, glm::vec3(0.0f, -0.030f, YARD_Z));
        glUniformMatrix4fv(uModel_g, 1, GL_FALSE, glm::value_ptr(groundM));
        glUniformMatrix4fv(uView_g, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProj_g, 1, GL_FALSE, glm::value_ptr(proj));
        glUniform1f(uTime_g, t);
        glUniform1f(uWindStrength_g, windStrength);
        glUniform1f(uWindSpeed_g, windSpeed);
        glUniform2fv(uWindDir_g, 1, glm::value_ptr(windDirXZ));
        glUniform1i(uDebugMode_g, debugMode);
        glUniform1i(uWindMode_g, windMode);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTex);
        glUniform1i(uGrassTexture_g, 0);
        glBindVertexArray(ground.VAO);
        glDrawElements(GL_TRIANGLES, ground.indexCount, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        bladeSh.UseShader();
        glUniformMatrix4fv(uModel_b, 1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix4fv(uView_b, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProj_b, 1, GL_FALSE, glm::value_ptr(proj));
        glUniform1f(uTime_b, t);
        glUniform1f(uWindStrength_b, windStrength);
        glUniform1f(uWindSpeed_b, windSpeed);
        glUniform3fv(uWindVector_b, 1, glm::value_ptr(windVector));
        glUniform1i(uWindMode_b, windMode);
        glUniform2f(uYardCenter_b, 0.0f, YARD_Z);
        glUniform2f(uYardSize_b, YARD_HALF_W * 2.0f, YARD_HALF_D * 2.0f);
        glUniform1i(uDebugMode_b, debugMode);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, grassTex);
        glUniform1i(uGrassTexture_b, 0);
        glBindVertexArray(instancedGrass.VAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, instancedGrass.bladeVertexCount, activeGrass);
        glBindVertexArray(0);
        glEnable(GL_BLEND);

        fishSh.UseShader();
        glEnable(GL_DEPTH_TEST);
        glUniformMatrix4fv(uModel_f, 1, GL_FALSE, glm::value_ptr(identity));
        glUniformMatrix4fv(uView_f, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProj_f, 1, GL_FALSE, glm::value_ptr(proj));
        glUniform1f(uTime_f, t);
        glUniform1f(uYardZ_f, YARD_Z);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fishTex);
        glUniform1i(uFishTexture_f, 0);
        glBindVertexArray(fishSchool.VAO);
        glDrawElementsInstanced(GL_TRIANGLES, fishSchool.indexCount, GL_UNSIGNED_INT, 0, activeFish);
        glBindVertexArray(0);

        glDisable(GL_DEPTH_TEST);
        lineSh.UseShader();
        glUniformMatrix4fv(uView_l, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(uProj_l, 1, GL_FALSE, glm::value_ptr(proj));
        glUniformMatrix4fv(uModel_l, 1, GL_FALSE, glm::value_ptr(identity));
        glBindVertexArray(debugLines.VAO);
        glDrawArrays(GL_LINES, 0, debugLines.vertCount);
        glBindVertexArray(0);

        if (showWindDebug)
        {
            glm::mat4 windBoxM =
                glm::translate(identity, windTarget) *
                glm::scale(identity, glm::vec3(0.13f, 0.13f, 0.13f));
            glUniformMatrix4fv(uModel_l, 1, GL_FALSE, glm::value_ptr(windBoxM));
            glBindVertexArray(windTargetBox.VAO);
            glDrawElements(GL_TRIANGLES, windTargetBox.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }

        if (showDebugLog)
        {
            glm::mat4 hudProj = glm::ortho(0.0f, float(WIDTH), float(HEIGHT), 0.0f, -1.0f, 1.0f);

            glEnable(GL_BLEND);
            texSh.UseShader();
            glm::mat4 hudPanelM =
                glm::translate(identity, glm::vec3(12.0f + hudTexture.width * 0.5f, 12.0f + hudTexture.height * 0.5f, 0.0f)) *
                glm::scale(identity, glm::vec3(hudTexture.width * 0.5f, hudTexture.height * 0.5f, 1.0f));
            glUniformMatrix4fv(uModel_t, 1, GL_FALSE, glm::value_ptr(hudPanelM));
            glUniformMatrix4fv(uView_t, 1, GL_FALSE, glm::value_ptr(identity));
            glUniformMatrix4fv(uProj_t, 1, GL_FALSE, glm::value_ptr(hudProj));
            glUniform1f(uAlpha_t, 1.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, hudTexture.texture);
            glUniform1i(uTexture_t, 0);
            glBindVertexArray(hudPanel.VAO);
            glDrawElements(GL_TRIANGLES, hudPanel.indexCount, GL_UNSIGNED_INT, 0);
            glBindVertexArray(0);
        }
        glEnable(GL_DEPTH_TEST);

        glUseProgram(0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_DEPTH_TEST);
        glfwSwapBuffers(w);
    }

    if (grassTex) glDeleteTextures(1, &grassTex);
    if (backdropTex) glDeleteTextures(1, &backdropTex);
    if (fishTex) glDeleteTextures(1, &fishTex);
    if (hudPanelTex) glDeleteTextures(1, &hudPanelTex);
    hudTexture.destroy();
    debugLines.destroy();
    instancedGrass.destroy();
    fishSchool.destroy();
    windTargetBox.destroy();
    hudPanel.destroy();
    ground.destroy();
    backdrop.destroy();
    return 0;
}
