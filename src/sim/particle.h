#pragma once

#include <iostream>
#include <math.h>
#include <random>
#include <vector>
#include <thrust/device_vector.h>
#include "grid.h"



struct Sphere {
    float cx = 0.f, cy = 0.f, cz = 0.f, radius = 0.f;
    bool active() const { return radius > 0.f; }
};

struct DeviceParticles {
    float *x, *y, *z;
    float *vx, *vy, *vz;
    //float fx, fy, fz;
};

class ParticleSystem{
    public:
        ParticleSystem();
        ~ParticleSystem();

        bool initialise_particles(int count, int preset = 1);
        void step(float dt, MACGrid& grid);

        int count;
        int active_count;  // particles currently visible to the simulation
        int emit_head;     // next reservoir particle to activate
        int emit_end;      // one past the last reservoir particle
        int emit_rate;     // particles activated per frame (0 = disabled)
        int emit_delay;    // frames to wait before emission starts
        float emit_x, emit_y, emit_z, emit_vy;  // nozzle position + downward velocity
        Sphere sphere;

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
