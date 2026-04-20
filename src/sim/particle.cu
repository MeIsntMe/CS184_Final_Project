#ifdef __INTELLISENSE__
    #define __CUDACC__
#endif


#include <random>
#include <thrust/device_vector.h>
#include "particle.h"

__global__ void simulate_particle(int count, float dt, DeviceParticles dp) {
    const float gravity = 9.81f;

    int i = blockIdx.x * blockDim.x + threadIdx.x;

    if (i < count) {
        dp.vy[i] -= gravity * dt;
        dp.x[i] += dp.vx[i] * dt;
        dp.y[i] += dp.vy[i] * dt;

        if (dp.x[i] < -1.f) { dp.x[i] = -1.f; dp.vx[i] *= -0.5f; }
        if (dp.x[i] > 1.f) { dp.x[i] = 1.f; dp.vx[i] *= -0.5f; }
        if (dp.y[i] < -1.f) { dp.y[i] = -1.f; dp.vy[i] *= -0.5f; }
        if (dp.y[i] > 1.f) { dp.y[i] = 1.f; dp.vy[i] *= -0.5f; }
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
    // temporary code to sync back to CPU to render
    cudaDeviceSynchronize();
    thrust::copy(d_x.begin(), d_x.end(), h_x.begin());
    thrust::copy(d_y.begin(), d_y.end(), h_y.begin());
    thrust::copy(d_z.begin(), d_z.end(), h_z.begin());
    // particle to grid kernel
    // grid solver kernel
    // grid to particle (transfer difference in velocity back)
}

