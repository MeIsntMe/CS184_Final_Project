#version 330 core

const vec3 quad[6] = vec3[6](
    vec3(-1.0, -1.0, -1.0), vec3( 1.0, -1.0, -1.0), vec3( 1.0, -1.0,  1.0),
    vec3(-1.0, -1.0, -1.0), vec3( 1.0, -1.0,  1.0), vec3(-1.0, -1.0,  1.0)
);

uniform vec3  camOffset;
uniform float focal;
uniform float camYaw;
uniform float camPitch;

vec3 rotY(vec3 p, float a) { float c=cos(a),s=sin(a); return vec3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z); }
vec3 rotX(vec3 p, float a) { float c=cos(a),s=sin(a); return vec3(p.x,c*p.y-s*p.z,s*p.y+c*p.z); }

void main() {
    vec3 vp = rotX(rotY(quad[gl_VertexID], camYaw), camPitch) + camOffset;
    float z = -vp.z;
    if (z <= 0.01) { gl_Position = vec4(0.0, 0.0, 2.0, 1.0); return; }
    // Use depth 0.5 so the domain (depth 0) always draws in front via depth test
    gl_Position = vec4(vp.x * focal, vp.y * focal, 0.5 * z, z);
}
