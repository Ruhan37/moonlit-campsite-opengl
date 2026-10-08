#define SHADING_NO_HOOKS
#include "Shading.h"
#include <iostream>

// ---------------------------------------------------------------------------
// GLSL 1.20 Phong shader (legacy/compatibility built-ins, works on macOS GL 2.1)
// ---------------------------------------------------------------------------
static const char *VERTEX_SRC = R"GLSL(
#version 120
varying vec3 vNormal;      // eye-space normal (interpolated -> Phong shading)
varying vec4 vEye;         // eye-space position
varying vec4 vColor;       // glColor / material colour
varying vec2 vUV;

void main() {
  vEye = gl_ModelViewMatrix * gl_Vertex;
  vNormal = gl_NormalMatrix * gl_Normal;
  vColor = gl_Color;
  vUV = gl_MultiTexCoord0.xy;
  gl_Position = ftransform();
}
)GLSL";

static const char *FRAGMENT_SRC = R"GLSL(
#version 120
varying vec3 vNormal;
varying vec4 vEye;
varying vec4 vColor;
varying vec2 vUV;

uniform bool uLighting;
uniform bool uTexture;
uniform bool uFog;
uniform sampler2D uSampler;

void main() {
  vec4 color = vColor;

  if (uLighting) {
    vec3 N = normalize(vNormal);
    vec3 V = normalize(-vEye.xyz);
    vec3 base = vColor.rgb;                        // GL_COLOR_MATERIAL: ka = kd = glColor
    vec3 total = gl_FrontMaterial.emission.rgb
               + gl_LightModel.ambient.rgb * base; // global ambient

    for (int i = 0; i < 2; ++i) {                  // GL_LIGHT0 (fire), GL_LIGHT1 (moon/sun)
      vec3 L;
      float attenuation = 1.0;
      if (gl_LightSource[i].position.w == 0.0) {   // directional light
        L = normalize(gl_LightSource[i].position.xyz);
      } else {                                     // point light + attenuation
        vec3 toLight = gl_LightSource[i].position.xyz - vEye.xyz;
        float d = length(toLight);
        L = toLight / d;
        attenuation = 1.0 / (gl_LightSource[i].constantAttenuation
                           + gl_LightSource[i].linearAttenuation * d
                           + gl_LightSource[i].quadraticAttenuation * d * d);
      }
      float NdotL = max(dot(N, L), 0.0);
      vec3 c = gl_LightSource[i].ambient.rgb * base;                 // ambient
      c += NdotL * gl_LightSource[i].diffuse.rgb * base;             // diffuse
      if (NdotL > 0.0) {                                             // specular (Phong)
        vec3 R = reflect(-L, N);
        float s = pow(max(dot(R, V), 0.0), max(gl_FrontMaterial.shininess, 0.0001));
        c += s * gl_LightSource[i].specular.rgb * gl_FrontMaterial.specular.rgb;
      }
      total += attenuation * c;
    }
    color = vec4(clamp(total, 0.0, 1.0), vColor.a);
  }

  if (uTexture)
    color *= texture2D(uSampler, vUV);

  if (uFog) {                                      // GL_EXP2 fog
    float d = abs(vEye.z);
    float f = clamp(exp(-pow(gl_Fog.density * d, 2.0)), 0.0, 1.0);
    color.rgb = mix(gl_Fog.color.rgb, color.rgb, f);
  }
  gl_FragColor = color;
}
)GLSL";

static GLuint g_program = 0;
static bool g_active = false;
static GLint g_loc_lighting = -1, g_loc_texture = -1, g_loc_fog = -1;

static GLuint compile(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);
  GLint ok = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
    std::cerr << "Shader compile error:\n" << log << std::endl;
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

// Push the current enable/disable state of lighting, texturing and fog into
// the shader (called whenever one of them changes while Phong is active).
static void sync_uniforms() {
  if (!g_active)
    return;
  glUniform1i(g_loc_lighting, glIsEnabled(GL_LIGHTING) ? 1 : 0);
  glUniform1i(g_loc_texture, glIsEnabled(GL_TEXTURE_2D) ? 1 : 0);
  glUniform1i(g_loc_fog, glIsEnabled(GL_FOG) ? 1 : 0);
}

bool shading_init() {
  GLuint vs = compile(GL_VERTEX_SHADER, VERTEX_SRC);
  GLuint fs = compile(GL_FRAGMENT_SHADER, FRAGMENT_SRC);
  if (!vs || !fs)
    return false;

  g_program = glCreateProgram();
  glAttachShader(g_program, vs);
  glAttachShader(g_program, fs);
  glLinkProgram(g_program);
  GLint ok = 0;
  glGetProgramiv(g_program, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[2048];
    glGetProgramInfoLog(g_program, sizeof(log), nullptr, log);
    std::cerr << "Shader link error:\n" << log << std::endl;
    glDeleteProgram(g_program);
    g_program = 0;
    return false;
  }
  g_loc_lighting = glGetUniformLocation(g_program, "uLighting");
  g_loc_texture = glGetUniformLocation(g_program, "uTexture");
  g_loc_fog = glGetUniformLocation(g_program, "uFog");
  glUseProgram(g_program);
  glUniform1i(glGetUniformLocation(g_program, "uSampler"), 0);
  glUseProgram(0);
  return true;
}

bool shading_available() { return g_program != 0; }

void shading_use(bool phong) {
  g_active = phong && g_program != 0;
  glUseProgram(g_active ? g_program : 0);
  sync_uniforms();
}

bool shading_active() { return g_active; }

void gl_hook_enable(GLenum cap) {
  glEnable(cap);
  if (cap == GL_LIGHTING || cap == GL_TEXTURE_2D || cap == GL_FOG)
    sync_uniforms();
}

void gl_hook_disable(GLenum cap) {
  glDisable(cap);
  if (cap == GL_LIGHTING || cap == GL_TEXTURE_2D || cap == GL_FOG)
    sync_uniforms();
}
