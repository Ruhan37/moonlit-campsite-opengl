#include "Primitives.h"
#include <cmath>
#include <random>

const float NO_EMISSION[4] = {0.0f, 0.0f, 0.0f, 1.0f};

void material(const float color[4], const float emission[4], float specular, float shininess) {
    glColor4fv(color);
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, color);
    float spec_arr[4] = {specular, specular, specular, color[3]};
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, spec_arr);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
}



Quadric::Quadric() {
    handle = gluNewQuadric();
    gluQuadricNormals(handle, GLU_SMOOTH);
    gluQuadricDrawStyle(handle, GLU_FILL);
    gluQuadricTexture(handle, GL_TRUE);
}

Quadric::~Quadric() {
    destroy();
}

void Quadric::destroy() {
    if (handle) {
        gluDeleteQuadric(handle);
        handle = nullptr;
    }
}

void cylinder_y(Quadric& quadric, float base_radius, float height, float top_radius, int slices, int stacks, GLuint texture) {
    float top = (top_radius < 0.0f) ? base_radius : top_radius;
    if (texture > 0) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, texture);
    }
    glPushMatrix();
    glRotatef(-90.0f, 1.0f, 0.0f, 0.0f);
    gluCylinder(quadric.handle, base_radius, top, height, slices, stacks);
    gluDisk(quadric.handle, 0.0f, base_radius, slices, 1);
    glTranslatef(0.0f, 0.0f, height);
    gluDisk(quadric.handle, 0.0f, top, slices, 1);
    glPopMatrix();
    if (texture > 0) {
        glDisable(GL_TEXTURE_2D);
    }
}

void cone_y(Quadric& quadric, float radius, float height, int slices, GLuint texture) {
    cylinder_y(quadric, radius, height, 0.015f, slices, 5, texture);
}

void scaled_sphere(float scale_x, float scale_y, float scale_z, int slices, int stacks) {
    glPushMatrix();
    glScalef(scale_x, scale_y, scale_z);
    glutSolidSphere(1.0, slices, stacks);
    glPopMatrix();
}

void scaled_cube(float scale_x, float scale_y, float scale_z) {
    glPushMatrix();
    glScalef(scale_x, scale_y, scale_z);
    glutSolidCube(1.0);
    glPopMatrix();
}

void shadow_ellipse(float x, float z, float radius_x, float radius_z, float alpha, float angle, float ground_y) {
    glEnable(GL_BLEND);
    float col[4] = {0.008f, 0.010f, 0.012f, alpha};
    material(col, NO_EMISSION, 0.0f, 0.0f);
    glPushMatrix();
    glTranslatef(x, ground_y, z);
    glRotatef(angle, 0.0f, 1.0f, 0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    for (int i = 0; i <= 32; ++i) {
        float theta = 2.0f * M_PI * i / 32.0f;
        if (i == 0) {
            glColor4f(0.008f, 0.010f, 0.012f, 0.0f);
        } else {
            glColor4f(0.008f, 0.010f, 0.012f, alpha);
        }
        glVertex3f(radius_x * std::cos(theta), 0.0f, radius_z * std::sin(theta));
    }
    glEnd();
    glPopMatrix();
    glDisable(GL_BLEND);
}

TextureBank::TextureBank() {
    ground = generate_ground_texture();
    bark = generate_bark_texture();
    canvas = generate_canvas_texture();
}

GLuint TextureBank::upload(int width, int height, const std::vector<unsigned char>& pixels) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gluBuild2DMipmaps(GL_TEXTURE_2D, GL_RGB, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

GLuint TextureBank::generate_ground_texture(int size) {
    std::mt19937 gen(4401);
    std::uniform_int_distribution<> dist_pos(0, size - 1);
    std::uniform_real_distribution<float> dist_rad(4.0f, 14.0f);
    std::uniform_int_distribution<> dist_grain(-14, 14);

    struct Blotch { int x, y; float r; };
    std::vector<Blotch> blotches;
    for (int i = 0; i < 32; ++i) {
        blotches.push_back({dist_pos(gen), dist_pos(gen), dist_rad(gen)});
    }

    std::vector<unsigned char> pixels;
    pixels.reserve(size * size * 3);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float moss = 0.0f;
            for (const auto& b : blotches) {
                float dist = std::hypot(x - b.x, y - b.y);
                float val = 1.0f - dist / b.r;
                if (val > 0.0f) moss += val;
            }
            int grain = dist_grain(gen);
            int red = 42 + grain + static_cast<int>(7 * moss);
            int green = 57 + grain + static_cast<int>(15 * moss);
            int blue = 31 + grain / 2 + static_cast<int>(3 * moss);
            pixels.push_back(std::max(0, red));
            pixels.push_back(std::max(0, green));
            pixels.push_back(std::max(0, blue));
        }
    }
    return upload(size, size, pixels);
}

GLuint TextureBank::generate_bark_texture(int width, int height) {
    std::mt19937 gen(2207);
    std::uniform_int_distribution<> dist_noise(-9, 9);
    std::vector<unsigned char> pixels;
    pixels.reserve(width * height * 3);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int groove = ((x + static_cast<int>(4 * std::sin(y * 0.15f))) % 13 < 3) ? 18 : 0;
            float knot = 11 * std::sin(x * 0.34f + y * 0.07f);
            int noise = dist_noise(gen);
            int r = std::max(12, static_cast<int>(76 + knot + noise - groove));
            int g = std::max(8, static_cast<int>(43 + noise * 0.5f - groove * 0.5f));
            int b = std::max(5, static_cast<int>(23 + noise * 0.25f - groove * 0.25f));
            pixels.push_back(r);
            pixels.push_back(g);
            pixels.push_back(b);
        }
    }
    return upload(width, height, pixels);
}

GLuint TextureBank::generate_canvas_texture(int size) {
    std::mt19937 gen(113);
    std::uniform_int_distribution<> dist_weather(-5, 5);
    std::vector<unsigned char> pixels;
    pixels.reserve(size * size * 3);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            int weave = (x % 3 == 0) ? 8 : ((y % 3 == 0) ? -4 : 0);
            int weather = dist_weather(gen);
            pixels.push_back(132 + weave + weather);
            pixels.push_back(140 + weave + weather);
            pixels.push_back(86 + weather);
        }
    }
    return upload(size, size, pixels);
}

void textured_quad(const float vertices[4][3], const float normal[3], GLuint texture, float uv_scale_x, float uv_scale_y) {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glBegin(GL_QUADS);
    glNormal3f(normal[0], normal[1], normal[2]);
    const float uvs[4][2] = {{0.0f, 0.0f}, {uv_scale_x, 0.0f}, {uv_scale_x, uv_scale_y}, {0.0f, uv_scale_y}};
    for (int i = 0; i < 4; ++i) {
        glTexCoord2f(uvs[i][0], uvs[i][1]);
        glVertex3f(vertices[i][0], vertices[i][1], vertices[i][2]);
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}
