- `main.cpp`:
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

// 把三维模型的顶点坐标转换为屏幕上的像素点位置
std::tuple<int, int> project(vec3 v)
{
    // First of all, (x,y) is an orthogonal projection of the vector (x,y,z).
    return {
        (v.x + 1.) * width / 2,
        // Second, since the input models are scaled to have fit in the [-1,1]^3 world coordinates,
        (v.y + 1.) * height / 2
    }; // we want to shift the vector (x,y) and then scale it to span the entire screen.
}

int main(int argc, char **argv)
{
    // if (argc != 2)
    // {
    //     std::cerr << "Usage: " << argv[0] << " Obj/model.obj" << std::endl;
    //     return 1;
    // }

    // Model model(argv[1]);
    // TGAImage framebuffer(width, height, TGAImage::RGB);

    Model model("../Obj/diablo3_pose.obj");
    TGAImage framebuffer(width, height, TGAImage::RGB);

    // 绘制模型的线框
    for (int i = 0; i < model.nfaces(); i++)
    {
        // iterate through all triangles
        // auto [ax, ay] = project(model.vert(i, 0));
        // auto [bx, by] = project(model.vert(i, 1));
        // auto [cx, cy] = project(model.vert(i, 2));
        int ax, ay, bx, by, cx, cy;
        std::tie(ax, ay) = project(model.vert(i, 0));
        std::tie(bx, by) = project(model.vert(i, 1));
        std::tie(cx, cy) = project(model.vert(i, 2));

        line(ax, ay, bx, by, framebuffer, red);
        line(bx, by, cx, cy, framebuffer, red);
        line(cx, cy, ax, ay, framebuffer, red);
    }

    // 绘制模型的顶点
    for (int i = 0; i < model.nverts(); i++)
    {
        // iterate through all vertices
        vec3 v = model.vert(i); // get i-th vertex
        // auto [x, y] = project(v); // project it to the screen

        int x, y;
        std::tie(x, y) = project(v); // 投影到屏幕上
        framebuffer.set(x, y, white);
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

```
---
- `geometry.h`:
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

// 模板类，2d向量，用法是Vec2<int>(2d整形向量)、Vec2<float>(2d浮点数向量)
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
std::ostream &operator<<(std::ostream &out, const vec<n> &v)
{
    for (int i = 0; i < n; i++) out << v[i] << " ";
    return out;
}

// 模板类，3d向量，用法是Vec3<int>(3d整形向量)、Vec3<float>(3d浮点数向量)
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

typedef vec<3> vec3;


#endif //GEOMETRY_H

```
---
- `model.h`:
```cpp
//
// Created by 25190 on 2025/9/14.
//

#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include "geometry.h"

class Model
{
    std::vector<vec3> verts = {}; // array of vertices
    std::vector<int> facet_vrt = {}; // per-triangle index in the above array
public:
    Model(const std::string filename); // 根据.obj文件路径导入模型

    int nverts() const; // number of vertices
    int nfaces() const; // number of triangles
    vec3 vert(const int i) const; // 0 <= i < nverts() => 返回第i个顶点
    vec3 vert(const int iface, const int nthvert) const; // 0 <= iface <= nfaces(), 0 <= nthvert < 3 => 返回第iface个三角形的第nthvert个顶点
};


#endif //MODEL_H

```
- `Model.cpp`:
```cpp
//
// Created by 25190 on 2025/9/14.
//

#include "Model.h"
#include <fstream>
#include <sstream>

// 构造函数，输入参数是.obj文件路径
Model::Model(const std::string filename)
{
    std::ifstream in;
    in.open(filename, std::ifstream::in); // 打开.obj文件
    if (in.fail()) return;
    std::string line;
    while (!in.eof()) // 没有到文件末尾的话
    {
        std::getline(in, line); // 读入一行
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) // 如果这一行的前两个字符是“v ”的话，代表是顶点数据
        {
            iss >> trash; // 省略掉"v "
            vec3 v; // 读入顶点坐标
            for (int i: {0, 1, 2}) iss >> v[i];
            verts.push_back(v); // 加入顶点集
        } else if (!line.compare(0, 2, "f ")) // 如果这一行的前两个字符是“f ”的话，代表是面片数据
        {
            int f, t, n, cnt = 0;
            iss >> trash; // 省略掉"f "
            // 读取面片的顶点索引、纹理索引、法线索引
            while (iss >> f >> trash >> t >> trash >> n) // 读取x/x/x格式
            {
                facet_vrt.push_back(--f); // 只保存顶点索引，obj索引从1开始，需减1
                cnt++;
            }
            if (3 != cnt) // 检查是否为三角面
            {
                std::cerr << "Error: the obj file is supposed to be triangulated" << std::endl;
                return;
            }
        }
    }
    // 输出顶点数和面数
    std::cerr << "# v# " << nverts() << " f# " << nfaces() << std::endl;
}

int Model::nverts() const { return verts.size(); }
int Model::nfaces() const { return facet_vrt.size() / 3; }

vec3 Model::vert(const int i) const
{
    return verts[i];
}

vec3 Model::vert(const int iface, const int nthvert) const
{
    return verts[facet_vrt[iface * 3 + nthvert]];
}

```
---
- `tgaimage.h`:
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

TGAColor() = default;
constexpr TGAColor(std::uint8_t R, std::uint8_t G, std::uint8_t B, std::uint8_t A, std::uint8_t bpp = 4)
: bgra{R, G, B, A}, bytespp(bpp) {}
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
- `tgaimage.cpp`:
```cpp
//
// Created by 25190 on 2025/9/14.
//

#include "tgaimage.h"
#include <iostream>
#include <cstring>

TGAImage::TGAImage(const int w, const int h, const int bpp) : w(w), h(h), bpp(bpp), data(w * h * bpp, 0)
{
}

bool TGAImage::read_tga_file(const std::string filename)
{
    std::ifstream in;
    in.open(filename, std::ios::binary);
    if (!in.is_open())
    {
        std::cerr << "can't open file " << filename << "\n";
        return false;
    }
    TGAHeader header;
    in.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (!in.good())
    {
        std::cerr << "an error occured while reading the header\n";
        return false;
    }
    w = header.width;
    h = header.height;
    bpp = header.bitsperpixel >> 3;
    if (w <= 0 || h <= 0 || (bpp != GRAYSCALE && bpp != RGB && bpp != RGBA))
    {
        std::cerr << "bad bpp (or width/height) value\n";
        return false;
    }
    size_t nbytes = bpp * w * h;
    data = std::vector<std::uint8_t>(nbytes, 0);
    if (3 == header.datatypecode || 2 == header.datatypecode)
    {
        in.read(reinterpret_cast<char *>(data.data()), nbytes);
        if (!in.good())
        {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
    } else if (10 == header.datatypecode || 11 == header.datatypecode)
    {
        if (!load_rle_data(in))
        {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
    } else
    {
        std::cerr << "unknown file format " << (int) header.datatypecode << "\n";
        return false;
    }
    if (!(header.imagedescriptor & 0x20))
        flip_vertically();
    if (header.imagedescriptor & 0x10)
        flip_horizontally();
    std::cerr << w << "x" << h << "/" << bpp * 8 << "\n";
    return true;
}

bool TGAImage::load_rle_data(std::ifstream &in)
{
    size_t pixelcount = w * h;
    size_t currentpixel = 0;
    size_t currentbyte = 0;
    TGAColor colorbuffer;
    do
    {
        std::uint8_t chunkheader = 0;
        chunkheader = in.get();
        if (!in.good())
        {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
        if (chunkheader < 128)
        {
            chunkheader++;
            for (int i = 0; i < chunkheader; i++)
            {
                in.read(reinterpret_cast<char *>(colorbuffer.bgra), bpp);
                if (!in.good())
                {
                    std::cerr << "an error occured while reading the header\n";
                    return false;
                }
                for (int t = 0; t < bpp; t++)
                    data[currentbyte++] = colorbuffer.bgra[t];
                currentpixel++;
                if (currentpixel > pixelcount)
                {
                    std::cerr << "Too many pixels read\n";
                    return false;
                }
            }
        } else
        {
            chunkheader -= 127;
            in.read(reinterpret_cast<char *>(colorbuffer.bgra), bpp);
            if (!in.good())
            {
                std::cerr << "an error occured while reading the header\n";
                return false;
            }
            for (int i = 0; i < chunkheader; i++)
            {
                for (int t = 0; t < bpp; t++)
                    data[currentbyte++] = colorbuffer.bgra[t];
                currentpixel++;
                if (currentpixel > pixelcount)
                {
                    std::cerr << "Too many pixels read\n";
                    return false;
                }
            }
        }
    } while (currentpixel < pixelcount);
    return true;
}

bool TGAImage::write_tga_file(const std::string filename, const bool vflip, const bool rle) const
{
    constexpr std::uint8_t developer_area_ref[4] = {0, 0, 0, 0};
    constexpr std::uint8_t extension_area_ref[4] = {0, 0, 0, 0};
    constexpr std::uint8_t footer[18] = {
        'T', 'R', 'U', 'E', 'V', 'I', 'S', 'I', 'O', 'N', '-', 'X', 'F', 'I', 'L', 'E', '.', '\0'
    };
    std::ofstream out;
    out.open(filename, std::ios::binary);
    if (!out.is_open())
    {
        std::cerr << "can't open file " << filename << "\n";
        return false;
    }
    TGAHeader header = {};
    header.bitsperpixel = bpp << 3;
    header.width = w;
    header.height = h;
    header.datatypecode = (bpp == GRAYSCALE ? (rle ? 11 : 3) : (rle ? 10 : 2));
    header.imagedescriptor = vflip ? 0x00 : 0x20; // top-left or bottom-left origin
    out.write(reinterpret_cast<const char *>(&header), sizeof(header));
    if (!out.good()) goto err;
    if (!rle)
    {
        out.write(reinterpret_cast<const char *>(data.data()), w * h * bpp);
        if (!out.good()) goto err;
    } else if (!unload_rle_data(out)) goto err;
    out.write(reinterpret_cast<const char *>(developer_area_ref), sizeof(developer_area_ref));
    if (!out.good()) goto err;
    out.write(reinterpret_cast<const char *>(extension_area_ref), sizeof(extension_area_ref));
    if (!out.good()) goto err;
    out.write(reinterpret_cast<const char *>(footer), sizeof(footer));
    if (!out.good()) goto err;
    return true;
err:
    std::cerr << "can't dump the tga file\n";
    return false;
}

bool TGAImage::unload_rle_data(std::ofstream &out) const
{
    const std::uint8_t max_chunk_length = 128;
    size_t npixels = w * h;
    size_t curpix = 0;
    while (curpix < npixels)
    {
        size_t chunkstart = curpix * bpp;
        size_t curbyte = curpix * bpp;
        std::uint8_t run_length = 1;
        bool raw = true;
        while (curpix + run_length < npixels && run_length < max_chunk_length)
        {
            bool succ_eq = true;
            for (int t = 0; succ_eq && t < bpp; t++)
                succ_eq = (data[curbyte + t] == data[curbyte + t + bpp]);
            curbyte += bpp;
            if (1 == run_length)
                raw = !succ_eq;
            if (raw && succ_eq)
            {
                run_length--;
                break;
            }
            if (!raw && !succ_eq)
                break;
            run_length++;
        }
        curpix += run_length;
        out.put(raw ? run_length - 1 : run_length + 127);
        if (!out.good()) return false;
        out.write(reinterpret_cast<const char *>(data.data() + chunkstart), (raw ? run_length * bpp : bpp));
        if (!out.good()) return false;
    }
    return true;
}

TGAColor TGAImage::get(const int x, const int y) const
{
    if (!data.size() || x < 0 || y < 0 || x >= w || y >= h) return {};
    TGAColor ret = {0, 0, 0, 0, bpp};
    const std::uint8_t *p = data.data() + (x + y * w) * bpp;
    for (int i = bpp; i--; ret.bgra[i] = p[i]);
    return ret;
}

void TGAImage::set(int x, int y, const TGAColor &c)
{
    if (!data.size() || x < 0 || y < 0 || x >= w || y >= h) return;
    memcpy(data.data() + (x + y * w) * bpp, c.bgra, bpp);
}

void TGAImage::flip_horizontally()
{
    for (int i = 0; i < w / 2; i++)
        for (int j = 0; j < h; j++)
            for (int b = 0; b < bpp; b++)
                std::swap(data[(i + j * w) * bpp + b], data[(w - 1 - i + j * w) * bpp + b]);
}

void TGAImage::flip_vertically()
{
    for (int i = 0; i < w; i++)
        for (int j = 0; j < h / 2; j++)
            for (int b = 0; b < bpp; b++)
                std::swap(data[(i + j * w) * bpp + b], data[(i + (h - 1 - j) * w) * bpp + b]);
}

int TGAImage::width() const
{
    return w;
}

int TGAImage::height() const
{
    return h;
}

```