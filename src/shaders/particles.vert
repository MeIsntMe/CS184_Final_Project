#version 330 core

// Particle position from the OpenGL vertex buffer.
// Your C++ sends particles as x, y, z.
layout (location = 0) in vec3 aPos;

// Data sent to the fragment shader.
out float vDepth;

void main() {
    // Your current camera setup looks down the negative Z axis.
    // Particles start at z = -2, so this gives them a positive depth of 2.
    float z = -aPos.z;

    // If a particle is behind the camera or too close, move it off-screen.
    if (z <= 0.01) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        gl_PointSize = 1.0;
        vDepth = 999.0;
        return;
    }

    // Simple perspective projection.
    // Higher focal value means more zoom.
    float focal = 1.2;
    vec2 projected = (aPos.xy / z) * focal;

    gl_Position = vec4(projected, 0.0, 1.0);

    // Makes closer particles larger.
    gl_PointSize = 18.0 / z;

    // Used by the fragment shader to calculate fog strength.
    vDepth = z;
}