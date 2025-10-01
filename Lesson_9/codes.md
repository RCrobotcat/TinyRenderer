- `main.cpp`
```c++
#include <algorithm>
#include <cmath>
#include <vector>
#include "geometry.h"
#include "gl_mine.h"
#include "model.h"
#include "tgaimage.h"

extern mat<4, 4> ModelView, Perspective, Viewport;
extern std::vector<double> zbuffer;

struct BlankShader : IShader {
    const Model &model;

    BlankShader(const Model &m) : model(m) {}

    virtual vec4 vertex(const int face, const int vert) {
        vec4 gl_Position = ModelView * model.vert(face, vert);
        return Perspective * gl_Position;
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bar) const {
        return {false, {255, 255, 255, 255}};
    }
};

struct BlinnPhongShader : IShader {
    const Model &model;
    vec4 l; // light direction in eye coordinates
    vec4 eye; // eye direction in eye coordinates
    vec2 varying_uv[3];  // triangle uv coordinates, written by the vertex shader, read by the fragment shader
    vec4 varying_nrm[3]; // normal per vertex to be interpolated by the fragment shader
    vec4 tri[3];         // triangle in view coordinates

    BlinnPhongShader(const vec3 light, const vec3 _eye, const Model &m) : model(m) {
        l = normalized((ModelView * vec4{light.x, light.y, light.z, 0.}));
        eye = normalized((ModelView * vec4{_eye.x, _eye.y, _eye.z, 0}));
    }

    virtual vec4 vertex(const int face, const int vert) {
        vec4 v = model.vert(face, vert); // current vertex in object coordinates
        vec4 gl_Position = ModelView * v; // transform it to screen coordinates

        tri[vert] = gl_Position;
        varying_uv[vert] = model.uv(face, vert); // uv coordinates
        varying_nrm[vert] = ModelView.invert_transpose() * model.normal(face, vert);

        return Perspective * gl_Position; // in clip coordinates
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bar) const {
        // TBN matrix
        mat<2, 4> E = {tri[1] - tri[0], tri[2] - tri[0]};
        mat<2, 2> U = {varying_uv[1] - varying_uv[0], varying_uv[2] - varying_uv[0]};
        mat<2, 4> T = U.invert() * E;
        mat<4, 4> D = {normalized(T[0]),  // tangent vector
                       normalized(T[1]),  // bitangent vector
                       normalized(varying_nrm[0] * bar[0] + varying_nrm[1] * bar[1] +
                                  varying_nrm[2] * bar[2]), // interpolated normal
                       {0, 0, 0, 1}}; // Darboux frame

        vec2 uv = varying_uv[0] * bar.x + varying_uv[1] * bar.y + varying_uv[2] * bar.z; // interpolate uv coordinates
        vec4 n = normalized(D.transpose() * model.normal(uv));
        vec4 h = normalized(l + eye); // half vector
        double ambient = .5; // ambient light intensity
        double diff = std::max(0., n * l); // diffuse light intensity
        double spec = std::pow(std::max(n * h, 0.), 70);
        // specular intensity, note that the camera lies on the z-axis (in eye coordinates), therefore simple r.z, since (0,0,1)*(r.x, r.y, r.z) = r.z
        spec *= (3. * sample2D(model.specular(), uv)[0] / 255.);
        TGAColor gl_FragColor = sample2D(model.diffuse(), uv);
        for (int channel: {0, 1, 2}) {
            gl_FragColor[channel] = std::min<int>(255, gl_FragColor[channel] * (ambient + diff + spec));
        }
        return {false, gl_FragColor}; // do not discard the pixel
    }
};

// 获取并保存z-buffer
void drop_zbuffer(std::string filename, std::vector<double> &zbuffer, int width, int height) {
    TGAImage zimg(width, height, TGAImage::GRAYSCALE, {0, 0, 0, 0});
    double minz = +1000;
    double maxz = -1000;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer[x + y * width];
            if (z < -100) continue;
            minz = std::min(z, minz);
            maxz = std::max(z, maxz);
        }
    }
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer[x + y * width];
            if (z < -100) continue;
            z = (z - minz) / (maxz - minz) * 255;
            zimg.set(x, y, {(uint8_t) z, 255, 255, 255});
        }
    }
    zimg.write_tga_file(filename);
}

int main() {
    constexpr int width = 800;    // 输出图像大小
    constexpr int height = 800;
    constexpr int shadoww = 8000;   // shadow map 分辨率
    constexpr int shadowh = 8000;
    constexpr vec3 light_dir{1, 1, 1};  // 光源方向/位置
    constexpr vec3 eye{-1, 0, 2};       // 相机位置
    constexpr vec3 center{0, 0, 0};     // 相机目标
    constexpr vec3 up{0, 1, 0};         // 相机上方向

    // ----------- 普通渲染 pass -----------
    lookat(eye, center, up);
    init_perspective(norm(eye - center));
    init_viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8);
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB, {177, 195, 209, 255});

    // 固定模型路径
    Model model_head("../Obj/diablo3_pose.obj");
    Model model_floor("../Obj/floor.obj");

    {   // head
        BlinnPhongShader shader(light_dir, eye, model_head);
        for (int f = 0; f < model_head.nfaces(); f++) {
            Triangle clip = {
                    shader.vertex(f, 0),
                    shader.vertex(f, 1),
                    shader.vertex(f, 2)
            };
            rasterize(clip, shader, framebuffer);
        }
    }
    {   // floor
        BlinnPhongShader shader(light_dir, eye, model_floor);
        for (int f = 0; f < model_floor.nfaces(); f++) {
            Triangle clip = {
                    shader.vertex(f, 0),
                    shader.vertex(f, 1),
                    shader.vertex(f, 2)
            };
            rasterize(clip, shader, framebuffer);
        }
    }
    framebuffer.write_tga_file("framebuffer.tga");
    drop_zbuffer("zbuffer1.tga", zbuffer, width, height);

    // ----------- Shadow map 渲染 pass -----------
    std::vector<bool> mask(width * height, false);
    std::vector<double> zbuffer_copy = zbuffer;
    mat<4, 4> M = (Viewport * Perspective * ModelView).invert();

    {
        lookat(light_dir, center, up);
        init_perspective(norm(light_dir - center));
        init_viewport(shadoww / 16, shadowh / 16, shadoww * 7 / 8, shadowh * 7 / 8);
        init_zbuffer(shadoww, shadowh);
        TGAImage trash(shadoww, shadowh, TGAImage::RGB, {177, 195, 209, 255});

        {   // head
            BlankShader shader{model_head};
            for (int f = 0; f < model_head.nfaces(); f++) {
                Triangle clip = {
                        shader.vertex(f, 0),
                        shader.vertex(f, 1),
                        shader.vertex(f, 2)
                };
                rasterize(clip, shader, trash);
            }
        }
        {   // floor
            BlankShader shader{model_floor};
            for (int f = 0; f < model_floor.nfaces(); f++) {
                Triangle clip = {
                        shader.vertex(f, 0),
                        shader.vertex(f, 1),
                        shader.vertex(f, 2)
                };
                rasterize(clip, shader, trash);
            }
        }

        trash.write_tga_file("shadowmap.tga");
    }
    drop_zbuffer("zbuffer2.tga", zbuffer, shadoww, shadowh);

    // ----------- 后处理 pass -----------
    mat<4, 4> N = Viewport * Perspective * ModelView;
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            vec4 fragment = M * vec4{static_cast<double>(x), static_cast<double>(y),
                                     zbuffer_copy[x + y * width], 1.};
            vec4 q = N * fragment;
            vec3 p = q.xyz() / q.w;
            bool lit = (fragment.z < -100 ||
                        (p.x < 0 || p.x >= shadoww || p.y < 0 || p.y >= shadowh) ||
                        (p.z > zbuffer[int(p.x) + int(p.y) * shadoww] - .03));
            mask[x + y * width] = lit;
        }
    }

    TGAImage maskimg(width, height, TGAImage::GRAYSCALE);
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (mask[x + y * width]) continue;
            maskimg.set(x, y, {255, 255, 255, 255});
        }
    }
    maskimg.write_tga_file("mask.tga");

    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (mask[x + y * width]) continue;
            TGAColor c = framebuffer.get(x, y);
            vec3 a = {static_cast<double>(c[0]), static_cast<double>(c[1]),
                      static_cast<double>(c[2])};
            if (norm(a) < 80) continue;
            a = normalized(a) * 80;
            framebuffer.set(x, y, {(uint8_t) a[0], (uint8_t) a[1], (uint8_t) a[2], 255});
        }
    }
    framebuffer.write_tga_file("shadow.tga");

    return 0;
}


```
---
- `tgaimage.h`
```c++
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

    TGAImage(const int w, const int h, const int bpp, TGAColor c = {});

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
---
- `tgaimage.cpp`
```c++
//
// Created by 25190 on 2025/9/14.
//

#include "tgaimage.h"
#include <iostream>
#include <cstring>

TGAImage::TGAImage(const int w, const int h, const int bpp, TGAColor c) : w(w), h(h), bpp(bpp),
                                                                          data(w * h * bpp, 0) {
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            set(i, j, c);
}

bool TGAImage::read_tga_file(const std::string filename) {
    std::ifstream in;
    in.open(filename, std::ios::binary);
    if (!in.is_open()) {
        std::cerr << "can't open file " << filename << "\n";
        return false;
    }
    TGAHeader header;
    in.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (!in.good()) {
        std::cerr << "an error occured while reading the header\n";
        return false;
    }
    w = header.width;
    h = header.height;
    bpp = header.bitsperpixel >> 3;
    if (w <= 0 || h <= 0 || (bpp != GRAYSCALE && bpp != RGB && bpp != RGBA)) {
        std::cerr << "bad bpp (or width/height) value\n";
        return false;
    }
    size_t nbytes = bpp * w * h;
    data = std::vector<std::uint8_t>(nbytes, 0);
    if (3 == header.datatypecode || 2 == header.datatypecode) {
        in.read(reinterpret_cast<char *>(data.data()), nbytes);
        if (!in.good()) {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
    } else if (10 == header.datatypecode || 11 == header.datatypecode) {
        if (!load_rle_data(in)) {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
    } else {
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

bool TGAImage::load_rle_data(std::ifstream &in) {
    size_t pixelcount = w * h;
    size_t currentpixel = 0;
    size_t currentbyte = 0;
    TGAColor colorbuffer;
    do {
        std::uint8_t chunkheader = 0;
        chunkheader = in.get();
        if (!in.good()) {
            std::cerr << "an error occured while reading the data\n";
            return false;
        }
        if (chunkheader < 128) {
            chunkheader++;
            for (int i = 0; i < chunkheader; i++) {
                in.read(reinterpret_cast<char *>(colorbuffer.bgra), bpp);
                if (!in.good()) {
                    std::cerr << "an error occured while reading the header\n";
                    return false;
                }
                for (int t = 0; t < bpp; t++)
                    data[currentbyte++] = colorbuffer.bgra[t];
                currentpixel++;
                if (currentpixel > pixelcount) {
                    std::cerr << "Too many pixels read\n";
                    return false;
                }
            }
        } else {
            chunkheader -= 127;
            in.read(reinterpret_cast<char *>(colorbuffer.bgra), bpp);
            if (!in.good()) {
                std::cerr << "an error occured while reading the header\n";
                return false;
            }
            for (int i = 0; i < chunkheader; i++) {
                for (int t = 0; t < bpp; t++)
                    data[currentbyte++] = colorbuffer.bgra[t];
                currentpixel++;
                if (currentpixel > pixelcount) {
                    std::cerr << "Too many pixels read\n";
                    return false;
                }
            }
        }
    } while (currentpixel < pixelcount);
    return true;
}

bool TGAImage::write_tga_file(const std::string filename, const bool vflip, const bool rle) const {
    constexpr std::uint8_t developer_area_ref[4] = {0, 0, 0, 0};
    constexpr std::uint8_t extension_area_ref[4] = {0, 0, 0, 0};
    constexpr std::uint8_t footer[18] = {
            'T', 'R', 'U', 'E', 'V', 'I', 'S', 'I', 'O', 'N', '-', 'X', 'F', 'I', 'L', 'E', '.', '\0'
    };
    std::ofstream out;
    out.open(filename, std::ios::binary);
    if (!out.is_open()) {
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
    if (!rle) {
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

bool TGAImage::unload_rle_data(std::ofstream &out) const {
    const std::uint8_t max_chunk_length = 128;
    size_t npixels = w * h;
    size_t curpix = 0;
    while (curpix < npixels) {
        size_t chunkstart = curpix * bpp;
        size_t curbyte = curpix * bpp;
        std::uint8_t run_length = 1;
        bool raw = true;
        while (curpix + run_length < npixels && run_length < max_chunk_length) {
            bool succ_eq = true;
            for (int t = 0; succ_eq && t < bpp; t++)
                succ_eq = (data[curbyte + t] == data[curbyte + t + bpp]);
            curbyte += bpp;
            if (1 == run_length)
                raw = !succ_eq;
            if (raw && succ_eq) {
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

TGAColor TGAImage::get(const int x, const int y) const {
    if (!data.size() || x < 0 || y < 0 || x >= w || y >= h) return {};
    TGAColor ret = {0, 0, 0, 0, bpp};
    const std::uint8_t *p = data.data() + (x + y * w) * bpp;
    for (int i = bpp; i--; ret.bgra[i] = p[i]);
    return ret;
}

void TGAImage::set(int x, int y, const TGAColor &c) {
    if (!data.size() || x < 0 || y < 0 || x >= w || y >= h) return;
    memcpy(data.data() + (x + y * w) * bpp, c.bgra, bpp);
}

void TGAImage::flip_horizontally() {
    for (int i = 0; i < w / 2; i++)
        for (int j = 0; j < h; j++)
            for (int b = 0; b < bpp; b++)
                std::swap(data[(i + j * w) * bpp + b], data[(w - 1 - i + j * w) * bpp + b]);
}

void TGAImage::flip_vertically() {
    for (int i = 0; i < w; i++)
        for (int j = 0; j < h / 2; j++)
            for (int b = 0; b < bpp; b++)
                std::swap(data[(i + j * w) * bpp + b], data[(i + (h - 1 - j) * w) * bpp + b]);
}

int TGAImage::width() const {
    return w;
}

int TGAImage::height() const {
    return h;
}

```
