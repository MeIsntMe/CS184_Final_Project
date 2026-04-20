#pragma once

#include <iostream>
#include <math.h>
#include <random>
#include <vector>
#include <thrust/device_vector.h>



struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float fx, fy, fz;
};

struct ParticleStorage {
    Particle* particles;
    int count;
};

class ParticleSystem{
    public:
        ParticleSystem();
        ~ParticleSystem();

        bool initialise_particles(int count);
        void step(float dt);

        // Host (CPU vectors just for initialization and maybe other stuff not sure yet)
        std::vector<float> h_x;
        std::vector<float> h_y;
        std::vector<float> h_z;

        std::vector<float> h_vx;
        std::vector<float> h_vy;
        std::vector<float> h_vz;

        std::vector<float> h_fx;
        std::vector<float> h_fy;
        std::vector<float> h_fz;

        // Device (GPU vectors for simulation)
        float* d_x;
        float* d_y;
        float* d_z;

        float* d_vx;
        float* d_vy;
        float* d_vz;

        float* d_fx;
        float* d_fy;
        float* d_fz;
    private:
        void destroyParticle();
        
};
