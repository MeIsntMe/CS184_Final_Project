#include "shader.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath) {
    // Read shader source code from files.
    std::string vertexSource = readFile(vertexPath);
    std::string fragmentSource = readFile(fragmentPath);

    // Compile each shader stage.
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    // Link both compiled shaders into one shader program.
    id = glCreateProgram();
    glAttachShader(id, vertexShader);
    glAttachShader(id, fragmentShader);
    glLinkProgram(id);

    // Check for linking errors.
    GLint success = 0;
    glGetProgramiv(id, GL_LINK_STATUS, &success);

    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(id, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> log(logLength);
        glGetProgramInfoLog(id, logLength, nullptr, log.data());

        std::cerr << "Shader program linking failed:\n";
        std::cerr << log.data() << "\n";
    }

    // The individual shader objects are no longer needed after linking.
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

Shader::Shader(const std::string& computePath) {
  // Read compute shader source code from file
  std::string computeSource = readFile(computePath);

  // Compile the compute shader
  // (Ensure your GLAD is generated for OpenGL 4.3+ so GL_COMPUTE_SHADER is defined)
  GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeSource);

  // Link into a shader program
  id = glCreateProgram();
  glAttachShader(id, computeShader);
  glLinkProgram(id);

  // Check for linking errors
  GLint success = 0;
  glGetProgramiv(id, GL_LINK_STATUS, &success);

  if (!success) {
    GLint logLength = 0;
    glGetProgramiv(id, GL_INFO_LOG_LENGTH, &logLength);

    std::vector<char> log(logLength);
    glGetProgramInfoLog(id, logLength, nullptr, log.data());

    std::cerr << "Compute Shader program linking failed:\n";
    std::cerr << log.data() << "\n";
  }

  // The individual compute shader object is no longer needed after linking
  glDeleteShader(computeShader);
}

Shader::~Shader() {
    if (id != 0) {
        glDeleteProgram(id);
    }
}

void Shader::use() const {
    glUseProgram(id);
}

void Shader::setFloat(const std::string& name, float value) const {
    GLint location = glGetUniformLocation(id, name.c_str());
    glUniform1f(location, value);
}

void Shader::setVec3(const std::string& name, float x, float y, float z) const {
    GLint location = glGetUniformLocation(id, name.c_str());
    glUniform3f(location, x, y, z);
}

void Shader::setInt(const std::string& name, int value) const {
  GLint location = glGetUniformLocation(id, name.c_str());
  glUniform1i(location, value);
}

std::string Shader::readFile(const std::string& path) {
    std::ifstream file(path);

    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << path << "\n";
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

GLuint Shader::compileShader(GLenum type, const std::string& source) {
    GLuint shader = glCreateShader(type);

    const char* sourcePtr = source.c_str();
    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);

    // Check for compilation errors.
    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> log(logLength);
        glGetShaderInfoLog(shader, logLength, nullptr, log.data());

        std::cerr << "Shader compilation failed:\n";
        std::cerr << log.data() << "\n";
    }

    return shader;
}