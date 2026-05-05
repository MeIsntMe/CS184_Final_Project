#pragma once

#include <iostream>
#include <math.h>
#include <random>
#include <vector>
#include <thrust/device_vector.h>
#include "grid.h"



struct DeviceParticles {
    float *x, *y, *z;
    float *vx, *vy, *vz;
    //float fx, fy, fz;
};

class ParticleSystem{
    public:
        ParticleSystem();
        ~ParticleSystem();

        bool initialise_particles(int count);
        void step(float dt, MACGrid& grid);

        int count;

        // sphere params
        float cx;
        float cy;
        float cz;
        float r;

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
        thrust::device_vector<float> d_x;
        thrust::device_vector<float> d_y;
        thrust::device_vector<float> d_z;

        thrust::device_vector<float> d_vx;
        thrust::device_vector<float> d_vy;
        thrust::device_vector<float> d_vz;

        thrust::device_vector<float> d_fx;
        thrust::device_vector<float> d_fy;
        thrust::device_vector<float> d_fz;
    private:
        void destroyParticle();
        DeviceParticles get_device_particles();
        
};
