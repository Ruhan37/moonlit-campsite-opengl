#include "Particles.h"
#include <random>

std::vector<SmokeParticle> make_smoke(int count, unsigned int seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist_speed(0.105f, 0.155f);
    std::uniform_real_distribution<float> dist_offset_x(-0.18f, 0.18f);
    std::uniform_real_distribution<float> dist_offset_z(-0.15f, 0.15f);
    std::uniform_real_distribution<float> dist_drift(0.0f, 6.283f);
    std::uniform_real_distribution<float> dist_size(0.25f, 0.42f);

    std::vector<SmokeParticle> particles;
    for (int i = 0; i < count; ++i) {
        SmokeParticle p;
        p.phase = static_cast<float>(i) / count;
        p.speed = dist_speed(gen);
        p.offset_x = dist_offset_x(gen);
        p.offset_z = dist_offset_z(gen);
        p.drift_phase = dist_drift(gen);
        p.size = dist_size(gen);
        particles.push_back(p);
    }
    return particles;
}

std::vector<EmberParticle> make_embers(int count, unsigned int seed) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<float> dist_phase(0.0f, 1.0f);
    std::uniform_real_distribution<float> dist_speed(0.30f, 0.58f);
    std::uniform_real_distribution<float> dist_radius(0.05f, 0.34f);
    std::uniform_real_distribution<float> dist_angle(0.0f, 6.283f);
    std::uniform_real_distribution<float> dist_drift(0.7f, 1.5f);

    std::vector<EmberParticle> particles;
    for (int i = 0; i < count; ++i) {
        EmberParticle p;
        p.phase = dist_phase(gen);
        p.speed = dist_speed(gen);
        p.radius = dist_radius(gen);
        p.angle = dist_angle(gen);
        p.drift = dist_drift(gen);
        particles.push_back(p);
    }
    return particles;
}

const std::vector<Firefly> FIREFLIES = {
    {-3.2f, 1.5f, 1.0f, 1.2f, 0.7f, 0.70f, 0.0f},
    {2.7f, 2.1f, 2.0f, 0.8f, 1.2f, 0.53f, 0.8f},
    {4.2f, 1.3f, -1.8f, 1.4f, 0.6f, 0.82f, 1.7f},
    {-4.6f, 2.7f, -2.0f, 0.9f, 1.3f, 0.46f, 2.4f},
    {0.0f, 2.8f, 3.7f, 1.6f, 0.8f, 0.61f, 3.1f},
    {1.2f, 1.0f, -4.0f, 0.7f, 1.5f, 0.91f, 3.8f},
    {-1.6f, 2.3f, -2.8f, 1.2f, 0.9f, 0.58f, 4.5f},
    {3.4f, 3.1f, 0.3f, 0.6f, 1.0f, 0.76f, 5.2f},
    {-3.6f, 1.2f, 3.6f, 1.0f, 0.6f, 0.66f, 5.9f},
    {0.8f, 2.0f, 0.4f, 0.9f, 0.5f, 0.84f, 1.2f}
};
