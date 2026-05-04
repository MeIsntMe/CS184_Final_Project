#version 330 core

const vec3 corners[8] = vec3[8](
    vec3(-0.5, -0.5, -0.5), vec3( 0.5, -0.5, -0.5),
    vec3( 0.5,  0.5, -0.5), vec3(-0.5,  0.5, -0.5),
    vec3(-0.5, -0.5,  0.5), vec3( 0.5, -0.5,  0.5),
    vec3( 0.5,  0.5,  0.5), vec3(-0.5,  0.5,  0.5)
);

const int indices[36] = int[36](
    // Front face (-Z)
    0, 3, 2, 2, 1, 0, 
    // Back face (+Z)
    4, 5, 6, 6, 7, 4, 
    // Top face (+Y)
    3, 7, 6, 6, 2, 3, 
    // Bottom face (-Y)
    0, 1, 5, 5, 4, 0, 
    // Right face (+X)
    1, 2, 6, 6, 5, 1, 
    // Left face (-X)
    0, 4, 7, 7, 3, 0  
);

uniform vec3  camOffset;
uniform float focal;
uniform float camYaw;
uniform float camPitch;
uniform vec3 domainSize; 
uniform vec3 domainCenter; 

out vec3 vWorldPos;
out vec3 vCamWorldPos;

vec3 rotY(vec3 p, float a) {
    float c = cos(a), s = sin(a);
    return vec3(c*p.x + s*p.z, p.y, -s*p.x + c*p.z);
}
vec3 rotX(vec3 p, float a) {
    float c = cos(a), s = sin(a);
    return vec3(p.x, c*p.y - s*p.z, s*p.y + c*p.z);
}

void main() {
    vec3 basePos = corners[indices[gl_VertexID]];
    vWorldPos = (basePos * domainSize) + domainCenter;

    // True Camera World Position
    vec3 cam = -camOffset;
    cam = rotX(cam, -camPitch);
    vCamWorldPos = rotY(cam, -camYaw);

    vec3 vp = rotX(rotY(vWorldPos, camYaw), camPitch) + camOffset;
    float z = -vp.z;

    if (z <= 0.01) {
        gl_Position = vec4(0.0, 0.0, 2.0, 1.0);
        return;
    }
    gl_Position = vec4(vp.x * focal, vp.y * focal, 0.0, z);
}