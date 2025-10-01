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
    Model(const std::string filename); // 根据.obj文件路径导入模型

    int nverts() const; // number of vertices
    int nfaces() const; // number of triangles

    vec4 vert(const int i) const; // 0 <= i < nverts() => 返回第i个顶点

    // 0 <= iface <= nfaces(), 0 <= nthvert < 3 => 返回第iface个三角形的第nthvert个顶点
    vec4 vert(const int iface, const int nthvert) const;

    // normal coming from the "vn x y z" entries in the .obj file => 返回iface三角形的第nthvert个顶点的法向量
    vec4 normal(const int iface, const int nthvert) const;

    // normal coming from the normal map texture => 返回iface三角形的第nthvert个顶点的法向量，法向量来自法线贴图
    vec4 normal(const vec2 &uv) const;

    const TGAImage &diffuse() const; // 返回纹理贴图在uv处的颜色
    const TGAImage &specular() const; // 返回高光贴图在uv处的颜色的r分量

    vec2 uv(const int iface, const int nthvert) const; // 返回第iface个三角形的第nthvert个顶点的uv坐标
};


#endif //MODEL_H
