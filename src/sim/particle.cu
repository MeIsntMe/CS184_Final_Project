#ifdef __INTELLISENSE__
    #define __CUDACC__
#endif


#include <random>
#include <thrust/device_vector.h>
#include <thrust/fill.h>
#include "particle.h"
#include "sim/p2g.cuh"

__global__ void compute_divergence(DeviceMACGrid grid);
__global__ void jacobi_iteration(DeviceMACGrid grid, float dt);
__global__ void apply_pressure(DeviceMACGrid grid, float dt);
__global__ void enforce_boundary(DeviceMACGrid grid);

// Apply gravity only to v-faces adjacent to at least one FLUID cell
__global__ void add_gravity_to_grid(DeviceMACGrid grid, float dt) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int v_size = grid.nx * (grid.ny + 1) * grid.nz;
    if (idx >= v_size) return;

    // Decompose linear index into (ix, iy, iz) for v-face layout
    int iz = idx / (grid.nx * (grid.ny + 1));
    int iy = (idx - iz * grid.nx * (grid.ny + 1)) / grid.nx;
    int ix = idx - iz * grid.nx * (grid.ny + 1) - iy * grid.nx;

    // v-face (ix, iy, iz) sits between cell (ix, iy-1, iz) and cell (ix, iy, iz)
    bool below_fluid = (iy > 0       && grid.cell_type[ix + grid.nx * ((iy-1) + grid.ny * iz)] == FLUID);
    bool above_fluid = (iy < grid.ny && grid.cell_type[ix + grid.nx * (iy     + grid.ny * iz)] == FLUID);

    if (below_fluid || above_fluid) {
        grid.vel_v[idx] -= 20.f * dt;
    }
}

// Advect particles using their (already-updated) velocities, then clamp to domain
__global__ void advect_and_bounce(int count, float dt, DeviceParticles dp) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < count) {
        dp.vy[i] -= 5.f * dt;  // gravity applied to all particles unconditionally

        dp.x[i] += dp.vx[i] * dt;
        dp.y[i] += dp.vy[i] * dt;
        dp.z[i] += dp.vz[i] * dt;

        // Clamp to domain [-1, 1]^3 with velocity reflection
        if (dp.x[i] < -1.f) { dp.x[i] = -1.f; dp.vx[i] *= -0.5f; }
        if (dp.x[i] >  1.f) { dp.x[i] =  1.f; dp.vx[i] *= -0.5f; }
        if (dp.y[i] < -1.f) { dp.y[i] = -1.f; dp.vy[i] *= -0.5f; }
        if (dp.y[i] >  1.f) { dp.y[i] =  1.f; dp.vy[i] *= -0.5f; }
        if (dp.z[i] < -1.f) { dp.z[i] = -1.f; dp.vz[i] *= -0.5f; }
        if (dp.z[i] >  1.f) { dp.z[i] =  1.f; dp.vz[i] *= -0.5f; }
    }
}

ParticleSystem::ParticleSystem() {
    // h for host (to distinguish from container for GPU)
    h_x = std::vector<float>();
    h_y = std::vector<float>();
    h_z = std::vector<float>();

    h_vx = std::vector<float>();
    h_vy = std::vector<float>();
    h_vz = std::vector<float>();

    h_fx = std::vector<float>();
    h_fy = std::vector<float>();
    h_fz = std::vector<float>();
}

ParticleSystem::~ParticleSystem() {}

bool ParticleSystem::initialise_particles(int count) {
    // sorry this is such a mess now
    h_x.clear();
    h_y.clear();
    h_z.clear();

    h_vx.clear();
    h_vy.clear();
    h_vz.clear();

    h_fx.clear();
    h_fy.clear();
    h_fz.clear();

    h_x.resize(count);
    h_y.resize(count);
    h_z.resize(count);

    h_vx.resize(count);
    h_vy.resize(count);
    h_vz.resize(count);

    h_fx.resize(count);
    h_fy.resize(count);
    h_fz.resize(count);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-0.8f, 0.8f);
    std::uniform_real_distribution<float> dist_z(-0.9f, -0.1f);
    std::uniform_real_distribution<float> dist_vz(-0.5f, 0.5f);

    for (int i = 0; i < count; i++) {
        h_x[i] = dist(rng);
        h_y[i] = dist(rng);
        h_z[i] = dist_z(rng);
        h_vx[i] = 0.f;
        h_vy[i] = 0.f;
        h_vz[i] = dist_vz(rng);
        h_fx[i] = 0.f;
        h_fy[i] = 0.f;
        h_fz[i] = 0.f;
    }

    // putting vectors in VRAM:
    d_x = h_x;
    d_y = h_y;
    d_z = h_z;

    d_vx = h_vx;
    d_vy = h_vy;
    d_vz = h_vz;

    d_fx = h_fx;
    d_fy = h_fy;
    d_fz = h_fz;

    this->count = count;
    return true;
}

void ParticleSystem::step(float dt, MACGrid& grid) {
    // Clamp dt to avoid instability on first frame or hitches
    if (dt <= 0.f) dt = 1e-4f;
    if (dt > 0.02f) dt = 0.02f;   // cap at 20 ms (~50 fps minimum)

    DeviceParticles dp = this->get_device_particles();
    DeviceMACGrid   dg = grid.get_device_grid();

    int tpb = 256;  // threads per block
    int particle_blocks = (count + tpb - 1) / tpb;

    int cell_total = grid.nx * grid.ny * grid.nz;
    int cell_blocks = (cell_total + tpb - 1) / tpb;

    int u_size = (grid.nx + 1) * grid.ny * grid.nz;
    int v_size =  grid.nx * (grid.ny + 1) * grid.nz;
    int w_size =  grid.nx * grid.ny * (grid.nz + 1);
    int max_face = max(u_size, max(v_size, w_size));
    int face_blocks = (max_face + tpb - 1) / tpb;

    // 1. Clear grid from previous frame
    grid.clear();
    dg = grid.get_device_grid();

    // 2. Particle-to-Grid transfer
    p2g_transfer<<<particle_blocks, tpb>>>(dg, dp, count);
    //cudaDeviceSynchronize();

    p2g_normalise<<<face_blocks, tpb>>>(dg);
    //cudaDeviceSynchronize();

    // Gravity is applied directly to particles in advect_and_bounce instead

    // Enforce wall BCs after gravity, so wall faces stay at zero
    enforce_boundary<<<face_blocks, tpb>>>(dg);
    //cudaDeviceSynchronize();

    // 3. Snapshot velocity for FLIP (before pressure solve)
    thrust::copy(grid.d_vel_u.begin(), grid.d_vel_u.end(), grid.d_vel_u_old.begin());
    thrust::copy(grid.d_vel_v.begin(), grid.d_vel_v.end(), grid.d_vel_v_old.begin());
    thrust::copy(grid.d_vel_w.begin(), grid.d_vel_w.end(), grid.d_vel_w_old.begin());
    dg = grid.get_device_grid();

    // 4. Pressure solve
    compute_divergence<<<cell_blocks, tpb>>>(dg);
    //cudaDeviceSynchronize();

    // Clear pressure before Jacobi iterations
    thrust::fill(grid.d_pressure.begin(), grid.d_pressure.end(), 0.f);
    thrust::fill(grid.d_pressure_tmp.begin(), grid.d_pressure_tmp.end(), 0.f);

    // Jacobi iterations with ping-pong swap
    const int jacobi_iters = 100;
    for (int iter = 0; iter < jacobi_iters; iter++) {
        dg = grid.get_device_grid();
        jacobi_iteration<<<cell_blocks, tpb>>>(dg, dt);
        //cudaDeviceSynchronize();
        grid.d_pressure.swap(grid.d_pressure_tmp);
    }
    dg = grid.get_device_grid();

    // Apply pressure gradient to velocity
    int apply_blocks = (max(u_size, max(v_size, w_size)) + tpb - 1) / tpb;
    apply_pressure<<<apply_blocks, tpb>>>(dg, dt);
    //cudaDeviceSynchronize();

    // Enforce wall BCs after pressure projection
    enforce_boundary<<<face_blocks, tpb>>>(dg);
    //cudaDeviceSynchronize();

    // 5. Grid-to-Particle transfer
    g2p_transfer<<<particle_blocks, tpb>>>(dg, dp, count);
    //cudaDeviceSynchronize();

    // 6. Advect particles with the new velocities
    advect_and_bounce<<<particle_blocks, tpb>>>(count, dt, dp);
    //cudaDeviceSynchronize();

    // 7. Sync positions back to CPU for rendering
    thrust::copy(d_x.begin(), d_x.end(), h_x.begin());
    thrust::copy(d_y.begin(), d_y.end(), h_y.begin());
    thrust::copy(d_z.begin(), d_z.end(), h_z.begin());

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) std::cout << "CUDA Error: " << cudaGetErrorString(err) << "\n";
}

DeviceParticles ParticleSystem::get_device_particles() {
    DeviceParticles dp;
    dp.x = thrust::raw_pointer_cast(d_x.data());
    dp.y = thrust::raw_pointer_cast(d_y.data());
    dp.z = thrust::raw_pointer_cast(d_z.data());
    dp.vx = thrust::raw_pointer_cast(d_vx.data());
    dp.vy = thrust::raw_pointer_cast(d_vy.data());
    dp.vz = thrust::raw_pointer_cast(d_vz.data());
    return dp;
}
