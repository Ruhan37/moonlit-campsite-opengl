#ifndef RAYTRACER_H
#define RAYTRACER_H

// ---------------------------------------------------------------------------
// RayTracer.h - small CPU Whitted-style ray tracer (no OpenGL dependency).
//
// Features: ray/ellipsoid, ray/frustum(cylinder/cone), ray/triangle and
// ray/plane intersection, Phong lighting (ambient + diffuse + specular),
// hard shadows, mirror reflection, refraction (Snell + Schlick Fresnel),
// exponential-squared fog, 2x2 super-sampling and multi-threaded rendering.
// ---------------------------------------------------------------------------

#include <cmath>
#include <cstddef>
#include <vector>

struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3() {}
  Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
  Vec3 operator+(const Vec3 &o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
  Vec3 operator*(const Vec3 &o) const { return {x * o.x, y * o.y, z * o.z}; }
  Vec3 operator-() const { return {-x, -y, -z}; }
  Vec3 &operator+=(const Vec3 &o) { x += o.x; y += o.y; z += o.z; return *this; }
};
inline float dot(const Vec3 &a, const Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3 &a, const Vec3 &b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3 &a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalize(const Vec3 &a) {
  float l = length(a);
  return l > 1e-12f ? a * (1.0f / l) : a;
}
inline Vec3 mix(const Vec3 &a, const Vec3 &b, float t) { return a * (1.0f - t) + b * t; }

struct RTMaterial {
  Vec3 color{0.8f, 0.8f, 0.8f};
  Vec3 emission{0, 0, 0};
  float specular = 0.1f;      // k_s
  float shininess = 16.0f;    // n_s
  float reflectivity = 0.0f;  // mirror coefficient
  float transparency = 0.0f;  // 0 = opaque, 1 = fully transparent
  float ior = 1.5f;           // index of refraction (glass 1.5)
  int pattern = 0;            // 0 flat, 1 mossy ground, 2 flame gradient
  bool casts_shadow = true;
  bool fogged = true;
};

struct RTLight {
  bool directional = false;
  Vec3 pos;                   // position, or direction TOWARD the light
  Vec3 ambient, diffuse, specular;
  float kc = 1.0f, kl = 0.0f, kq = 0.0f;  // attenuation
};

struct RTCamera {
  Vec3 eye, target;
  float fov_deg = 51.0f;
};

struct RTEnvironment {
  Vec3 ambient;
  Vec3 sky_top, sky_horizon;
  Vec3 fog_color;
  float fog_density = 0.02f;
  bool fog = true;
};

class RayTracer {
public:
  void clear();
  void add_plane(float y, const RTMaterial &m);
  void add_ellipsoid(Vec3 center, Vec3 radii, float yaw_deg, const RTMaterial &m);
  // Truncated cone / cylinder: starts at 'base', runs 'len' along unit 'axis'.
  void add_frustum(Vec3 base, Vec3 axis, float r0, float r1, float len, const RTMaterial &m);
  void add_triangle(Vec3 a, Vec3 b, Vec3 c, const RTMaterial &m);
  void add_light(const RTLight &l) { lights.push_back(l); }
  size_t object_count() const { return objects.size(); }

  // Returns w*h*3 RGB bytes, first row = top of the image.
  std::vector<unsigned char> render(const RTCamera &cam, const RTEnvironment &env,
                                    int w, int h, int ss = 2, int max_depth = 4) const;

private:
  enum Type { PLANE, ELLIPSOID, FRUSTUM, TRIANGLE };
  struct Object {
    Type type = PLANE;
    Vec3 pos, ax, ay, az;      // local frame (world-space axes)
    Vec3 radii;
    float r0 = 0, r1 = 0, len = 0;
    Vec3 a, b, c;              // triangle
    RTMaterial mat;
    Vec3 bc;                   // bounding sphere
    float br = -1.0f;
  };
  struct Hit {
    float t = 1e30f;
    Vec3 n;
    const Object *obj = nullptr;
  };

  bool intersect(const Object &o, const Vec3 &ro, const Vec3 &rd, float &t, Vec3 &n) const;
  bool closest(const Vec3 &ro, const Vec3 &rd, Hit &hit) const;
  float shadow_transmittance(const Vec3 &ro, const Vec3 &rd, float tmax) const;
  Vec3 trace(const Vec3 &ro, const Vec3 &rd, int depth, const RTEnvironment &env) const;

  std::vector<Object> objects;
  std::vector<RTLight> lights;
};

#endif // RAYTRACER_H
