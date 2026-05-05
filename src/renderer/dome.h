#pragma once
#include <glad/glad.h>
#include <vector>
#include <string>
#include <cmath>
#include <iostream>
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Dome {
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLuint texture = 0;
    GLuint shader = 0;
    int    index_count = 0;

    void init(const std::string& texture_path) {
        build_sphere(64, 32);
        load_texture(texture_path);
        build_shader();
    }

    void draw() {
        glDepthMask(GL_FALSE);         // don't write to depth buffer
        glUseProgram(shader);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);          // restore depth writing
    }

    void cleanup() {
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteBuffers(1, &ebo);
        glDeleteTextures(1, &texture);
        glDeleteProgram(shader);
    }

private:
    void build_sphere(int stacks, int slices) {
        std::vector<float> verts;
        std::vector<unsigned int> indices;

        for (int i = 0; i <= stacks; i++) {
            float phi = M_PI * i / stacks;          // 0 → π
            for (int j = 0; j <= slices; j++) {
                float theta = 2.0f * M_PI * j / slices;  // 0 → 2π

                float x = std::sin(phi) * std::cos(theta);
                float y = std::cos(phi);
                float z = std::sin(phi) * std::sin(theta);

                // Position
                verts.push_back(x * 50.f);  // large radius
                verts.push_back(y * 50.f);
                verts.push_back(z * 50.f);

                // UV — equirectangular mapping
                verts.push_back((float)j / slices);
                verts.push_back((float)i / stacks);
            }
        }

        for (int i = 0; i < stacks; i++) {
            for (int j = 0; j < slices; j++) {
                int a = i * (slices + 1) + j;
                int b = a + slices + 1;
                indices.push_back(a);
                indices.push_back(b);
                indices.push_back(a + 1);
                indices.push_back(b);
                indices.push_back(b + 1);
                indices.push_back(a + 1);
            }
        }

        index_count = (int)indices.size();

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);

        glBindVertexArray(vao);

        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER,
            verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
            indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Position attribute (location 0)
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
            5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // UV attribute (location 1)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE,
            5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void load_texture(const std::string& path) {
        // include stb_image.h at top of dome.h or in a shared header
        extern unsigned char* stbi_load(const char*, int*, int*, int*, int);
        extern void stbi_image_free(void*);

        int w, h, channels;
        unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 3);
        if (!data) {
            std::cerr << "Failed to load dome texture: " << path << "\n";
            return;
        }

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h,
            0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }

    void build_shader() {
        const char* vert = R"(
            #version 330 core
            layout(location = 0) in vec3 aPos;
            layout(location = 1) in vec2 aUV;

            out vec2 vUV;

            uniform mat4 uVP;   // view-projection matrix

            void main() {
                vUV = aUV;
                gl_Position = uVP * vec4(aPos, 1.0);
            }
        )";

        const char* frag = R"(
            #version 330 core
            in vec2 vUV;
            out vec4 FragColor;

            uniform sampler2D uTex;
            uniform float uBrightness;  // tweak env brightness

            void main() {
                vec3 col = texture(uTex, vUV).rgb;
                FragColor = vec4(col * uBrightness, 1.0);
            }
        )";

        auto compile = [](GLenum type, const char* src) {
            GLuint s = glCreateShader(type);
            glShaderSource(s, 1, &src, nullptr);
            glCompileShader(s);
            return s;
            };

        GLuint v = compile(GL_VERTEX_SHADER, vert);
        GLuint f = compile(GL_FRAGMENT_SHADER, frag);
        shader = glCreateProgram();
        glAttachShader(shader, v);
        glAttachShader(shader, f);
        glLinkProgram(shader);
        glDeleteShader(v);
        glDeleteShader(f);
    }
};