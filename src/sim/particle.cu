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
__global__ void mark_solid_sphere(DeviceMACGrid grid, float cx, float cy, float cz, float radius);
__global__ void enforce_solid_face_velocities(DeviceMACGrid grid);

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
        grid.vel_v[idx] -= 17.f * dt;
    }
}

// Activate n reservoir particles at the nozzle position with a hash-based jitter
__global__ void emit_particles(DeviceParticles dp, int start, int n,
                               float ex, float ey, float ez, float evy) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= n) return;
    int idx = start + i;
    float jx = (float)((unsigned)(idx * 2654435761u) % 1000u) / 1000.f * 0.02f - 0.01f;
    float jz = (float)((unsigned)(idx * 2246822519u) % 1000u) / 1000.f * 0.02f - 0.01f;
    float jy = (float)((unsigned)(idx * 2166136261u) % 1000u) / 1000.f * 0.30f - 0.15f;
    dp.x[idx] = ex + jx;
    dp.y[idx] = ey + jy;
    dp.z[idx] = ez + jz;
    // Give particles below the nozzle the velocity they'd have after falling that extra distance,
    // so the stream looks like a continuous flow rather than discrete batches.
    float vy_out = evy;
    if (jy < 0.f) {
        float v2 = evy * evy + 2.f * 14.45f * (-jy);
        vy_out = -sqrtf(v2);
    }
    dp.vx[idx] = 0.f;
    dp.vy[idx] = vy_out;
    dp.vz[idx] = 0.f;
}

// Advect particles using their (already-updated) velocities, then clamp to domain
__global__ void advect_and_bounce(int count, float dt, DeviceParticles dp,
                                   float sph_cx, float sph_cy, float sph_cz, float sph_r) {
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < count) {
        dp.x[i] += dp.vx[i] * dt;
        dp.y[i] += dp.vy[i] * dt;
        dp.z[i] += dp.vz[i] * dt;

        // Box walls (ceiling removed intentionally)
        if (dp.x[i] < -1.f) { dp.x[i] = -1.f; dp.vx[i] *= -0.65f; }
        if (dp.x[i] >  1.f) { dp.x[i] =  1.f; dp.vx[i] *= -0.65f; }
        if (dp.y[i] < -1.f) { dp.y[i] = -1.f; dp.vy[i] *= -0.65f; }
        if (dp.z[i] < -1.f) { dp.z[i] = -1.f; dp.vz[i] *= -0.65f; }
        if (dp.z[i] >  1.f) { dp.z[i] =  1.f; dp.vz[i] *= -0.65f; }

        // Sphere collision
        if (sph_r > 0.f) {
            float dx = dp.x[i] - sph_cx;
            float dy = dp.y[i] - sph_cy;
            float dz = dp.z[i] - sph_cz;
            float dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist < sph_r && dist > 0.f) {
                float nx = dx/dist, ny = dy/dist, nz = dz/dist;
                // push particle to surface
                dp.x[i] = sph_cx + nx * sph_r;
                dp.y[i] = sph_cy + ny * sph_r;
                dp.z[i] = sph_cz + nz * sph_r;
                // reflect velocity with restitution 0.65
                float vdotn = dp.vx[i]*nx + dp.vy[i]*ny + dp.vz[i]*nz;
                if (vdotn < 0.f) {
                    dp.vx[i] -= 1.65f * vdotn * nx;
                    dp.vy[i] -= 1.65f * vdotn * ny;
                    dp.vz[i] -= 1.65f * vdotn * nz;
                }
            }
        }
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

bool ParticleSystem::initialise_particles(int count, int preset) {
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

    switch (preset) {
        case 4: sphere = {0.f, 0.f, 0.f, 0.35f}; break;
        default: sphere = {}; break;
    }

    std::mt19937 rng(42);

    if (preset == 3) {
        // 90% settled pool at the bottom (active immediately).
        // 10% reservoir — activated gradually each frame by emit_particles kernel.
        int pool = (count * 9) / 10;

        std::uniform_real_distribution<float> px(-0.8f, 0.8f);
        std::uniform_real_distribution<float> py(-0.9f, -0.6f);
        std::uniform_real_distribution<float> pz(-0.8f, 0.8f);
        for (int i = 0; i < pool; i++) {
            h_x[i] = px(rng); h_y[i] = py(rng); h_z[i] = pz(rng);
            h_vx[i] = 0.f;    h_vy[i] = 0.f;    h_vz[i] = 0.f;
            h_fx[i] = 0.f;    h_fy[i] = 0.f;    h_fz[i] = 0.f;
        }
        // Reservoir: parked at floor so they don't affect physics before activation
        for (int i = pool; i < count; i++) {
            h_x[i] = 0.f; h_y[i] = -0.99f; h_z[i] = 0.f;
            h_vx[i] = 0.f; h_vy[i] = 0.f;  h_vz[i] = 0.f;
            h_fx[i] = 0.f; h_fy[i] = 0.f;  h_fz[i] = 0.f;
        }

        active_count = pool;
        emit_head    = pool;
        emit_end     = count;
        emit_rate    = 500;   // particles per frame → ~4 s of pour at 50 fps
        emit_delay   = 120;   // ~2.5 s at 50 fps for pool to reach equilibrium
        emit_x = 0.f; emit_y = 0.95f; emit_z = 0.f; emit_vy = -7.0f;
    } else if (preset == 4) {
        // Small pool below the sphere, rest emitted from a tap directly above it.
        int pool = count / 5;  // 20% pool below sphere

        std::uniform_real_distribution<float> px(-0.8f, 0.8f);
        std::uniform_real_distribution<float> py(-0.9f, -0.5f);  // below sphere (bottom is y=-0.35)
        std::uniform_real_distribution<float> pz(-0.8f, 0.8f);
        for (int i = 0; i < pool; i++) {
            h_x[i] = px(rng); h_y[i] = py(rng); h_z[i] = pz(rng);
            h_vx[i] = 0.f;    h_vy[i] = 0.f;    h_vz[i] = 0.f;
            h_fx[i] = 0.f;    h_fy[i] = 0.f;    h_fz[i] = 0.f;
        }
        for (int i = pool; i < count; i++) {
            h_x[i] = 0.f; h_y[i] = -0.99f; h_z[i] = 0.f;
            h_vx[i] = 0.f; h_vy[i] = 0.f;  h_vz[i] = 0.f;
            h_fx[i] = 0.f; h_fy[i] = 0.f;  h_fz[i] = 0.f;
        }

        active_count = pool;
        emit_head    = pool;
        emit_end     = count;
        emit_rate    = 500;
        emit_delay   = 120;
        emit_x = 0.f; emit_y = 0.95f; emit_z = 0.f; emit_vy = -7.0f;
    } else {
        active_count = count;
        emit_head = emit_end = emit_rate = emit_delay = 0;

        struct SpawnBox { float xlo, xhi, ylo, yhi, zlo, zhi; };
        struct VelBias  { float vxlo, vxhi, vylo, vyhi, vzlo, vzhi; };

        SpawnBox box;
        VelBias  vel;
        switch (preset) {
            case 2:
                box = {-0.15f, 0.15f,  0.5f, 0.9f, -0.15f, 0.15f};
                vel = {-0.5f,  0.5f,  -8.0f,-4.0f,  -0.5f,  0.5f};
                break;
            default: // preset 1
                box = {-0.8f,  0.8f,  -0.8f, 0.8f,  -0.9f, -0.1f};
                vel = {-1.5f,  1.5f,  -1.5f, 1.5f,  -1.5f,  1.5f};
                break;
        }

        std::uniform_real_distribution<float> dist_x(box.xlo, box.xhi);
        std::uniform_real_distribution<float> dist_y(box.ylo, box.yhi);
        std::uniform_real_distribution<float> dist_z(box.zlo, box.zhi);
        std::uniform_real_distribution<float> dist_vx(vel.vxlo, vel.vxhi);
        std::uniform_real_distribution<float> dist_vy(vel.vylo, vel.vyhi);
        std::uniform_real_distribution<float> dist_vz(vel.vzlo, vel.vzhi);

        for (int i = 0; i < count; i++) {
            h_x[i] = dist_x(rng); h_y[i] = dist_y(rng); h_z[i] = dist_z(rng);
            h_vx[i] = dist_vx(rng); h_vy[i] = dist_vy(rng); h_vz[i] = dist_vz(rng);
            h_fx[i] = 0.f; h_fy[i] = 0.f; h_fz[i] = 0.f;
        }
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

    // Emit next batch of reservoir particles at the nozzle before physics
    if (emit_delay > 0) { emit_delay--; }
    else if (emit_head < emit_end && emit_rate > 0) {
        int n = min(emit_rate, emit_end - emit_head);
        int emit_blocks = (n + tpb - 1) / tpb;
        emit_particles<<<emit_blocks, tpb>>>(dp, emit_head, n,
            emit_x, emit_y, emit_z, emit_vy);
        emit_head    += n;
        active_count += n;
    }

    int particle_blocks = (active_count + tpb - 1) / tpb;
    if (particle_blocks == 0) particle_blocks = 1;

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
    p2g_transfer<<<particle_blocks, tpb>>>(dg, dp, active_count);
    //cudaDeviceSynchronize();

    p2g_normalise<<<face_blocks, tpb>>>(dg);
    //cudaDeviceSynchronize();

    // Stamp sphere cells as SOLID so the pressure solve routes around them
    if (sphere.active()) {
        mark_solid_sphere<<<cell_blocks, tpb>>>(dg, sphere.cx, sphere.cy, sphere.cz, sphere.radius);
    }

    // 3. Snapshot velocity for FLIP before gravity+pressure so delta carries both
    thrust::copy(grid.d_vel_u.begin(), grid.d_vel_u.end(), grid.d_vel_u_old.begin());
    thrust::copy(grid.d_vel_v.begin(), grid.d_vel_v.end(), grid.d_vel_v_old.begin());
    thrust::copy(grid.d_vel_w.begin(), grid.d_vel_w.end(), grid.d_vel_w_old.begin());
    dg = grid.get_device_grid();

    // 4. Apply gravity to grid so pressure solve must balance it (hydrostatic support)
    add_gravity_to_grid<<<face_blocks, tpb>>>(dg, dt);
    //cudaDeviceSynchronize();

    // Enforce wall BCs after gravity
    enforce_boundary<<<face_blocks, tpb>>>(dg);
    if (sphere.active()) enforce_solid_face_velocities<<<face_blocks, tpb>>>(dg);
    //cudaDeviceSynchronize();

    // 4. Pressure solve
    compute_divergence<<<cell_blocks, tpb>>>(dg);
    //cudaDeviceSynchronize();

    // Clear pressure before Jacobi iterations
    thrust::fill(grid.d_pressure.begin(), grid.d_pressure.end(), 0.f);
    thrust::fill(grid.d_pressure_tmp.begin(), grid.d_pressure_tmp.end(), 0.f);

    // Jacobi iterations with ping-pong swap
    const int jacobi_iters = 300;
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
    if (sphere.active()) enforce_solid_face_velocities<<<face_blocks, tpb>>>(dg);
    //cudaDeviceSynchronize();

    // 5. Grid-to-Particle transfer
    g2p_transfer<<<particle_blocks, tpb>>>(dg, dp, active_count);
    //cudaDeviceSynchronize();

    // 6. Advect particles with the new velocities
    advect_and_bounce<<<particle_blocks, tpb>>>(active_count, dt, dp,
        sphere.cx, sphere.cy, sphere.cz, sphere.radius);
    //cudaDeviceSynchronize();

    // 7. Sync active positions back to CPU for rendering
    thrust::copy(d_x.begin(), d_x.begin() + active_count, h_x.begin());
    thrust::copy(d_y.begin(), d_y.begin() + active_count, h_y.begin());
    thrust::copy(d_z.begin(), d_z.begin() + active_count, h_z.begin());

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
