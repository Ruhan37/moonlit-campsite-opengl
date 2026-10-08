#include "Scene.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>

#ifdef __APPLE__
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TAU (2.0f * M_PI)

const float NIGHT_COLOR[4] = {0.008f, 0.014f, 0.052f, 1.0f};

struct TreeData {
  float x, z, scale, phase;
  bool has_owl;
};
const TreeData MAIN_TREES[] = {
    {-4.8f, -3.3f, 1.08f, 0.0f, true}, {4.9f, -3.9f, 0.98f, 1.4f, false},
    {-5.6f, 4.0f, 0.91f, 2.7f, false}, {8.0f, 5.8f, 0.96f, 4.1f, false},
    {-1.8f, 7.0f, 0.82f, 5.0f, false}, {7.1f, 0.8f, 0.88f, 0.7f, false}};

struct ScatterData {
  float x, z, scale, angle;
};

std::vector<TreeData> generate_distant_trees() {
  std::vector<TreeData> trees;
  for (int i = 0; i < 15; ++i) {
    float x = 12.6f * std::cos(i * TAU / 15.0f);
    float z = 12.6f * std::sin(i * TAU / 15.0f);
    float scale = 0.74f + 0.16f * std::sin(i * 2.37f);
    trees.push_back({x, z, scale, 0.0f, false});
  }
  return trees;
}

std::vector<ScatterData> scatter(unsigned int seed, int count, float inner,
                                 float outer) {
  std::mt19937 gen(seed);
  std::uniform_real_distribution<float> dist_angle(0.0f, TAU);
  std::uniform_real_distribution<float> dist_radius(inner, outer);
  std::uniform_real_distribution<float> dist_scale(0.65f, 1.25f);
  std::uniform_real_distribution<float> dist_rot(0.0f, 360.0f);

  std::vector<ScatterData> items;
  while (items.size() < static_cast<size_t>(count)) {
    float angle = dist_angle(gen);
    float radius = dist_radius(gen);
    float x = std::cos(angle) * radius;
    float z = std::sin(angle) * radius;
    if (std::hypot(x - 3.2f, z - 2.5f) > 2.0f) {
      items.push_back({x, z, dist_scale(gen), dist_rot(gen)});
    }
  }
  return items;
}

static std::vector<TreeData> DISTANT_TREES;
static std::vector<ScatterData> GRASS_TUFTS;
static std::vector<ScatterData> FOREST_ROCKS;

// Camera
Camera::Camera()
    : azimuth(35.0f), elevation(24.0f), radius(16.8f), tx(0.0f), ty(1.7f),
      tz(0.0f), dragging(false), last_mouse_x(0), last_mouse_y(0) {}
void Camera::reset() {
  azimuth = 35.0f;
  elevation = 24.0f;
  radius = 16.8f;
  tx = 0.0f;
  ty = 1.7f;
  tz = 0.0f;
}
void Camera::eye(float &ex, float &ey, float &ez) const {
  float az = azimuth * M_PI / 180.0f;
  float el = elevation * M_PI / 180.0f;
  ex = tx + radius * std::cos(el) * std::sin(az);
  ey = (ty - 1.2f) + radius * std::sin(el);
  ez = tz + radius * std::cos(el) * std::cos(az);
}

ForestCampsite *ForestCampsite::instance = nullptr;

// Callbacks wrappers
extern "C" {
void displayFunc() {
  if (ForestCampsite::instance)
    ForestCampsite::instance->display();
}
void reshapeFunc(int w, int h) {
  if (ForestCampsite::instance)
    ForestCampsite::instance->reshape(w, h);
}
void idleFunc() {
  if (ForestCampsite::instance)
    ForestCampsite::instance->idle();
}
void keyboardFunc(unsigned char key, int x, int y) {
  if (ForestCampsite::instance)
    ForestCampsite::instance->keyboard(key, x, y);
}
void specialFunc(int key, int x, int y) {
  if (ForestCampsite::instance)
    ForestCampsite::instance->special_key(key, x, y);
}
void mouseFunc(int button, int state, int x, int y) {
  if (ForestCampsite::instance)
    ForestCampsite::instance->mouse(button, state, x, y);
}
void motionFunc(int x, int y) {
  if (ForestCampsite::instance)
    ForestCampsite::instance->mouse_motion(x, y);
}
}

ForestCampsite::ForestCampsite()
    : time(0.0f), last_ms(0), paused(false), fog_enabled(true),
      wind_active(false), sunrise_active(false), fire_fuel(1.0f),
      smoke_puff(0.0f), wind_strength(0.0f), time_of_day(0.0f),
      quadric(nullptr), textures(nullptr), window(0), capture_step(-1),
      capture_start_ms(0) {
  std::srand(std::random_device{}());
  smoke = make_smoke();
  embers = make_embers();
  instance = this;
  DISTANT_TREES = generate_distant_trees();
  GRASS_TUFTS = scatter(718, 52, 2.2f, 10.8f);
  FOREST_ROCKS = scatter(904, 19, 2.7f, 10.2f);
}

ForestCampsite::~ForestCampsite() {
  if (quadric)
    delete quadric;
  if (textures)
    delete textures;
  instance = nullptr;
}

void ForestCampsite::run(int argc, char **argv) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::string(argv[i]) == "--capture")
      capture_dir = argv[i + 1];
  }
  glutInit(&argc, argv);
#ifdef GLUT_MULTISAMPLE
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_MULTISAMPLE);
#else
  glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
#endif
  glutInitWindowSize(1280, 800);
  glutInitWindowPosition(70, 45);
  window = glutCreateWindow("Moonlit Forest Campsite - C++ OpenGL");

  init_gl();

  glutDisplayFunc(displayFunc);
  glutReshapeFunc(reshapeFunc);
  glutKeyboardFunc(keyboardFunc);
  glutSpecialFunc(specialFunc);
  glutMouseFunc(mouseFunc);
  glutMotionFunc(motionFunc);
  glutIdleFunc(idleFunc);

  std::cout << "Controls: drag/arrow keys orbit, +/- zoom, L throw log, W "
               "wind, T sunrise, Space pause, F fog, R reset, "
               "P Gouraud<->Phong shading, G ray-trace view, H HUD, Q quit\n";
  glutMainLoop();
}

void ForestCampsite::init_gl() {
  glClearColor(NIGHT_COLOR[0], NIGHT_COLOR[1], NIGHT_COLOR[2], NIGHT_COLOR[3]);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_LIGHT1);
  glEnable(GL_NORMALIZE);
  glEnable(GL_COLOR_MATERIAL);
  glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
  glShadeModel(GL_SMOOTH);
#ifdef GL_MULTISAMPLE
  glEnable(GL_MULTISAMPLE);
#endif

  float ambient_model[] = {0.060f, 0.068f, 0.105f, 1.0f};
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient_model);

  float light0_ambient[] = {0.055f, 0.018f, 0.003f, 1.0f};
  glLightfv(GL_LIGHT0, GL_AMBIENT, light0_ambient);
  glLightf(GL_LIGHT0, GL_CONSTANT_ATTENUATION, 1.0f);
  glLightf(GL_LIGHT0, GL_LINEAR_ATTENUATION, 0.075f);
  glLightf(GL_LIGHT0, GL_QUADRATIC_ATTENUATION, 0.026f);

  float light1_ambient[] = {0.018f, 0.024f, 0.055f, 1.0f};
  glLightfv(GL_LIGHT1, GL_AMBIENT, light1_ambient);

  glFogi(GL_FOG_MODE, GL_EXP2);
  float fog_color[] = {0.022f, 0.030f, 0.073f, 1.0f};
  glFogfv(GL_FOG_COLOR, fog_color);
  glFogf(GL_FOG_DENSITY, 0.022f);
  glHint(GL_FOG_HINT, GL_DONT_CARE);
  glEnable(GL_FOG);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  quadric = new Quadric();
  textures = new TextureBank();

  if (!shading_init())
    std::cerr << "Phong shader unavailable - staying in Gouraud mode.\n";
}

float ForestCampsite::terrain_height(float x, float z) {
  float radius = std::hypot(x, z);
  float clearing_blend = std::min(1.0f, std::max(0.0f, (radius - 3.8f) / 4.5f));
  float waves = 0.16f * std::sin(x * 0.38f) * std::cos(z * 0.29f) +
                0.07f * std::sin(x * 0.91f + z * 0.54f);
  return waves * clearing_blend;
}

void ForestCampsite::terrain_normal(float x, float z, float &nx, float &ny,
                                    float &nz) {
  float step = 0.08f;
  float dx = terrain_height(x - step, z) - terrain_height(x + step, z);
  float dz = terrain_height(x, z - step) - terrain_height(x, z + step);
  float length = std::sqrt(dx * dx + (2.0f * step) * (2.0f * step) + dz * dz);
  nx = dx / length;
  ny = 2.0f * step / length;
  nz = dz / length;
}

float ForestCampsite::flicker(float t, float phase) {
  return 0.54f * std::sin(t * 12.7f + phase) +
         0.29f * std::sin(t * 21.3f + phase * 2.1f) +
         0.17f * std::sin(t * 37.9f + phase * 0.7f);
}

void ForestCampsite::update_lights() {
  float base_intensity =
      std::max(0.60f, std::min(1.08f, 0.86f + 0.12f * std::sin(time * 10.0f) +
                                          0.10f * flicker(time, 0.9f)));
  float intensity = base_intensity * std::max(0.1f, fire_fuel);

  // Color shift: bright yellow/orange (high fuel) to deep red (low fuel)
  float t =
      std::max(0.0f, std::min(1.0f, fire_fuel * 2.0f)); // 1.0 when fuel > 0.5
  float r = 1.0f * t + 0.8f * (1.0f - t);
  float g = 0.43f * t + 0.05f * (1.0f - t);
  float b = 0.12f * t + 0.0f * (1.0f - t);

  float l0_pos[] = {0.0f, 1.08f, 0.0f, 1.0f};
  glLightfv(GL_LIGHT0, GL_POSITION, l0_pos);
  float l0_diff[] = {r * intensity, g * intensity, b * intensity, 1.0f};
  float l0_spec[] = {0.82f * intensity * t, 0.31f * intensity * t,
                     0.06f * intensity * t, 1.0f};
  glLightfv(GL_LIGHT0, GL_DIFFUSE, l0_diff);
  glLightfv(GL_LIGHT0, GL_SPECULAR, l0_spec);

  float l1_pos[] = {0.24f - time_of_day * 0.24f, 0.82f + time_of_day * 0.18f,
                    0.18f + time_of_day * 0.82f, 0.0f};
  glLightfv(GL_LIGHT1, GL_POSITION, l1_pos);

  // Global ambient
  float amb_r = 0.060f * (1.0f - time_of_day) + 0.3f * time_of_day;
  float amb_g = 0.068f * (1.0f - time_of_day) + 0.3f * time_of_day;
  float amb_b = 0.105f * (1.0f - time_of_day) + 0.3f * time_of_day;
  float ambient_model[] = {amb_r, amb_g, amb_b, 1.0f};
  glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient_model);

  // Moonlight / Sunlight
  float l1_r = 0.20f * (1.0f - time_of_day) + 0.95f * time_of_day;
  float l1_g = 0.27f * (1.0f - time_of_day) + 0.85f * time_of_day;
  float l1_b = 0.46f * (1.0f - time_of_day) + 0.65f * time_of_day;
  float l1_diff[] = {l1_r, l1_g, l1_b, 1.0f};
  glLightfv(GL_LIGHT1, GL_DIFFUSE, l1_diff);

  float l1_spec_r = 0.06f * (1.0f - time_of_day) + 0.6f * time_of_day;
  float l1_spec_g = 0.08f * (1.0f - time_of_day) + 0.6f * time_of_day;
  float l1_spec_b = 0.14f * (1.0f - time_of_day) + 0.6f * time_of_day;
  float l1_spec[] = {l1_spec_r, l1_spec_g, l1_spec_b, 1.0f};
  glLightfv(GL_LIGHT1, GL_SPECULAR, l1_spec);

  // Update fog dynamically based on time of day
  float fog_r = NIGHT_COLOR[0] * (1.0f - time_of_day) + 0.5f * time_of_day;
  float fog_g = NIGHT_COLOR[1] * (1.0f - time_of_day) + 0.6f * time_of_day;
  float fog_b = NIGHT_COLOR[2] * (1.0f - time_of_day) + 0.8f * time_of_day;
  float fog_color[] = {fog_r, fog_g, fog_b, 1.0f};
  glFogfv(GL_FOG_COLOR, fog_color);
  float fog_density = 0.020f * (1.0f - time_of_day) + 0.02f * time_of_day;
  glFogf(GL_FOG_DENSITY, fog_density);
}

void ForestCampsite::display() {
  if (raytrace_view) {
    // Ray-traced frame is shown instead of the rasterised scene.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    draw_raytrace_view();
    draw_hud();
    capture_update();
    glutSwapBuffers();
    return;
  }

  float bg_r = NIGHT_COLOR[0] * (1.0f - time_of_day) + 0.45f * time_of_day;
  float bg_g = NIGHT_COLOR[1] * (1.0f - time_of_day) + 0.65f * time_of_day;
  float bg_b = NIGHT_COLOR[2] * (1.0f - time_of_day) + 0.85f * time_of_day;
  glClearColor(bg_r, bg_g, bg_b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  float ex, ey, ez;
  camera.eye(ex, ey, ez);
  gluLookAt(ex, ey, ez, camera.tx, camera.ty, camera.tz, 0.0, 1.0, 0.0);

  update_lights();

  // Select Gouraud (fixed-function, per-vertex) or Phong (GLSL, per-pixel)
  shading_use(phong_mode);

  draw_sky();
  draw_ground();
  draw_contact_shadows();
  draw_distant_forest();
  draw_forest_details();

  for (const auto &t : MAIN_TREES) {
    draw_tree(t.x, t.z, t.scale, t.phase, t.has_owl);
  }

  draw_tent();
  draw_campfire_base();
  draw_log_pile();
  draw_stumps();
  draw_tripod();
  draw_demo_orb();

  draw_fire_glow();
  draw_flames();
  draw_thrown_logs();
  draw_embers();
  draw_smoke();
  draw_fireflies();

  float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
  material(white, NO_EMISSION);
  shading_use(false);
  draw_hud();
  capture_update();
  glutSwapBuffers();

  if (rt_pending) {
    rt_pending = false;
    ray_trace_render();
    raytrace_view = true;
    glutPostRedisplay();
  }
}

void ForestCampsite::draw_sky() {
  glDisable(GL_LIGHTING);
  glDisable(GL_FOG);
  glDepthMask(GL_FALSE);
  glPointSize(2.2f);
  float star_alpha = std::max(0.0f, 0.88f - time_of_day * 4.0f);
  glColor4f(0.72f, 0.80f, 1.0f, star_alpha);
  glBegin(GL_POINTS);
  for (int i = 0; i < 90; ++i) {
    float angle = i * 2.399963f;
    float radius = 15.0f + (i % 7) * 1.2f;
    float y = 6.5f + ((i * 17) % 37) * 0.16f;
    glVertex3f(std::cos(angle) * radius, y, std::sin(angle) * radius);
  }
  glEnd();

  glEnable(GL_LIGHTING);

  // Moon: sinks below the horizon as the morning arrives
  float moon_t = std::min(1.0f, time_of_day / 0.6f);
  if (moon_t < 1.0f) {
    float col[] = {0.82f, 0.86f, 0.98f, 1.0f};
    float em[] = {0.28f + moon_t * 0.2f, 0.32f + moon_t * 0.2f,
                  0.46f + moon_t * 0.2f, 1.0f};
    material(col, em, 0.4f, 30.0f);

    glPushMatrix();
    glTranslatef(-8.2f, 8.8f - moon_t * 12.0f, -9.5f);
    glutSolidSphere(0.78, 30, 20);
    glPopMatrix();
  }

  // Sun: rises from the horizon on the opposite side of the sky
  float sun_t = std::max(0.0f, std::min(1.0f, (time_of_day - 0.1f) / 0.9f));
  if (sun_t > 0.0f) {
    float sun_y = -3.0f + sun_t * 12.0f;
    // Deep orange near the horizon, pale yellow when high
    float sr = 1.0f;
    float sg = 0.50f + 0.40f * sun_t;
    float sb = 0.15f + 0.45f * sun_t;

    glDisable(GL_LIGHTING);
    glPushMatrix();
    glTranslatef(8.5f, sun_y, -10.5f);

    glColor4f(sr, sg, sb, 1.0f);
    glutSolidSphere(1.05, 32, 22);

    // Soft additive glow halos
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glDepthMask(GL_FALSE);
    for (int i = 1; i <= 4; ++i) {
      glColor4f(sr, sg * 0.9f, sb * 0.7f, 0.16f / i);
      glutSolidSphere(1.05 + i * 0.55, 28, 18);
    }
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glPopMatrix();
    glEnable(GL_LIGHTING);
  }

  // Shooting stars
  glDisable(GL_LIGHTING);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glLineWidth(2.0f);
  glBegin(GL_LINES);
  for (const auto &star : shooting_stars) {
    float alpha = std::sin(star.progress * M_PI) * star_alpha;
    glColor4f(1.0f, 1.0f, 1.0f, alpha);
    float x1 = star.start_x + (star.end_x - star.start_x) * star.progress;
    float y1 = star.start_y + (star.end_y - star.start_y) * star.progress;
    float z1 = star.start_z + (star.end_z - star.start_z) * star.progress;

    float tail_len = 0.15f;
    float x2 = star.start_x + (star.end_x - star.start_x) *
                                  std::max(0.0f, star.progress - tail_len);
    float y2 = star.start_y + (star.end_y - star.start_y) *
                                  std::max(0.0f, star.progress - tail_len);
    float z2 = star.start_z + (star.end_z - star.start_z) *
                                  std::max(0.0f, star.progress - tail_len);

    glVertex3f(x1, y1, z1);
    glColor4f(1.0f, 1.0f, 1.0f, 0.0f);
    glVertex3f(x2, y2, z2);
  }
  glEnd();
  glDisable(GL_BLEND);

  float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
  material(white, NO_EMISSION);
  glDepthMask(GL_TRUE);
  if (fog_enabled)
    glEnable(GL_FOG);
}

void ForestCampsite::draw_ground() {
  float col[] = {0.86f, 0.90f, 0.76f, 1.0f};
  material(col, NO_EMISSION, 0.03f, 2.0f);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, textures->ground);

  float size = 16.0f;
  int cells = 44;
  float step = size * 2.0f / cells;

  for (int row = 0; row < cells; ++row) {
    float z0 = -size + row * step;
    float z1 = z0 + step;
    glBegin(GL_TRIANGLE_STRIP);
    for (int column = 0; column <= cells; ++column) {
      float x = -size + column * step;
      float zs[2] = {z0, z1};
      for (int i = 0; i < 2; ++i) {
        float z = zs[i];
        float nx, ny, nz;
        terrain_normal(x, z, nx, ny, nz);
        glNormal3f(nx, ny, nz);
        glTexCoord2f((x + size) * 0.42f, (z + size) * 0.42f);
        glVertex3f(x, terrain_height(x, z), z);
      }
    }
    glEnd();
  }
  glDisable(GL_TEXTURE_2D);
}

void ForestCampsite::draw_contact_shadows() {
  glDisable(GL_LIGHTING);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  for (const auto &t : MAIN_TREES) {
    shadow_ellipse(t.x + 0.7f, t.z + 0.35f, 1.15f * t.scale, 0.52f * t.scale,
                   0.23f, 22.0f);
  }
  shadow_ellipse(3.2f, 2.7f, 1.85f, 1.16f, 0.24f, -25.0f);
  shadow_ellipse(0.15f, 0.12f, 1.25f, 0.72f, 0.16f);
  glEnable(GL_LIGHTING);
}

void ForestCampsite::draw_distant_forest() {
  for (const auto &t : DISTANT_TREES) {
    float y = terrain_height(t.x, t.z);
    glPushMatrix();
    glTranslatef(t.x, y, t.z);
    glScalef(t.scale, t.scale, t.scale);

    float mat1[] = {0.10f, 0.072f, 0.047f, 1.0f};
    material(mat1, NO_EMISSION, 0.02f);
    cylinder_y(*quadric, 0.25f, 2.5f, 0.18f, 12);

    float mat2[] = {0.025f, 0.105f, 0.060f, 1.0f};
    material(mat2, NO_EMISSION, 0.02f);

    float rads[] = {1.15f, 0.92f, 0.67f};
    for (int layer = 0; layer < 3; ++layer) {
      glPushMatrix();
      glTranslatef(0.0f, 1.9f + layer * 0.72f, 0.0f);
      cone_y(*quadric, rads[layer], 1.65f, 14);
      glPopMatrix();
    }
    glPopMatrix();
  }
}

void ForestCampsite::draw_tree(float x, float z, float scale, float phase,
                               bool has_owl) {
  float y = terrain_height(x, z);
  glPushMatrix();
  glTranslatef(x, y, z);
  glScalef(scale, scale, scale);

  float mat1[] = {0.60f, 0.49f, 0.36f, 1.0f};
  material(mat1, NO_EMISSION, 0.05f, 4.0f);

  for (int index = 0; index < 5; ++index) {
    glPushMatrix();
    glRotatef(index * 72.0f + 13.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(-72.0f, 0.0f, 0.0f, 1.0f);
    cylinder_y(*quadric, 0.10f, 0.78f, 0.035f, 10, 4, textures->bark);
    glPopMatrix();
  }

  cylinder_y(*quadric, 0.38f, 3.45f, 0.25f, 24, 7, textures->bark);

  struct Branch {
    float y, angle, length;
  };
  const Branch branches[] = {
      {1.85f, -68.0f, 1.42f}, {2.20f, 67.0f, 0.90f}, {2.72f, -70.0f, 0.75f}};
  for (const auto &b : branches) {
    glPushMatrix();
    glTranslatef(0.0f, b.y, 0.0f);
    glRotatef(b.angle, 0.0f, 0.0f, 1.0f);
    cylinder_y(*quadric, 0.11f, b.length, 0.045f, 12, 4, textures->bark);
    glPopMatrix();
  }

  if (has_owl) {
    glPushMatrix();
    glTranslatef(1.28f, 2.78f, 0.02f);
    draw_owl();
    glPopMatrix();
  }

  glPushMatrix();
  glTranslatef(0.0f, 2.55f, 0.0f);
  glRotatef((2.8f + wind_strength * 5.0f) * std::sin(time * 1.22f + phase) +
                wind_strength * 6.0f,
            0.0f, 0.0f, 1.0f);

  struct Layer {
    float h, r, ch, col[3];
  };
  const Layer layers[] = {{0.00f, 1.43f, 1.95f, {0.060f, 0.31f, 0.13f}},
                          {0.72f, 1.20f, 1.85f, {0.065f, 0.36f, 0.15f}},
                          {1.40f, 0.94f, 1.62f, {0.075f, 0.40f, 0.17f}},
                          {2.00f, 0.66f, 1.28f, {0.085f, 0.43f, 0.18f}}};

  for (int index = 0; index < 4; ++index) {
    const auto &l = layers[index];
    glPushMatrix();
    glTranslatef(0.06f * std::sin(phase + index), l.h,
                 0.04f * std::cos(phase + index));
    material(l.col, NO_EMISSION, 0.04f, 3.0f);
    cone_y(*quadric, l.r, l.ch, 24);
    glPopMatrix();
  }
  glPopMatrix();
  glPopMatrix();
}

void ForestCampsite::draw_owl() {
  float bob = 0.028f * std::sin(time * 2.0f);
  glPushMatrix();
  glTranslatef(0.0f, bob, 0.0f);

  float mat1[] = {0.24f, 0.15f, 0.09f, 1.0f};
  material(mat1, NO_EMISSION, 0.03f);
  glPushMatrix();
  glTranslatef(0.0f, -0.18f, -0.18f);
  glRotatef(18.0f, 1.0f, 0.0f, 0.0f);
  scaled_cube(0.24f, 0.48f, 0.12f);
  glPopMatrix();

  float mat2[] = {0.50f, 0.34f, 0.20f, 1.0f};
  material(mat2, NO_EMISSION, 0.06f, 5.0f);
  scaled_sphere(0.36f, 0.51f, 0.31f, 24, 18);

  float flap = 12.0f + 14.0f * std::sin(time * 4.6f);
  float sides[] = {-1.0f, 1.0f};
  for (float side : sides) {
    glPushMatrix();
    glTranslatef(side * 0.29f, 0.06f, -0.015f);
    glRotatef(side * flap, 0.0f, 0.0f, 1.0f);
    float mat3[] = {0.27f, 0.17f, 0.10f, 1.0f};
    material(mat3, NO_EMISSION, 0.03f);
    scaled_sphere(0.16f, 0.43f, 0.24f, 18, 12);
    glPopMatrix();
  }

  glPushMatrix();
  glTranslatef(0.0f, 0.46f, 0.02f);
  glRotatef(25.0f * std::sin(time * 1.08f), 0.0f, 1.0f, 0.0f);

  float mat4[] = {0.63f, 0.47f, 0.29f, 1.0f};
  material(mat4, NO_EMISSION, 0.05f);
  scaled_sphere(0.32f, 0.29f, 0.30f, 24, 18);

  for (float side : sides) {
    glPushMatrix();
    glTranslatef(side * 0.19f, 0.21f, -0.01f);
    glRotatef(side * -12.0f, 0.0f, 0.0f, 1.0f);
    float mat5[] = {0.30f, 0.19f, 0.11f, 1.0f};
    material(mat5, NO_EMISSION, 0.02f);
    cone_y(*quadric, 0.085f, 0.24f, 10);
    glPopMatrix();
  }

  for (float side : sides) {
    glPushMatrix();
    glTranslatef(side * 0.12f, 0.035f, 0.252f);

    float mat6[] = {0.70f, 0.62f, 0.44f, 1.0f};
    material(mat6, NO_EMISSION, 0.04f);
    scaled_sphere(0.115f, 0.125f, 0.045f, 16, 10);

    glTranslatef(0.0f, 0.0f, 0.041f);
    float mat7[] = {0.92f, 0.68f, 0.10f, 1.0f};
    float em1[] = {0.12f, 0.07f, 0.0f, 1.0f};
    material(mat7, em1, 0.45f, 40.0f);
    scaled_sphere(0.052f, 0.058f, 0.025f, 14, 9);

    glTranslatef(0.0f, 0.0f, 0.021f);
    float mat8[] = {0.018f, 0.012f, 0.008f, 1.0f};
    material(mat8, NO_EMISSION, 0.4f, 50.0f);
    scaled_sphere(0.020f, 0.028f, 0.012f, 10, 8);
    glPopMatrix();
  }

  float mat9[] = {0.74f, 0.42f, 0.09f, 1.0f};
  material(mat9, NO_EMISSION, 0.08f);
  glBegin(GL_TRIANGLES);
  glNormal3f(0.0f, -0.15f, 1.0f);
  glVertex3f(-0.055f, -0.035f, 0.285f);
  glVertex3f(0.055f, -0.035f, 0.285f);
  glVertex3f(0.0f, -0.13f, 0.41f);
  glEnd();
  glPopMatrix();

  float mat10[] = {0.52f, 0.35f, 0.10f, 1.0f};
  material(mat10, NO_EMISSION, 0.1f);
  for (float side : sides) {
    float toes[] = {-0.045f, 0.045f};
    for (float toe : toes) {
      glPushMatrix();
      glTranslatef(side * 0.12f + toe, -0.43f, 0.08f);
      glRotatef(70.0f, 1.0f, 0.0f, 0.0f);
      cylinder_y(*quadric, 0.018f, 0.16f, 0.008f, 7);
      glPopMatrix();
    }
  }
  glPopMatrix();
}

void ForestCampsite::draw_forest_details() {
  for (const auto &r : FOREST_ROCKS) {
    float y = terrain_height(r.x, r.z);
    glPushMatrix();
    glTranslatef(r.x, y + 0.08f * r.scale, r.z);
    glRotatef(r.angle, 0.0f, 1.0f, 0.0f);
    float mat1[] = {0.19f, 0.21f, 0.20f, 1.0f};
    material(mat1, NO_EMISSION, 0.10f, 7.0f);
    scaled_sphere(0.30f * r.scale, 0.13f * r.scale, 0.22f * r.scale, 12, 8);
    glPopMatrix();
  }

  glDisable(GL_LIGHTING);
  glColor4f(0.08f, 0.24f, 0.10f, 1.0f);
  for (const auto &g : GRASS_TUFTS) {
    float y = terrain_height(g.x, g.z);
    glPushMatrix();
    glTranslatef(g.x, y, g.z);
    glRotatef(g.angle, 0.0f, 1.0f, 0.0f);
    glScalef(g.scale, g.scale, g.scale);
    glBegin(GL_TRIANGLES);
    float offsets[] = {-0.13f, 0.0f, 0.13f};
    for (float offset : offsets) {
      glVertex3f(offset - 0.04f, 0.0f, 0.0f);
      glVertex3f(offset + 0.04f, 0.0f, 0.0f);
      glVertex3f(offset + 0.02f, 0.44f, 0.02f);
      glVertex3f(0.0f, 0.0f, offset - 0.04f);
      glVertex3f(0.0f, 0.0f, offset + 0.04f);
      glVertex3f(0.02f, 0.38f, offset);
    }
    glEnd();
    glPopMatrix();
  }
  glEnable(GL_LIGHTING);
}

void ForestCampsite::draw_tent() {
  glPushMatrix();
  glTranslatef(3.25f, 0.05f, 2.65f);
  glRotatef(-28.0f, 0.0f, 1.0f, 0.0f);

  float mat1[] = {0.82f, 0.85f, 0.58f, 1.0f};
  material(mat1, NO_EMISSION, 0.05f, 5.0f);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, textures->canvas);

  glBegin(GL_QUADS);
  const float p1_norm[] = {-0.79f, 0.61f, 0.0f};
  glNormal3f(p1_norm[0], p1_norm[1], p1_norm[2]);
  const float p1_v[][3] = {{-1.15f, 0.0f, -1.5f},
                           {0.0f, 1.5f, -1.5f},
                           {0.0f, 1.5f, 1.5f},
                           {-1.15f, 0.0f, 1.5f}};
  const float uvs[][2] = {
      {0.0f, 0.0f}, {2.0f, 0.0f}, {2.0f, 3.0f}, {0.0f, 3.0f}};
  for (int i = 0; i < 4; ++i) {
    glTexCoord2f(uvs[i][0], uvs[i][1]);
    glVertex3f(p1_v[i][0], p1_v[i][1], p1_v[i][2]);
  }

  const float p2_norm[] = {0.79f, 0.61f, 0.0f};
  glNormal3f(p2_norm[0], p2_norm[1], p2_norm[2]);
  const float p2_v[][3] = {{0.0f, 1.5f, -1.5f},
                           {1.15f, 0.0f, -1.5f},
                           {1.15f, 0.0f, 1.5f},
                           {0.0f, 1.5f, 1.5f}};
  for (int i = 0; i < 4; ++i) {
    glTexCoord2f(uvs[i][0], uvs[i][1]);
    glVertex3f(p2_v[i][0], p2_v[i][1], p2_v[i][2]);
  }
  glEnd();

  glBegin(GL_TRIANGLES);
  glNormal3f(0.0f, 0.0f, 1.0f);
  glTexCoord2f(0.0f, 0.0f);
  glVertex3f(-1.15f, 0.0f, 1.5f);
  glTexCoord2f(2.0f, 0.0f);
  glVertex3f(1.15f, 0.0f, 1.5f);
  glTexCoord2f(1.0f, 1.8f);
  glVertex3f(0.0f, 1.5f, 1.5f);
  glEnd();
  glDisable(GL_TEXTURE_2D);

  float mat2[] = {0.025f, 0.030f, 0.025f, 1.0f};
  material(mat2, NO_EMISSION, 0.0f);
  glBegin(GL_TRIANGLES);
  glNormal3f(0.0f, 0.0f, -1.0f);
  glVertex3f(-0.72f, 0.02f, -1.515f);
  glVertex3f(0.72f, 0.02f, -1.515f);
  glVertex3f(0.0f, 1.17f, -1.515f);
  glEnd();

  float mat3[] = {0.49f, 0.53f, 0.31f, 1.0f};
  material(mat3, NO_EMISSION, 0.03f);
  glBegin(GL_TRIANGLES);
  glNormal3f(0.0f, 0.0f, -1.0f);
  glVertex3f(-1.13f, 0.0f, -1.525f);
  glVertex3f(-0.73f, 0.02f, -1.525f);
  glVertex3f(0.0f, 1.48f, -1.525f);
  glVertex3f(0.73f, 0.02f, -1.525f);
  glVertex3f(1.13f, 0.0f, -1.525f);
  glVertex3f(0.0f, 1.48f, -1.525f);
  glEnd();

  float mat4[] = {0.24f, 0.19f, 0.11f, 1.0f};
  material(mat4, NO_EMISSION, 0.08f);
  glPushMatrix();
  glTranslatef(0.0f, 1.5f, -1.62f);
  glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
  cylinder_y(*quadric, 0.035f, 3.24f, -1.0f, 9);
  glPopMatrix();

  glPushMatrix();
  glTranslatef(0.0f, 0.0f, -1.62f);
  cylinder_y(*quadric, 0.035f, 1.58f, -1.0f, 9);
  glPopMatrix();

  glDisable(GL_LIGHTING);
  glLineWidth(1.2f);
  glColor4f(0.55f, 0.51f, 0.34f, 0.9f);
  glBegin(GL_LINES);
  glVertex3f(0.0f, 1.49f, -1.60f);
  glVertex3f(0.0f, 0.02f, -2.45f);
  glVertex3f(0.0f, 1.49f, 1.60f);
  glVertex3f(0.0f, 0.02f, 2.45f);
  glEnd();
  glEnable(GL_LIGHTING);

  glPopMatrix();
}

void ForestCampsite::draw_campfire_base() {
  for (int index = 0; index < 13; ++index) {
    float angle = index * TAU / 13.0f;
    float radius = 0.88f + 0.035f * std::sin(index * 2.1f);
    glPushMatrix();
    glTranslatef(std::cos(angle) * radius, 0.13f, std::sin(angle) * radius);
    glRotatef(index * 37.0f, 0.0f, 1.0f, 0.0f);
    float mat1[] = {0.28f, 0.27f, 0.24f, 1.0f};
    material(mat1, NO_EMISSION, 0.14f, 10.0f);
    scaled_sphere(0.27f, 0.17f, 0.23f, 14, 9);
    glPopMatrix();
  }

  float log_angles[] = {18.0f, 90.0f, 162.0f, 54.0f, 126.0f};
  for (int index = 0; index < 5; ++index) {
    glPushMatrix();
    glTranslatef(0.0f, 0.22f + (index % 2) * 0.11f, 0.0f);
    glRotatef(log_angles[index], 0.0f, 1.0f, 0.0f);
    glTranslatef(-0.73f, 0.0f, 0.0f);
    glRotatef(-90.0f, 0.0f, 0.0f, 1.0f);
    float mat2[] = {0.72f, 0.48f, 0.27f, 1.0f};
    material(mat2, NO_EMISSION, 0.04f);
    cylinder_y(*quadric, 0.13f, 1.46f, -1.0f, 16, 4, textures->bark);
    glPopMatrix();
  }

  float mat3[] = {0.90f, 0.18f, 0.02f, 1.0f};
  float em1[] = {0.35f, 0.035f, 0.0f, 1.0f};
  material(mat3, em1, 0.05f);
  for (int index = 0; index < 17; ++index) {
    float angle = index * 2.31f;
    float radius = 0.11f + (index % 5) * 0.075f;
    glPushMatrix();
    glTranslatef(std::cos(angle) * radius, 0.26f, std::sin(angle) * radius);
    glutSolidSphere(0.055f + (index % 3) * 0.012f, 8, 6);
    glPopMatrix();
  }
  float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
  material(white, NO_EMISSION);
}

void ForestCampsite::draw_fire_glow() {
  float pulse =
      (0.88f + 0.08f * std::sin(time * 8.0f) + 0.05f * flicker(time)) *
      std::max(0.1f, fire_fuel);
  glDisable(GL_LIGHTING);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(GL_FALSE);
  glBegin(GL_TRIANGLE_FAN);
  float t = std::max(0.0f, std::min(1.0f, fire_fuel * 2.0f));
  float r = 1.0f * t + 0.8f * (1.0f - t);
  float g = 0.25f * t + 0.05f * (1.0f - t);
  float b = 0.025f * t + 0.0f * (1.0f - t);
  glColor4f(r, g, b, 0.22f);
  glVertex3f(0.0f, 0.035f, 0.0f);
  for (int index = 0; index <= 32; ++index) {
    float angle = index * TAU / 32.0f;
    glColor4f(r, g * 0.48f, b * 0.4f, 0.0f);
    glVertex3f(std::cos(angle) * 2.15f * pulse, 0.035f,
               std::sin(angle) * 2.15f * pulse);
  }
  glEnd();
  glDepthMask(GL_TRUE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  glEnable(GL_LIGHTING);
}

void ForestCampsite::draw_flames() {
  struct Layer {
    float x, z, r, h, col[4], phase;
  };
  const Layer layers[] = {
      {0.00f, 0.00f, 0.52f, 1.55f, {1.00f, 0.12f, 0.01f, 0.62f}, 0.0f},
      {-0.18f, 0.10f, 0.37f, 1.25f, {1.00f, 0.35f, 0.015f, 0.70f}, 1.3f},
      {0.19f, 0.08f, 0.32f, 1.10f, {1.00f, 0.58f, 0.035f, 0.75f}, 2.6f},
      {0.03f, -0.16f, 0.25f, 0.88f, {1.00f, 0.82f, 0.12f, 0.82f}, 3.9f},
      {-0.06f, -0.02f, 0.17f, 0.67f, {1.00f, 0.96f, 0.48f, 0.90f}, 5.2f}};

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(GL_FALSE);
  float fuel_scale = std::max(0.01f, fire_fuel);
  for (const auto &l : layers) {
    float base_pulse = 1.0f + 0.17f * flicker(time, l.phase);
    float lean = 4.0f * std::sin(time * 7.0f + l.phase) + wind_strength * 20.0f;
    glPushMatrix();
    glTranslatef(l.x, 0.29f, l.z);
    glRotatef(lean, 0.0f, 0.0f, 1.0f);
    glScalef((1.0f / base_pulse) * fuel_scale, base_pulse * fuel_scale,
             (1.0f / base_pulse) * fuel_scale);

    float em[] = {l.col[0] * 0.45f, l.col[1] * 0.28f, 0.01f, 1.0f};
    material(l.col, em, 0.0f);
    cone_y(*quadric, l.r, l.h, 22);
    glPopMatrix();
  }
  glDepthMask(GL_TRUE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
  float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
  material(white, NO_EMISSION);
}

void ForestCampsite::draw_smoke() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthMask(GL_FALSE);

  std::vector<std::pair<float, const SmokeParticle *>> states;
  for (const auto &particle : smoke) {
    float life = std::fmod(particle.phase + time * particle.speed, 1.0f);
    if (life < 0)
      life += 1.0f;
    states.push_back({life, &particle});
  }
  std::sort(states.begin(), states.end(),
            [](const auto &a, const auto &b) { return a.first > b.first; });

  for (const auto &state : states) {
    float life = state.first;
    const SmokeParticle *p = state.second;
    float curl_x = std::sin(time * 0.72f + p->drift_phase + life * 3.0f);
    float curl_z = std::cos(time * 0.61f + p->drift_phase + life * 2.2f);
    float x = p->offset_x + curl_x * (0.10f + life * 0.44f) +
              wind_strength * life * 2.0f;
    float z = p->offset_z + curl_z * (0.08f + life * 0.30f);
    float y = 1.08f + life * 4.25f;
    float size = p->size * (0.75f + life * 1.20f);
    float alpha =
        0.22f * std::pow(1.0f - life, 1.4f) * (1.0f + smoke_puff * 3.0f);

    glPushMatrix();
    glTranslatef(x, y, z);
    float col[] = {0.46f, 0.48f, 0.52f, alpha};
    material(col, NO_EMISSION, 0.0f);
    scaled_sphere(size, size * 0.72f, size, 14, 10);
    glPopMatrix();
  }
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
}

void ForestCampsite::draw_embers() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(GL_FALSE);

  for (const auto &p : embers) {
    float life = std::fmod(p.phase + time * p.speed, 1.0f);
    if (life < 0)
      life += 1.0f;
    float angle = p.angle + time * p.drift + life * 2.4f;
    float radius = p.radius * (0.4f + life);
    float alpha = std::pow(1.0f - life, 1.5f);

    glPushMatrix();
    glTranslatef(std::cos(angle) * radius, 0.75f + life * 2.6f,
                 std::sin(angle) * radius);
    float col[] = {1.0f, 0.38f + 0.4f * (1.0f - life), 0.03f, alpha};
    float em[] = {0.55f, 0.13f, 0.0f, 1.0f};
    material(col, em, 0.0f);
    glutSolidSphere(0.018 + 0.018 * (1.0 - life), 6, 5);
    glPopMatrix();
  }

  // Draw burst embers
  for (const auto &p : burst_embers) {
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    float col[] = {1.0f, 0.8f * p.life, 0.1f * p.life, p.life};
    float em[] = {0.8f * p.life, 0.6f * p.life, 0.0f, 1.0f};
    material(col, em, 0.0f);
    glutSolidSphere(0.02f, 6, 5);
    glPopMatrix();
  }

  glDepthMask(GL_TRUE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
}

void ForestCampsite::draw_fireflies() {
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  glDepthMask(GL_FALSE);

  for (const auto &f : FIREFLIES) {
    float angle = time * f.speed + f.phase;
    float pulse = 0.72f + 0.28f * std::sin(time * 4.2f + f.phase);
    float x = f.center_x + f.radius_x * std::cos(angle);
    float y = f.center_y + 0.22f * std::sin(angle * 1.7f);
    float z = f.center_z + f.radius_z * std::sin(angle);

    glPushMatrix();
    glTranslatef(x, y, z);

    float col1[] = {0.75f, 1.0f, 0.18f, 0.09f * pulse};
    float em1[] = {0.50f, 0.68f, 0.05f, 1.0f};
    material(col1, em1, 0.0f);
    scaled_sphere(0.15f * pulse, 0.15f * pulse, 0.15f * pulse, 10, 8);

    float col2[] = {0.96f, 1.0f, 0.35f, 1.0f};
    float em2[] = {0.82f, 0.90f, 0.12f, 1.0f};
    material(col2, em2, 0.0f);
    glutSolidSphere(0.052f * pulse, 10, 8);

    glPopMatrix();
  }
  glDepthMask(GL_TRUE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_BLEND);
}

void ForestCampsite::reshape(int width, int height) {
  int safe_width = std::max(1, width);
  int safe_height = std::max(1, height);
  glViewport(0, 0, safe_width, safe_height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluPerspective(51.0, (double)safe_width / safe_height, 0.10, 85.0);
  glMatrixMode(GL_MODELVIEW);
}

void ForestCampsite::idle() {
  int now = glutGet(GLUT_ELAPSED_TIME);
  if (last_ms == 0)
    last_ms = now;
  float delta = std::min(0.05f, std::max(0.0f, (now - last_ms) / 1000.0f));
  last_ms = now;
  if (!paused) {
    time += delta;
    fire_fuel = std::max(0.0f, fire_fuel - delta * 0.025f);
    smoke_puff = std::max(0.0f, smoke_puff - delta * 0.4f);

    if (sunrise_active) {
      time_of_day =
          std::min(1.0f, time_of_day + delta * 0.05f); // 20 seconds to sunrise
    } else {
      time_of_day = std::max(
          0.0f, time_of_day - delta * 0.05f); // 20 seconds back to night
    }

    if (wind_active) {
      wind_strength = std::min(1.0f, wind_strength + delta * 0.5f);
    } else {
      wind_strength = std::max(0.0f, wind_strength - delta * 0.5f);
    }

    // Spawn shooting stars randomly only at night
    if (time_of_day < 0.2f && std::rand() % 1000 < 5) {
      float sx = (std::rand() % 100 / 50.0f - 1.0f) * 15.0f;
      float sy = 12.0f + (std::rand() % 100 / 50.0f) * 3.0f;
      float sz = (std::rand() % 100 / 50.0f - 1.0f) * 15.0f;
      float dx = (std::rand() % 100 / 50.0f - 1.0f) * 10.0f;
      float dy = -5.0f - (std::rand() % 100 / 50.0f) * 5.0f;
      float dz = (std::rand() % 100 / 50.0f - 1.0f) * 10.0f;
      shooting_stars.push_back({sx, sy, sz, sx + dx, sy + dy, sz + dz, 0.0f,
                                0.5f + (std::rand() % 100 / 100.0f) * 1.5f});
    }

    for (auto it = shooting_stars.begin(); it != shooting_stars.end();) {
      it->progress += delta * it->speed;
      if (it->progress >= 1.0f) {
        it = shooting_stars.erase(it);
      } else {
        ++it;
      }
    }

    for (auto it = thrown_logs.begin(); it != thrown_logs.end();) {
      it->progress += delta * 1.5f;
      if (it->progress >= 1.0f) {
        fire_fuel = std::min(1.0f, fire_fuel + 0.35f);
        smoke_puff = std::min(1.0f, smoke_puff + 0.8f);

        // Spawn burst embers
        for (int i = 0; i < 8; ++i) {
          float vx = (std::rand() % 100 / 50.0f - 1.0f) * 1.2f;
          float vy = 1.5f + (std::rand() % 100 / 100.0f) * 1.5f;
          float vz = (std::rand() % 100 / 50.0f - 1.0f) * 1.2f;
          burst_embers.push_back({0.0f, 0.2f, 0.0f, vx, vy, vz, 1.0f});
        }

        it = thrown_logs.erase(it);
      } else {
        ++it;
      }
    }

    for (auto it = burst_embers.begin(); it != burst_embers.end();) {
      it->x += it->vx * delta;
      it->y += it->vy * delta;
      it->z += it->vz * delta;
      it->vy -= 2.0f * delta; // Gravity
      it->life -= delta * 1.2f;
      if (it->life <= 0.0f) {
        it = burst_embers.erase(it);
      } else {
        ++it;
      }
    }
  }
  glutPostRedisplay();
}

void ForestCampsite::keyboard(unsigned char key, int /*x*/, int /*y*/) {
  key = std::tolower(key);
  if (key == 'q' || key == 27) { // Escape
    exit(0);
  } else if (key == ' ') {
    paused = !paused;
    last_ms = glutGet(GLUT_ELAPSED_TIME);
  } else if (key == 'r') {
    camera.reset();
  } else if (key == '+' || key == '=') {
    camera.radius = std::max(8.5f, camera.radius - 0.75f);
  } else if (key == '-' || key == '_') {
    camera.radius = std::min(29.0f, camera.radius + 0.75f);
  } else if (key == 'a') {
    camera.azimuth -= 4.0f;
  } else if (key == 'd') {
    camera.azimuth += 4.0f;
  } else if (key == 'f') {
    fog_enabled = !fog_enabled;
    if (fog_enabled)
      glEnable(GL_FOG);
    else
      glDisable(GL_FOG);
  } else if (key == 'l') {
    throw_log();
  } else if (key == 'w') {
    wind_active = !wind_active;
  } else if (key == 't') {
    sunrise_active = !sunrise_active;
  } else if (key == 'p') {
    if (shading_available())
      phong_mode = !phong_mode;
  } else if (key == 'h') {
    show_hud = !show_hud;
  } else if (key == 'g') {
    if (raytrace_view)
      raytrace_view = false; // back to the real-time view
    else
      rt_pending = true;     // render a ray-traced frame after this redraw
  }
  glutPostRedisplay();
}

void ForestCampsite::special_key(int key, int /*x*/, int /*y*/) {
  if (key == GLUT_KEY_LEFT) {
    camera.azimuth -= 4.0f;
  } else if (key == GLUT_KEY_RIGHT) {
    camera.azimuth += 4.0f;
  } else if (key == GLUT_KEY_UP) {
    camera.elevation = std::min(72.0f, camera.elevation + 3.0f);
  } else if (key == GLUT_KEY_DOWN) {
    camera.elevation = std::max(7.0f, camera.elevation - 3.0f);
  } else if (key == GLUT_KEY_HOME) {
    camera.reset();
  }
  glutPostRedisplay();
}

void ForestCampsite::mouse(int button, int state, int x, int y) {
  if (state == GLUT_DOWN && button == 3) {
    camera.radius = std::max(8.5f, camera.radius - 0.8f);
  } else if (state == GLUT_DOWN && button == 4) {
    camera.radius = std::min(29.0f, camera.radius + 0.8f);
  } else if (button == GLUT_LEFT_BUTTON) {
    if (state == GLUT_DOWN) {
      GLint viewport[4];
      GLdouble modelview[16];
      GLdouble projection[16];
      glGetIntegerv(GL_VIEWPORT, viewport);

      glMatrixMode(GL_PROJECTION);
      glPushMatrix();
      glLoadIdentity();
      gluPerspective(51.0, (double)viewport[2] / viewport[3], 0.10, 85.0);
      glGetDoublev(GL_PROJECTION_MATRIX, projection);
      glPopMatrix();

      glMatrixMode(GL_MODELVIEW);
      glPushMatrix();
      glLoadIdentity();
      float ex, ey, ez;
      camera.eye(ex, ey, ez);
      gluLookAt(ex, ey, ez, camera.tx, camera.ty, camera.tz, 0.0, 1.0, 0.0);
      glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
      glPopMatrix();

      GLdouble near_x, near_y, near_z;
      GLdouble far_x, far_y, far_z;
      gluUnProject(x, viewport[3] - y, 0.0, modelview, projection, viewport,
                   &near_x, &near_y, &near_z);
      gluUnProject(x, viewport[3] - y, 1.0, modelview, projection, viewport,
                   &far_x, &far_y, &far_z);

      if (far_y != near_y) {
        double t = -near_y / (far_y - near_y);
        double hit_x = near_x + (far_x - near_x) * t;
        double hit_z = near_z + (far_z - near_z) * t;
        if (std::hypot(hit_x - 1.2f, hit_z - 1.2f) < 1.0f) {
          throw_log();
          glutPostRedisplay();
          return; // Prevent dragging
        }
      }
    }
    camera.dragging = (state == GLUT_DOWN);
    camera.last_mouse_x = x;
    camera.last_mouse_y = y;
  }
  glutPostRedisplay();
}

void ForestCampsite::mouse_motion(int x, int y) {
  if (!camera.dragging)
    return;
  int old_x = camera.last_mouse_x;
  int old_y = camera.last_mouse_y;
  camera.azimuth += (x - old_x) * 0.32f;
  camera.elevation =
      std::max(7.0f, std::min(72.0f, camera.elevation + (old_y - y) * 0.26f));
  camera.last_mouse_x = x;
  camera.last_mouse_y = y;
  glutPostRedisplay();
}

void ForestCampsite::draw_log_pile() {
  glPushMatrix();
  glTranslatef(1.2f, 0.05f, 1.2f);
  glRotatef(60.0f, 0.0f, 1.0f, 0.0f);
  float mat[] = {0.72f, 0.48f, 0.27f, 1.0f};
  material(mat, NO_EMISSION, 0.04f);
  for (int i = 0; i < 3; ++i) {
    glPushMatrix();
    glTranslatef((i - 1) * 0.22f, 0.10f, 0.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, -0.6f, 0.0f);
    cylinder_y(*quadric, 0.10f, 1.2f, -1.0f, 12, 4, textures->bark);
    glPopMatrix();
  }
  for (int i = 0; i < 2; ++i) {
    glPushMatrix();
    glTranslatef((i - 0.5f) * 0.22f, 0.28f, 0.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, -0.6f, 0.0f);
    cylinder_y(*quadric, 0.10f, 1.2f, -1.0f, 12, 4, textures->bark);
    glPopMatrix();
  }
  glPopMatrix();
}

void ForestCampsite::draw_thrown_logs() {
  float mat[] = {0.72f, 0.48f, 0.27f, 1.0f};
  material(mat, NO_EMISSION, 0.04f);
  for (const auto &log : thrown_logs) {
    float x = log.start_x + (0.0f - log.start_x) * log.progress;
    float z = log.start_z + (0.0f - log.start_z) * log.progress;
    float arc = std::sin(log.progress * M_PI);
    float y = 0.2f + arc * 1.5f;

    glPushMatrix();
    glTranslatef(x, y, z);
    glRotatef(log.progress * 360.0f * 2.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);
    glTranslatef(0.0f, -0.6f, 0.0f);
    cylinder_y(*quadric, 0.10f, 1.2f, -1.0f, 12, 4, textures->bark);
    glPopMatrix();
  }
}

void ForestCampsite::throw_log() {
  ThrownLog log = {1.2f, 1.2f, 0.0f};
  thrown_logs.push_back(log);
}

void ForestCampsite::draw_stumps() {
  float angles[] = {120.0f, 240.0f};
  float radius = 1.6f;
  float mat[] = {0.6f, 0.5f, 0.4f, 1.0f};
  material(mat, NO_EMISSION, 0.05f);

  for (float angle_deg : angles) {
    float angle = angle_deg * M_PI / 180.0f;
    float x = radius * std::cos(angle);
    float z = radius * std::sin(angle);

    glPushMatrix();
    glTranslatef(x, 0.2f, z);
    glRotatef(angle_deg, 0.0f, 1.0f, 0.0f);

    // main stump
    glPushMatrix();
    glTranslatef(0.0f, -0.2f, 0.0f);
    cylinder_y(*quadric, 0.25f, 0.4f, 0.22f, 16, 1, textures->bark);
    glPopMatrix();

    glPopMatrix();
  }
}

void ForestCampsite::draw_tripod() {
  float mat_metal[] = {0.15f, 0.15f, 0.15f, 1.0f};
  material(mat_metal, NO_EMISSION, 0.2f);

  float height = 1.8f;
  float radius = 0.8f;

  for (int i = 0; i < 3; ++i) {
    float angle = i * TAU / 3.0f;
    float x = radius * std::cos(angle);
    float z = radius * std::sin(angle);

    glPushMatrix();
    glTranslatef(x, 0.0f, z);

    // Angle pointing towards center top (0, height, 0)
    float dist = std::hypot(x, z);
    float pitch = std::atan2(dist, height) * 180.0f / M_PI;
    glRotatef(angle * 180.0f / M_PI + 90.0f, 0.0f, 1.0f, 0.0f);
    glRotatef(-pitch, 0.0f, 0.0f, 1.0f);
    glRotatef(90.0f, 1.0f, 0.0f, 0.0f);

    float length = std::hypot(dist, height);
    glTranslatef(0.0f, -length, 0.0f);
    cylinder_y(*quadric, 0.02f, length + 0.1f, 0.02f, 8);
    glPopMatrix();
  }

  // Pot hanging
  float sway = 0.0f;
  if (wind_strength > 0)
    sway = wind_strength * 10.0f * std::sin(time * 3.0f);

  glPushMatrix();
  glTranslatef(0.0f, height - 0.1f, 0.0f);
  glRotatef(sway, 0.0f, 0.0f, 1.0f);

  // String
  cylinder_y(*quadric, 0.005f, 0.6f, 0.005f, 4);

  // Pot body
  glTranslatef(0.0f, -0.6f, 0.0f);
  float mat_pot[] = {0.05f, 0.05f, 0.05f, 1.0f};
  material(mat_pot, NO_EMISSION, 0.1f);
  glutSolidSphere(0.18f, 16, 12);

  glPopMatrix();
}

// ---------------------------------------------------------------------------
// Screenshot automation: ./campsite --capture <output_dir>
// Renders a fixed list of views/states, saves each as a PPM, then exits.
// ---------------------------------------------------------------------------
struct CaptureShot {
  const char *name;
  int wait_ms;
};
static const CaptureShot CAPTURE_SHOTS[] = {
    {"01_overview_night", 3500}, {"02_owl_closeup", 2500},
    {"03_campfire", 2500},       {"04_fire_burst", 850},
    {"05_tent", 2000},           {"06_trees_wide", 2000},
    {"07_moon_stars", 2000},     {"08_sunrise_mid", 1500},   {"09_sunrise_day", 1500},
    {"10_gouraud_closeup", 1800}, {"11_phong_closeup", 1800},
    {"12_gouraud_fire_ground", 1800}, {"13_phong_fire_ground", 1800},
    {"14_raytrace_overview", 3500}, {"15_raytrace_closeup", 3500},
};
static const int CAPTURE_COUNT =
    sizeof(CAPTURE_SHOTS) / sizeof(CAPTURE_SHOTS[0]);

void ForestCampsite::apply_capture_shot(int index) {
  camera.reset();
  time_of_day = 0.0f;
  sunrise_active = false;
  wind_active = false;
  wind_strength = 0.0f;
  fire_fuel = 1.0f;
  thrown_logs.clear();
  burst_embers.clear();
  phong_mode = false;
  raytrace_view = false;
  rt_pending = false;
  show_hud = (index >= 9); // HUD only in the new shading/ray-trace shots

  switch (index) {
  case 1: // owl close-up
    camera.tx = -3.4f; camera.ty = 3.0f; camera.tz = -3.3f;
    camera.azimuth = 20.0f; camera.elevation = 10.0f; camera.radius = 3.6f;
    break;
  case 2: // campfire
    camera.tx = 0.0f; camera.ty = 0.8f; camera.tz = 0.0f;
    camera.azimuth = 25.0f; camera.elevation = 16.0f; camera.radius = 4.8f;
    break;
  case 3: // log thrown into the fire
    camera.tx = 0.0f; camera.ty = 0.8f; camera.tz = 0.0f;
    camera.azimuth = 25.0f; camera.elevation = 16.0f; camera.radius = 4.8f;
    throw_log();
    break;
  case 4: // tent
    camera.tx = 3.25f; camera.ty = 1.0f; camera.tz = 2.65f;
    camera.azimuth = 45.0f; camera.elevation = 14.0f; camera.radius = 7.0f;
    break;
  case 5: // trees
    camera.tx = -2.0f; camera.ty = 2.5f; camera.tz = 0.0f;
    camera.azimuth = 200.0f; camera.elevation = 10.0f; camera.radius = 15.0f;
    break;
  case 6: // moon and stars
    camera.tx = -4.0f; camera.ty = 6.0f; camera.tz = -8.0f;
    camera.azimuth = 30.0f; camera.elevation = 4.0f; camera.radius = 12.0f;
    break;
  case 7: // sunrise (sun just above the horizon)
    time_of_day = 0.45f;
    sunrise_active = true;
    break;
  case 8: // full day
    time_of_day = 1.0f;
    sunrise_active = true;
    break;
  case 9:  // Gouraud close-up (polished orb)
  case 10: // Phong close-up (same view)
    camera.tx = -1.0f; camera.ty = 0.5f; camera.tz = 1.6f;
    camera.azimuth = 5.0f; camera.elevation = 12.0f; camera.radius = 4.3f;
    phong_mode = (index == 10);
    break;
  case 11: // Gouraud: fire light on ground
  case 12: // Phong: fire light on ground
    camera.tx = 0.4f; camera.ty = 0.3f; camera.tz = 0.4f;
    camera.azimuth = 25.0f; camera.elevation = 38.0f; camera.radius = 5.5f;
    phong_mode = (index == 12);
    break;
  case 13: // ray-traced overview
    rt_pending = true;
    break;
  case 14: // ray-traced close-up (reflective + glass orbs)
    camera.tx = -1.0f; camera.ty = 0.5f; camera.tz = 1.6f;
    camera.azimuth = 5.0f; camera.elevation = 12.0f; camera.radius = 4.3f;
    rt_pending = true;
    break;
  default:
    break;
  }
}

void ForestCampsite::save_screenshot(const std::string &name) {
  GLint viewport[4];
  glGetIntegerv(GL_VIEWPORT, viewport);
  int w = viewport[2], h = viewport[3];
  std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 3);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadBuffer(GL_BACK);
  glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

  std::ofstream out(capture_dir + "/" + name + ".ppm", std::ios::binary);
  out << "P6\n" << w << " " << h << "\n255\n";
  for (int row = h - 1; row >= 0; --row) // flip vertically
    out.write(reinterpret_cast<const char *>(&pixels[static_cast<size_t>(row) * w * 3]),
              static_cast<std::streamsize>(w) * 3);
  std::cout << "Saved " << name << " (" << w << "x" << h << ")\n";
}

void ForestCampsite::capture_update() {
  if (capture_dir.empty())
    return;
  int now = glutGet(GLUT_ELAPSED_TIME);
  if (capture_step < 0) {
    capture_step = 0;
    capture_start_ms = now;
    apply_capture_shot(0);
    return;
  }
  if (now - capture_start_ms >= CAPTURE_SHOTS[capture_step].wait_ms) {
    save_screenshot(CAPTURE_SHOTS[capture_step].name);
    ++capture_step;
    if (capture_step >= CAPTURE_COUNT)
      exit(0);
    capture_start_ms = now;
    apply_capture_shot(capture_step);
  }
}

// ---------------------------------------------------------------------------
// Polished demo orb: a deliberately LOW-POLY (12x8) shiny sphere. Its specular
// highlight is the clearest way to compare Gouraud and Phong shading (P key).
// ---------------------------------------------------------------------------
void ForestCampsite::draw_demo_orb() {
  float col[] = {0.55f, 0.66f, 0.88f, 1.0f};
  material(col, NO_EMISSION, 0.95f, 70.0f);
  glPushMatrix();
  glTranslatef(-1.7f, 0.45f, 1.5f);
  glutSolidSphere(0.45, 12, 8);
  glPopMatrix();
  float white[] = {1.0f, 1.0f, 1.0f, 1.0f};
  material(white, NO_EMISSION);
}

// ---------------------------------------------------------------------------
// HUD: shows the active shading model / ray-trace statistics.
// ---------------------------------------------------------------------------
void ForestCampsite::draw_hud() {
  if (!show_hud && !rt_pending)
    return;
  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);

  std::vector<std::string> lines;
  if (raytrace_view) {
    char buf[160];
    lines.push_back("RAY TRACING  (CPU, Whitted-style)");
    std::snprintf(buf, sizeof(buf), "%dx%d  2x2 AA  depth 4  %zu objects  %.2f s",
                  rt_w, rt_h, rt_objects, rt_seconds);
    lines.push_back(buf);
    lines.push_back("shadows + reflection + refraction + fog");
    lines.push_back("[G] back to real-time view   [H] hide panel");
  } else {
    lines.push_back(phong_mode ? "SHADING: PHONG  (per-pixel, GLSL)"
                               : "SHADING: GOURAUD  (per-vertex, fixed-function)");
    lines.push_back("[P] Gouraud/Phong   [G] ray trace   [H] hide panel");
    if (rt_pending)
      lines.push_back("Ray tracing... please wait");
  }

  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  gluOrtho2D(0, vp[2], 0, vp[3]);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glDisable(GL_LIGHTING);
  glDisable(GL_FOG);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  size_t longest = 0;
  for (const auto &l : lines)
    longest = std::max(longest, l.size());
  float line_h = 20.0f, pad = 10.0f;
  float panel_w = longest * 9.0f + 2.0f * pad;
  float panel_h = lines.size() * line_h + pad;
  float x0 = 14.0f, y1 = vp[3] - 14.0f, y0 = y1 - panel_h;

  glColor4f(0.02f, 0.03f, 0.07f, 0.72f);
  glBegin(GL_QUADS);
  glVertex2f(x0, y0);
  glVertex2f(x0 + panel_w, y0);
  glVertex2f(x0 + panel_w, y1);
  glVertex2f(x0, y1);
  glEnd();

  for (size_t i = 0; i < lines.size(); ++i) {
    if (i == 0)
      glColor3f(1.0f, 0.78f, 0.30f);
    else
      glColor3f(0.86f, 0.90f, 0.98f);
    glRasterPos2f(x0 + pad, y1 - pad * 0.5f - (i + 1) * line_h + 5.0f);
    for (char ch : lines[i])
      glutBitmapCharacter(GLUT_BITMAP_9_BY_15, ch);
  }

  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  if (fog_enabled && !raytrace_view)
    glEnable(GL_FOG);
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}

void ForestCampsite::draw_raytrace_view() {
  if (!rt_texture)
    return;
  glMatrixMode(GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity();
  glOrtho(0, 1, 0, 1, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glPushMatrix();
  glLoadIdentity();
  glDisable(GL_LIGHTING);
  glDisable(GL_FOG);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, rt_texture);
  glColor3f(1.0f, 1.0f, 1.0f);
  glBegin(GL_QUADS);
  glTexCoord2f(0, 1); glVertex2f(0, 0);
  glTexCoord2f(1, 1); glVertex2f(1, 0);
  glTexCoord2f(1, 0); glVertex2f(1, 1);
  glTexCoord2f(0, 0); glVertex2f(0, 1);
  glEnd();
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_LIGHTING);
  glMatrixMode(GL_PROJECTION);
  glPopMatrix();
  glMatrixMode(GL_MODELVIEW);
  glPopMatrix();
}

// ---------------------------------------------------------------------------
// Ray tracing: rebuilds the current scene state (lights, fog, day/night) as a
// ray-traceable description, renders it on the CPU and uploads it as texture.
// ---------------------------------------------------------------------------
void ForestCampsite::ray_trace_render() {
  RayTracer rt;
  RTEnvironment env;
  build_ray_scene(rt, env);

  GLint vp[4];
  glGetIntegerv(GL_VIEWPORT, vp);
  float aspect = static_cast<float>(vp[2]) / std::max(1, static_cast<int>(vp[3]));
  rt_w = 960;
  rt_h = std::max(1, static_cast<int>(rt_w / aspect + 0.5f));

  RTCamera cam;
  float ex, ey, ez;
  camera.eye(ex, ey, ez);
  cam.eye = Vec3(ex, ey, ez);
  cam.target = Vec3(camera.tx, camera.ty, camera.tz);
  cam.fov_deg = 51.0f;

  int start = glutGet(GLUT_ELAPSED_TIME);
  std::vector<unsigned char> pixels = rt.render(cam, env, rt_w, rt_h, 2, 4);
  rt_seconds = (glutGet(GLUT_ELAPSED_TIME) - start) / 1000.0f;
  rt_objects = rt.object_count();

  if (!rt_texture)
    glGenTextures(1, &rt_texture);
  glBindTexture(GL_TEXTURE_2D, rt_texture);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, rt_w, rt_h, 0, GL_RGB, GL_UNSIGNED_BYTE,
               pixels.data());
  glBindTexture(GL_TEXTURE_2D, 0);

  if (!capture_dir.empty()) {
    std::ofstream out(capture_dir + "/raytrace_full_" + std::to_string(capture_step) + ".ppm",
                      std::ios::binary);
    out << "P6\n" << rt_w << " " << rt_h << "\n255\n";
    out.write(reinterpret_cast<const char *>(pixels.data()),
              static_cast<std::streamsize>(pixels.size()));
  }
  last_ms = glutGet(GLUT_ELAPSED_TIME);
  std::cout << "Ray traced " << rt_w << "x" << rt_h << " (" << rt_objects
            << " objects) in " << rt_seconds << " s\n";
}

void ForestCampsite::build_ray_scene(RayTracer &rt, RTEnvironment &env) {
  rt.clear();
  const float tau = time_of_day;
  auto mk = [](Vec3 c, float spec, float shin) {
    RTMaterial m;
    m.color = c;
    m.specular = spec;
    m.shininess = shin;
    return m;
  };

  // ---------- lights (same formulas as update_lights) ----------
  float base_intensity =
      std::max(0.60f, std::min(1.08f, 0.86f + 0.12f * std::sin(time * 10.0f) +
                                          0.10f * flicker(time, 0.9f)));
  float intensity = base_intensity * std::max(0.1f, fire_fuel);
  float t = std::max(0.0f, std::min(1.0f, fire_fuel * 2.0f));
  float fr = 1.0f * t + 0.8f * (1.0f - t);
  float fg = 0.43f * t + 0.05f * (1.0f - t);
  float fb = 0.12f * t;

  RTLight fire;
  fire.directional = false;
  fire.pos = Vec3(0.0f, 1.08f, 0.0f);
  fire.ambient = Vec3(0.055f, 0.018f, 0.003f);
  fire.diffuse = Vec3(fr, fg, fb) * intensity;
  fire.specular = Vec3(0.82f, 0.31f, 0.06f) * (intensity * t);
  fire.kc = 1.0f;
  fire.kl = 0.075f;
  fire.kq = 0.026f;
  rt.add_light(fire);

  RTLight moon;
  moon.directional = true;
  moon.pos = normalize(Vec3(0.24f - tau * 0.24f, 0.82f + tau * 0.18f, 0.18f + tau * 0.82f));
  moon.ambient = Vec3(0.018f, 0.024f, 0.055f);
  moon.diffuse = Vec3(0.20f * (1 - tau) + 0.95f * tau, 0.27f * (1 - tau) + 0.85f * tau,
                      0.46f * (1 - tau) + 0.65f * tau);
  moon.specular = Vec3(0.06f * (1 - tau) + 0.6f * tau, 0.08f * (1 - tau) + 0.6f * tau,
                       0.14f * (1 - tau) + 0.6f * tau);
  rt.add_light(moon);

  // ---------- environment ----------
  env.ambient = Vec3(0.060f * (1 - tau) + 0.3f * tau, 0.068f * (1 - tau) + 0.3f * tau,
                     0.105f * (1 - tau) + 0.3f * tau);
  env.sky_top = mix(Vec3(NIGHT_COLOR[0], NIGHT_COLOR[1], NIGHT_COLOR[2]) * 0.9f,
                    Vec3(0.30f, 0.52f, 0.88f), tau);
  env.sky_horizon = mix(Vec3(0.022f, 0.032f, 0.080f), Vec3(0.62f, 0.76f, 0.92f), tau);
  env.fog_color = Vec3(NIGHT_COLOR[0] * (1 - tau) + 0.5f * tau,
                       NIGHT_COLOR[1] * (1 - tau) + 0.6f * tau,
                       NIGHT_COLOR[2] * (1 - tau) + 0.8f * tau);
  env.fog_density = 0.020f;
  env.fog = fog_enabled;

  // ---------- ground (flat plane approximation of the terrain) ----------
  RTMaterial ground = mk(Vec3(0.15f, 0.21f, 0.10f), 0.03f, 2.0f);
  ground.pattern = 1;
  rt.add_plane(0.0f, ground);

  // ---------- campfire: stone ring, logs, ember bed, flames, tripod ----------
  RTMaterial stone = mk(Vec3(0.28f, 0.27f, 0.24f), 0.14f, 10.0f);
  for (int i = 0; i < 13; ++i) {
    float a = i * TAU / 13.0f;
    float r = 0.88f + 0.035f * std::sin(i * 2.1f);
    rt.add_ellipsoid(Vec3(std::cos(a) * r, 0.13f, std::sin(a) * r), Vec3(0.27f, 0.17f, 0.23f),
                     i * 37.0f, stone);
  }
  RTMaterial wood = mk(Vec3(0.22f, 0.11f, 0.055f), 0.04f, 8.0f);
  const float log_angles[] = {18.0f, 90.0f, 162.0f, 54.0f, 126.0f};
  for (int i = 0; i < 5; ++i) {
    float a = log_angles[i] * static_cast<float>(M_PI) / 180.0f;
    Vec3 dir(std::cos(a), 0.0f, -std::sin(a));
    Vec3 mid(0.0f, 0.22f + (i % 2) * 0.11f, 0.0f);
    rt.add_frustum(mid - dir * 0.73f, dir, 0.13f, 0.13f, 1.46f, wood);
  }
  RTMaterial ember = mk(Vec3(0.9f, 0.18f, 0.02f), 0.05f, 8.0f);
  ember.emission = Vec3(1.0f, 0.25f, 0.03f);
  ember.casts_shadow = false;
  for (int i = 0; i < 17; ++i) {
    float a = i * 2.31f, r = 0.11f + (i % 5) * 0.075f;
    rt.add_ellipsoid(Vec3(std::cos(a) * r, 0.26f, std::sin(a) * r),
                     Vec3(1, 1, 1) * (0.055f + (i % 3) * 0.012f), 0.0f, ember);
  }
  RTMaterial flame = mk(Vec3(1.0f, 0.5f, 0.05f), 0.0f, 1.0f);
  flame.pattern = 2;
  flame.transparency = 0.55f;
  flame.ior = 1.0f;
  flame.casts_shadow = false;
  const float fl[5][4] = {{0.00f, 0.00f, 0.52f, 1.55f}, {-0.18f, 0.10f, 0.37f, 1.25f},
                          {0.19f, 0.08f, 0.32f, 1.10f}, {0.03f, -0.16f, 0.25f, 0.88f},
                          {-0.06f, -0.02f, 0.17f, 0.67f}};
  for (const auto &f : fl)
    rt.add_frustum(Vec3(f[0], 0.29f, f[1]), Vec3(0, 1, 0), f[2], 0.015f, f[3], flame);

  RTMaterial metal = mk(Vec3(0.15f, 0.15f, 0.15f), 0.2f, 12.0f);
  for (int i = 0; i < 3; ++i) {
    float a = i * TAU / 3.0f;
    Vec3 base(0.8f * std::cos(a), 0.0f, 0.8f * std::sin(a));
    Vec3 top(0.0f, 1.8f, 0.0f);
    Vec3 d = top - base;
    rt.add_frustum(base, normalize(d), 0.02f, 0.02f, length(d), metal);
  }
  RTMaterial pot = mk(Vec3(0.05f, 0.05f, 0.05f), 0.6f, 40.0f);
  pot.reflectivity = 0.35f;
  pot.casts_shadow = false; // the fire light sits inside the pot
  rt.add_ellipsoid(Vec3(0.0f, 1.1f, 0.0f), Vec3(0.18f, 0.18f, 0.18f), 0.0f, pot);

  // stumps
  RTMaterial stump = mk(Vec3(0.20f, 0.10f, 0.05f), 0.05f, 8.0f);
  for (float deg : {120.0f, 240.0f}) {
    float a = deg * static_cast<float>(M_PI) / 180.0f;
    rt.add_frustum(Vec3(1.6f * std::cos(a), 0.0f, 1.6f * std::sin(a)), Vec3(0, 1, 0), 0.25f,
                   0.22f, 0.4f, stump);
  }

  // ---------- trees ----------
  RTMaterial bark = mk(Vec3(0.20f, 0.10f, 0.05f), 0.05f, 4.0f);
  struct L {
    float h, r, ch, c[3];
  };
  const L layers[] = {{0.00f, 1.43f, 1.95f, {0.060f, 0.31f, 0.13f}},
                      {0.72f, 1.20f, 1.85f, {0.065f, 0.36f, 0.15f}},
                      {1.40f, 0.94f, 1.62f, {0.075f, 0.40f, 0.17f}},
                      {2.00f, 0.66f, 1.28f, {0.085f, 0.43f, 0.18f}}};
  for (const auto &tr : MAIN_TREES) {
    float s = tr.scale;
    Vec3 root(tr.x, 0.0f, tr.z);
    rt.add_frustum(root, Vec3(0, 1, 0), 0.38f * s, 0.25f * s, 3.45f * s, bark);
    struct B { float y, ang, len; };
    const B branches[] = {{1.85f, -68.0f, 1.42f}, {2.20f, 67.0f, 0.90f}, {2.72f, -70.0f, 0.75f}};
    for (const auto &b : branches) {
      float a = b.ang * static_cast<float>(M_PI) / 180.0f;
      Vec3 dir(-std::sin(a), std::cos(a), 0.0f);
      rt.add_frustum(root + Vec3(0, b.y * s, 0), dir, 0.11f * s, 0.045f * s, b.len * s, bark);
    }
    for (const auto &l : layers) {
      RTMaterial leaf = mk(Vec3(l.c[0], l.c[1], l.c[2]), 0.04f, 3.0f);
      rt.add_frustum(root + Vec3(0, (2.55f + l.h) * s, 0), Vec3(0, 1, 0), l.r * s, 0.015f * s,
                     l.ch * s, leaf);
    }
    if (tr.has_owl) { // owl perched on the first branch
      Vec3 o = root + Vec3(1.28f, 2.78f, 0.02f) * s;
      rt.add_ellipsoid(o, Vec3(0.36f, 0.51f, 0.31f) * s, 0.0f, mk(Vec3(0.50f, 0.34f, 0.20f), 0.06f, 5.0f));
      Vec3 head = o + Vec3(0.0f, 0.46f, 0.02f) * s;
      rt.add_ellipsoid(head, Vec3(0.32f, 0.29f, 0.30f) * s, 0.0f, mk(Vec3(0.63f, 0.47f, 0.29f), 0.05f, 8.0f));
      RTMaterial wing = mk(Vec3(0.27f, 0.17f, 0.10f), 0.03f, 4.0f);
      for (float side : {-1.0f, 1.0f})
        rt.add_ellipsoid(o + Vec3(side * 0.29f, 0.06f, -0.015f) * s, Vec3(0.16f, 0.43f, 0.24f) * s, 0.0f, wing);
      RTMaterial eye_w = mk(Vec3(0.70f, 0.62f, 0.44f), 0.04f, 8.0f);
      RTMaterial iris = mk(Vec3(0.92f, 0.68f, 0.10f), 0.45f, 40.0f);
      iris.emission = Vec3(0.12f, 0.07f, 0.0f);
      RTMaterial pupil = mk(Vec3(0.018f, 0.012f, 0.008f), 0.4f, 50.0f);
      for (float side : {-1.0f, 1.0f}) {
        Vec3 e = head + Vec3(side * 0.12f, 0.035f, 0.252f) * s;
        rt.add_ellipsoid(e, Vec3(0.115f, 0.125f, 0.045f) * s, 0.0f, eye_w);
        rt.add_ellipsoid(e + Vec3(0, 0, 0.041f) * s, Vec3(0.052f, 0.058f, 0.025f) * s, 0.0f, iris);
        rt.add_ellipsoid(e + Vec3(0, 0, 0.062f) * s, Vec3(0.020f, 0.028f, 0.012f) * s, 0.0f, pupil);
      }
      rt.add_triangle(head + Vec3(-0.055f, -0.035f, 0.285f) * s, head + Vec3(0.055f, -0.035f, 0.285f) * s,
                      head + Vec3(0.0f, -0.13f, 0.41f) * s, mk(Vec3(0.74f, 0.42f, 0.09f), 0.08f, 10.0f));
    }
  }
  RTMaterial far_trunk = mk(Vec3(0.10f, 0.072f, 0.047f), 0.02f, 4.0f);
  RTMaterial far_leaf = mk(Vec3(0.025f, 0.105f, 0.060f), 0.02f, 4.0f);
  const float far_r[] = {1.15f, 0.92f, 0.67f};
  for (const auto &tr : DISTANT_TREES) {
    float s = tr.scale;
    Vec3 root(tr.x, 0.0f, tr.z);
    rt.add_frustum(root, Vec3(0, 1, 0), 0.25f * s, 0.18f * s, 2.5f * s, far_trunk);
    for (int l = 0; l < 3; ++l)
      rt.add_frustum(root + Vec3(0, (1.9f + l * 0.72f) * s, 0), Vec3(0, 1, 0), far_r[l] * s,
                     0.015f * s, 1.65f * s, far_leaf);
  }

  // ---------- rocks ----------
  RTMaterial rock = mk(Vec3(0.19f, 0.21f, 0.20f), 0.10f, 7.0f);
  for (const auto &r : FOREST_ROCKS)
    rt.add_ellipsoid(Vec3(r.x, 0.08f * r.scale, r.z),
                     Vec3(0.30f * r.scale, 0.13f * r.scale, 0.22f * r.scale), r.angle, rock);

  // ---------- tent (6 triangles, same local frame as draw_tent) ----------
  {
    const float ang = -28.0f * static_cast<float>(M_PI) / 180.0f;
    const float c = std::cos(ang), s = std::sin(ang);
    auto W = [&](float x, float y, float z) {
      return Vec3(3.25f + x * c + z * s, 0.05f + y, 2.65f - x * s + z * c);
    };
    RTMaterial canvas = mk(Vec3(0.42f, 0.45f, 0.22f), 0.05f, 5.0f);
    Vec3 a = W(-1.15f, 0, -1.5f), b = W(0, 1.5f, -1.5f), cc = W(0, 1.5f, 1.5f), d = W(-1.15f, 0, 1.5f);
    rt.add_triangle(a, b, cc, canvas);
    rt.add_triangle(a, cc, d, canvas);
    Vec3 e = W(1.15f, 0, -1.5f), f = W(1.15f, 0, 1.5f);
    rt.add_triangle(b, e, f, canvas);
    rt.add_triangle(b, f, cc, canvas);
    rt.add_triangle(d, f, cc, canvas);                                  // front gable
    rt.add_triangle(a, e, b, canvas);                                   // back gable
    RTMaterial door = mk(Vec3(0.025f, 0.030f, 0.025f), 0.0f, 1.0f);
    rt.add_triangle(W(-0.72f, 0.02f, -1.515f), W(0.72f, 0.02f, -1.515f), W(0, 1.17f, -1.515f), door);
  }

  // ---------- showpieces: polished (mirror) orb + glass orb ----------
  RTMaterial orb = mk(Vec3(0.55f, 0.66f, 0.88f), 0.95f, 70.0f);
  orb.reflectivity = 0.45f;
  rt.add_ellipsoid(Vec3(-1.7f, 0.45f, 1.5f), Vec3(0.45f, 0.45f, 0.45f), 0.0f, orb);
  RTMaterial glass = mk(Vec3(0.90f, 0.95f, 1.0f), 0.95f, 90.0f);
  glass.transparency = 0.85f;
  glass.ior = 1.5f;
  rt.add_ellipsoid(Vec3(-0.3f, 0.38f, 2.7f), Vec3(0.38f, 0.38f, 0.38f), 0.0f, glass);

  // ---------- moon / sun ----------
  float moon_t = std::min(1.0f, tau / 0.6f);
  if (moon_t < 1.0f) {
    RTMaterial m = mk(Vec3(0, 0, 0), 0.0f, 1.0f);
    m.emission = Vec3(0.82f, 0.86f, 0.98f) * (0.55f + 0.25f * moon_t);
    m.casts_shadow = false;
    m.fogged = false;
    rt.add_ellipsoid(Vec3(-8.2f, 8.8f - moon_t * 12.0f, -9.5f), Vec3(0.78f, 0.78f, 0.78f), 0.0f, m);
  }
  float sun_t = std::max(0.0f, std::min(1.0f, (tau - 0.1f) / 0.9f));
  if (sun_t > 0.0f) {
    RTMaterial m = mk(Vec3(0, 0, 0), 0.0f, 1.0f);
    m.emission = Vec3(1.0f, 0.50f + 0.40f * sun_t, 0.15f + 0.45f * sun_t);
    m.casts_shadow = false;
    m.fogged = false;
    rt.add_ellipsoid(Vec3(8.5f, -3.0f + sun_t * 12.0f, -10.5f), Vec3(1.05f, 1.05f, 1.05f), 0.0f, m);
  }
}

