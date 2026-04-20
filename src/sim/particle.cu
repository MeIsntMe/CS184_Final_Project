#ifdef __INTELLISENSE__
    #define __CUDACC__
#endif


#include <random>
#include <thrust/device_vector.h>
#include "particle.h"

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
    thrust::device_vector<float> d_x = h_x;
    thrust::device_vector<float> d_y = h_y;
    thrust::device_vector<float> d_z = h_z;

    thrust::device_vector<float> d_vx = h_vx;
    thrust::device_vector<float> d_vy = h_vy;
    thrust::device_vector<float> d_vz = h_vz;

    thrust::device_vector<float> d_fx = h_fx;
    thrust::device_vector<float> d_fy = h_fy;
    thrust::device_vector<float> d_fz = h_fz;
    return true;
}

void ParticleSystem::step(float dt) {
    const float gravity = 9.81f;

    //plan:
    // particle sim kernel
    // particle to grid kernel
    // grid solver kernel
    // grid to particle (transfer difference in velocity back)

    /*for (Particle& p : particles) {
        p.vy -= gravity * dt;

        p.x += p.vx * dt;
        p.y += p.vy * dt;

        if (p.x < -1.f) { p.x = -1.f; p.vx *= -0.5f; }
        if (p.x > 1.f) { p.x = 1.f; p.vx *= -0.5f; }
        if (p.y < -1.f) { p.y = -1.f; p.vy *= -0.5f; }
        if (p.y > 1.f) { p.y = 1.f; p.vy *= -0.5f; }
    }*/
}