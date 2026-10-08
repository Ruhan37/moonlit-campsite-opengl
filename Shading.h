#ifndef SHADING_H
#define SHADING_H

// ---------------------------------------------------------------------------
// Shading.h - Gouraud (fixed-function) <-> Phong (GLSL per-pixel) switching.
//
//  * Gouraud : OpenGL fixed-function lighting, evaluated per VERTEX and the
//              resulting colours are interpolated (glShadeModel(GL_SMOOTH)).
//  * Phong   : a GLSL 1.20 program that interpolates the NORMAL and evaluates
//              ambient + diffuse + specular per PIXEL.
//
// The GLSL program re-uses the exact same OpenGL state as the fixed-function
// path (gl_LightSource, gl_FrontMaterial, gl_Fog, glColor, textures), so every
// existing draw call works unchanged in both modes.  To know whether lighting /
// texturing / fog is currently enabled, glEnable()/glDisable() are wrapped by
// the two hook functions below (the macros at the bottom of this header).
// ---------------------------------------------------------------------------

#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <GLUT/glut.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#endif

void gl_hook_enable(GLenum cap);
void gl_hook_disable(GLenum cap);

bool shading_init();            // compile + link the Phong program
bool shading_available();       // true when the shader compiled successfully
void shading_use(bool phong);   // select Phong (true) or Gouraud (false)
bool shading_active();          // true while the Phong program is bound

#ifndef SHADING_NO_HOOKS
#define glEnable(cap) gl_hook_enable(cap)
#define glDisable(cap) gl_hook_disable(cap)
#endif

#endif // SHADING_H
