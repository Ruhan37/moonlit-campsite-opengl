#include "RayTracer.h"
#include <algorithm>
#include <thread>

namespace {
const float EPS = 1e-3f;
const float INF = 1e30f;
const float PI_F = 3.14159265358979f;

Vec3 clamp01(const Vec3 &c) {
  return {std::min(1.0f, std::max(0.0f, c.x)), std::min(1.0f, std::max(0.0f, c.y)),
          std::min(1.0f, std::max(0.0f, c.z))};
}
Vec3 reflect_dir(const Vec3 &d, const Vec3 &n) { return d - n * (2.0f * dot(d, n)); }
} // namespace

void RayTracer::clear() {
  objects.clear();
  lights.clear();
}

void RayTracer::add_plane(float y, const RTMaterial &m) {
  Object o;
  o.type = PLANE;
  o.pos = Vec3(0, y, 0);
  o.mat = m;
  objects.push_back(o);
}

void RayTracer::add_ellipsoid(Vec3 c, Vec3 radii, float yaw_deg, const RTMaterial &m) {
  Object o;
  o.type = ELLIPSOID;
  o.pos = c;
  float a = yaw_deg * PI_F / 180.0f;
  o.ax = Vec3(std::cos(a), 0, -std::sin(a));
  o.ay = Vec3(0, 1, 0);
  o.az = Vec3(std::sin(a), 0, std::cos(a));
  o.radii = radii;
  o.mat = m;
  o.bc = c;
  o.br = std::max(radii.x, std::max(radii.y, radii.z)) * 1.01f;
  objects.push_back(o);
}

void RayTracer::add_frustum(Vec3 base, Vec3 axis, float r0, float r1, float len,
                            const RTMaterial &m) {
  Object o;
  o.type = FRUSTUM;
  o.pos = base;
  o.ay = normalize(axis);
  Vec3 ref = std::fabs(o.ay.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
  o.ax = normalize(cross(ref, o.ay));
  o.az = cross(o.ay, o.ax);
  o.r0 = r0;
  o.r1 = r1;
  o.len = len;
  o.mat = m;
  o.bc = base + o.ay * (len * 0.5f);
  float rmax = std::max(r0, r1);
  o.br = std::sqrt((len * 0.5f) * (len * 0.5f) + rmax * rmax) * 1.01f;
  objects.push_back(o);
}

void RayTracer::add_triangle(Vec3 a, Vec3 b, Vec3 c, const RTMaterial &m) {
  Object o;
  o.type = TRIANGLE;
  o.a = a;
  o.b = b;
  o.c = c;
  o.mat = m;
  objects.push_back(o);
}

// ---------------------------------------------------------------------------
// Intersection routines. 't' is returned in world units because the ray is
// transformed to the object's local frame without re-normalising direction.
// ---------------------------------------------------------------------------
bool RayTracer::intersect(const Object &o, const Vec3 &ro, const Vec3 &rd, float &t_out,
                          Vec3 &n_out) const {
  switch (o.type) {
  case PLANE: { // y = const : t = (y0 - o.y) / d.y
    if (std::fabs(rd.y) < 1e-8f)
      return false;
    float t = (o.pos.y - ro.y) / rd.y;
    if (t <= EPS)
      return false;
    t_out = t;
    n_out = Vec3(0, 1, 0);
    return true;
  }
  case TRIANGLE: { // Moller-Trumbore
    Vec3 e1 = o.b - o.a, e2 = o.c - o.a;
    Vec3 p = cross(rd, e2);
    float det = dot(e1, p);
    if (std::fabs(det) < 1e-9f)
      return false;
    float inv = 1.0f / det;
    Vec3 s = ro - o.a;
    float u = dot(s, p) * inv;
    if (u < 0.0f || u > 1.0f)
      return false;
    Vec3 q = cross(s, e1);
    float v = dot(rd, q) * inv;
    if (v < 0.0f || u + v > 1.0f)
      return false;
    float t = dot(e2, q) * inv;
    if (t <= EPS)
      return false;
    t_out = t;
    n_out = normalize(cross(e1, e2));
    return true;
  }
  default:
    break;
  }

  // bounding-sphere early reject
  if (o.br > 0.0f) {
    Vec3 oc = o.bc - ro;
    float tca = dot(oc, rd);
    float oc2 = dot(oc, oc);
    if (tca < 0.0f && oc2 > o.br * o.br)
      return false;
    if (oc2 - tca * tca > o.br * o.br)
      return false;
  }

  // transform ray to local frame
  Vec3 v = ro - o.pos;
  Vec3 lo(dot(o.ax, v), dot(o.ay, v), dot(o.az, v));
  Vec3 ld(dot(o.ax, rd), dot(o.ay, rd), dot(o.az, rd));

  if (o.type == ELLIPSOID) {
    // scale to unit sphere: |o' + t d'|^2 = 1
    Vec3 so(lo.x / o.radii.x, lo.y / o.radii.y, lo.z / o.radii.z);
    Vec3 sd(ld.x / o.radii.x, ld.y / o.radii.y, ld.z / o.radii.z);
    float a = dot(sd, sd), b = dot(so, sd), c = dot(so, so) - 1.0f;
    float disc = b * b - a * c;
    if (disc < 0.0f)
      return false;
    float s = std::sqrt(disc);
    float t = (-b - s) / a;
    if (t <= EPS)
      t = (-b + s) / a;
    if (t <= EPS)
      return false;
    Vec3 lp = lo + ld * t;
    Vec3 nl(lp.x / (o.radii.x * o.radii.x), lp.y / (o.radii.y * o.radii.y),
            lp.z / (o.radii.z * o.radii.z));
    nl = normalize(nl);
    t_out = t;
    n_out = o.ax * nl.x + o.ay * nl.y + o.az * nl.z;
    return true;
  }

  // FRUSTUM: x^2 + z^2 = r(y)^2 , r(y) = r0 + k y , 0 <= y <= len, plus caps
  float best = INF;
  Vec3 bn;
  auto consider = [&](float t, const Vec3 &nl) {
    if (t > EPS && t < best) {
      best = t;
      bn = nl;
    }
  };
  float k = (o.r1 - o.r0) / o.len;
  float ro_r = o.r0 + k * lo.y, rd_r = k * ld.y;
  float A = ld.x * ld.x + ld.z * ld.z - rd_r * rd_r;
  float B = lo.x * ld.x + lo.z * ld.z - ro_r * rd_r;
  float C = lo.x * lo.x + lo.z * lo.z - ro_r * ro_r;
  auto side = [&](float t) {
    float y = lo.y + t * ld.y;
    float r = o.r0 + k * y;
    if (y >= 0.0f && y <= o.len && r >= 0.0f) {
      float px = lo.x + t * ld.x, pz = lo.z + t * ld.z;
      consider(t, normalize(Vec3(px, -k * r, pz)));
    }
  };
  if (std::fabs(A) > 1e-8f) {
    float disc = B * B - A * C;
    if (disc >= 0.0f) {
      float s = std::sqrt(disc);
      side((-B - s) / A);
      side((-B + s) / A);
    }
  } else if (std::fabs(B) > 1e-8f) {
    side(-C / (2.0f * B));
  }
  if (std::fabs(ld.y) > 1e-8f) {
    float t0 = -lo.y / ld.y;
    float x0 = lo.x + t0 * ld.x, z0 = lo.z + t0 * ld.z;
    if (x0 * x0 + z0 * z0 <= o.r0 * o.r0)
      consider(t0, Vec3(0, -1, 0));
    float t1 = (o.len - lo.y) / ld.y;
    float x1 = lo.x + t1 * ld.x, z1 = lo.z + t1 * ld.z;
    if (x1 * x1 + z1 * z1 <= o.r1 * o.r1)
      consider(t1, Vec3(0, 1, 0));
  }
  if (best >= INF)
    return false;
  t_out = best;
  n_out = o.ax * bn.x + o.ay * bn.y + o.az * bn.z;
  return true;
}

bool RayTracer::closest(const Vec3 &ro, const Vec3 &rd, Hit &hit) const {
  hit = Hit();
  for (const Object &o : objects) {
    float t;
    Vec3 n;
    if (intersect(o, ro, rd, t, n) && t < hit.t) {
      hit.t = t;
      hit.n = n;
      hit.obj = &o;
    }
  }
  return hit.obj != nullptr;
}

// Fraction of light that survives between 'ro' and distance tmax (0 = blocked).
float RayTracer::shadow_transmittance(const Vec3 &ro, const Vec3 &rd, float tmax) const {
  float transmit = 1.0f;
  for (const Object &o : objects) {
    if (!o.mat.casts_shadow)
      continue;
    float t;
    Vec3 n;
    if (intersect(o, ro, rd, t, n) && t < tmax) {
      if (o.mat.transparency <= 0.0f)
        return 0.0f;
      transmit *= o.mat.transparency * 0.6f;
    }
  }
  return transmit;
}

Vec3 RayTracer::trace(const Vec3 &ro, const Vec3 &rd, int depth, const RTEnvironment &env) const {
  Hit hit;
  if (!closest(ro, rd, hit)) {
    float t = std::pow(std::max(0.0f, rd.y), 0.55f);
    return mix(env.sky_horizon, env.sky_top, std::min(1.0f, t));
  }

  const RTMaterial &m = hit.obj->mat;
  Vec3 p = ro + rd * hit.t;
  Vec3 n = hit.n;
  bool inside = dot(n, rd) > 0.0f;
  if (inside)
    n = -n;

  Vec3 base = m.color;
  Vec3 emission = m.emission;
  if (m.pattern == 1) { // mossy forest floor
    float v = 0.86f + 0.14f * std::sin(p.x * 1.9f) * std::cos(p.z * 1.5f) +
              0.08f * std::sin(p.x * 5.3f + p.z * 3.1f);
    base = base * v;
  } else if (m.pattern == 2) { // flame: yellow-white at the base -> red tip
    float h = std::min(1.0f, std::max(0.0f, (p.y - 0.29f) / 1.3f));
    Vec3 f = mix(Vec3(1.0f, 0.88f, 0.35f), Vec3(1.0f, 0.18f, 0.02f), std::pow(h, 0.8f));
    base = f;
    emission = f * 0.9f;
  }

  // ---- Phong local illumination ----
  Vec3 V = -rd;
  Vec3 color = emission + env.ambient * base;
  for (const RTLight &l : lights) {
    Vec3 L;
    float att = 1.0f, dist = INF;
    if (l.directional) {
      L = normalize(l.pos);
    } else {
      Vec3 d = l.pos - p;
      dist = length(d);
      L = d * (1.0f / dist);
      att = 1.0f / (l.kc + l.kl * dist + l.kq * dist * dist);
    }
    color += l.ambient * base * att;
    float ndl = dot(n, L);
    if (ndl <= 0.0f)
      continue;
    float vis = shadow_transmittance(p + n * 1e-3f, L, dist);
    if (vis <= 0.0f)
      continue;
    Vec3 R = reflect_dir(-L, n);
    float spec = std::pow(std::max(0.0f, dot(R, V)), m.shininess) * m.specular;
    color += (l.diffuse * base * ndl + l.specular * spec) * (att * vis);
  }

  // ---- reflection / refraction ----
  if (depth > 0 && (m.reflectivity > 0.0f || m.transparency > 0.0f)) {
    Vec3 reflected = reflect_dir(rd, n);
    if (m.transparency > 0.0f) {
      float eta = inside ? m.ior : 1.0f / m.ior;
      float cosi = std::min(1.0f, std::max(0.0f, -dot(n, rd)));
      float k = 1.0f - eta * eta * (1.0f - cosi * cosi);
      float r0 = (1.0f - m.ior) / (1.0f + m.ior);
      r0 *= r0;
      float fresnel = r0 + (1.0f - r0) * std::pow(1.0f - cosi, 5.0f); // Schlick
      Vec3 rc = trace(p + n * 2e-3f, reflected, depth - 1, env);
      Vec3 through;
      if (k < 0.0f) { // total internal reflection
        through = rc;
      } else {
        Vec3 refr = normalize(rd * eta + n * (eta * cosi - std::sqrt(k)));
        Vec3 tc = trace(p - n * 2e-3f, refr, depth - 1, env);
        through = rc * fresnel + tc * (1.0f - fresnel);
      }
      color = color * (1.0f - m.transparency) + through * m.transparency;
    } else {
      Vec3 rc = trace(p + n * 2e-3f, reflected, depth - 1, env);
      color = color * (1.0f - m.reflectivity) + rc * m.reflectivity;
    }
  }

  // ---- exponential-squared fog (same law as GL_EXP2) ----
  if (env.fog && m.fogged) {
    float f = std::exp(-std::pow(env.fog_density * hit.t, 2.0f));
    color = mix(env.fog_color, color, f);
  }
  return color;
}

std::vector<unsigned char> RayTracer::render(const RTCamera &cam, const RTEnvironment &env,
                                             int w, int h, int ss, int max_depth) const {
  std::vector<unsigned char> img(static_cast<size_t>(w) * h * 3);
  Vec3 fwd = normalize(cam.target - cam.eye);
  Vec3 right = normalize(cross(fwd, Vec3(0, 1, 0)));
  Vec3 up = cross(right, fwd);
  float th = std::tan(cam.fov_deg * 0.5f * PI_F / 180.0f);
  float aspect = static_cast<float>(w) / h;

  unsigned nt = std::max(1u, std::thread::hardware_concurrency());
  auto worker = [&](unsigned id) {
    for (int y = static_cast<int>(id); y < h; y += static_cast<int>(nt)) {
      for (int x = 0; x < w; ++x) {
        Vec3 sum;
        for (int sy = 0; sy < ss; ++sy) {
          for (int sx = 0; sx < ss; ++sx) {
            float px = (x + (sx + 0.5f) / ss) / w;
            float py = (y + (sy + 0.5f) / ss) / h;
            float u = (2.0f * px - 1.0f) * aspect * th;
            float v = (1.0f - 2.0f * py) * th;
            Vec3 d = normalize(fwd + right * u + up * v);
            sum += trace(cam.eye, d, max_depth, env);
          }
        }
        Vec3 c = clamp01(sum * (1.0f / (ss * ss)));
        size_t i = (static_cast<size_t>(y) * w + x) * 3;
        img[i] = static_cast<unsigned char>(c.x * 255.0f + 0.5f);
        img[i + 1] = static_cast<unsigned char>(c.y * 255.0f + 0.5f);
        img[i + 2] = static_cast<unsigned char>(c.z * 255.0f + 0.5f);
      }
    }
  };
  std::vector<std::thread> pool;
  for (unsigned i = 0; i < nt; ++i)
    pool.emplace_back(worker, i);
  for (auto &t : pool)
    t.join();
  return img;
}
