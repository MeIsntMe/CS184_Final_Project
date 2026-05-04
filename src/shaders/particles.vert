#version 330 core

// Particle position from the OpenGL vertex buffer.
// Your C++ sends particles as x, y, z.
layout (location = 0) in vec3 aPos;

// Data sent to the fragment shader.
out float vDepth;

uniform vec3  camOffset;
uniform float focal;
uniform float camYaw;
uniform float camPitch;

vec3 rotY(vec3 p, float a) {
    float c = cos(a), s = sin(a);
    return vec3(c*p.x + s*p.z, p.y, -s*p.x + c*p.z);
}
vec3 rotX(vec3 p, float a) {
    float c = cos(a), s = sin(a);
    return vec3(p.x, c*p.y - s*p.z, s*p.y + c*p.z);
}

void main() {
    // Your current camera setup looks down the negative Z axis.
    // Particles start at z = -2, so this gives them a positive depth of 2.
    vec3 vp = rotX(rotY(aPos, camYaw), camPitch) + camOffset;
    float z = -vp.z;

    // If a particle is behind the camera or too close, move it off-screen.
    if (z <= 0.01) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        gl_PointSize = 1.0;
        vDepth = 999.0;
        return;
    }

    // Simple perspective projection.
    // Higher focal value means more zoom.
    vec2 projected = (vp.xy / z) * focal;

    gl_Position = vec4(projected, 0.0, 1.0);

    // Makes closer particles larger.
    gl_PointSize = 18.0 / z;

    // Used by the fragment shader to calculate fog strength.
    vDepth = z;
}