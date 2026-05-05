#include <glad/glad.h>
#include <GLFW/glfw3.h>

#ifdef USE_CUDA
#include "sim/particle.h"
#else
#include "sim/particle_cpu.h"
#endif

#include "sim/grid.h"
#include "renderer/shader.h"
#include "renderer/dome.h"
#include "renderer/camera.h"

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

static void frame_step(float& prev_time, ParticleSystem &partSys) {
    float new_time = static_cast<float>(glfwGetTime());
    float dt = new_time - prev_time;
    prev_time = new_time;

   // std::cout << dt << "\n";
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

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::cout << "OpenGL Vendor:   " << glGetString(GL_VENDOR) << "\n";
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "OpenGL Version:  " << glGetString(GL_VERSION) << "\n";

    Shader particleShader(
        "src/shaders/particles.vert",
        "src/shaders/particles.frag"
    );


    //dome stuff
    Dome dome;
    dome.init("textures/sky.jpg");  // path to your texture
    if (dome.texture == 0)
        std::cout << "TEXTURE FAILED TO LOAD\n";
    else
        std::cout << "Texture loaded OK, id = " << dome.texture << "\n";

    FILE* f = fopen("textures/sky.jpg", "rb");
    std::cout << "sky.jpg found: " << (f ? "YES" : "NO") << "\n";
    if (f) fclose(f);

    Mat4 proj = perspective(
        3.14159f / 3.f,   // 60 degree FOV
        900.f / 700.f,    // window aspect ratio
        0.01f, 200.f      // near/far
    );
    Mat4 view = identity();   // static camera for now

    // Combine into VP matrix
    Mat4 vp{};
    // simple multiply proj * view (identity so vp = proj for now)
    vp = proj;
    //end dome stuff

    ParticleSystem particleSys;
    particleSys.initialise_particles(1000);

    float domain_size = 2.0f;
    int   grid_res = 32;
    float dx = domain_size / grid_res;
    //MACGrid grid(grid_res, grid_res, grid_res, dx);

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

        // 1. Clear
        glClearColor(0.45f, 0.55f, 0.70f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 2. Draw dome (background)
        glUseProgram(dome.shader);
        glUniformMatrix4fv(glGetUniformLocation(dome.shader, "uVP"),
            1, GL_FALSE, vp.data());
        glUniform1f(glGetUniformLocation(dome.shader, "uBrightness"), 1.0f);
        dome.draw();

        // 3. Upload particle positions
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

        // 4. Draw particles on top
        particleShader.use();
        particleShader.setVec3("particleColor", 1.0f, 1.0f, 1.0f);
        particleShader.setVec3("fogColor", 0.45f, 0.55f, 0.70f);
        particleShader.setFloat("fogDensity", 0.35f);
        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, particleSys.count);

        // 5. Swap
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);    
    dome.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}