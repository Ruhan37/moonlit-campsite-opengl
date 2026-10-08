cmake_minimum_required(VERSION 3.10)
project(MoonlitForestCampsite CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find OpenGL and GLUT (macOS framework, or Linux/Windows packages)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

# Include directories
include_directories(${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIR})

# Source files
set(SOURCES
    main.cpp
    Particles.cpp
    Primitives.cpp
    Scene.cpp
    Shading.cpp
    RayTracer.cpp
)

# Create the executable
add_executable(campsite ${SOURCES})

# Link libraries
find_package(Threads REQUIRED)
target_link_libraries(campsite ${OPENGL_LIBRARIES} ${GLUT_LIBRARY} Threads::Threads)
