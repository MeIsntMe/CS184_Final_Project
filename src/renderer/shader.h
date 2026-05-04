#pragma once

#include <string>
#include <glad/glad.h>

// Small helper class for loading, compiling, linking, and using GLSL shaders.
class Shader {
public:
    // OpenGL shader program ID.
    GLuint id = 0;

    Shader() = default;

    // Loads a vertex shader file and a fragment shader file.
    Shader(const std::string& vertexPath, const std::string& fragmentPath);

    // Deletes the OpenGL shader program.
    ~Shader();

    // Makes this shader program active.
    void use() const;

    // Uniform helpers.
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, float x, float y, float z) const;

private:
    static std::string readFile(const std::string& path);
    static GLuint compileShader(GLenum type, const std::string& source);
};