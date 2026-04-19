#pragma once
#include "grid.h"
#include "particle.h"

// Linear interpolation weight
inline float weight(float x, float node) {
    float d = std::abs(x - node);
    return (d < 1.f) ? (1.f - d) : 0.f;
}

inline void p2g_transfer(MACGrid& grid, ParticleSystem& psys) {
    grid.clear();

    float dx = grid.dx;

    for (auto& p : psys.particles) {
        // Convert world position to grid coordinates
        float gx = (p.x + 1.0f) / dx;
        float gy = (p.y + 1.0f) / dx;
        float gz = (p.z + 1.0f) / dx;

      
        int i = (int)gx;
        int j = (int)gy;
        int k = (int)gz;

        // Splat onto u-faces (x velocity component)
        for (int di = 0; di <= 1; di++)
            for (int dj = 0; dj <= 1; dj++)
                for (int dk = 0; dk <= 1; dk++) {
                    int ni = i + di;
                    int nj = j + dj;
                    int nk = k + dk;

                    if (ni < 0 || ni > grid.nx) continue;
                    if (nj < 0 || nj >= grid.ny) continue;
                    if (nk < 0 || nk >= grid.nz) continue;

                    float wx = weight(gx, ni);         
                    float wy = weight(gy, nj + 0.5f);  
                    float wz = weight(gz, nk + 0.5f);  
                    float w = wx * wy * wz;

                    int idx = ni + (grid.nx + 1) * (nj + grid.ny * nk);
                    grid.vel_u[idx] += w * p.vx;
                    grid.weight_u[idx] += w;
                }

        // Splat onto v-faces (y velocity component)
        for (int di = 0; di <= 1; di++)
            for (int dj = 0; dj <= 1; dj++)
                for (int dk = 0; dk <= 1; dk++) {
                    int ni = i + di;
                    int nj = j + dj;
                    int nk = k + dk;

                    if (ni < 0 || ni >= grid.nx) continue;
                    if (nj < 0 || nj > grid.ny) continue;
                    if (nk < 0 || nk >= grid.nz) continue;

                    float wx = weight(gx, ni + 0.5f);
                    float wy = weight(gy, nj);         
                    float wz = weight(gz, nk + 0.5f);
                    float w = wx * wy * wz;

                    int idx = ni + grid.nx * (nj + (grid.ny + 1) * nk);
                    grid.vel_v[idx] += w * p.vy;
                    grid.weight_v[idx] += w;
                }
    }

    // Normalise — divide accumulated velocity by total weight
    for (int i = 0; i < (int)grid.vel_u.size(); i++)
        if (grid.weight_u[i] > 1e-6f)
            grid.vel_u[i] /= grid.weight_u[i];

    for (int i = 0; i < (int)grid.vel_v.size(); i++)
        if (grid.weight_v[i] > 1e-6f)
            grid.vel_v[i] /= grid.weight_v[i];
}