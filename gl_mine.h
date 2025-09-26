//
// Created by 25190 on 2025/9/26.
//

#ifndef GL_MINE_H
#define GL_MINE_H
#include "geometry.h"
#include "tgaimage.h"

void lookat(const vec3 eye, const vec3 center, const vec3 up);
void init_perspective(const double f);
void init_viewport(const int x, const int y, const int w, const int h);
void init_zbuffer(const int width, const int height);

struct IShader {
    virtual std::pair<bool,TGAColor> fragment(const vec3 bar) const = 0; // abstract class
};

typedef vec4 Triangle[3]; // a triangle primitive is made of three ordered points
void rasterize(const Triangle &clip, const IShader &shader, TGAImage &framebuffer);

#endif //GL_MINE_H
