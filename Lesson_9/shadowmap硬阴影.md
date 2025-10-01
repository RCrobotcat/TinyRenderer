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
            vec3 p = q.xyz() / q.w; // 像素在摄像机坐标系下的位置
            bool lit = (fragment.z < -100 || // 背景，直接亮
                        (p.x < 0 || p.x >= shadoww || p.y < 0 || p.y >= shadowh) || // 超出 shadow map 范围
                        (p.z > zbuffer[int(p.x) + int(p.y) * shadoww] - .03)); // 点比 shadowmap 更靠近光源 => 受光照
            // -0.03 是一个 bias，防止 z-fighting
            // p.z <= shadowmap_depth + bias → 点比 shadowmap 更远 → 在阴影里
            // p.z 的方向是: 越大越靠近相机
            mask[x + y * width] = lit;
        }
    }

    TGAImage maskimg(width, height, TGAImage::GRAYSCALE);
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (mask[x + y * width]) continue;
            // 阴影的地方显示白色
            maskimg.set(x, y, {255, 255, 255, 255});
        }
    }
    maskimg.write_tga_file("mask.tga");

    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            if (mask[x + y * width]) continue; // 亮点不处理
            // 阴影点处理
            TGAColor c = framebuffer.get(x, y);
            vec3 a = {static_cast<double>(c[0]), static_cast<double>(c[1]),
                      static_cast<double>(c[2])};
            if (norm(a) < 80) continue;
            a = normalized(a) * 80; // 颜色变暗
            framebuffer.set(x, y, {(uint8_t) a[0], (uint8_t) a[1], (uint8_t) a[2], 255});
        }
    }
    framebuffer.write_tga_file("shadow.tga");

    return 0;
}

```
---
# 🌑 阴影渲染代码笔记（Shadow Mapping）

这份代码实现了一个经典的 **Shadow Mapping 阴影渲染流程**，整体分为三步：

1. **普通渲染（Camera pass）**
    从相机视角渲染场景，得到颜色图和相机深度。
2. **阴影贴图生成（Shadow map pass / Light pass）**
    从光源视角渲染场景，得到光源深度图（shadow map）。
3. **后处理判断（Post-process）**
    对每个相机像素，反变换回相机空间 → 投影到光源空间 → 和 shadow map 深度比较 → 判断是否在阴影里。

------

## 1️⃣ 普通渲染 pass

```cpp
lookat(eye, center, up);                     // 相机视角矩阵
init_perspective(norm(eye - center));        // 透视投影矩阵
init_viewport(...);                          // 视口矩阵
init_zbuffer(width, height);                 // 初始化 z-buffer
TGAImage framebuffer(width, height, TGAImage::RGB);
```

- 设置相机的 ModelView、Perspective、Viewport。
- 初始化相机 z-buffer。
- `framebuffer` 保存最终颜色。

然后渲染两个模型：

```cpp
Model model_head("../Obj/diablo3_pose.obj");
Model model_floor("../Obj/floor.obj");

BlinnPhongShader shader(light_dir, eye, model_head);
for (faces in head) rasterize(...);

BlinnPhongShader shader(light_dir, eye, model_floor);
for (faces in floor) rasterize(...);
```

👉 得到：

- **framebuffer** = 彩色渲染图（Blinn-Phong 光照）。
- **zbuffer** = 相机视角下的深度图。

------

## 2️⃣ 阴影贴图生成（光源视角）

首先保存相机矩阵的逆矩阵：

```cpp
mat<4,4> M = (Viewport * Perspective * ModelView).invert();
```

- 以后要用它把屏幕像素坐标 **反变换回相机空间**。

然后切换到光源视角：

```cpp
lookat(light_dir, center, up);             // 光源视角
init_perspective(norm(light_dir - center));
init_viewport(...);
init_zbuffer(shadoww, shadowh);
```

渲染场景，但这里用 **BlankShader**：

```cpp
BlankShader shader{model_head};  // 不计算光照，只写深度
for (faces in head) rasterize(...);

BlankShader shader{model_floor};
for (faces in floor) rasterize(...);
```

- `BlankShader` 的 fragment 总是白色 → 颜色没意义。
- 但是 rasterizer 写入的 **zbuffer** 就是光源能看到的深度。

👉 得到：

- `zbuffer` = **shadow map**（光源视角下的深度图）。
- 保存为 `shadowmap.tga` 和 `zbuffer2.tga`。

------

## 3️⃣ 后处理（阴影判定）

关键逻辑：

```cpp
mat<4,4> N = Viewport * Perspective * ModelView; // 光源的投影矩阵
```

### (a) 屏幕像素 → 相机空间坐标

```cpp
vec4 fragment = M * vec4{x, y, zbuffer_copy[x+y*width], 1.};
```

- `(x,y)` = 当前像素位置
- `zbuffer_copy[...]` = 相机 pass 下该像素的深度
- 乘以 `M = (VP*MV)^-1` = 从屏幕 → 相机空间坐标

------

### (b) 相机空间 → 光源投影坐标

```cpp
vec4 q = N * fragment;
vec3 p = q.xyz() / q.w; 
```

- `N` = 光源的投影矩阵 (Viewport * Perspective * ModelView)
- 得到 `p = (p.x, p.y, p.z)`：
  - `p.x, p.y` → 在 shadow map 上的坐标
  - `p.z` → 当前像素在**摄像机坐标系**下的深度

------

### (c) 与 shadow map 比较

```cpp
bool lit = (fragment.z < -100 ||                    // 背景直接亮
            (p.x<0 || p.x>=shadoww || p.y<0 || p.y>=shadowh) || // 超出shadowmap范围 → 亮
            (p.z > zbuffer[int(p.x)+int(p.y)*shadoww] - .03));  // 深度比较
```

解释：

- 如果是背景 → 亮
- 如果超出 shadow map 范围 → 亮
- 否则：
  - `shadowmap_depth = zbuffer[...]`
  - **约定：你的坐标系里 p.z 越大越靠近光源**
  - 若 `p.z > shadowmap_depth - bias` → 点比 shadowmap 记录的更靠近光源 → 可见（亮）
  - 否则 → 被挡住（阴影里）
  - `bias` 用于防止 **z-fighting**

👉 `lit` = 是否受光照

------

### (d) 阴影标记与后处理

1. 生成 mask 图：

   ```cpp
   if (!lit) maskimg.set(x,y,{255,255,255,255});
   ```

   阴影区域显示白色。

2. 调暗阴影区域：

   ```cpp
   if (!lit) {
       vec3 a = {c[0], c[1], c[2]};
       if (norm(a) >= 80) {
           a = normalized(a) * 80;   // 压低亮度
           framebuffer.set(x,y,{a[0],a[1],a[2],255});
       }
   }
   ```

   👉 得到带阴影的 `shadow.tga`。

------

# 📌 总结笔记

1. **普通渲染 pass**：相机视角渲染，得到颜色 + 深度。
2. **阴影贴图 pass**：光源视角渲染，得到 shadow map（深度图）。
3. **后处理**：
   - 相机像素 → 相机空间
   - 相机空间 → 光源空间
   - 深度比较：
     - `p.z > shadowmap_depth - bias` → **受光照**
     - 否则 → **在阴影中**
4. **输出**：
   - framebuffer.tga（相机渲染）
   - zbuffer1.tga（相机深度）
   - shadowmap.tga / zbuffer2.tga（光源深度）
   - mask.tga（阴影区域）
   - shadow.tga（最终带阴影效果的图）
