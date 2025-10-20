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

    BlinnPhongShader(const vec3 light, const vec3 _eye, const Model &m) : model(m) {
        l = normalized((ModelView * vec4{light.x, light.y, light.z, 0.}));
        eye = normalized((ModelView * vec4{_eye.x, _eye.y, _eye.z, 0}));
        // transform the light and eye vectors to view coordinates
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
        double ambient = .3; // ambient light intensity
        double diff = std::max(0., n * l); // diffuse light intensity
        double spec = std::pow(std::max(n * h, 0.), 70);
        // specular intensity, note that the camera lies on the z-axis (in eye coordinates), therefore simple r.z, since (0,0,1)*(r.x, r.y, r.z) = r.z
        for (int channel: {0, 1, 2})
            gl_FragColor[channel] *= std::min(1., ambient + .4 * diff + .9 * spec);
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

    // PhongShader shader(light_dir, model);
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

```
---
# 1. 法线贴图在代码中的应用流程

看你的代码，关键逻辑在这里：

### **(1) 顶点着色器部分**

```cpp
varying_uv[vert] = model.uv(face, vert); // 每个顶点的UV坐标
```

* 顶点着色器把 **UV 坐标** 传递给片元阶段（保存在 `varying_uv[]`）。
* 在光栅化时，这些 UV 会通过 **重心坐标 (bar)** 插值，得到片元的 UV。

---

### **(2) 片元着色器部分**

```cpp
vec2 uv = varying_uv[0] * bar.x + varying_uv[1] * bar.y + varying_uv[2] * bar.z; 
vec4 n = normalized((ModelView.invert_transpose() * model.normal(uv)));
```

* `uv = 插值结果`：得到当前像素对应的 UV 坐标。
* `model.normal(uv)`：从 **法线贴图** 中采样出一条法线（在切线空间 Tangent Space）。
* `ModelView.invert_transpose()`：把法线从 **模型空间/切线空间** 转换到 **眼睛空间 (Eye/View Space)**，用于光照计算。

---

### **(3) `model.normal(uv)` 内部**

```cpp
vec4 Model::normal(const vec2 &uv) const {
    TGAColor c = normalmap.get(uv[0] * normalmap.width(), uv[1] * normalmap.height());
    return vec4{(double)c[2], (double)c[1], (double)c[0], 0} * 2. / 255. - vec4{1, 1, 1, 0};
}
```

逐行解释：

1. **取法线贴图像素**

   ```cpp
   TGAColor c = normalmap.get(u * width, v * height);
   ```

    * 根据 UV 坐标采样法线贴图。
    * 得到 `(B,G,R)` 三个通道的整数值 `0~255`。

2. **调整颜色顺序**

   ```cpp
   (c[2], c[1], c[0])  // R,G,B
   ```

    * 因为 TGA 存储是 **BGR 顺序**，所以需要换成 **RGB**。

3. **归一化到 [0,1]**

   ```cpp
   vec4(R, G, B, 0) / 255
   ```

4. **映射到 [-1,1]**

   ```cpp
   * 2. - vec4(1,1,1,0)
   ```

    * 例如：

        * R=128 → 128/255 ≈ 0.5 → 0.5*2-1 = 0 （表示法线的分量为 0）。
        * R=255 → 255/255=1 → 1*2-1 = +1。
        * R=0 → 0*2-1 = -1。
    * 最终得到一个 **三维向量 (x,y,z) ∈ [-1,1]**，可作为法线。

---

### **(4) UV 翻转问题**

在 `.obj` 解析里：

```cpp
tex.push_back({uv.x, 1 - uv.y});
```

原因是：

* `.obj` 文件里的 UV 原点在 **左下角**；
* TGA 图像的原点在 **左上角**；
* 如果不 `1 - uv.y`，纹理会颠倒。

---

# 2. TBN（Tangent-Bitangent-Normal）逻辑

到这里有一个关键点：
**法线贴图里的 RGB 存储的法线不是模型空间的法线，而是切线空间 (Tangent Space) 的法线。**

### 什么是 TBN？

* **T**：Tangent 切线，沿着 **U 方向**。
* **B**：Bitangent 副切线，沿着 **V 方向**。
* **N**：Normal 法线，来自模型本身的几何法线。

它们组成一个正交基底矩阵：
$$
TBN = [T, B, N]
$$

### 为什么要有 TBN？

* 法线贴图只存局部扰动，相对于贴图的 UV 空间。
* 法线 `(R,G,B)` 默认是在切线空间。
* 要让光照计算正确，就必须把它变换到与光源/眼睛一致的 **世界空间/视空间**。

公式是：

$$
n_{world} = normalize(TBN \cdot n_{tangent})
$$
其中：

* `n_tangent` 来自法线贴图 (RGB → [-1,1])。
* `TBN` 用于把切线空间法线转换到世界/视空间。

---

# 3. 你的代码里的简化

在标准实现中，应当是：

```cpp
vec3 n_tangent = model.normal(uv);  // 切线空间
vec3 n_eye = normalize(TBN * n_tangent); // 转到眼睛空间
```

但是你的代码直接写了：

```cpp
vec4 n = normalized((ModelView.invert_transpose() * model.normal(uv)));
```

说明你的 `model.normal(uv)` 直接返回的已经是“近似模型空间的法线”，并且跳过了 **TBN 构建**，直接把它丢进 `ModelView.inverse_transpose()`。
这是 **简化写法**，适合初学/教学 Demo，但严格来说是不完整的。

---

# 4. 总结

1. **法线贴图流程**

    * 从 UV 坐标采样 RGB → 映射到 [-1,1] → 得到扰动法线 → 转换到视空间 → 光照计算。

2. **为什么 `*2./255 - 1`**

    * 把 `[0,255]` 的颜色值映射到 `[-1,1]`，才能当作向量分量。

3. **为什么 `1-uv.y`**

    * 修正 `.obj` UV 坐标系和纹理坐标系的差异，避免贴图倒置。

4. **TBN 的作用**

    * 法线贴图存储的是 **切线空间法线**，必须通过 TBN 矩阵转换到世界/视空间。
    * 你的代码跳过了 TBN，直接把 `normalmap` 的结果丢进 `ModelView.inverse_transpose()`，这是一个简化实现。
