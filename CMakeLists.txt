# --- 1. Language Setup ---
cmake_minimum_required(VERSION 3.18)

option(CPU_ONLY "Build without CUDA" OFF)

if(CPU_ONLY)
    project(StreamProcessor LANGUAGES C CXX)
    message(STATUS "CPU-only build — CUDA disabled")
else()
    project(StreamProcessor LANGUAGES C CXX CUDA)
    set(CMAKE_CUDA_STANDARD 17)
    set(CMAKE_CUDA_ARCHITECTURES 75)
endif()

set(CMAKE_CXX_STANDARD 17)

# --- 2. Find Packages ---
find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

if(NOT CPU_ONLY)
    find_package(Thrust REQUIRED CONFIG)
    find_package(CUDAToolkit REQUIRED)
    add_definitions(-DUSE_CUDA)
endif()

# --- 3. Source Files ---
set(SRC_FILES
    src/main.cpp
    src/sim/grid.cpp
    src/renderer/shader.cpp
    external/stb/stb_image.cpp
)

if(NOT CPU_ONLY)
    list(APPEND SRC_FILES
        src/sim/particle.cu
        src/sim/particle.h
        src/sim/p2g.cuh
        src/sim/pressure_solver.cu
        src/sim/grid.cu
        src/sim/particle.cu
        src/sim/grid.cu
        src/sim/pressure_solver.cu
        src/sim/particle.h
        src/sim/p2g.cuh
        src/renderer/shader.cpp
        external/stb/stb_image.cpp
        src/shaders/particles.vert
        src/shaders/particles.frag
        src/shaders/density.comp
        src/shaders/domain.frag
        src/shaders/domain.vert
    )
endif()

# --- 4. Targets ---
add_library(glad external/glad/src/glad.c)
target_include_directories(glad PUBLIC external/glad/include)

add_executable(stream_processor ${SRC_FILES})

# --- 5. Modern Linking & Properties ---
target_include_directories(stream_processor PRIVATE src external/glad/include external/stb ${CUDAToolkit_INCLUDE_DIRS})
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
# --- 5. Linking & Properties ---
target_include_directories(stream_processor PRIVATE
    src
    external
    external/stb
    external/glad/include
)

if(CPU_ONLY)
    target_link_libraries(stream_processor PRIVATE
        OpenGL::GL
        glfw
        glad
    )
else()
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
        target_include_directories(stream_processor PUBLIC
            ${CMAKE_CUDA_TOOLKIT_INCLUDE_DIRECTORIES})
    endif()

    # This passes the absolute path of your source shaders folder directly into your C++ code
    target_compile_definitions(stream_processor PRIVATE 
        SHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
endif()