//
// Created by 25190 on 2025/9/14.
//

#ifndef TGAIMAGE_H
#define TGAIMAGE_H

#pragma once
#include <cstdint>
#include <fstream>
#include <vector>

#pragma pack(push,1) // ��Ĭ�ϵı������뷽ʽ�����ջ�������µĶ���ϵ��Ϊ1
// TGAͷ�ļ��ṹ
struct TGAHeader
{
    std::uint8_t idlength = 0; // ���ֶ���1�ֽ��޷������ͣ�ָ��ͼ����Ϣ�ֶγ��ȣ���ȡֵ��Χ��0��255������Ϊ0ʱ��ʾû��ͼ�����Ϣ�ֶΡ�
    std::uint8_t colormaptype = 0; // ��ɫ�����ͣ�1�ֽڣ�:0��ʾû����ɫ��1��ʾ��ɫ����ڡ����ڱ���ʽ������ɫ��ģ���˴���ͨ�������ԡ�
    std::uint8_t datatypecode = 0; // ͼ��������:2:��ѹ��RGB��ʽ 10:ѹ��RGB��ʽ
    std::uint16_t colormaporigin = 0; // ��ɫ����ַ:��ɫ���׵�������������ͣ���λ-��λ��
    std::uint16_t colormaplength = 0; // ��ɫ����:��ɫ��ı������������ͣ���λ-��λ��
    std::uint8_t colormapdepth = 0; // ��ɫ����λ��:λ����bit����16���� 16λ TGA��24���� 24λ TGA��32���� 32λ TGA
    std::uint16_t x_origin = 0; // ͼ��X�������ʼλ��:ͼ�����½�X��������ͣ���λ-��λ��ֵ
    std::uint16_t y_origin = 0; // ͼ��Y�������ʼλ��:ͼ�����½�Y��������ͣ���λ-��λ��ֵ
    std::uint16_t width = 0; // ͼ����:������Ϊ��λ��ͼ���ȵ����ͣ���λ-��λ��
    std::uint16_t height = 0; // ͼ��߶�:������Ϊ��λ��ͼ��߶ȵ����ͣ���λ-��λ��
    std::uint8_t bitsperpixel = 0; // ͼ��ÿ���ش洢ռ��λ��(bpp):����ֵΪ16��24�� 32�ȵȡ������˸�ͼ���� TGA 16��TGA24,TGA 32�ȵȡ�
    std::uint8_t imagedescriptor = 0; // ͼ���������ֽ�
};
#pragma pack(pop) // �ָ�Ĭ�ϵı������뷽ʽ
// �����ṹ��Ĵ�С��18�ֽ�

struct TGAColor
{
    std::uint8_t bgra[4] = {0, 0, 0, 0};
    std::uint8_t bytespp = 4;
    std::uint8_t &operator[](const int i) { return bgra[i]; }
};

struct TGAImage
{
    enum Format { GRAYSCALE = 1, RGB = 3, RGBA = 4 }; // ͼ���ʽ���Ҷ�ͼ��RGBͼ��RGBAͼ

    TGAImage() = default;

    TGAImage(const int w, const int h, const int bpp);

    bool read_tga_file(const std::string filename);

    bool write_tga_file(const std::string filename, const bool vflip = true, const bool rle = true) const;

    void flip_horizontally();

    void flip_vertically();

    TGAColor get(const int x, const int y) const; // ��ȡ��x,y��λ�ô����ص���ɫֵ

    void set(const int x, const int y, const TGAColor &c); // ����x,y��λ�ô�����������Ϊ��ɫc

    int width() const; // ��ȡͼ����

    int height() const; // ��ȡͼ��߶�

private:
    bool load_rle_data(std::ifstream &in);

    bool unload_rle_data(std::ofstream &out) const;

    int w = 0, h = 0;
    std::uint8_t bpp = 0;
    std::vector<std::uint8_t> data = {};
};

#endif //TGAIMAGE_H
