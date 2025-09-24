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

// Bresenham's line algorithm
void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color)
{
    bool steep = std::abs(ax - bx) < std::abs(ay - by);
    if (steep)
    {
        // if the line is steep, we transpose the image
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax > bx)
    {
        // make it left?to?right
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    int y = ay;
    int ierror = 0;
    for (int x = ax; x <= bx; x++)
    {
        if (steep) // if transposed, detranspose
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
        ierror += 2 * std::abs(by - ay);
        if (ierror > bx - ax)
        {
            y += by > ay ? 1 : -1;
            ierror -= 2 * (bx - ax);
        }
    }
}

// 计算三角形面积（有方向）
// 通过向量叉积计算面积
// Area = 1/2 * |AB x AC|
double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy)
{
    return .5 * ((by - ay) * (bx + ax) + (cy - by) * (cx + bx) + (ay - cy) * (ax + cx));
}

// 加上背面剔除和微小三角形剔除的版本
void triangle(int ax, int ay, int az, int bx, int by, int bz, int cx, int cy, int cz, TGAImage &zbuffer,
              TGAImage &framebuffer, TGAColor color)
{
    // 计算AABB轴对齐包围盒
    int bbminx = std::min(std::min(ax, bx), cx); // bounding box for the triangle
    int bbminy = std::min(std::min(ay, by), cy); // defined by its top left and bottom right corners
    int bbmaxx = std::max(std::max(ax, bx), cx);
    int bbmaxy = std::max(std::max(ay, by), cy);
    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area < 1) return; // backface culling + discarding triangles that cover less than a pixel

    // 遍历包围盒内的所有像素，根据重心坐标判断是否在三角形内部，如果在，就绘制这个像素，否则就忽略它
#pragma omp parallel for
    for (int x = bbminx; x <= bbmaxx; x++)
    {
        for (int y = bbminy; y <= bbmaxy; y++)
        {
            double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            double beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            if (alpha < 0 || beta < 0 || gamma < 0) // 像素在三角形外部
                continue; // negative barycentric coordinate => the pixel is outside the triangle

            unsigned char z = static_cast<unsigned char>(alpha * az + beta * bz + gamma * cz);
            if (zbuffer.get(x, y)[0] >= z) continue; // z-buffer test
            // z越大，代表越靠近观察者

            zbuffer.set(x, y, {z}); // write the z value in the z-buffer
            // {z} uses aggregate initialization(聚合类型 => 没有自定义构造函数, 可以用列表初始化) to create a TGAColor with only the first channel set to z and the rest to 0
            // 由于是灰度图(只有第一个通道来表示图片的灰度)，所以只需要设置第一个通道即可
            framebuffer.set(x, y, color);
        }
    }
}

vec3 rot(vec3 v)
{
    constexpr double a = M_PI / 6; // 30度
    // 旋转矩阵 绕Y轴旋转
    const mat<3, 3> Ry = {{{std::cos(a), 0, std::sin(a)}, {0, 1, 0}, {-std::sin(a), 0, std::cos(a)}}};
    return Ry * v;
}

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

// 透视投影
vec3 persp(vec3 v)
{
    constexpr double c = 3.;
    return v / (1 - v.z / c);
}

int main()
{
    Model model("../Obj/diablo3_pose.obj");
    TGAImage framebuffer(width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE); // z-buffer

    for (int i = 0; i < model.nfaces(); i++)
    {
        // iterate through all triangles
        //        auto [ax, ay] = project(model.vert(i, 0));
        //        auto [bx, by] = project(model.vert(i, 1));
        //        auto [cx, cy] = project(model.vert(i, 2));
        int ax, ay, bx, by, cx, cy;
        int az, bz, cz;
        std::tie(ax, ay, az) = project(persp(rot(model.vert(i, 0))));
        std::tie(bx, by, bz) = project(persp(rot(model.vert(i, 1))));
        std::tie(cx, cy, cz) = project(persp(rot(model.vert(i, 2))));

        TGAColor rnd;
        for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255; // random color
        // draw the triangle
        triangle(ax, ay, az, bx, by, bz, cx, cy, cz, zbuffer, framebuffer, rnd);
    }

    framebuffer.write_tga_file("framebuffer.tga");
    zbuffer.write_tga_file("zbuffer.tga");
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