#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "sim/particle.h"
#include "sim/grid.h"
#include "renderer/shader.h"

#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <ctime>
#include <cmath>
#include <sstream>
#include <charconv>


static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

static void process_input(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
}



void save_log(const std::string& folder, const std::string& filename, float num_particles, float total_time, float num_frames) {
  if (!std::filesystem::exists(folder)) {
    std::filesystem::create_directories(folder);
  }

  std::string fullPath = folder + "/" + filename;

  std::ofstream logFile(fullPath);
  float frame_time = total_time / num_frames;
  float frame_time_out = std::round(frame_time * 10000) / 10000.0;
  float fps = std::round(1/frame_time * 100) / 100.0;
  if (logFile.is_open()) {
    logFile << "Particle Number: " << num_particles << "\n";
    logFile << "Time Elapsed: " << total_time << "\t" << "Total Frames: " << num_frames << "\n";
    logFile << "Average Framerate: " << fps << "\t" << "Average Frame Time (ms): " << frame_time_out << "\n";
    logFile.close();
  }
}

static float frame_step(float& prev_time, ParticleSystem &partSys, MACGrid &grid) {
    float new_time = static_cast<float>(glfwGetTime());
    float dt = new_time - prev_time;
    prev_time = new_time;

    partSys.step(dt, grid);

    return dt;
}

int main(int argc, char* argv[]) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    
    bool benchmark = false;
    int num_particles = 10000;
    float benchmark_time;
    std::string log_name;
    std::vector<std::string> args(argv + 1, argv + argc);

    for (int i = 0; i < args.size(); i++) {
      if (args[i] == "-b" || args[i] == "--benchmark") {
        if (i + 1 < args.size()) {
          const std::string& val = args[++i];
          int result;
          auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), result);

          if (ec == std::errc()) {
            if (result >= 0) {
              benchmark_time = result;
              benchmark = true;
            }
            else {
              std::cerr << "Error: Benchmark time cannot be negative." << std::endl;
              return 1;
            }
          }
          else if (ec == std::errc::invalid_argument) {
            std::cerr << "Error: '" << val << "' is not a number!" << std::endl;
            return 1;
          }
          else if (ec == std::errc::result_out_of_range) {
            std::cerr << "Error: '" << val << "' is too large!" << std::endl;
            return 1;
          }
        }
        else {
          std::cerr << "Error: -b requires an argument." << std::endl;
          return 1;
        }
      }
      else if (args[i] == "-n") {
        if (i + 1 < args.size()) {
          const std::string& val = args[++i];
          int result;
          auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), result);

          if (ec == std::errc()) {
            if (result >= 0) {
              num_particles = result;
            }
            else {
              std::cerr << "Error: Number of particles cannot be negative." << std::endl;
              return 1;
            }
          }
          else if (ec == std::errc::invalid_argument) {
            std::cerr << "Error: '" << val << "' is not a number!" << std::endl;
            return 1;
          }
          else if (ec == std::errc::result_out_of_range) {
            std::cerr << "Error: '" << val << "' is too large!" << std::endl;
            return 1;
          }
        }
        else {
          std::cerr << "Error: -n requires an argument." << std::endl;
          return 1;
        }
      }
      else if (args[i] == "-o" || args[i] == "--output") {
        // Check if there is a value after the flag
        if (i + 1 < args.size()) {
          log_name = args[++i]; // Increment i to skip the value
        }
        else {
          std::cerr << "Error: --output requires a filename." << std::endl;
          return 1;
        }
      }
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

    ParticleSystem particleSys;
    particleSys.initialise_particles(num_particles);

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

    float start_time = prev_time;
    float total_time = 0;
    int frames = 0;

    while (!glfwWindowShouldClose(window)) {
        process_input(window);

        float dt = frame_step(prev_time, particleSys, grid);

        if (benchmark && (prev_time - start_time > benchmark_time)) {
          glfwSetWindowShouldClose(window, true);
        }

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

        glClearColor(0.45f, 0.55f, 0.70f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        particleShader.use();

        // Normal color of the particles.
        particleShader.setVec3("particleColor", 1.0f, 1.0f, 1.0f);

        // Fog color.
        particleShader.setVec3("fogColor", 0.45f, 0.55f, 0.70f);

        // Fog strength.
        // Lower = clearer.
        // Higher = foggier.
        particleShader.setFloat("fogDensity", 0.35f);

        glBindVertexArray(vao);
        glDrawArrays(GL_POINTS, 0, particleSys.count);

        glfwSwapBuffers(window);
        glfwPollEvents();
        frames++;
    }
    float end_time = glfwGetTime();
    std::string folder = "logs";

    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);
    std::stringstream ss;
    ss << std::put_time(localTime, "%Y-%m-%d_%H-%M");

    std::string filename = ss.str() + "_"+log_name+".log";

    save_log(folder, filename, num_particles, end_time - start_time, frames);

    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);    

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
