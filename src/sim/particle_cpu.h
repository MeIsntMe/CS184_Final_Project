#pragma once
#include <vector>
#include <random>

struct DeviceParticles {
    float* x, * y, * z;
    float* vx, * vy, * vz;
};

class ParticleSystem {
public:
    ParticleSystem() {}
    ~ParticleSystem() {}

    int count = 0;

    std::vector<float> h_x, h_y, h_z;
    std::vector<float> h_vx, h_vy, h_vz;
    std::vector<float> h_fx, h_fy, h_fz;

    bool initialise_particles(int n) {
        count = n;
        h_x.resize(n); h_y.resize(n); h_z.resize(n);
        h_vx.resize(n, 0.f); h_vy.resize(n, 0.f); h_vz.resize(n, 0.f);
        h_fx.resize(n, 0.f); h_fy.resize(n, 0.f); h_fz.resize(n, 0.f);

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-0.8f, 0.8f);
        for (int i = 0; i < n; i++) {
            h_x[i] = dist(rng);
            h_y[i] = dist(rng);
            h_z[i] = -2.0f;
        }
        return true;
    }

    void step(float dt) {
        const float gravity = 9.81f;
        for (int i = 0; i < count; i++) {
            h_vy[i] -= gravity * dt;
            h_x[i] += h_vx[i] * dt;
            h_y[i] += h_vy[i] * dt;

            if (h_x[i] < -1.f) { h_x[i] = -1.f; h_vx[i] *= -0.5f; }
            if (h_x[i] > 1.f) { h_x[i] = 1.f; h_vx[i] *= -0.5f; }
            if (h_y[i] < -1.f) { h_y[i] = -1.f; h_vy[i] *= -0.5f; }
            if (h_y[i] > 1.f) { h_y[i] = 1.f; h_vy[i] *= -0.5f; }
        }
    }
};