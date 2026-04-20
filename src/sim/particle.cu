#ifdef __INTELLISENSE__
    #define __CUDACC__
#endif


#include <random>
#include <thrust/device_vector.h>
#include "particle.h"

__global__ void simulate_particle(int count, float dt, DeviceParticles dp) {
    const float gravity = 9.81f;

    int index = blockIdx.x * blockDim.x + threadIdx.x;

    if (index < count) {
        dp.d_vy[i] -= gravity * dt;
        dp.d_x[i] += ps.d_vx[i] * dt;
        dp.d_y[i] += ps.d_vy[i] * dt;

        if (ps.d_x[i] < -1.f) { ps.d_x[i] = -1.f; ps.d_vx[i] *= -0.5f; }
        if (ps.d_x[i] > 1.f) { ps.d_x[i] = 1.f; ps.d_vx[i] *= -0.5f; }
        if (ps.d_y[i] < -1.f) { ps.d_y[i] = -1.f; ps.d_vy[i] *= -0.5f; }
        if (ps.d_y[i] > 1.f) { ps.d_y[i] = 1.f; ps.d_vy[i] *= -0.5f; }
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

    for (int i = 0; i < count; i++) {
        h_x[i] = dist(rng);
        h_y[i] = dist(rng);
        h_z[i] = -2.0f;
        h_vx[i] = 0.f;
        h_vy[i] = 0.f;
        h_vz[i] = 0.f;
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

void ParticleSystem::step(float dt) {
    const float gravity = 9.81f;

    //plan:
    // particle sim kernel
    // create struct to send data to GPU:
    DeviceParticles dp;
    dp.x = thrust::raw_pointer_cast(d_x.data());
    dp.y = thrust::raw_pointer_cast(d_y.data());
    dp.z = thrust::raw_pointer_cast(d_z.data());
    dp.vx = thrust::raw_pointer_cast(d_vx.data());
    dp.vy = thrust::raw_pointer_cast(d_vy.data());
    dp.vz = thrust::raw_pointer_cast(d_vz.data());

    int threads_per_block = 256;
    int blocks_per_grid = (count + threads_per_block - 1) / threads_per_block;

    simulate_particle <<< blocks_per_grid, threads_per_block >>> (count, dt, dp);
    // particle to grid kernel
    // grid solver kernel
    // grid to particle (transfer difference in velocity back)


}

