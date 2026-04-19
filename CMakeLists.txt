cmake_minimum_required(VERSION 3.18)
project(StreamProcessor LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_DEBUG "Build with debug settings" OFF)

if(BUILD_DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
endif()

if("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
  set(BUILD_DEBUG ON CACHE BOOL "Build with debug settings" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

add_library(glad external/glad/src/glad.c)
set_target_properties(glad PROPERTIES LINKER_LANGUAGE C)
target_include_directories(glad PUBLIC external/glad/include)

set(SRC_FILES
    src/main.cpp
    src/sim/particle.cpp
)

add_executable(stream_processor ${SRC_FILES})

target_include_directories(stream_processor PRIVATE
    src
    external/glad/include
)

target_link_libraries(stream_processor PRIVATE
    OpenGL::GL
    glfw
    glad
)

set_target_properties(stream_processor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}
)


# cmake_minimum_required(VERSION 3.18)

# project(StreamProcessor LANGUAGES CXX CUDA)

# # ---- Language standards ----
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_STANDARD_REQUIRED ON)

# set(CMAKE_CUDA_STANDARD 17)
# set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# # ---- Find packages ----
# find_package(OpenGL REQUIRED)
# find_package(glfw3 REQUIRED)

# # ---- Source files ----
# set(SRC_FILES
#     src/main.cpp
#     src/app.cpp

#     src/renderer/renderer.cpp
#     src/renderer/shader.cpp

#     src/sim/particle_system.cu
#     src/sim/grid.cu
#     src/sim/pressure_solver.cu
# )

# # ---- Executable ----
# add_executable(stream_processor ${SRC_FILES})

# # ---- Include directories ----
# target_include_directories(stream_processor PRIVATE
#     src
#     src/renderer
#     src/sim
#     external
# )

# # ---- GLAD (if you installed it manually) ----
# add_library(glad external/glad/src/glad.c)
# target_include_directories(glad PUBLIC external/glad/include)

# # ---- Link libraries ----
# target_link_libraries(stream_processor PRIVATE
#     OpenGL::GL
#     glfw
#     glad
# )

# # ---- CUDA settings ----
# set_target_properties(stream_processor PROPERTIES
#     CUDA_SEPARABLE_COMPILATION ON
# )

# # Change this to match your GPU if needed
# set(CMAKE_CUDA_ARCHITECTURES 75)