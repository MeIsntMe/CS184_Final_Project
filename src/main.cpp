#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "sim/particle.h"
#include "sim/grid.h"
#include "sim/p2g.h"

#include <iostream>
#include <vector>

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

static void process_input(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);

        std::vector<char> log(log_length);
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());

        std::cerr << "Shader compilation failed:\n" << log.data() << "\n";
    }

    return shader;
}

static GLuint create_shader_program() {
    const char* vertex_shader_source = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;

        void main() {
            float z = -aPos.z;

            if (z <= 0.01) {
                gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
                gl_PointSize = 1.0;
                return;
            }

            float focal = 1.2;
            vec2 projected = (aPos.xy / z) * focal;

            gl_Position = vec4(projected, 0.0, 1.0);
            gl_PointSize = 18.0 / z;
        }
    )";

    const char* fragment_shader_source = R"(
        #version 330 core
        out vec4 FragColor;

        void main() {
            FragColor = vec4(1.0, 1.0, 1.0, 1.0);
        }
    )";

    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);

        std::vector<char> log(log_length);
        glGetProgramInfoLog(program, log_length, nullptr, log.data());

        std::cerr << "Program linking failed:\n" << log.data() << "\n";
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}

static void frame_step(float& prev_time, ParticleSystem &partSys) {
    float new_time = static_cast<float>(glfwGetTime());
    float dt = new_time - prev_time;
    prev_time = new_time;

    std::cout << dt << "\n";
    partSys.step(dt);

    return;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(900, 700, "Stream Processor - 3D Test", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << "\n";
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << "\n";

    GLuint shader_program = create_shader_program();

    ParticleSystem particleSys;
    particleSys.initialise_particles(1000);

    float domain_size = 2.0f;
    int   grid_res = 32;
    float dx = domain_size / grid_res;
    MACGrid grid(grid_res, grid_res, grid_res, dx);

    // Build initial position buffer
    std::vector<float> pos_buffer;
    pos_buffer.reserve(particleSys.h_x.size() * 3);
    for (int i = 0; i < particleSys.count; i++) {
        pos_buffer.push_back(particleSys.h_x[i]);
        pos_buffer.push_back(particleSys.h_y[i]);
        pos_buffer.push_back(particleSys.h_z[i]);
    }

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
        pos_buffer.size() * sizeof(float),
        pos_buffer.data(),
        GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE,
        3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glEnable(GL_PROGRAM_POINT_SIZE);

    float prev_time = static_cast<float>(glfwGetTime());

    while (!glfwWindowShouldClose(window)) {
        process_input(window);

        frame_step(prev_time, particleSys);
       
        //p2g_transfer(grid, particleSys);


        //debug block, remove after working
        float min_x = 999, max_x = -999;
        float min_y = 999, max_y = -999;
        for (int i = 0; i < particleSys.count; i++) {
            min_x = min(min_x, particleSys.h_x[i]);
            max_x = max(max_x, particleSys.h_x[i]);
            min_y = min(min_y, particleSys.h_y[i]);
            max_y = max(max_y, particleSys.h_y[i]);
        }
        std::cout << "Particle x range: " << min_x << " to " << max_x << "\n";
        std::cout << "Particle y range: " << min_y << " to " << max_y << "\n";
        std::cout << "dx = " << grid.dx << "\n";
        std::cout << "Grid size: " << grid.nx << "x" << grid.ny << "x" << grid.nz << "\n";

        //// Check all vel_v not just cell-centred array
        //int nonzero = 0;
        //for (int i = 0; i < (int)grid.vel_v.size(); i++)
        //    if (grid.vel_v[i] != 0.f) nonzero++;
        //std::cout << "Non-zero vel_v count: " << nonzero << "\n";

        //// Check vel_u too
        //nonzero = 0;
        //for (int i = 0; i < (int)grid.vel_u.size(); i++)
        //    if (grid.vel_u[i] != 0.f) nonzero++;
        //std::cout << "Non-zero vel_u count: " << nonzero << "\n";
        ////end of debug block

        pos_buffer.clear();
        for (int i = 0; i < particleSys.count; i++) {
            pos_buffer.push_back(particleSys.h_x[i]);
            pos_buffer.push_back(particleSys.h_y[i]);
            pos_buffer.push_back(particleSys.h_z[i]);
        }
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
            pos_buffer.size() * sizeof(float),
            pos_buffer.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader_program);
        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, particleSys.count);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shader_program);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}