# --- 1. Language Setup ---
cmake_minimum_required(VERSION 3.18)
project(StreamProcessor LANGUAGES C CXX CUDA)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_ARCHITECTURES 75)

# --- 2. Find Packages (Keep your active ones) ---
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Thrust REQUIRED CONFIG)
find_package(CUDAToolkit REQUIRED)

# --- 3. Source Files (Clean & include headers) ---
set(SRC_FILES 
    src/main.cpp 
    src/sim/particle.cu 
    src/sim/grid.cpp
    src/sim/particle.h
    src/sim/p2g.cuh
    src/renderer/shader.cpp
)

# --- 4. Targets ---
add_library(glad external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

add_executable(stream_processor ${SRC_FILES})

# --- 5. Modern Linking & Properties ---
target_include_directories(stream_processor PRIVATE src external/glad/include)
target_link_libraries(stream_processor PRIVATE 
    OpenGL::GL 
    glfw 
    glad 
    Thrust::Thrust 
    CUDA::cudart
)

set_target_properties(stream_processor PROPERTIES 
    CUDA_SEPARABLE_COMPILATION ON
)

if(MSVC)
    target_include_directories(stream_processor PUBLIC ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
endif()