- `main.cpp` (除了main.cpp, tgaimage.cpp有变化，其他文件和Lesson_1完全一样)
```cpp
#include <cmath>
#include <tuple>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"

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
        if (steep) // if transposed, de?transpose
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
            framebuffer.set(x, y, color);
        }
    }
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

int main()
{
    Model model("../Obj/african_head.obj");
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
        std::tie(ax, ay, az) = project(model.vert(i, 0));
        std::tie(bx, by, bz) = project(model.vert(i, 1));
        std::tie(cx, cy, cz) = project(model.vert(i, 2));

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

- `tgaimage.h`
```cpp
//
// Created by 25190 on 2025/9/14.
//

#ifndef TGAIMAGE_H
#define TGAIMAGE_H

#pragma once
#include <cstdint>
#include <fstream>
#include <vector>

#pragma pack(push,1) // 将默认的变量对齐方式推入堆栈，设置新的对齐系数为1
// TGA头文件结构
struct TGAHeader
{
    std::uint8_t idlength = 0; // 本字段是1字节无符号整型，指出图像信息字段长度，其取值范围是0到255，当它为0时表示没有图像的信息字段。
    std::uint8_t colormaptype = 0; // 颜色表类型（1字节）:0表示没有颜色表，1表示颜色表存在。由于本格式是无颜色表的，因此此项通常被忽略。
    std::uint8_t datatypecode = 0; // 图像类型码:2:非压缩RGB格式 10:压缩RGB格式
    std::uint16_t colormaporigin = 0; // 颜色表首址:颜色表首的入口索引，整型（低位-高位）
    std::uint16_t colormaplength = 0; // 颜色表长度:颜色表的表项总数，整型（低位-高位）
    std::uint8_t colormapdepth = 0; // 颜色表项位数:位数（bit），16代表 16位 TGA，24代表 24位 TGA，32代表 32位 TGA
    std::uint16_t x_origin = 0; // 图像X坐标的起始位置:图像左下角X坐标的整型（低位-高位）值
    std::uint16_t y_origin = 0; // 图像Y坐标的起始位置:图像左下角Y坐标的整型（低位-高位）值
    std::uint16_t width = 0; // 图像宽度:以像素为单位，图像宽度的整型（低位-高位）
    std::uint16_t height = 0; // 图像高度:以像素为单位，图像高度的整型（低位-高位）
    std::uint8_t bitsperpixel = 0; // 图像每像素存储占用位数(bpp):它的值为16，24或 32等等。决定了该图像是 TGA 16，TGA24,TGA 32等等。
    std::uint8_t imagedescriptor = 0; // 图像描述符字节
};
#pragma pack(pop) // 恢复默认的变量对齐方式
// 上述结构体的大小是18字节

struct TGAColor
{
    std::uint8_t bgra[4] = {0, 0, 0, 0};
    std::uint8_t bytespp = 4;
    std::uint8_t &operator[](const int i) { return bgra[i]; }
    const std::uint8_t &operator[](const int i) const { return bgra[i]; }
};

struct TGAImage
{
    enum Format { GRAYSCALE = 1, RGB = 3, RGBA = 4 }; // 图像格式：灰度图，RGB图，RGBA图

    TGAImage() = default;

    TGAImage(const int w, const int h, const int bpp);

    bool read_tga_file(const std::string filename);

    bool write_tga_file(const std::string filename, const bool vflip = true, const bool rle = true) const;

    void flip_horizontally();

    void flip_vertically();

    TGAColor get(const int x, const int y) const; // 获取（x,y）位置处像素的颜色值

    void set(const int x, const int y, const TGAColor &c); // 将（x,y）位置处的像素设置为颜色c

    int width() const; // 获取图像宽度

    int height() const; // 获取图像高度

private:
    bool load_rle_data(std::ifstream &in);

    bool unload_rle_data(std::ofstream &out) const;

    int w = 0, h = 0;
    std::uint8_t bpp = 0;
    std::vector<std::uint8_t> data = {};
};

#endif //TGAIMAGE_H

```