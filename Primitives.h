#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

#include "Shading.h"
#include <vector>

extern const float NO_EMISSION[4];

void material(const float color[4], const float emission[4] = NO_EMISSION, float specular = 0.12f, float shininess = 12.0f);

class Quadric {
public:
    GLUquadric* handle;
    Quadric();
    ~Quadric();
    void destroy();
};

void cylinder_y(Quadric& quadric, float base_radius, float height, float top_radius = -1.0f, int slices = 24, int stacks = 4, GLuint texture = 0);
void cone_y(Quadric& quadric, float radius, float height, int slices = 24, GLuint texture = 0);

void scaled_sphere(float scale_x, float scale_y, float scale_z, int slices = 20, int stacks = 14);
void scaled_cube(float scale_x, float scale_y, float scale_z);

void shadow_ellipse(float x, float z, float radius_x, float radius_z, float alpha = 0.20f, float angle = 0.0f, float ground_y = 0.012f);

class TextureBank {
public:
    GLuint ground;
    GLuint bark;
    GLuint canvas;

    TextureBank();
private:
    static GLuint upload(int width, int height, const std::vector<unsigned char>& pixels);
    static GLuint generate_ground_texture(int size = 96);
    static GLuint generate_bark_texture(int width = 64, int height = 96);
    static GLuint generate_canvas_texture(int size = 64);
};

void textured_quad(const float vertices[4][3], const float normal[3], GLuint texture, float uv_scale_x = 1.0f, float uv_scale_y = 1.0f);

#endif // PRIMITIVES_H
