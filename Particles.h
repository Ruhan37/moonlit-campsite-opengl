#ifndef PARTICLES_H
#define PARTICLES_H

#include <vector>

struct SmokeParticle {
    float phase;
    float speed;
    float offset_x;
    float offset_z;
    float drift_phase;
    float size;
};

struct EmberParticle {
    float phase;
    float speed;
    float radius;
    float angle;
    float drift;
};

struct Firefly {
    float center_x;
    float center_y;
    float center_z;
    float radius_x;
    float radius_z;
    float speed;
    float phase;
};

std::vector<SmokeParticle> make_smoke(int count = 14, unsigned int seed = 814);
std::vector<EmberParticle> make_embers(int count = 18, unsigned int seed = 912);
extern const std::vector<Firefly> FIREFLIES;

#endif // PARTICLES_H
