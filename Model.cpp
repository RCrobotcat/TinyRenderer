//
// Created by 25190 on 2025/9/14.
//

#include "Model.h"
#include <fstream>
#include <sstream>

// 构造函数，输入参数是.obj文件路径
Model::Model(const std::string filename) {
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
            vec4 v = {0, 0, 0, 1}; // 读入顶点坐标
            for (int i: {0, 1, 2}) iss >> v[i];
            verts.push_back(v); // 加入顶点集
        } else if (!line.compare(0, 3, "vt ")) {
            iss >> trash >> trash;
            vec2 uv;
            for (int i: {0, 1}) iss >> uv[i];
            tex.push_back({uv.x, 1 - uv.y});
        } else if (!line.compare(0, 3, "vn ")) {
            iss >> trash >> trash;
            vec4 n = {0, 0, 0, 1};
            for (int i: {0, 1, 2}) iss >> n[i];
            norms.push_back(normalized(n));
        } else if (!line.compare(0, 2, "f ")) // 如果这一行的前两个字符是“f ”的话，代表是面片数据
        {
            int f, t, n, cnt = 0;
            iss >> trash; // 省略掉"f "
            // 读取面片的顶点索引、纹理索引、法线索引
            while (iss >> f >> trash >> t >> trash >> n) // 读取x/x/x格式
            {
                facet_vrt.push_back(--f); // 只保存顶点索引，obj索引从1开始，需减1
                facet_tex.push_back(--t); // 只保存纹理索引，obj索引从1开始，需减1
                facet_nrm.push_back(--n); // 只保存法线索引，obj索引从1开始，需减1
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

    auto load_texture = [&filename](const std::string suffix, TGAImage &img) {
        size_t dot = filename.find_last_of(".");
        if (dot == std::string::npos) return;
        std::string texfile = filename.substr(0, dot) + suffix;
        std::cerr << "texture file " << texfile << " loading " << (img.read_tga_file(texfile.c_str()) ? "ok" : "failed")
                  << std::endl;
    };
    load_texture("_nm.tga", normalmap);
}

int Model::nverts() const { return verts.size(); }

int Model::nfaces() const { return facet_vrt.size() / 3; }

vec4 Model::vert(const int i) const {
    return verts[i];
}

vec4 Model::vert(const int iface, const int nthvert) const {
    return verts[facet_vrt[iface * 3 + nthvert]];
}

vec4 Model::normal(const int iface, const int nthvert) const {
    return norms[facet_nrm[iface * 3 + nthvert]];
}

vec4 Model::normal(const vec2 &uv) const {
    TGAColor c = normalmap.get(uv[0] * normalmap.width(), uv[1] * normalmap.height());
    return vec4{(double) c[2], (double) c[1], (double) c[0], 0} * 2. / 255. - vec4{1, 1, 1, 0};
}

vec2 Model::uv(const int iface, const int nthvert) const {
    return tex[facet_tex[iface * 3 + nthvert]];
}
