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

    BlinnPhongShader(const vec3 light, const vec3 _eye, const Model &m) : model(m) {
        l = normalized((ModelView * vec4{light.x, light.y, light.z, 0.}));
        eye = normalized((ModelView * vec4{_eye.x, _eye.y, _eye.z, 0}));
    }

    virtual vec4 vertex(const int face, const int vert) {
        vec4 v = model.vert(face, vert); // current vertex in object coordinates
        vec4 gl_Position = ModelView * v; // transform it to screen coordinates
        varying_uv[vert] = model.uv(face, vert); // uv coordinates

        return Perspective * gl_Position; // in clip coordinates
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bar) const {
        TGAColor gl_FragColor = {255, 255, 255, 255}; // output color of the fragment
        vec2 uv = varying_uv[0] * bar.x + varying_uv[1] * bar.y + varying_uv[2] * bar.z; // interpolate uv coordinates
        vec4 n = normalized((ModelView.invert_transpose() * model.normal(uv)));
        vec4 h = normalized(l + eye); // half vector
        vec4 d = model.diffuse(uv);
        vec4 s = model.specular(uv);
        double ambient = .5; // ambient light intensity
        double diff = std::max(0., n * l); // diffuse light intensity
        double spec = std::pow(std::max(n * h, 0.), 70);
        // specular intensity, note that the camera lies on the z-axis (in eye coordinates), therefore simple r.z, since (0,0,1)*(r.x, r.y, r.z) = r.z
        for (int channel: {0, 1, 2}) {
            gl_FragColor[channel] *= std::min(1., ambient + .4 * diff + .9 * spec);
            gl_FragColor[channel] *= std::min(1., (double) d[channel]);
            gl_FragColor[channel] += std::min(0., (double) s[channel]) * spec;
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
```c++
//
// Created by 25190 on 2025/9/14.
//

#include "Model.h"
#include <fstream>
#include <sstream>

// ���캯�������������.obj�ļ�·��
Model::Model(const std::string filename) {
    std::ifstream in;
    in.open(filename, std::ifstream::in); // ��.obj�ļ�
    if (in.fail()) return;
    std::string line;
    while (!in.eof()) // û�е��ļ�ĩβ�Ļ�
    {
        std::getline(in, line); // ����һ��
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) // �����һ�е�ǰ�����ַ��ǡ�v ���Ļ��������Ƕ�������
        {
            iss >> trash; // ʡ�Ե�"v "
            vec4 v = {0, 0, 0, 1}; // ���붥������
            for (int i: {0, 1, 2}) iss >> v[i];
            verts.push_back(v); // ���붥�㼯
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
        } else if (!line.compare(0, 2, "f ")) // �����һ�е�ǰ�����ַ��ǡ�f ���Ļ�����������Ƭ����
        {
            int f, t, n, cnt = 0;
            iss >> trash; // ʡ�Ե�"f "
            // ��ȡ��Ƭ�Ķ���������������������������
            while (iss >> f >> trash >> t >> trash >> n) // ��ȡx/x/x��ʽ
            {
                facet_vrt.push_back(--f); // ֻ���涥��������obj������1��ʼ�����1
                facet_tex.push_back(--t); // ֻ��������������obj������1��ʼ�����1
                facet_nrm.push_back(--n); // ֻ���淨��������obj������1��ʼ�����1
                cnt++;
            }
            if (3 != cnt) // ����Ƿ�Ϊ������
            {
                std::cerr << "Error: the obj file is supposed to be triangulated" << std::endl;
                return;
            }
        }
    }
    // ���������������
    std::cerr << "# v# " << nverts() << " f# " << nfaces() << std::endl;

    auto load_texture = [&filename](const std::string suffix, TGAImage &img) {
        size_t dot = filename.find_last_of(".");
        if (dot == std::string::npos) return;
        std::string texfile = filename.substr(0, dot) + suffix;
        std::cerr << "texture file " << texfile << " loading " << (img.read_tga_file(texfile.c_str()) ? "ok" : "failed")
                  << std::endl;
    };
    load_texture("_nm.tga", normalmap);
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

vec4 Model::diffuse(const vec2 &uv) const {
    TGAColor c = diffusemap.get(uv[0] * diffusemap.width(), uv[1] * diffusemap.height());
    return vec4{(double) c[0], (double) c[1], (double) c[2], 1} * 1.8 / 255.;
}

vec4 Model::specular(const vec2 &uv) const {
    TGAColor c = diffusemap.get(uv[0] * diffusemap.width(), uv[1] * diffusemap.height());
    return vec4{(double) c[0], (double) c[1], (double) c[2], 1} * 2 / 255.;
}

vec2 Model::uv(const int iface, const int nthvert) const {
    return tex[facet_tex[iface * 3 + nthvert]];
}

```
---
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

    vec4 diffuse(const vec2 &uv) const; // ����������ͼ��uv������ɫ
    vec4 specular(const vec2 &uv) const; // ���ظ߹���ͼ��uv������ɫ��r����

    vec2 uv(const int iface, const int nthvert) const; // ���ص�iface�������εĵ�nthvert�������uv����
};


#endif //MODEL_H

```