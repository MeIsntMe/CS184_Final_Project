#pragma once
#include "grid.h"
#include "particle.h"

// Linear interpolation weight
__device__ inline float weight(float x, float node) {
    float d = std::abs(x - node);
    return (d < 1.f) ? (1.f - d) : 0.f;
}

__global__ void p2g_transfer(DeviceMACGrid grid, DeviceParticles dp, int count) {

    float dx = grid.dx;
    int pid = blockIdx.x * blockDim.x + threadIdx.x;  // particle index

    if (pid < count) {
        // Convert world position to grid coordinates
        float gx = (dp.x[pid] + 1.0f) / dx;
        float gy = (dp.y[pid] + 1.0f) / dx;
        float gz = (dp.z[pid] + 1.0f) / dx;

        int ix = (int)gx;  // renamed to avoid shadowing pid
        int iy = (int)gy;
        int iz = (int)gz;

        if (ix >= 0 && ix < grid.nx && iy >= 0 && iy < grid.ny && iz >= 0 && iz < grid.nz)
            grid.cell_type[ix + grid.nx * (iy + grid.ny * iz)] = FLUID;

        // Splat onto u-faces (x velocity component)
        for (int di = 0; di <= 1; di++)
            for (int dj = 0; dj <= 1; dj++)
                for (int dk = 0; dk <= 1; dk++) {
                    int ni = ix + di;
                    int nj = iy + dj;
                    int nk = iz + dk;

                    if (ni < 0 || ni > grid.nx) continue;
                    if (nj < 0 || nj >= grid.ny) continue;
                    if (nk < 0 || nk >= grid.nz) continue;

                    float wx = weight(gx, ni);
                    float wy = weight(gy, nj + 0.5f);
                    float wz = weight(gz, nk + 0.5f);
                    float w = wx * wy * wz;

                    int idx = ni + (grid.nx + 1) * (nj + grid.ny * nk);
                    atomicAdd(&grid.vel_u[idx],    w * dp.vx[pid]);
                    atomicAdd(&grid.weight_u[idx], w);
                }

        // Splat onto v-faces (y velocity component)
        for (int di = 0; di <= 1; di++)
            for (int dj = 0; dj <= 1; dj++)
                for (int dk = 0; dk <= 1; dk++) {
                    int ni = ix + di;
                    int nj = iy + dj;
                    int nk = iz + dk;

                    if (ni < 0 || ni >= grid.nx) continue;
                    if (nj < 0 || nj > grid.ny) continue;
                    if (nk < 0 || nk >= grid.nz) continue;

                    float wx = weight(gx, ni + 0.5f);
                    float wy = weight(gy, nj);
                    float wz = weight(gz, nk + 0.5f);
                    float w = wx * wy * wz;

                    int idx = ni + grid.nx * (nj + (grid.ny + 1) * nk);
                    atomicAdd(&grid.vel_v[idx],    w * dp.vy[pid]);
                    atomicAdd(&grid.weight_v[idx], w);
                }

        // Splat onto w-faces (z velocity component)
        for (int di = 0; di <= 1; di++)
            for (int dj = 0; dj <= 1; dj++)
                for (int dk = 0; dk <= 1; dk++) {
                    int ni = ix + di;
                    int nj = iy + dj;
                    int nk = iz + dk;

                    if (ni < 0 || ni >= grid.nx) continue;
                    if (nj < 0 || nj >= grid.ny) continue;
                    if (nk < 0 || nk > grid.nz) continue;

                    float wx = weight(gx, ni + 0.5f);
                    float wy = weight(gy, nj + 0.5f);
                    float wz = weight(gz, nk);
                    float w = wx * wy * wz;

                    int idx = ni + grid.nx * (nj + grid.ny * nk);
                    atomicAdd(&grid.vel_w[idx],    w * dp.vz[pid]);
                    atomicAdd(&grid.weight_w[idx], w);
                }
    }
}

// Generic trilinear interpolation for u-faces, reads from the given velocity array
__device__ inline float interp_u(const DeviceMACGrid& grid, const float* vel, float gx, float gy, float gz) {
    float x = fmaxf(0.f, fminf(gx,         (float)grid.nx));
    float y = fmaxf(0.f, fminf(gy - 0.5f,  (float)(grid.ny - 1)));
    float z = fmaxf(0.f, fminf(gz - 0.5f,  (float)(grid.nz - 1)));
    int ix = min((int)x, grid.nx - 1);
    int iy = min((int)y, max(0, grid.ny - 2));
    int iz = min((int)z, max(0, grid.nz - 2));
    float fx = x - ix, fy = y - iy, fz = z - iz;
    auto s = [&](int i, int j, int k) {
        i = max(0, min(i, grid.nx)); j = max(0, min(j, grid.ny-1)); k = max(0, min(k, grid.nz-1));
        return vel[i + (grid.nx+1)*(j + grid.ny*k)];
    };
    return (1-fz)*((1-fy)*((1-fx)*s(ix,iy,iz)   + fx*s(ix+1,iy,iz))   + fy*((1-fx)*s(ix,iy+1,iz)   + fx*s(ix+1,iy+1,iz)))
          +   fz *((1-fy)*((1-fx)*s(ix,iy,iz+1)  + fx*s(ix+1,iy,iz+1)) + fy*((1-fx)*s(ix,iy+1,iz+1) + fx*s(ix+1,iy+1,iz+1)));
}

__device__ inline float interp_v(const DeviceMACGrid& grid, const float* vel, float gx, float gy, float gz) {
    float x = fmaxf(0.f, fminf(gx - 0.5f,  (float)(grid.nx - 1)));
    float y = fmaxf(0.f, fminf(gy,          (float)grid.ny));
    float z = fmaxf(0.f, fminf(gz - 0.5f,  (float)(grid.nz - 1)));
    int ix = min((int)x, max(0, grid.nx - 2));
    int iy = min((int)y, grid.ny - 1);
    int iz = min((int)z, max(0, grid.nz - 2));
    float fx = x - ix, fy = y - iy, fz = z - iz;
    auto s = [&](int i, int j, int k) {
        i = max(0, min(i, grid.nx-1)); j = max(0, min(j, grid.ny)); k = max(0, min(k, grid.nz-1));
        return vel[i + grid.nx*(j + (grid.ny+1)*k)];
    };
    return (1-fz)*((1-fy)*((1-fx)*s(ix,iy,iz)   + fx*s(ix+1,iy,iz))   + fy*((1-fx)*s(ix,iy+1,iz)   + fx*s(ix+1,iy+1,iz)))
          +   fz *((1-fy)*((1-fx)*s(ix,iy,iz+1)  + fx*s(ix+1,iy,iz+1)) + fy*((1-fx)*s(ix,iy+1,iz+1) + fx*s(ix+1,iy+1,iz+1)));
}

__device__ inline float interp_w(const DeviceMACGrid& grid, const float* vel, float gx, float gy, float gz) {
    float x = fmaxf(0.f, fminf(gx - 0.5f,  (float)(grid.nx - 1)));
    float y = fmaxf(0.f, fminf(gy - 0.5f,  (float)(grid.ny - 1)));
    float z = fmaxf(0.f, fminf(gz,          (float)grid.nz));
    int ix = min((int)x, max(0, grid.nx - 2));
    int iy = min((int)y, max(0, grid.ny - 2));
    int iz = min((int)z, grid.nz - 1);
    float fx = x - ix, fy = y - iy, fz = z - iz;
    auto s = [&](int i, int j, int k) {
        i = max(0, min(i, grid.nx-1)); j = max(0, min(j, grid.ny-1)); k = max(0, min(k, grid.nz));
        return vel[i + grid.nx*(j + grid.ny*k)];
    };
    return (1-fz)*((1-fy)*((1-fx)*s(ix,iy,iz)   + fx*s(ix+1,iy,iz))   + fy*((1-fx)*s(ix,iy+1,iz)   + fx*s(ix+1,iy+1,iz)))
          +   fz *((1-fy)*((1-fx)*s(ix,iy,iz+1)  + fx*s(ix+1,iy,iz+1)) + fy*((1-fx)*s(ix,iy+1,iz+1) + fx*s(ix+1,iy+1,iz+1)));
}

// FLIP/PIC blended G2P transfer
// Blend: alpha * PIC + (1-alpha) * FLIP
__global__ void g2p_transfer(DeviceMACGrid grid, DeviceParticles dp, int count) {
    int pid = blockIdx.x * blockDim.x + threadIdx.x;
    if (pid >= count) return;

    const float alpha = 0.05f;  // 5% PIC, 95% FLIP

    float gx = (dp.x[pid] + 1.0f) / grid.dx;
    float gy = (dp.y[pid] + 1.0f) / grid.dx;
    float gz = (dp.z[pid] + 1.0f) / grid.dx;

    // Interpolate new (post-pressure) and old (pre-pressure) grid velocities
    float u_new = interp_u(grid, grid.vel_u,     gx, gy, gz);
    float u_old = interp_u(grid, grid.vel_u_old, gx, gy, gz);
    float v_new = interp_v(grid, grid.vel_v,     gx, gy, gz);
    float v_old = interp_v(grid, grid.vel_v_old, gx, gy, gz);
    float w_new = interp_w(grid, grid.vel_w,     gx, gy, gz);
    float w_old = interp_w(grid, grid.vel_w_old, gx, gy, gz);

    // FLIP: particle velocity + grid velocity change
    float flip_vx = dp.vx[pid] + (u_new - u_old);
    float flip_vy = dp.vy[pid] + (v_new - v_old);
    float flip_vz = dp.vz[pid] + (w_new - w_old);

    // Blend PIC and FLIP
    dp.vx[pid] = alpha * u_new + (1.f - alpha) * flip_vx;
    dp.vy[pid] = alpha * v_new + (1.f - alpha) * flip_vy;
    dp.vz[pid] = alpha * w_new + (1.f - alpha) * flip_vz;
}

// Separate normalisation kernel — one thread per grid face
__global__ void p2g_normalise(DeviceMACGrid grid) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    int u_size = (grid.nx + 1) * grid.ny * grid.nz;
    int v_size =  grid.nx * (grid.ny + 1) * grid.nz;
    int w_size =  grid.nx * grid.ny * (grid.nz + 1);

    if (idx < u_size && grid.weight_u[idx] > 1e-6f)
        grid.vel_u[idx] /= grid.weight_u[idx];

    if (idx < v_size && grid.weight_v[idx] > 1e-6f)
        grid.vel_v[idx] /= grid.weight_v[idx];

    if (idx < w_size && grid.weight_w[idx] > 1e-6f)
        grid.vel_w[idx] /= grid.weight_w[idx];
}