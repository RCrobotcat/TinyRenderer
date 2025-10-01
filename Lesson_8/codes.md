- `main.cpp`
```c++
#include <algorithm>
#include <cmath>
#include <vector>
#include "geometry.h"
#include "gl_mine.h"
#include "model.h"
#include "tgaimage.h"

extern mat<4, 4> ModelView, Perspective;
extern std::vector<double> zbuffer;

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

int main() {
    Model model("../Obj/african_head.obj");
    constexpr int width = 800; // output image size
    constexpr int height = 800;
    constexpr vec3 light_dir{1, 1, 1}; // light direction
    constexpr vec3 eye{-1, 0, 2}; // camera position
    constexpr vec3 center{0, 0, 0}; // camera direction
    constexpr vec3 up{0, 1, 0}; // camera up vector

    lookat(eye, center, up); // build the ModelView   matrix
    init_perspective(norm(eye - center)); // build the Perspective matrix
    init_viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8); // build the Viewport matrix
    init_zbuffer(width, height); // build the z-buffer
    TGAImage framebuffer(width, height, TGAImage::RGB);

    BlinnPhongShader shader(light_dir, eye, model);
    for (int f = 0; f < model.nfaces(); f++) {
        Triangle clip = {
                shader.vertex(f, 0), // assemble the primitive
                shader.vertex(f, 1),
                shader.vertex(f, 2)
        };
        rasterize(clip, shader, framebuffer); // rasterize the primitive
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

```
---
- `Model.h`
```c++
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

```
---
- `Model.cpp`
```c++
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
    load_texture("_nm_tangent.tga", normalmap);
    load_texture("_diffuse.tga", diffusemap);
    load_texture("_spec.tga", specularmap);
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

const TGAImage &Model::diffuse() const { return diffusemap; }

const TGAImage &Model::specular() const { return specularmap; }

```