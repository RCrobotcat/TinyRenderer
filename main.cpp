#include <algorithm>
#include <cmath>
#include <vector>
#include "geometry.h"
#include "gl_mine.h"
#include "model.h"
#include "tgaimage.h"

#define M_PI 3.14159265358979323846

constexpr int width = 800;
constexpr int height = 800;

extern mat<4, 4> ModelView, Perspective;
extern std::vector<double> zbuffer;

struct RandomShader : IShader
{
    const Model &model;
    TGAColor color = {};
    vec3 tri[3]; // triangle in eye coordinates

    RandomShader(const Model &m) : model(m)
    {
    }

    virtual vec4 vertex(const int face, const int vert)
    {
        vec3 v = model.vert(face, vert); // current vertex in object coordinates
        vec4 gl_Position = ModelView * vec4{v.x, v.y, v.z, 1.};
        tri[vert] = gl_Position.xyz(); // in eye coordinates
        return Perspective * gl_Position; // in clip coordinates
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bar) const
    {
        return {false, color}; // do not discard the pixel
    }
};

int main()
{
    Model model("../Obj/african_head.obj");
    constexpr int width = 800; // output image size
    constexpr int height = 800;
    constexpr vec3 eye{-1, 0, 2}; // camera position
    constexpr vec3 center{0, 0, 0}; // camera direction
    constexpr vec3 up{0, 1, 0}; // camera up vector

    lookat(eye, center, up); // build the ModelView   matrix
    init_perspective(norm(eye - center)); // build the Perspective matrix
    init_viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8); // build the Viewport matrix
    init_zbuffer(width, height); // build the z-buffer
    TGAImage framebuffer(width, height, TGAImage::RGB);

    RandomShader shader(model);
    for (int f = 0; f < model.nfaces(); f++)
    {
        // iterate through all facets
        // shader.color = {std::rand() % 255, std::rand() % 255, std::rand() % 255, 255};
        shader.color[0] = std::rand() % 255;
        shader.color[1] = std::rand() % 255;
        shader.color[2] = std::rand() % 255;
        shader.color[3] = 255;

        Triangle clip = {
            shader.vertex(f, 0), // assemble the primitive
            shader.vertex(f, 1),
            shader.vertex(f, 2)
        };
        rasterize(clip, shader, framebuffer); // rasterize the primitive
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}
