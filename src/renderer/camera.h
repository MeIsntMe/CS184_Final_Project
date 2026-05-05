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

// Identity — no camera movement yet, dome is always centred
inline Mat4 identity() {
    Mat4 m{};
    m[0] = m[5] = m[10] = m[15] = 1.f;
    return m;
}