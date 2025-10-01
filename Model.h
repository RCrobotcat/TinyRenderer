//
// Created by 25190 on 2025/9/14.
//

#ifndef MODEL_H
#define MODEL_H

#include <vector>
#include "geometry.h"
#include "tgaimage.h"

class Model {
    std::vector<vec4> verts = {}; // array of vertices
    std::vector<vec4> norms = {};    // array of normal vectors
    std::vector<int> facet_vrt = {}; // per-triangle index of vertex
    std::vector<int> facet_nrm = {}; // per-triangle index of normal vector

    std::vector<vec2> tex = {};      // array of tex coords(uv)
    std::vector<int> facet_tex = {}; // per-triangle index of tex coords
    TGAImage normalmap = {}; // normal map texture
    TGAImage diffusemap = {}; // diffuse map texture
    TGAImage specularmap = {}; // specular map texture
public:
    Model(const std::string filename); // ����.obj�ļ�·������ģ��

    int nverts() const; // number of vertices
    int nfaces() const; // number of triangles

    vec4 vert(const int i) const; // 0 <= i < nverts() => ���ص�i������

    // 0 <= iface <= nfaces(), 0 <= nthvert < 3 => ���ص�iface�������εĵ�nthvert������
    vec4 vert(const int iface, const int nthvert) const;

    // normal coming from the "vn x y z" entries in the .obj file => ����iface�����εĵ�nthvert������ķ�����
    vec4 normal(const int iface, const int nthvert) const;

    // normal coming from the normal map texture => ����iface�����εĵ�nthvert������ķ����������������Է�����ͼ
    vec4 normal(const vec2 &uv) const;

    const TGAImage &diffuse() const; // ����������ͼ��uv������ɫ
    const TGAImage &specular() const; // ���ظ߹���ͼ��uv������ɫ��r����

    vec2 uv(const int iface, const int nthvert) const; // ���ص�iface�������εĵ�nthvert�������uv����
};


#endif //MODEL_H
