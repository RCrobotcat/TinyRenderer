- `main.cpp`
```cpp
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

struct PhongShader : IShader
{
    const Model &model;
    vec3 l; // light direction in eye coordinates
    vec3 tri[3]; // triangle in eye coordinates

    PhongShader(const vec3 light, const Model &m) : model(m)
    {
        l = normalized((ModelView * vec4{light.x, light.y, light.z, 0.}).xyz());
        // transform the light vector to view coordinates
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
        TGAColor gl_FragColor = {255, 255, 255, 255}; // output color of the fragment
        vec3 n = normalized(cross(tri[1] - tri[0], tri[2] - tri[0])); // triangle normal in eye coordinates
        vec3 r = normalized(n * (n * l) * 2 - l); // reflected light direction
        double ambient = .3; // ambient light intensity
        double diff = std::max(0., n * l); // diffuse light intensity
        double spec = std::pow(std::max(r.z, 0.), 35);
        // specular intensity, note that the camera lies on the z-axis (in eye coordinates), therefore simple r.z, since (0,0,1)*(r.x, r.y, r.z) = r.z
        for (int channel: {0, 1, 2})
            gl_FragColor[channel] *= std::min(1., ambient + .4 * diff + .9 * spec);
        return {false, gl_FragColor}; // do not discard the pixel
    }
};

int main()
{
    Model model("../Obj/african_head.obj");
    constexpr int width = 800; // output image size
    constexpr int height = 800;
    constexpr vec3 light_dir{1, 1, 1}; // light direction
    constexpr vec3 eye{-1, 0, 2}; // camera position
    constexpr vec3 center{0, 0, 0}; // camera direction
    constexpr vec3 up{0, 1, 0}; // camera up vector

    lookat(eye, center, up); // build the ModelView   matrix
    init_perspective(norm(eye - center)); // build the Perspective matrix
    init_viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8); // build the Viewport matrix
    init_zbuffer(width, height); // build the z-buffer
    TGAImage framebuffer(width, height, TGAImage::RGB);

    PhongShader shader(light_dir, model);
    for (int f = 0; f < model.nfaces(); f++)
    {
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

```

- `gl_mine.h`
```cpp
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

```

- `gl_mine.cpp`
```cpp
//
// Created by 25190 on 2025/9/26.
//

#include "gl_mine.h"

#include <algorithm>
#include <vector>
#include "geometry.h"
#include "tgaimage.h"

struct TGAImage;
mat<4, 4> ModelView, Viewport, Perspective;
std::vector<double> zbuffer; // depth buffer

// 视口变换矩阵
void init_viewport(const int x, const int y, const int w, const int h)
{
    Viewport = {{{w / 2., 0, 0, x + w / 2.}, {0, h / 2., 0, y + h / 2.}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
}

// 透视投影矩阵 projection matrix (f是焦距, f越大, 视野越窄)
void init_perspective(const double f)
{
    Perspective = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, -1 / f, 1}}};
}

// 视图变换矩阵 ModelView matrix
void lookat(const vec3 eye, const vec3 center, const vec3 up)
{
    vec3 n = normalized(eye - center);
    vec3 l = normalized(cross(up, n));
    vec3 m = normalized(cross(n, l));
    ModelView = mat<4, 4>{{{l.x, l.y, l.z, 0}, {m.x, m.y, m.z, 0}, {n.x, n.y, n.z, 0}, {0, 0, 0, 1}}} *
                mat<4, 4>{{{1, 0, 0, -center.x}, {0, 1, 0, -center.y}, {0, 0, 1, -center.z}, {0, 0, 0, 1}}};
}

void init_zbuffer(const int width, const int height)
{
    zbuffer = std::vector(width * height, -1000.); // 初始化zbuffer设置为负无穷（无限远远）
}

void rasterize(const Triangle &clip, const IShader &shader, TGAImage &framebuffer)
{
    vec4 ndc[3] = {clip[0] / clip[0].w, clip[1] / clip[1].w, clip[2] / clip[2].w}; // normalized device coordinates
    vec2 screen[3] = {(Viewport * ndc[0]).xy(), (Viewport * ndc[1]).xy(), (Viewport * ndc[2]).xy()};
    // screen coordinates

    mat<3, 3> ABC = {{{screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.}}};
    if (ABC.det() < 1) return; // backface culling + discarding triangles that cover less than a pixel
    // 三角形面积：1/2*det(ABC)

    auto [bbminx,bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x}); // bounding box for the triangle
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y});
    // defined by its top left and bottom right corners
#pragma omp parallel for
    for (int x = std::max<int>(bbminx, 0); x <= std::min<int>(bbmaxx, framebuffer.width() - 1); x++)
    {
        // clip the bounding box by the screen
        for (int y = std::max<int>(bbminy, 0); y <= std::min<int>(bbmaxy, framebuffer.height() - 1); y++)
        {
            vec3 bc = ABC.invert_transpose() * vec3{static_cast<double>(x), static_cast<double>(y), 1.}; // 求得重心坐标
            // barycentric coordinates of {x,y} w.r.t the triangle
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;
            // negative barycentric coordinate => the pixel is outside the triangle

            double z = bc * vec3{ndc[0].z, ndc[1].z, ndc[2].z}; // linear interpolation of the depth
            if (z <= zbuffer[x + y * framebuffer.width()]) continue;
            // discard fragments that are too deep w.r.t the z-buffer
            auto [discard, color] = shader.fragment(bc);
            if (discard) continue; // fragment shader can discard current fragment
            zbuffer[x + y * framebuffer.width()] = z; // update the z-buffer
            framebuffer.set(x, y, color); // update the framebuffer
        }
    }
}

```