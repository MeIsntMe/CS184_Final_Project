# --- 1. Language Setup ---
cmake_minimum_required(VERSION 3.18)
project(StreamProcessor LANGUAGES C CXX CUDA)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_ARCHITECTURES 86)  # Ampere — RTX 3060 Ti

# --- 2. Find Packages (Keep your active ones) ---
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(CUDAToolkit REQUIRED)
# Thrust is bundled with the CUDA Toolkit — no separate find_package needed

# --- 3. Source Files (Clean & include headers) ---
set(SRC_FILES 
    src/main.cpp 
    src/sim/particle.cu
    src/sim/grid.cu
    src/sim/pressure_solver.cu
    src/sim/particle.h
    src/sim/p2g.cuh
    src/renderer/shader.cpp
    src/shaders/particles.vert
    src/shaders/particles.frag
    src/shaders/density.comp
    src/shaders/domain.frag
    src/shaders/domain.vert
)

# --- 4. Targets ---
add_library(glad external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

add_executable(stream_processor ${SRC_FILES})

# --- 5. Modern Linking & Properties ---
target_include_directories(stream_processor PRIVATE src external/glad/include ${CUDAToolkit_INCLUDE_DIRS})
target_link_libraries(stream_processor PRIVATE
    OpenGL::GL
    glfw
    glad
    CUDA::cudart
)

set_target_properties(stream_processor PROPERTIES 
    CUDA_SEPARABLE_COMPILATION ON
)

if(MSVC)
    target_include_directories(stream_processor PUBLIC ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
    # CUDA 13.x CCCL headers require the conforming MSVC preprocessor
    target_compile_options(stream_processor PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor>
        $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/Zc:preprocessor>
    )
endif()

# This passes the absolute path of your source shaders folder directly into your C++ code
target_compile_definitions(stream_processor PRIVATE 
    SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)