# 1. Ambient Occlusion環境光遮蔽: Brute-force Ambient Occlusion
## 环境光遮蔽: 暴力算法计算环境光遮蔽 => 通过多次渲染阴影贴图, 计算每个像素点被光照到的概率, 作为环境光遮蔽系数
```cpp
#include <random>
#include <algorithm>
#include <cmath>

#include "gl_mine.h"
#include "model.h"

#define M_PI 3.14159265358979323846

extern mat<4, 4> Viewport, ModelView, Perspective; // "OpenGL" state matrices and
extern std::vector<double> zbuffer; // the depth buffer

struct EmptyShader : IShader
{
    const Model &model;

    EmptyShader(const Model &m) : model(m)
    {
    }

    virtual vec4 vertex(const int face, const int vert)
    {
        vec4 gl_Position = ModelView * model.vert(face, vert);
        return Perspective * gl_Position;
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bar) const
    {
        TGAColor gl_FragColor = {255, 255, 255, 255};
        return {false, gl_FragColor};
    }
};

int main()
{
    constexpr int width = 800; // output image size
    constexpr int height = 800;
    constexpr int shadoww = 8000; // shadow map buffer size
    constexpr int shadowh = 8000;
    constexpr vec3 eye{-1, 0, 2}; // camera position
    constexpr vec3 center{0, 0, 0}; // camera direction
    constexpr vec3 up{0, 1, 0}; // camera up vector

    // usual rendering pass
    lookat(eye, center, up);
    init_perspective(norm(eye - center));
    init_viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8);
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB, {177, 195, 209, 255});

    Model model("../Obj/diablo3_pose.obj"); // load the data
    EmptyShader shader{model};
    for (int f = 0; f < model.nfaces(); f++)
    {
        // iterate through all facets
        Triangle clip = {
            shader.vertex(f, 0), // assemble the primitive
            shader.vertex(f, 1),
            shader.vertex(f, 2)
        };
        rasterize(clip, shader, framebuffer); // rasterize the primitive
    }
    framebuffer.write_tga_file("framebuffer.tga");

    std::vector<double> mask(width * height, 0);
    std::vector<double> zbuffer_copy = zbuffer;
    mat<4, 4> M = (Viewport * Perspective * ModelView).invert();

    // shadow rendering pass
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    constexpr int n = 1000;

    auto smoothstep = [](double edge0, double edge1, double x)
    {
        // smoothstep returns 0 if the input is less than the left edge,
        double t = std::clamp((x - edge0) / (edge1 - edge0), 0., 1.); // 1 if the input is greater than the right edge,
        return t * t * (3 - 2 * t);
        // Hermite interpolation inbetween. The derivative of the smoothstep function is zero at both edges.
    };

    for (int i = 0; i < n; ++i)
    {
        std::cerr << i << std::endl;
        double y = dist(gen);
        double theta = 2.0 * M_PI * dist(gen);
        double r = std::sqrt(1.0 - y * y);
        vec3 light = vec3{r * std::cos(theta), y, r * std::sin(theta)} * 1.5;

        std::cout << "v " << light << std::endl;

        lookat(light, center, up);
        ModelView[3][3] = norm(light - center);

        init_perspective(norm(eye - center));
        init_viewport(shadoww / 16, shadowh / 16, shadoww * 7 / 8, shadowh * 7 / 8);
        init_zbuffer(shadoww, shadowh);
        TGAImage trash(shadoww, shadowh, TGAImage::RGB, {177, 195, 209, 255});

        Model model("../Obj/diablo3_pose.obj"); // load the data
        EmptyShader shader{model};
        for (int f = 0; f < model.nfaces(); f++)
        {
            // iterate through all facets
            Triangle clip = {
                shader.vertex(f, 0), // assemble the primitive
                shader.vertex(f, 1),
                shader.vertex(f, 2)
            };
            rasterize(clip, shader, trash); // rasterize the primitive
        }
        //      trash.write_tga_file(std::string("shadowmap") + std::to_string(i) + std::string(".tga"));

        mat<4, 4> N = Viewport * Perspective * ModelView;

        // post-processing
#pragma omp parallel for
        for (int x = 0; x < width; x++)
        {
            for (int y = 0; y < height; y++)
            {
                vec4 fragment = M * vec4{
                                    static_cast<double>(x), static_cast<double>(y), zbuffer_copy[x + y * width], 1.
                                };
                vec4 q = N * fragment;
                vec3 p = q.xyz() / q.w;
                double lit = (fragment.z < -100 || // it's the background or
                              (p.x >= 0 && p.x < shadoww && p.y >= 0 && p.y < shadowh &&
                               // it is not out of bounds of the shadow buffer
                               (p.z > zbuffer[int(p.x) + int(p.y) * shadoww] - .03))); // it is visible
                mask[x + y * width] += (lit - mask[x + y * width]) / (i + 1.);
            }
        }
    }

#pragma omp parallel for
    for (int x = 0; x < width; x++)
    {
        for (int y = 0; y < height; y++)
        {
            double m = smoothstep(-1, 1, mask[x + y * width]);
            TGAColor c = framebuffer.get(x, y);
            framebuffer.set(x, y, {
                                static_cast<uint8_t>(c[0] * m), static_cast<uint8_t>(c[1] * m),
                                static_cast<uint8_t>(c[2] * m), c[3]
                            });
        }
    }
    framebuffer.write_tga_file("shadow.tga");

    // post-processing 2: edge detection
    constexpr double threshold = .15;
    for (int y = 1; y < framebuffer.height() - 1; ++y)
    {
        for (int x = 1; x < framebuffer.width() - 1; ++x)
        {
            vec2 sum;
            for (int j = -1; j <= 1; ++j)
            {
                for (int i = -1; i <= 1; ++i)
                {
                    constexpr int Gx[3][3] = {{-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1}};
                    constexpr int Gy[3][3] = {{-1, -2, -1}, {0, 0, 0}, {1, 2, 1}};
                    sum = sum + vec2{
                              Gx[j + 1][i + 1] * zbuffer_copy[x + i + (y + j) * width],
                              Gy[j + 1][i + 1] * zbuffer_copy[x + i + (y + j) * width]
                          };
                }
            }
            if (norm(sum) > threshold)
                framebuffer.set(x, y, TGAColor{0, 0, 0, 255}); // 绘制黑色轮廓线
        }
    }
    framebuffer.write_tga_file("edges.tga");

    return 0;
}

```
---
这是一个**CPU 实现的简化渲染器**，流程包括模型渲染、阴影模拟（软阴影/环境遮蔽风格）、以及后处理（边缘检测）。

---

## 1. 基础框架和数据结构

* **矩阵和缓冲区**

    * `ModelView`, `Perspective`, `Viewport`：模拟 OpenGL 渲染管线的三个关键矩阵。

        * `ModelView`：把模型从世界空间变换到相机空间。
        * `Perspective`：进行透视投影。
        * `Viewport`：把裁剪坐标映射到屏幕像素坐标。
    * `zbuffer`：深度缓存，记录屏幕上每个像素的最近深度。

* **EmptyShader**
  一个最简化的着色器，顶点阶段只做变换，片元阶段只返回白色 `{255,255,255}`。
  相当于：渲染一个没有光照、没有纹理的白模。

---

## 2. 正常渲染阶段（framebuffer.tga）

```cpp
lookat(eye, center, up);
init_perspective(norm(eye-center));
init_viewport(width/16, height/16, width*7/8, height*7/8);
init_zbuffer(width, height);
```

* 设置相机视角、投影矩阵、视口以及初始化深度缓存。
* 遍历每个模型 → 每个三角形：

    * 调用 `shader.vertex()` 得到裁剪坐标。
    * 调用 `rasterize()` 在 framebuffer 上绘制三角形。
* 得到一张最初的场景图（白色模型，背景色淡蓝 `{177,195,209}`）。

输出：`framebuffer.tga`

---

## 3. 阴影/光照采样阶段（mask 计算）

这一部分就是代码的“核心亮点”：用**多光源随机采样**来近似软阴影或环境光遮蔽（Ambient Occlusion）。

1. **准备逆矩阵**

   ```cpp
   mat<4,4> M = (Viewport * Perspective * ModelView).invert();
   ```

    * 这样可以把屏幕坐标 (x, y, zbuffer) 还原回世界坐标。

2. **随机采样光源方向**

   ```cpp
   double y = dist(gen);
   double theta = 2.0 * M_PI * dist(gen);
   double r = std::sqrt(1.0 - y*y);
   vec3 light = vec3{r*cos(theta), y, r*sin(theta)} * 1.5;
   ```

    * 在单位半球上随机取点，生成一个光源方向。
    * 一共采样 `n=1000` 个方向。

3. **从光源角度渲染 shadow map**

   ```cpp
   lookat(light, center, up);
   init_perspective(...);
   init_viewport(... shadoww ...);
   init_zbuffer(shadoww, shadowh);
   ```

    * 把光源当作相机，从光源视角绘制场景，得到深度图（shadow map）。
    * 这个结果用来判断场景某个点是否被遮挡。

4. **投影并判断可见性**

   ```cpp
   vec4 fragment = M * vec4{x, y, zbuffer_copy[x+y*width], 1.};
   vec4 q = N * fragment;
   vec3 p = q.xyz()/q.w;
   bool lit = (fragment.z<-100 || 
              (p在shadowmap范围 && 
               p.z > zbuffer_shadow[p.x,p.y]-0.03));
   ```

    * `fragment`：把屏幕像素还原到世界坐标。
    * `N`：光源的变换矩阵，把世界坐标变换到光源的屏幕空间。
    * 判断当前像素是否在光源能看到的位置：

        * 如果在阴影里，`lit=0`；如果可见，`lit=1`。
    * 用累积平均公式更新 `mask[x+y*width]`，得到**遮蔽率**。

结果：`mask` 数组保存每个像素的光照平均情况（0 = 完全遮挡，1 = 完全照亮）。

---

## 4. 第一次后处理（阴影融合）

```cpp
double m = smoothstep(-1, 1, mask[x+y*width]);
TGAColor c = framebuffer.get(x, y);
framebuffer.set(x, y, { c[0]*m, c[1]*m, c[2]*m, c[3] });
```

* 使用 **smoothstep** 平滑插值遮蔽率。
* 把原 framebuffer 的颜色乘以遮蔽因子 `m`，让暗处更暗，亮处保持。
* 输出 `shadow.tga`，效果类似软阴影/环境光遮蔽。

---

## 5. 第二次后处理（边缘检测）

使用 Sobel 算子对深度图做边缘检测：

```cpp
Gx = { {-1,0,1}, {-2,0,2}, {-1,0,1} }
Gy = { {-1,-2,-1}, {0,0,0}, {1,2,1} }
```

* 遍历每个像素，用 Sobel 计算梯度 `(dx,dy)`。
* 如果梯度的模长 > 阈值 0.15 → 判定为边缘。
* 把该像素设为黑色。

输出 `edges.tga`，得到类似轮廓线的效果。

---

我来给你完整推导 **Sobel 算子计算图像梯度 $(dx,dy)$ 的公式过程**。

## 背景

在图像处理里，**边缘检测**的核心思想就是计算图像的梯度（即灰度值变化率）。

二维灰度图像 $(I(x,y))$：

- 水平方向梯度 $(G_x)$：检测垂直边缘
- 垂直方向梯度 $(G_y)$：检测水平边缘

## Sobel 卷积核

Sobel 算子定义了两个 3×3 卷积核：
$$
K_x =
 \begin{bmatrix}
 -1 & 0 & +1 \\
 -2 & 0 & +2 \\
 -1 & 0 & +1 \\
 \end{bmatrix},
 \quad
 K_y =
 \begin{bmatrix}
 -1 & -2 & -1 \\
 0 & 0 & 0 \\
 +1 & +2 & +1 \\
 \end{bmatrix}
$$

## 卷积计算公式

对于某个像素点 $(x,y)$，考虑它周围的 3×3 邻域：
$$
\begin{bmatrix}
 I(x-1,y-1) & I(x,y-1) & I(x+1,y-1) \\
 I(x-1,y)   & I(x,y)   & I(x+1,y) \\
 I(x-1,y+1) & I(x,y+1) & I(x+1,y+1) \\
 \end{bmatrix}
$$

- **水平方向梯度**：
  $$
  G_x = (K_x * I)(x,y) = \sum_{i=-1}^{1} \sum_{j=-1}^{1} K_x(i,j), I(x+i, y+j)
  $$

- **垂直方向梯度**：
  $$
  G_y = (K_y * I)(x,y) = \sum_{i=-1}^{1} \sum_{j=-1}^{1} K_y(i,j), I(x+i, y+j)
  $$

## 展开公式

代入 ($K_x, K_y$)：
$$
G_x = -I(x-1,y-1) + I(x+1,y-1)
 -2I(x-1,y)   + 2I(x+1,y)
 -I(x-1,y+1) + I(x+1,y+1)
$$

$$
G_y = -I(x-1,y-1) -2I(x,y-1) - I(x+1,y-1)
 +I(x-1,y+1) +2I(x,y+1) + I(x+1,y+1)
$$

## 梯度幅值与方向

有了 $(G_x, G_y)$，就能计算边缘强度和方向：

- **梯度幅值（边缘强度）**：
  $$
  G = \sqrt{G_x^2 + G_y^2}
   （有时用近似 (|G_x|+|G_y|) 以减少计算量）
  $$

- **梯度方向（边缘方向）**：
  $$
  \theta = \arctan\left(\frac{G_y}{G_x}\right)
  $$

## 小结

- **Sobel 本质**：对图像做加权差分，近似一阶导数。
- **dx = Gx**：水平方向亮度变化率
- **dy = Gy**：垂直方向亮度变化率
- 组合 $(\sqrt{dx^2+dy^2})$ 得到梯度大小，用于检测边缘。

---

## 6. 总结

这段代码实现了一个 **简易 CPU 渲染器**，包含三个主要阶段：

1. **正常渲染（framebuffer.tga）**
   → 白模场景。

2. **基于多光源采样的软阴影/环境光遮蔽（shadow.tga）**
   → 使用随机光源方向生成阴影概率，平滑合成到场景中。

3. **边缘检测（edges.tga）**
   → 对深度图应用 Sobel 算子，绘制黑色轮廓线。
---
`smoothstep` 函数其实是 **图形学里常用的插值函数**，尤其是着色器语言（GLSL）里也有同名函数。我们逐行拆解：

```cpp
auto smoothstep = [](double edge0, double edge1, double x)
{
	// smoothstep returns 0 if the input is less than the left edge,
	double t = std::clamp((x - edge0) / (edge1 - edge0), 0., 1.); 
	// 1 if the input is greater than the right edge,
	return t * t * (3 - 2 * t);
	// Hermite interpolation inbetween. The derivative of the smoothstep function is zero at both edges.
};
```
## 1. 函数签名

```cpp
auto smoothstep = [](double edge0, double edge1, double x)
```

* **edge0, edge1**：两个阈值（范围区间）。
* **x**：输入值。
* **返回值**：一个在 `[0,1]` 之间平滑过渡的数。

## 2. clamped 归一化

```cpp
double t = std::clamp((x - edge0) / (edge1 - edge0), 0., 1.);
```

* `(x - edge0) / (edge1 - edge0)` 把 `x` 映射到 `[0,1]` 范围：

    * 当 `x ≤ edge0` → 映射结果 ≤ 0
    * 当 `x ≥ edge1` → 映射结果 ≥ 1
* `std::clamp(...,0,1)` 把结果钳制在 `[0,1]` 之间。
  所以 `t` 始终在 `[0,1]`。

举例：

* `edge0=0, edge1=10, x=2` → `t=0.2`
* `x=-5` → `t=0`
* `x=15` → `t=1`

## 3. Hermite 插值

```cpp
return t*t*(3 - 2*t);
```

这是一个三次 Hermite 插值公式：

$$
f(t) = t^2 (3 - 2t)
$$
它的性质：

* 当 `t=0` → `f=0`
* 当 `t=1` → `f=1`
* 当 `t` 在 (0,1) 之间时 → 平滑过渡。
* **导数**：
  $$
  f'(t) = 6t - 6t^2 = 6t(1-t)
  $$
  当 `t=0` 或 `t=1`，导数为 0 → 两端平滑收敛（不会突然跳变）。

这比线性插值 (`f(t)=t`) 更平滑，避免了边界突变。

## 4. 功能总结

* 如果 `x < edge0`，返回 `0`。
* 如果 `x > edge1`，返回 `1`。
* 如果 `edge0 ≤ x ≤ edge1`，返回一个**平滑的 S 曲线值**。

这就是一个“软阈值函数”，常用于：

* 图形学里的 **渐变/抗锯齿/软阴影**。
* 代替硬切换，产生平滑过渡。

## 5. 可视化效果

相比普通线性插值（直线），`smoothstep` 会形成一个 S 形曲线：

```
y
1 |        ________
  |      /
  |    /
  |  /
0 |/______________ x
   0      1
```

👉 总结：
这个 `smoothstep` 函数就是一个 **带上下限的平滑阶跃函数**，核心思想是：

* **Clamp** 把输入限制在 `[0,1]`
* **Hermite 多项式** (`t^2(3-2t)`) 让过渡变得光滑，两端斜率为零。
