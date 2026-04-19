#include <iostream>
#include <math.h>
#include <random>
#include <vector>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "particle.h"

ParticleSystem::ParticleSystem(){
    particles = std::vector<Particle>();
}

ParticleSystem::~ParticleSystem(){

}

bool ParticleSystem::initialise_particles(int count){
    for (int i = 0; i < count; i++){
        Particle p{};
        p.x = (float)i * 0.1f;
        p.y = 1.0f;
        p.z = -2.0f;
        particles.push_back(p);
    }
    return true;
}

void ParticleSystem::step(float dt){
    float force = 9.81f;
    for (Particle &p: this->particles){
        p.vy -= force * dt;
        p.y -= p.vy * dt;
    }
    return;
}

