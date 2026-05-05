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
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <ctime>
#include <cmath>
#include <sstream>
#include <charconv>


// Camera state � must be global so GLFW callbacks can reach them.
static float g_cam_x    = 1.2f;
static float g_cam_y    = 0.0f;
static float g_cam_z    = -3.5f;
static float g_focal    = 4.0f;
static float g_cam_yaw  = 0.785f;
static float g_cam_pitch = 0.35f;

// Drag state.
static bool   g_dragging     = false;
static double g_last_mouse_x = 0.0;
static double g_last_mouse_y = 0.0;

// Simulation control.
static bool  g_paused            = false;
static bool  g_restart_requested = false;
static int   g_preset            = 1;
static float g_time_scale        = 0.5f;

// Raymarching variables
static float densityMultiplier = 0.09;

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    glViewport(0, 0, width, height);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            g_dragging = true;
            glfwGetCursorPos(window, &g_last_mouse_x, &g_last_mouse_y);
        } else {
            g_dragging = false;
        }
    }
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
    (void)window;
    if (!g_dragging) return;
    double dx = xpos - g_last_mouse_x;
    double dy = ypos - g_last_mouse_y;
    g_last_mouse_x = xpos;
    g_last_mouse_y = ypos;

    const float sensitivity = 0.005f;
    g_cam_yaw   += (float)dx * sensitivity;
    g_cam_pitch += (float)dy * sensitivity;

    // Prevent flipping past straight up/down.
    if (g_cam_pitch < -1.52f) g_cam_pitch = -1.52f;
    if (g_cam_pitch >  1.52f) g_cam_pitch =  1.52f;
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    (void)window; (void)xoffset;
    g_focal += (float)yoffset * 0.3f;
    if (g_focal < 0.5f)  g_focal = 0.5f;
    if (g_focal > 20.0f) g_focal = 20.0f;
}

// dt: time since last frame, used for frame-rate-independent movement.
static void process_input(GLFWwindow* window, float dt) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) densityMultiplier += 0.05;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) densityMultiplier -= 0.05;
    const float speed = 2.0f;
    // W/S move forward/backward along the camera's look axis.
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) g_cam_z += speed * dt;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) g_cam_z -= speed * dt;
    // A/D strafe left/right.
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) g_cam_x += speed * dt;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) g_cam_x -= speed * dt;
    // Space/Shift move up/down in world.
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) g_cam_y -= speed * dt;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)  == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS) g_cam_y += speed * dt;

    // P toggles pause; R requests a restart. Both fire once per press.
    static bool p_was_pressed = false;
    static bool r_was_pressed = false;
    bool p_now = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    bool r_now = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if (p_now && !p_was_pressed) g_paused = !g_paused;
    if (r_now && !r_was_pressed) g_restart_requested = true;
    p_was_pressed = p_now;
    r_was_pressed = r_now;

    // Keys 1–9 load presets (fire once per press).
    static bool preset_was_pressed[9] = {};
    const int preset_keys[9] = {
        GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4, GLFW_KEY_5,
        GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9
    };
    for (int i = 0; i < 9; i++) {
        bool now = glfwGetKey(window, preset_keys[i]) == GLFW_PRESS;
        if (now && !preset_was_pressed[i]) {
            g_preset = i + 1;
            g_restart_requested = true;
        }
        preset_was_pressed[i] = now;
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

static float frame_step(float& prev_time, ParticleSystem &partSys, MACGrid &grid, bool paused) {
    float new_time = static_cast<float>(glfwGetTime());
    float dt = new_time - prev_time;
    prev_time = new_time;

    if (!paused)
        partSys.step(dt * g_time_scale, grid);

    return dt;
}

int main(int argc, char* argv[]) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return -1;
    }
    
    bool benchmark = false;
    int num_particles = 1000000;
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

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
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
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetCursorPosCallback(window, cursor_pos_callback);

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
    std::string domePath = std::string(SHADER_DIR) + "/textures/sky.jpg";
    std::cout << domePath << "\n";
    dome.init(domePath);  // path to your texture
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

    Shader densityComp("src/shaders/density.comp");

    Shader domainShader(
      "src/shaders/domain.vert",
      "src/shaders/domain_optics.frag"
    );

    ParticleSystem particleSys;
    particleSys.initialise_particles(num_particles, g_preset);

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

    // setting up compute shader
    GLuint densityTexture;
    glGenTextures(1, &densityTexture);
    glBindTexture(GL_TEXTURE_3D, densityTexture);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, grid_res, grid_res, grid_res, 0, GL_RED, GL_FLOAT, nullptr);
    glBindTexture(GL_TEXTURE_3D, 0);

    float borderColor[] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // for rendering the domain
    GLuint emptyVAO;
    glGenVertexArrays(1, &emptyVAO);

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
    float prev_dt = 1.0f / 60.0f;

    while (!glfwWindowShouldClose(window)) {
        process_input(window, prev_dt);

        if (g_restart_requested) {
            particleSys.initialise_particles(num_particles, g_preset);
            g_restart_requested = false;
            g_paused = false;
        }

        float dt = frame_step(prev_time, particleSys, grid, g_paused);
        prev_dt = dt;

        if (benchmark && (prev_time - start_time > benchmark_time)) {
          glfwSetWindowShouldClose(window, true);
        }

        pos_buffer.clear();
        for (int i = 0; i < particleSys.active_count; i++) {
            pos_buffer.push_back(particleSys.h_x[i]);
            pos_buffer.push_back(particleSys.h_y[i]);
            pos_buffer.push_back(particleSys.h_z[i]);
        }
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0,
            pos_buffer.size() * sizeof(float),
            pos_buffer.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glClearColor(0.03f, 0.03f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Sky dome — drawn first as background, no depth write
        {
            Mat4 proj = perspective(1.05f, 900.f / 700.f, 0.01f, 200.f); // ~60 deg FOV
            Mat4 view = domeView(g_cam_yaw, g_cam_pitch);
            Mat4 vp   = matMul(proj, view);
            glUseProgram(dome.shader);
            glUniformMatrix4fv(glGetUniformLocation(dome.shader, "uVP"), 1, GL_FALSE, vp.data());
            glUniform1f(glGetUniformLocation(dome.shader, "uBrightness"), 1.0f);
            dome.draw();
        }

        particleShader.use();

        particleShader.setVec3("camOffset", g_cam_x, g_cam_y, g_cam_z);
        particleShader.setFloat("focal", g_focal);
        particleShader.setFloat("camYaw", g_cam_yaw);
        particleShader.setFloat("camPitch", g_cam_pitch);

        // Normal color of the particles.
        particleShader.setVec3("particleColor", 1.0f, 1.0f, 1.0f);
        particleShader.setVec3("fogColor", 0.45f, 0.55f, 0.70f);
        particleShader.setFloat("fogDensity", 0.35f);
        glBindVertexArray(vao);
        //glDrawArrays(GL_POINTS, 0, particleSys.count);


        float clearColor = 0.0f;
        glClearTexImage(densityTexture, 0, GL_RED, GL_FLOAT, &clearColor);
        densityComp.use();

        densityComp.setVec3("domainSize", 2.0f, 2.0f, 2.0f);
        densityComp.setVec3("domainCenter", 0.0f, 0.0f, 0.0f);
        densityComp.setInt("gridRes", grid_res);
        densityComp.setInt("numParticles", particleSys.active_count);

        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, vbo);
        glBindImageTexture(0, densityTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32F);

        glDispatchCompute((particleSys.active_count + 255) / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

        domainShader.use();

        
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        domainShader.setVec3("camOffset", g_cam_x, g_cam_y, g_cam_z);
        domainShader.setFloat("focal", g_focal);
        domainShader.setFloat("camYaw", g_cam_yaw);
        domainShader.setFloat("camPitch", g_cam_pitch);
        domainShader.setVec3("domainSize", 2.0f, 2.0f, 2.0f);
        domainShader.setVec3("domainCenter", 0.0f, 0.0f, 0.0f);
        domainShader.setFloat("densityMultiplier", densityMultiplier);
        domainShader.setVec3("scatteringCoefficients", 4.0f, 2.8f, 1.5f);
        domainShader.setVec3("lightPos", 0.0f, 1.0f, 0.0f);
        // debugging
        //std::cout << "densityMultiplier = " << densityMultiplier << "\n";

        domainShader.setVec3("lightDir", 0.0, 1.0, 0.0);
        domainShader.setVec3("lightColor", 1.0, 0.9, 0.8);
        domainShader.setVec3("sphereCenter", particleSys.sphere.cx, particleSys.sphere.cy, particleSys.sphere.cz);
        domainShader.setFloat("sphereRadius", particleSys.sphere.radius);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_3D, densityTexture);
        domainShader.setInt("densityTexture", 0);

        glBindVertexArray(emptyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);


        // 5. Swap
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
    dome.cleanup();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
