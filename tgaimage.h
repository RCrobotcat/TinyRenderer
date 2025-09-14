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
