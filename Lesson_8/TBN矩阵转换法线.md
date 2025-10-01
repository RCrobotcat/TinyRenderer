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

## 1. 为什么要用 TBN

在 Blinn-Phong 或其他带法线贴图的光照模型中，**法线贴图里的法线向量是存储在切线空间 (Tangent Space)** 的，而不是在世界空间或者眼空间。

切线空间的三个正交基向量是：

* **T (Tangent)** 切线方向
* **B (Bitangent 或 Binormal)** 副切线方向
* **N (Normal)** 法线方向

有了这三个向量，就能从切线空间转换到世界/眼空间。这样在片元着色阶段，就能把贴图里的法线正确应用到光照计算中。

---

## 2. 代码中的 TBN 推导

在你的 `fragment` 里：

```cpp
// TBN matrix
mat<2, 4> E = {tri[1] - tri[0], tri[2] - tri[0]};  
mat<2, 2> U = {varying_uv[1] - varying_uv[0], varying_uv[2] - varying_uv[0]};  
mat<2, 4> T = U.invert() * E;
```

这里的逻辑是：

1. **E**：三角形两个边向量（在视空间中）

    * `tri[1] - tri[0]`
    * `tri[2] - tri[0]`

   这两个向量是三角形在空间里的边。

2. **U**：UV 坐标差

    * `uv1 - uv0`
    * `uv2 - uv0`

   它表示纹理坐标在三角形两条边方向上的差值。

3. **解线性方程求切线/副切线**
   公式来源是把空间边向量 **E** 和 UV 差值 **U** 建立线性关系：

   ```
   E = U * [T, B]^T
   ```

   求解得到：

   ```
   [T, B]^T = U^-1 * E
   ```

   于是 `T[0]` 就是 **切线向量**，`T[1]` 就是 **副切线向量**。

---

## 3. 构造 Darboux Frame (TBN 矩阵)

```cpp
mat<4, 4> D = { normalized(T[0]),     // Tangent
                normalized(T[1]),     // Bitangent
                normalized(varying_nrm[0] * bar[0] +
                           varying_nrm[1] * bar[1] +
                           varying_nrm[2] * bar[2]), // Normal
                {0, 0, 0, 1} };
```

* **T**：切线
* **B**：副切线
* **N**：插值后的顶点法线（再归一化）
* 这三者形成一个局部坐标系（Darboux Frame），用来把切线空间里的法线变换到视空间。

---

## 4. 应用 TBN

```cpp
vec2 uv = varying_uv[0] * bar.x + varying_uv[1] * bar.y + varying_uv[2] * bar.z;
vec4 n = normalized(D.transpose() * model.normal(uv));
```

* `model.normal(uv)` ：从 **法线贴图**采样出的法线，是在切线空间的。
* `D.transpose()` ：把切线空间 → 视空间。

  > 注意这里用 `D.transpose()` 而不是 `D`，因为矩阵是列向量形式存储的，所以转置等价于把 `[T, B, N]` 当作基向量矩阵。

得到的 `n` 就是最终在 **眼空间里的法线**。

---

## 5. 后续光照

```cpp
vec4 h = normalized(l + eye); // 半程向量
double ambient = .5;
double diff = std::max(0., n * l);
double spec = std::pow(std::max(n * h, 0.), 70);
```

* **diff** = 漫反射项，取 `max(0, n·l)`
* **spec** = 高光项，用 `n·h`（Blinn-Phong）
* **ambient** = 环境光常数

把它们叠加后，再乘上漫反射贴图、镜面贴图，就得到了最终像素颜色。

---

## 6. 总结一句话

这段代码的 TBN 逻辑做的事情就是：
**通过 UV 坐标差和三角形边向量计算切线/副切线，再加上插值法线组成 TBN 矩阵，把法线贴图里的切线空间法线转换到眼空间进行 Blinn-Phong 光照计算。**
