#include <random>
#include "particle.h"

ParticleSystem::ParticleSystem() {
    particles = std::vector<Particle>();
}

ParticleSystem::~ParticleSystem() {}

bool ParticleSystem::initialise_particles(int count) {
    particles.clear();
    particles.reserve(count);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-0.8f, 0.8f);

    for (int i = 0; i < count; i++) {
        Particle p{};
        p.x = dist(rng);
        p.y = dist(rng);
        p.z = -2.0f;
        p.vx = 0.f;
        p.vy = 0.f;
        p.vz = 0.f;
        p.fx = 0.f;
        p.fy = 0.f;
        p.fz = 0.f;
        particles.push_back(p);
    }
    return true;
}

void ParticleSystem::step(float dt) {
    const float gravity = 9.81f;

    for (Particle& p : particles) {
        p.vy -= gravity * dt;

        p.x += p.vx * dt;
        p.y += p.vy * dt;

        if (p.x < -1.f) { p.x = -1.f; p.vx *= -0.5f; }
        if (p.x > 1.f) { p.x = 1.f; p.vx *= -0.5f; }
        if (p.y < -1.f) { p.y = -1.f; p.vy *= -0.5f; }
        if (p.y > 1.f) { p.y = 1.f; p.vy *= -0.5f; }
    }
}