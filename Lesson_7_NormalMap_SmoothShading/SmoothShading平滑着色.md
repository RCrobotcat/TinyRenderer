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
    vec3 l; // light direction in eye coordinates
    vec3 eye; // eye direction in eye coordinates
    vec3 varying_nrm[3]; // normal per vertex to be interpolated by the fragment shader

    BlinnPhongShader(const vec3 light, const vec3 _eye, const Model &m) : model(m) {
        l = normalized((ModelView * vec4{light.x, light.y, light.z, 0.}).xyz());
        eye = normalized((ModelView * vec4{_eye.x, _eye.y, _eye.z, 0}).xyz());
        // transform the light and eye vectors to view coordinates
    }

    virtual vec4 vertex(const int face, const int vert) {
        vec3 v = model.vert(face, vert); // current vertex in object coordinates
        vec4 gl_Position = ModelView * vec4{v.x, v.y, v.z, 1.};

        vec3 n = model.normal(face, vert); // normal at that vertex
        varying_nrm[vert] = (ModelView.invert_transpose() * vec4{n.x, n.y, n.z, 0.}).xyz();

        return Perspective * gl_Position; // in clip coordinates
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bar) const {
        TGAColor gl_FragColor = {255, 255, 255, 255}; // output color of the fragment
        // vec3 n = normalized(cross(tri[1] - tri[0], tri[2] - tri[0])); // triangle normal in eye coordinates
        vec3 n = normalized(varying_nrm[0] * bar.x + varying_nrm[1] * bar.y +
                            varying_nrm[2] * bar.z); // per-vertex normal interpolation
        vec3 h = normalized(l + eye); // half vector
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

好的，我们来逐步分析你这段 **Blinn–Phong smooth shading** 的逻辑，并特别解释为什么要用 **ModelView 的逆转置矩阵 (invert_transpose)** 来变换法向量。

---

## 1. Smooth Shading 的核心思想

在光照计算中：

* **Flat shading**：整个三角形用一个统一的法线（比如面法线）。
* **Smooth shading (Gouraud/Phong/Blinn-Phong)**：每个顶点存储法线，光照会根据**顶点法线插值**。这样可以在片元阶段得到连续平滑的光照过渡。

你的代码就是 **每个顶点的法线 → 经过变换 → 插值 → 片元光照计算**，实现了 smooth shading。

---

## 2. 关键代码逻辑

```cpp
vec3 n = model.normal(face, vert); 
varying_nrm[vert] = (ModelView.invert_transpose() * vec4{n.x, n.y, n.z, 0.}).xyz();
```

* `model.normal(face, vert)`：获取模型空间的顶点法线。
* `ModelView.invert_transpose()`：把模型空间的法线转到**视空间**。
* `varying_nrm[vert]`：存储在顶点着色器输出的“varying”中，供片元着色器进行插值。

---

## 3. 为什么要用 **逆转置矩阵**？

这是图形学里经典的 **法线变换问题**。

### (a) 顶点位置的变换

顶点 `v` 的位置用 **ModelView 矩阵**变换即可：
$$
v' = M v
$$
这里的 ( M = ModelView )。

### (b) 法线的变换不一样

法线向量 ( n ) 定义为平面的垂直方向。数学上，它和位置变换有不同的约束条件：

* 平面方程：
  $$
  n^T v = 0
  $$

* 对于变换后的位置：
  $$
  (Mv) \cdot n' = 0
  $$

推导得到：
$$
n' = (M^{-1})^T n
$$
也就是说，**法线要用矩阵的逆转置来变换**，这样才能保证它仍然垂直于变换后的表面。

---

## 4. 为什么不能直接用 `ModelView * n`？

如果 `ModelView` 包含：

* **平移**：对法线没意义（因为法线是方向，w=0 已经避免了）。
* **旋转**：没问题，旋转矩阵正交，逆转置就是它自己。
* **非均匀缩放 (scaling)**：问题出现了！直接乘会导致法线不再垂直于表面。

例如：

* 如果把 x 方向缩放 2 倍，位置向量会被拉伸。
* 直接用同样矩阵变换法线，会让法线方向偏移，不再垂直。
* 用逆转置矩阵则能修正这一点。

---

## 5. 总结 smooth shading 流程

1. **顶点阶段**

    * 顶点坐标用 `ModelView` 和 `Perspective` 变换。
    * 顶点法线用 `ModelView.inverse_transpose` 变换到视空间。
    * 保存变换后的法线到 `varying_nrm[]`。

2. **片元阶段**

    * 插值三个顶点的法线，得到当前像素的法线。
    * 和光照方向、视线方向做 Blinn-Phong 模型计算。
    * 得到平滑过渡的光照效果。

---

- 核心答案一句话总结：
  `ModelView.invert_transpose()` 是为了保证法线在经过非正交变换（如非均匀缩放、剪切）后，仍然保持与变换后表面垂直，这是 smooth shading 正确计算光照的必要条件。



---

我来帮你拆开这两段代码的逻辑：

## 1. 顶点法线的处理

```cpp
vec3 n = model.normal(face, vert); // 顶点的法线（模型空间）
varying_nrm[vert] = (ModelView.invert_transpose() * vec4{n.x, n.y, n.z, 0.}).xyz();
```

这里的逻辑是：

- **输入**：法线 `n` 是 **模型坐标系 (object space)** 下的。

- **目标**：我们要把法线变换到 **眼睛坐标系 (eye/view space)**，因为光照计算（光源向量 `l`、观察向量 `eye`）都在这个空间中。

- **为什么用逆转置矩阵**：

  - 普通顶点位置可以直接 `ModelView * v`，因为这是仿射变换。

  - 但是法线必须保持“垂直于表面”的性质。经过非均匀缩放、剪切后，如果直接用 `ModelView * n`，法线可能不再垂直。

  - 数学推导告诉我们：
    $$
    n' = (M^{-1})^T n
    $$

    其中 (M = ModelView)。所以必须用 **逆转置矩阵**。

- **存储**：把变换后的法线保存到 `varying_nrm[vert]`，这是一个 **逐顶点输出 (varying variable)**，之后在片元阶段会被插值。

------

## 2. 片元法线的插值

```cpp
vec3 n = normalized(
    varying_nrm[0] * bar.x + 
    varying_nrm[1] * bar.y + 
    varying_nrm[2] * bar.z
);
```

这里 `bar` 是 **重心坐标 (barycentric coordinates)**，三个分量 `(bar.x, bar.y, bar.z)` 对应当前片元在三角形内的相对权重。

- **作用**：
  - 对三个顶点的法线 `varying_nrm[0..2]` 做 **线性插值**。
  - 插值的结果就是当前片元的“平滑法线”。
- **为什么要插值**：
  - 如果不用插值，而是整个三角形用一个统一法线 → 就是 **Flat Shading**，光照会“一块一块”的。
  - 插值之后，每个像素的法线都不同 → 光照能平滑过渡 → 产生 **Smooth Shading (Phong shading)** 的效果。

------

## 3. 整体流程总结

1. **顶点着色阶段 (vertex)**
   - 取出顶点法线（模型空间）。
   - 用 `ModelView.inverse_transpose` 变换到眼睛空间。
   - 存到 `varying_nrm[]`，供插值用。
2. **片元着色阶段 (fragment)**
   - 用重心坐标 `bar` 对三角形三个顶点的法线插值。
   - 得到片元所在位置的平滑法线 `n`。
   - 拿 `n`、光源方向 `l`、视线方向 `eye` 做 Blinn–Phong 光照计算。
3. **最终效果**
   - 光照从一个顶点平滑过渡到另一个顶点，不再“一块块”，而是圆润、连续。

------

✅ 关键点一句话：
 `ModelView.inverse_transpose` 保证法线在变换后仍然正确（不歪），而 **重心插值** 保证法线在三角形内逐像素平滑过渡，这就是 **Smooth Shading** 的核心。
