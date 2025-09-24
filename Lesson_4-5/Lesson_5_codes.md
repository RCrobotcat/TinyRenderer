- `main.cpp`
```cpp
#include <algorithm>
#include <cmath>
#include <tuple>
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

// 把三维模型的顶点坐标转换为屏幕上的像素点位置 (视口变换 => NDC to screen space)
std::tuple<int, int, int> project(vec3 v)
{
    // First of all, (x,y) is an orthogonal projection of the vector (x,y,z).
    return {
        (v.x + 1.) * width / 2,
        // Second, since the input models are scaled to have fit in the [-1,1]^3 world coordinates,
        (v.y + 1.) * height / 2,
        (v.z + 1.) * 255. / 2 // z is between -1 and 1
        // with higher z values meaning closer to the camera
    }; // we want to shift the vector (x,y) and then scale it to span the entire screen.
}

// 视口变换矩阵
void viewport(const int x, const int y, const int w, const int h)
{
    Viewport = {{{w / 2., 0, 0, x + w / 2.}, {0, h / 2., 0, y + h / 2.}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
}

// 透视投影矩阵 projection matrix (f是焦距, f越大, 视野越窄)
void perspective(const double f)
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

void rasterize(const vec4 clip[3], std::vector<double> &zbuffer, TGAImage &framebuffer, const TGAColor color)
{
    vec4 ndc[3] = {clip[0] / clip[0].w, clip[1] / clip[1].w, clip[2] / clip[2].w}; // normalized device coordinates
    vec2 screen[3] = {(Viewport * ndc[0]).xy(), (Viewport * ndc[1]).xy(), (Viewport * ndc[2]).xy()};
    // screen coordinates

    mat<3, 3> ABC = {{{screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.}}};
    if (ABC.det() < 1) return; // backface culling + discarding triangles that cover less than a pixel

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
    std::vector<double> zbuffer(width * height, -std::numeric_limits<double>::max());

    for (int i = 0; i < model.nfaces(); i++)
    {
        // iterate through all triangles
        vec4 clip[3];
        for (int d: {0, 1, 2})
        {
            // assemble the primitive
            vec3 v = model.vert(i, d);
            clip[d] = Perspective * ModelView * vec4{v.x, v.y, v.z, 1.};
        }
        TGAColor rnd;
        for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255;
        rasterize(clip, zbuffer, framebuffer, rnd); // rasterize the primitive
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

```
- `geometry.h`
```cpp
//
// Created by 25190 on 2025/9/14.
//

#ifndef GEOMETRY_H
#define GEOMETRY_H

#pragma once
#include <cmath>
#include <cassert>
#include <iostream>

template<int n>
struct vec
{
    double data[n] = {0};

    double &operator[](const int i)
    {
        assert(i>=0 && i<n);
        return data[i];
    }

    double operator[](const int i) const
    {
        assert(i>=0 && i<n);
        return data[i];
    }
};

template<int n>
double operator*(const vec<n> &lhs, const vec<n> &rhs)
{
    double ret = 0; // N.B. Do not ever, ever use such for loops! They are highly confusing.
    for (int i = n; i--; ret += lhs[i] * rhs[i]);
    // Here I used them as a tribute to old-school game programmers fighting for every CPU cycle.
    return ret; // Once upon a time reverse loops were faster than the normal ones, it is not the case anymore.
}

template<int n>
vec<n> operator+(const vec<n> &lhs, const vec<n> &rhs)
{
    vec<n> ret = lhs;
    for (int i = n; i--; ret[i] += rhs[i]);
    return ret;
}

template<int n>
vec<n> operator-(const vec<n> &lhs, const vec<n> &rhs)
{
    vec<n> ret = lhs;
    for (int i = n; i--; ret[i] -= rhs[i]);
    return ret;
}

template<int n>
vec<n> operator*(const vec<n> &lhs, const double &rhs)
{
    vec<n> ret = lhs;
    for (int i = n; i--; ret[i] *= rhs);
    return ret;
}

template<int n>
vec<n> operator*(const double &lhs, const vec<n> &rhs)
{
    return rhs * lhs;
}

template<int n>
vec<n> operator/(const vec<n> &lhs, const double &rhs)
{
    vec<n> ret = lhs;
    for (int i = n; i--; ret[i] /= rhs);
    return ret;
}

template<int n>
std::ostream &operator<<(std::ostream &out, const vec<n> &v)
{
    for (int i = 0; i < n; i++) out << v[i] << " ";
    return out;
}

template<>
struct vec<2>
{
    double x = 0, y = 0;

    double &operator[](const int i)
    {
        assert(i>=0 && i<2);
        return i ? y : x;
    }

    double operator[](const int i) const
    {
        assert(i>=0 && i<2);
        return i ? y : x;
    }
};

template<>
struct vec<3>
{
    double x = 0, y = 0, z = 0;

    double &operator[](const int i)
    {
        assert(i>=0 && i<3);
        return i ? (1 == i ? y : z) : x;
    }

    double operator[](const int i) const
    {
        assert(i>=0 && i<3);
        return i ? (1 == i ? y : z) : x;
    }
};

template<>
struct vec<4>
{
    double x = 0, y = 0, z = 0, w = 0;

    double &operator[](const int i)
    {
        assert(i>=0 && i<4);
        return i < 2 ? (i ? y : x) : (2 == i ? z : w);
    }

    double operator[](const int i) const
    {
        assert(i>=0 && i<4);
        return i < 2 ? (i ? y : x) : (2 == i ? z : w);
    }

    vec<2> xy() const { return {x, y}; }
    vec<3> xyz() const { return {x, y, z}; }
};

typedef vec<2> vec2;
typedef vec<3> vec3;
typedef vec<4> vec4;

template<int n>
double norm(const vec<n> &v)
{
    return std::sqrt(v * v);
}

template<int n>
vec<n> normalized(const vec<n> &v)
{
    return v / norm(v);
}

inline vec3 cross(const vec3 &v1, const vec3 &v2)
{
    return {v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x};
}

template<int n>
struct dt;

template<int nrows, int ncols>
struct mat
{
    vec<ncols> rows[nrows] = {{}};

    vec<ncols> &operator[](const int idx)
    {
        assert(idx>=0 && idx<nrows);
        return rows[idx];
    }

    const vec<ncols> &operator[](const int idx) const
    {
        assert(idx>=0 && idx<nrows);
        return rows[idx];
    }

    double det() const
    {
        return dt<ncols>::det(*this);
    }

    // 伴随矩阵 cofactor matrix or adjoint matrix
    double cofactor(const int row, const int col) const
    {
        mat<nrows - 1, ncols - 1> submatrix;
        for (int i = nrows - 1; i--;)
            for (int j = ncols - 1; j--; submatrix[i][j] = rows[i + int(i >= row)][j + int(j >= col)]);
        return submatrix.det() * ((row + col) % 2 ? -1 : 1);
    }

    mat<nrows, ncols> invert_transpose() const
    {
        mat<nrows, ncols> adjugate_transpose; // transpose to ease determinant computation, check the last line
        for (int i = nrows; i--;)
            for (int j = ncols; j--; adjugate_transpose[i][j] = cofactor(i, j));
        return adjugate_transpose / (adjugate_transpose[0] * rows[0]);
    }

    mat<nrows, ncols> invert() const
    {
        return invert_transpose().transpose();
    }

    mat<ncols, nrows> transpose() const
    {
        mat<ncols, nrows> ret;
        for (int i = ncols; i--;)
            for (int j = nrows; j--; ret[i][j] = rows[j][i]);
        return ret;
    }
};

template<int nrows, int ncols>
vec<ncols> operator*(const vec<nrows> &lhs, const mat<nrows, ncols> &rhs)
{
    return (mat<1, nrows>{{lhs}} * rhs)[0];
}

template<int nrows, int ncols>
vec<nrows> operator*(const mat<nrows, ncols> &lhs, const vec<ncols> &rhs)
{
    vec<nrows> ret;
    for (int i = nrows; i--; ret[i] = lhs[i] * rhs);
    return ret;
}

template<int R1, int C1, int C2>
mat<R1, C2> operator*(const mat<R1, C1> &lhs, const mat<C1, C2> &rhs)
{
    mat<R1, C2> result;
    for (int i = R1; i--;)
        for (int j = C2; j--;)
            for (int k = C1; k--; result[i][j] += lhs[i][k] * rhs[k][j]);
    return result;
}

template<int nrows, int ncols>
mat<nrows, ncols> operator*(const mat<nrows, ncols> &lhs, const double &val)
{
    mat<nrows, ncols> result;
    for (int i = nrows; i--; result[i] = lhs[i] * val);
    return result;
}

template<int nrows, int ncols>
mat<nrows, ncols> operator/(const mat<nrows, ncols> &lhs, const double &val)
{
    mat<nrows, ncols> result;
    for (int i = nrows; i--; result[i] = lhs[i] / val);
    return result;
}

template<int nrows, int ncols>
mat<nrows, ncols> operator+(const mat<nrows, ncols> &lhs, const mat<nrows, ncols> &rhs)
{
    mat<nrows, ncols> result;
    for (int i = nrows; i--;)
        for (int j = ncols; j--; result[i][j] = lhs[i][j] + rhs[i][j]);
    return result;
}

template<int nrows, int ncols>
mat<nrows, ncols> operator-(const mat<nrows, ncols> &lhs, const mat<nrows, ncols> &rhs)
{
    mat<nrows, ncols> result;
    for (int i = nrows; i--;)
        for (int j = ncols; j--; result[i][j] = lhs[i][j] - rhs[i][j]);
    return result;
}

template<int nrows, int ncols>
std::ostream &operator<<(std::ostream &out, const mat<nrows, ncols> &m)
{
    for (int i = 0; i < nrows; i++) out << m[i] << std::endl;
    return out;
}

template<int n>
struct dt
{
    // template metaprogramming to compute the determinant recursively
    static double det(const mat<n, n> &src)
    {
        double ret = 0;
        for (int i = n; i--; ret += src[0][i] * src.cofactor(0, i));
        return ret;
    }
};

template<>
struct dt<1>
{
    // template specialization to stop the recursion
    static double det(const mat<1, 1> &src)
    {
        return src[0][0];
    }
};

#endif //GEOMETRY_H

```