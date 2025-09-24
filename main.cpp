#include <algorithm>
#include <cmath>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"

#define M_PI 3.14159265358979323846

constexpr int width = 800;
constexpr int height = 800;

constexpr TGAColor white = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green = {0, 255, 0, 255};
constexpr TGAColor red = {0, 0, 255, 255};
constexpr TGAColor blue = {255, 128, 64, 255};
constexpr TGAColor yellow = {0, 200, 255, 255};

mat<4, 4> ModelView, Viewport, Perspective;

// ͸��ͶӰ
vec3 persp(vec3 v)
{
    constexpr double c = 3.;
    return v / (1 - v.z / c);
}

// �ӿڱ任����
void viewport(const int x, const int y, const int w, const int h)
{
    Viewport = {{{w / 2., 0, 0, x + w / 2.}, {0, h / 2., 0, y + h / 2.}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
}

// ͸��ͶӰ���� projection matrix (f�ǽ���, fԽ��, ��ҰԽխ)
void perspective(const double f)
{
    Perspective = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, -1 / f, 1}}};
}

// ��ͼ�任���� ModelView matrix
void lookat(const vec3 eye, const vec3 center, const vec3 up)
{
    vec3 n = normalized(eye - center);
    vec3 l = normalized(cross(up, n));
    vec3 m = normalized(cross(n, l));
    ModelView = mat<4, 4>{{{l.x, l.y, l.z, 0}, {m.x, m.y, m.z, 0}, {n.x, n.y, n.z, 0}, {0, 0, 0, 1}}} *
                mat<4, 4>{{{1, 0, 0, -center.x}, {0, 1, 0, -center.y}, {0, 0, 1, -center.z}, {0, 0, 0, 1}}};
}

void rasterize(const vec4 clip[3], std::vector<double> &zbuffer, TGAImage &framebuffer, const TGAColor color)
{
    // �����ε�3���������βü�����
    vec4 ndc[3] = {clip[0] / clip[0].w, clip[1] / clip[1].w, clip[2] / clip[2].w}; // normalized device coordinates
    vec2 screen[3] = {(Viewport * ndc[0]).xy(), (Viewport * ndc[1]).xy(), (Viewport * ndc[2]).xy()};
    // screen coordinates

    mat<3, 3> ABC = {{{screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.}}};
    if (ABC.det() < 1) return; // backface culling + discarding triangles that cover less than a pixel
    // ԭ��ABC������ʽ����2������������������С��0˵���Ǳ��棬���С��1˵�����С��һ������
    // ���������(�з���)��1/2 * (AB x AC)

    auto [bbminx,bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x}); // bounding box for the triangle
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y});
    // defined by its top left and bottom right corners
#pragma omp parallel for
    for (int x = std::max<int>(bbminx, 0); x <= std::min<int>(bbmaxx, framebuffer.width() - 1); x++)
    {
        // clip the bounding box by the screen
        for (int y = std::max<int>(bbminy, 0); y <= std::min<int>(bbmaxy, framebuffer.height() - 1); y++)
        {
            vec3 bc = ABC.invert_transpose() * vec3{static_cast<double>(x), static_cast<double>(y), 1.};
            // barycentric coordinates of {x,y} w.r.t the triangle
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;
            // negative barycentric coordinate => the pixel is outside the triangle

            // ��zbuffer[x + y * width]���洢��ǰ���ص�����ֵ
            // x + y * width�������ǽ���ά����ӳ�䵽һά����
            double z = bc * vec3{ndc[0].z, ndc[1].z, ndc[2].z};
            if (z <= zbuffer[x + y * framebuffer.width()]) continue;
            zbuffer[x + y * framebuffer.width()] = z;
            framebuffer.set(x, y, color);
        }
    }
}

int main()
{
    Model model("../Obj/diablo3_pose.obj");
    constexpr int width = 800; // output image size
    constexpr int height = 800;
    constexpr vec3 eye{-1, 0, 2}; // camera position
    constexpr vec3 center{0, 0, 0}; // camera direction
    constexpr vec3 up{0, 1, 0}; // camera up vector

    lookat(eye, center, up); // build the ModelView   matrix
    perspective(norm(eye - center)); // build the Perspective matrix
    viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8); // build the Viewport    matrix

    TGAImage framebuffer(width, height, TGAImage::RGB);
    std::vector<double> zbuffer(width * height, -std::numeric_limits<double>::max()); // ��ʼ��zbuffer����Ϊ���������ԶԶ��

    for (int i = 0; i < model.nfaces(); i++)
    {
        // iterate through all triangles
        vec4 clip[3];
        for (int d: {0, 1, 2})
        {
            // assemble the primitive
            vec3 v = model.vert(i, d);
            clip[d] = Perspective * ModelView * vec4{v.x, v.y, v.z, 1.}; // transform to clip coordinates
        }
        TGAColor rnd;
        for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255; // random color
        rasterize(clip, zbuffer, framebuffer, rnd); // rasterize the primitive
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}
