#ifndef SCENE_H
#define SCENE_H

#include "Particles.h"
#include "Primitives.h"
#include "RayTracer.h"
#include <vector>
#include <string>

struct ThrownLog {
    float start_x;
    float start_z;
    float progress;
};

struct BurstEmber {
    float x, y, z;
    float vx, vy, vz;
    float life;
};

struct ShootingStar {
    float start_x, start_y, start_z;
    float end_x, end_y, end_z;
    float progress;
    float speed;
};


class Camera {
public:
    float azimuth;
    float elevation;
    float radius;
    float tx, ty, tz;  // look-at target
    bool dragging;
    int last_mouse_x;
    int last_mouse_y;

    Camera();
    void reset();
    void eye(float& ex, float& ey, float& ez) const;
};

class ForestCampsite {
public:
    ForestCampsite();
    ~ForestCampsite();

    void run(int argc, char** argv);

    // Callbacks
    void display();
    void reshape(int width, int height);
    void idle();
    void keyboard(unsigned char key, int x, int y);
    void special_key(int key, int x, int y);
    void mouse(int button, int state, int x, int y);
    void mouse_motion(int x, int y);

    static ForestCampsite* instance;

private:
    float time;
    int last_ms;
    bool paused;
    bool fog_enabled;
    bool wind_active;
    bool sunrise_active;
    float fire_fuel;
    float smoke_puff;
    float wind_strength;
    float time_of_day;

    Camera camera;
    Quadric* quadric;
    TextureBank* textures;
    std::vector<SmokeParticle> smoke;
    std::vector<EmberParticle> embers;
    std::vector<BurstEmber> burst_embers;
    std::vector<ThrownLog> thrown_logs;
    std::vector<ShootingStar> shooting_stars;
    int window;

    // Shading / ray-tracing state
    bool phong_mode = false;      // P : Gouraud (false) <-> Phong (true)
    bool raytrace_view = false;   // G : show the CPU ray-traced frame
    bool rt_pending = false;      // ray trace requested, render after this frame
    bool show_hud = true;         // H : show/hide on-screen info
    GLuint rt_texture = 0;
    int rt_w = 0, rt_h = 0;
    float rt_seconds = 0.0f;
    size_t rt_objects = 0;

    // Optional screenshot automation: ./campsite --capture <output_dir>
    std::string capture_dir;
    int capture_step;
    int capture_start_ms;
    void capture_update();
    void apply_capture_shot(int index);
    void save_screenshot(const std::string& name);


    void init_gl();
    void draw_demo_orb();
    void draw_hud();
    void draw_raytrace_view();
    void ray_trace_render();
    void build_ray_scene(RayTracer& rt, RTEnvironment& env);
    void update_lights();

    void draw_sky();
    void draw_ground();
    void draw_contact_shadows();
    void draw_distant_forest();
    void draw_forest_details();
    void draw_tree(float x, float z, float scale, float phase, bool has_owl);
    void draw_owl();
    void draw_tent();
    void draw_campfire_base();
    void draw_fire_glow();
    void draw_flames();
    void draw_smoke();
    void draw_embers();
    void draw_fireflies();
    void draw_log_pile();
    void draw_thrown_logs();
    void draw_stumps();
    void draw_tripod();
    void throw_log();

    static float terrain_height(float x, float z);
    static void terrain_normal(float x, float z, float& nx, float& ny, float& nz);
    static float flicker(float t, float phase = 0.0f);
};

#endif // SCENE_H
