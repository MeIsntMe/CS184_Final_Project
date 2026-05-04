#ifdef __INTELLISENSE__
    #define __CUDACC__
#endif

#include "grid.h"

__global__ void compute_divergence(DeviceMACGrid grid) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = grid.nx * grid.ny * grid.nz;
    if (idx >= total) return;

    int iz = idx / (grid.nx * grid.ny);
    int iy = (idx - iz * grid.nx * grid.ny) / grid.nx;
    int ix = idx - iz * grid.nx * grid.ny - iy * grid.nx;

    if (grid.cell_type[idx] != FLUID) {
        grid.divergence[idx] = 0.f;
        return;
    }

    int u_idx = ix + (grid.nx + 1) * (iy + grid.ny * iz);
    int u_idx1 = (ix+1) + (grid.nx + 1) * (iy + grid.ny * iz);
    int v_idx = ix + grid.nx * (iy + (grid.ny + 1) * iz);
    int v_idx1 = ix + grid.nx * ((iy+1) + (grid.ny + 1) * iz);
    int w_idx = ix + grid.nx * (iy + grid.ny * iz);
    int w_idx1 = ix + grid.nx * (iy + grid.ny * (iz+1));

    float div = (grid.vel_u[u_idx1] - grid.vel_u[u_idx]
               + grid.vel_v[v_idx1] - grid.vel_v[v_idx]
               + grid.vel_w[w_idx1] - grid.vel_w[w_idx]) / grid.dx;

    grid.divergence[idx] = div;
}

__device__ inline void jacobi_sample(const DeviceMACGrid& grid,
                                      int nx, int ny, int nz,
                                      float& sum, int& count) {
    if (nx < 0 || nx >= grid.nx) return;
    if (ny < 0 || ny >= grid.ny) return;
    if (nz < 0 || nz >= grid.nz) return;
    int nidx = nx + grid.nx * (ny + grid.ny * nz);
    if (grid.cell_type[nidx] == SOLID) return;
    if (grid.cell_type[nidx] == FLUID) { sum += grid.pressure[nidx]; }
    count++;
}

__global__ void jacobi_iteration(DeviceMACGrid grid, float dt) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = grid.nx * grid.ny * grid.nz;
    if (idx >= total) return;

    int iz = idx / (grid.nx * grid.ny);
    int iy = (idx - iz * grid.nx * grid.ny) / grid.nx;
    int ix = idx - iz * grid.nx * grid.ny - iy * grid.nx;

    if (grid.cell_type[idx] != FLUID) {
        grid.pressure_tmp[idx] = 0.f;
        return;
    }

    float sum = 0.f;
    int   count = 0;

    jacobi_sample(grid, ix-1, iy, iz, sum, count);
    jacobi_sample(grid, ix+1, iy, iz, sum, count);
    jacobi_sample(grid, ix, iy-1, iz, sum, count);
    jacobi_sample(grid, ix, iy+1, iz, sum, count);
    jacobi_sample(grid, ix, iy, iz-1, sum, count);
    jacobi_sample(grid, ix, iy, iz+1, sum, count);

    if (count == 0) { grid.pressure_tmp[idx] = 0.f; return; }

    grid.pressure_tmp[idx] = (sum - grid.divergence[idx] * grid.dx * grid.dx / dt) / count;
}

__global__ void apply_pressure(DeviceMACGrid grid, float dt) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    int u_size = (grid.nx + 1) * grid.ny * grid.nz;
    int v_size = grid.nx * (grid.ny + 1) * grid.nz;
    int w_size =  grid.nx * grid.ny * (grid.nz + 1);

    if (idx < u_size) {
        int iz = idx / ((grid.nx + 1) * grid.ny);
        int iy = (idx - iz * (grid.nx + 1) * grid.ny) / (grid.nx + 1);
        int ix = idx - iz * (grid.nx + 1) * grid.ny - iy * (grid.nx + 1);
        if (ix > 0 && ix < grid.nx) {
            int l = (ix-1) + grid.nx * (iy + grid.ny * iz);
            int r =  ix    + grid.nx * (iy + grid.ny * iz);
            if (grid.cell_type[l] == FLUID || grid.cell_type[r] == FLUID)
                grid.vel_u[idx] -= dt * (grid.pressure[r] - grid.pressure[l]) / grid.dx;
        }
    }

    if (idx < v_size) {
        int iz = idx / (grid.nx * (grid.ny + 1));
        int iy = (idx - iz * grid.nx * (grid.ny + 1)) / grid.nx;
        int ix = idx - iz * grid.nx * (grid.ny + 1) - iy * grid.nx;
        if (iy > 0 && iy < grid.ny) {
            int b = ix + grid.nx * ((iy-1) + grid.ny * iz);
            int t = ix + grid.nx * ( iy    + grid.ny * iz);
            if (grid.cell_type[b] == FLUID || grid.cell_type[t] == FLUID)
                grid.vel_v[idx] -= dt * (grid.pressure[t] - grid.pressure[b]) / grid.dx;
        }
    }

    if (idx < w_size) {
        int iz = idx / (grid.nx * grid.ny);
        int iy = (idx - iz * grid.nx * grid.ny) / grid.nx;
        int ix = idx - iz * grid.nx * grid.ny - iy * grid.nx;
        if (iz > 0 && iz < grid.nz) {
            int bk = ix + grid.nx * (iy + grid.ny * (iz-1));
            int fr = ix + grid.nx * (iy + grid.ny *  iz);
            if (grid.cell_type[bk] == FLUID || grid.cell_type[fr] == FLUID)
                grid.vel_w[idx] -= dt * (grid.pressure[fr] - grid.pressure[bk]) / grid.dx;
        }
    }
}

// Enforce solid wall BCs: zero normal velocity on all domain boundary faces
__global__ void enforce_boundary(DeviceMACGrid grid) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    // U-faces: faces at ix=0 and ix=nx are the left/right walls
    int u_size = (grid.nx + 1) * grid.ny * grid.nz;
    if (idx < u_size) {
        int iz = idx / ((grid.nx + 1) * grid.ny);
        int iy = (idx - iz * (grid.nx + 1) * grid.ny) / (grid.nx + 1);
        int ix = idx - iz * (grid.nx + 1) * grid.ny - iy * (grid.nx + 1);
        if (ix == 0 || ix == grid.nx)
            grid.vel_u[idx] = 0.f;
    }

    // V-faces: faces at iy=0 and iy=ny are the bottom/top walls
    int v_size = grid.nx * (grid.ny + 1) * grid.nz;
    if (idx < v_size) {
        int iz = idx / (grid.nx * (grid.ny + 1));
        int iy = (idx - iz * grid.nx * (grid.ny + 1)) / grid.nx;
        int ix = idx - iz * grid.nx * (grid.ny + 1) - iy * grid.nx;
        if (iy == 0 || iy == grid.ny)
            grid.vel_v[idx] = 0.f;
    }

    // W-faces: faces at iz=0 and iz=nz are the back/front walls
    int w_size = grid.nx * grid.ny * (grid.nz + 1);
    if (idx < w_size) {
        int iz = idx / (grid.nx * grid.ny);
        int iy = (idx - iz * grid.nx * grid.ny) / grid.nx;
        int ix = idx - iz * grid.nx * grid.ny - iy * grid.nx;
        if (iz == 0 || iz == grid.nz)
            grid.vel_w[idx] = 0.f;
    }
}
