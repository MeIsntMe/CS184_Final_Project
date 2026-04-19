#include <iostream>
#include <math.h>
#include <random>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#pragma once



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

        std::vector<Particle> particles;
    private:
        void destroyParticle();
        
};
