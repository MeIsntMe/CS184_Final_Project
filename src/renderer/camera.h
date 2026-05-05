#pragma once
#include <cmath>
#include <array>

using Mat4 = std::array<float, 16>;

inline Mat4 perspective(float fov, float aspect, float zNear, float zFar) {
    float f = 1.0f / std::tan(fov * 0.5f);
    Mat4 m{};
    m[0] = f / aspect;
    m[5] = f;
    m[10] = (zFar + zNear) / (zNear - zFar);
    m[11] = -1.f;
    m[14] = (2.f * zFar * zNear) / (zNear - zFar);
    return m;
}

// Identity � no camera movement yet, dome is always centred
inline Mat4 identity() {
    Mat4 m{};
    m[0] = m[5] = m[10] = m[15] = 1.f;
    return m;
}

// Column-major 4x4 multiply: c = a * b
inline Mat4 matMul(const Mat4& a, const Mat4& b) {
    Mat4 c{};
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            for (int k = 0; k < 4; k++)
                c[col*4 + row] += a[k*4 + row] * b[col*4 + k];
    return c;
}

// Rotation-only view matrix from yaw (Y-axis) + pitch (X-axis).
// No translation so the dome always surrounds the camera.
inline Mat4 domeView(float yaw, float pitch) {
    float cy = std::cos(yaw),   sy = std::sin(yaw);
    float cp = std::cos(pitch), sp = std::sin(pitch);
    Mat4 m{};
    m[0] = cy;     m[1] = sp*sy;  m[2] = -cp*sy; m[3] = 0;
    m[4] = 0;      m[5] = cp;     m[6] = sp;     m[7] = 0;
    m[8] = sy;     m[9] = -sp*cy; m[10] = cp*cy; m[11] = 0;
    m[12] = 0;     m[13] = 0;     m[14] = 0;     m[15] = 1;
    return m;
}