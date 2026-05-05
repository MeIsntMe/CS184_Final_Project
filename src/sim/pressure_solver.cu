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

__device__ float sdf_sphere(float x, float y, float z, float cx, float cy, float cz, float r) {
  float dx = x - cx; float dy = y - cy; float dz = z - cz;
  return sqrtf(dx * dx + dy * dy + dz * dz) - r;
}

__global__ void mark_solids(DeviceMACGrid grid, float cx, float cy, float cz, float r) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;
  int total = grid.nx * grid.ny * grid.nz;
  if (idx >= total) return;

  int iz = idx / (grid.nx * grid.ny);
  int iy = (idx - iz * grid.nx * grid.ny) / grid.nx;
  int ix = idx - iz * grid.nx * grid.ny - iy * grid.nx;

  // Convert grid indices to world space cell centers (Domain: -1 to 1)
  float x = -1.f + (ix + 0.5f) * grid.dx;
  float y = -1.f + (iy + 0.5f) * grid.dx;
  float z = -1.f + (iz + 0.5f) * grid.dx;

  if (sdf_sphere(x, y, z, cx, cy, cz, r) <= 0.f) {
    grid.cell_type[idx] = SOLID;
  }
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
            if (grid.cell_type[l] != SOLID && grid.cell_type[r] != SOLID)
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
            if (grid.cell_type[b] != SOLID && grid.cell_type[t] != SOLID)
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
            if (grid.cell_type[bk] != SOLID && grid.cell_type[fr] != SOLID)
                grid.vel_w[idx] -= dt * (grid.pressure[fr] - grid.pressure[bk]) / grid.dx;
        }
    }
}

// Enforce solid wall BCs: zero normal velocity on all domain boundary faces
// and fluid-solid interfaces. Tangential velocities inside solids are preserved
// to allow for natural free-slip behavior from P2G splatting.
__global__ void enforce_boundary(DeviceMACGrid grid) {
  int idx = blockIdx.x * blockDim.x + threadIdx.x;

  // U-faces
  int u_size = (grid.nx + 1) * grid.ny * grid.nz;
  if (idx < u_size) {
    int iz = idx / ((grid.nx + 1) * grid.ny);
    int iy = (idx - iz * (grid.nx + 1) * grid.ny) / (grid.nx + 1);
    int ix = idx - iz * (grid.nx + 1) * grid.ny - iy * (grid.nx + 1);

    bool is_boundary_normal = false;
    if (ix > 0 && ix < grid.nx) {
      int l = (ix - 1) + grid.nx * (iy + grid.ny * iz);
      int r = ix + grid.nx * (iy + grid.ny * iz);
      // ONLY zero if it is the precise fluid-solid interface boundary
      if ((grid.cell_type[l] == SOLID && grid.cell_type[r] != SOLID) ||
        (grid.cell_type[l] != SOLID && grid.cell_type[r] == SOLID)) {
        is_boundary_normal = true;
      }
    }

    if (ix == 0 || ix == grid.nx || is_boundary_normal) grid.vel_u[idx] = 0.f;
  }

  // V-faces
  int v_size = grid.nx * (grid.ny + 1) * grid.nz;
  if (idx < v_size) {
    int iz = idx / (grid.nx * (grid.ny + 1));
    int iy = (idx - iz * grid.nx * (grid.ny + 1)) / grid.nx;
    int ix = idx - iz * grid.nx * (grid.ny + 1) - iy * grid.nx;

    bool is_boundary_normal = false;
    if (iy > 0 && iy < grid.ny) {
      int b = ix + grid.nx * ((iy - 1) + grid.ny * iz);
      int t = ix + grid.nx * (iy + grid.ny * iz);
      // ONLY zero if it is the precise fluid-solid interface boundary
      if ((grid.cell_type[b] == SOLID && grid.cell_type[t] != SOLID) ||
        (grid.cell_type[b] != SOLID && grid.cell_type[t] == SOLID)) {
        is_boundary_normal = true;
      }
    }

    if (iy == 0 || is_boundary_normal) grid.vel_v[idx] = 0.f; // keeps ceiling open
  }

  // W-faces
  int w_size = grid.nx * grid.ny * (grid.nz + 1);
  if (idx < w_size) {
    int iz = idx / (grid.nx * grid.ny);
    int iy = (idx - iz * grid.nx * grid.ny) / grid.nx;
    int ix = idx - iz * grid.nx * grid.ny - iy * grid.nx;

    bool is_boundary_normal = false;
    if (iz > 0 && iz < grid.nz) {
      int bk = ix + grid.nx * (iy + grid.ny * (iz - 1));
      int fr = ix + grid.nx * (iy + grid.ny * iz);
      // ONLY zero if it is the precise fluid-solid interface boundary
      if ((grid.cell_type[bk] == SOLID && grid.cell_type[fr] != SOLID) ||
        (grid.cell_type[bk] != SOLID && grid.cell_type[fr] == SOLID)) {
        is_boundary_normal = true;
      }
    }

    if (iz == 0 || iz == grid.nz || is_boundary_normal) grid.vel_w[idx] = 0.f;
  }
}
