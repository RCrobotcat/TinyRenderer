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
