```cpp
#include <algorithm>
#include <cmath>
#include <vector>
#include "geometry.h"
#include "gl_mine.h"
#include "model.h"
#include "tgaimage.h"

#define M_PI 3.14159265358979323846

constexpr int width = 800;
constexpr int height = 800;

extern mat<4, 4> ModelView, Perspective;
extern std::vector<double> zbuffer;

struct PhongShader : IShader
{
    const Model &model;
    vec3 l; // light direction in eye coordinates
    vec3 tri[3]; // triangle in eye coordinates

    PhongShader(const vec3 light, const Model &m) : model(m)
    {
        l = normalized((ModelView * vec4{light.x, light.y, light.z, 0.}).xyz());
        // transform the light vector to view coordinates
    }

    virtual vec4 vertex(const int face, const int vert)
    {
        vec3 v = model.vert(face, vert); // current vertex in object coordinates
        vec4 gl_Position = ModelView * vec4{v.x, v.y, v.z, 1.};
        tri[vert] = gl_Position.xyz(); // in eye coordinates
        return Perspective * gl_Position; // in clip coordinates
    }

    virtual std::pair<bool, TGAColor> fragment(const vec3 bar) const
    {
        TGAColor gl_FragColor = {255, 255, 255, 255}; // output color of the fragment
        vec3 n = normalized(cross(tri[1] - tri[0], tri[2] - tri[0])); // triangle normal in eye coordinates
        vec3 r = normalized(n * (n * l) * 2 - l); // reflected light direction
        double ambient = .3; // ambient light intensity
        double diff = std::max(0., n * l); // diffuse light intensity
        double spec = std::pow(std::max(r.z, 0.), 35);
        // specular intensity, note that the camera lies on the z-axis (in eye coordinates), therefore simple r.z, since (0,0,1)*(r.x, r.y, r.z) = r.z
        for (int channel: {0, 1, 2})
            gl_FragColor[channel] *= std::min(1., ambient + .4 * diff + .9 * spec);
        return {false, gl_FragColor}; // do not discard the pixel
    }
};

int main()
{
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

    PhongShader shader(light_dir, model);
    for (int f = 0; f < model.nfaces(); f++)
    {
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

------

## 一、代码整体结构

这段代码是一个简易的软件渲染器，功能类似 OpenGL/DirectX 的简化版渲染管线：

1. **加载模型**（OBJ 文件）：`Model model("../Obj/african_head.obj");`
2. **设置摄像机和投影矩阵**：`lookat`、`init_perspective`、`init_viewport`
3. **初始化 z-buffer 和帧缓冲**
4. **定义 Shader（PhongShader）**
   - `vertex()` → 顶点着色器：处理坐标变换
   - `fragment()` → 片段着色器：计算光照颜色
5. **主循环 rasterize**：对每个三角形执行光栅化并调用 shader 进行 shading
6. **输出 framebuffer 到 tga 文件**

------

## 二、PhongShader 的实现

### 1. 顶点着色器 `vertex`

```cpp
vec4 vertex(const int face, const int vert)
{
    vec3 v = model.vert(face, vert);               // 顶点（物体坐标系）
    vec4 gl_Position = ModelView * vec4{v.x, v.y, v.z, 1.}; 
    tri[vert] = gl_Position.xyz();                 // 存储眼坐标下的三角形顶点
    return Perspective * gl_Position;              // 返回裁剪空间坐标
}
```

流程：

- 把模型坐标（Object Space）→ 眼坐标（Eye Space）
- 保存变换后的顶点坐标到 `tri[]`
- 输出透视投影后的坐标，供光栅化使用

------

### 2. 片段着色器 `fragment`

```cpp
TGAColor gl_FragColor = {255, 255, 255, 255}; 
vec3 n = normalized(cross(tri[1] - tri[0], tri[2] - tri[0])); // 法线
vec3 r = normalized(n * (n * l) * 2 - l); // 反射光
double ambient = .3; 
double diff = std::max(0., n * l); // 漫反射
double spec = std::pow(std::max(r.z, 0.), 35); // 镜面反射
for (int channel: {0, 1, 2})
    gl_FragColor[channel] *= std::min(1., ambient + .4 * diff + .9 * spec);
return {false, gl_FragColor};
```

这里就是 **Phong 光照模型**核心部分。

------

## 三、Phong 光照模型详解

Phong 模型将光照分解为三部分：

### 1. 环境光 (Ambient)

```cpp
double ambient = 0.3;
```

- 模拟“全局背景亮度”，即使没有光源照射，也不会全黑。

- 数学公式：
  $$
  I_{ambient} = k_a I_a
  $$
  这里简化为常数 `0.3`。

------

### 2. 漫反射 (Diffuse)

```cpp
double diff = std::max(0., n * l);
```

- 根据 **Lambert 余弦定律**：光照强度与法线和光线的夹角余弦成正比。

- $n$ = 法向量，$l$ = 光方向向量

- 数学公式：
  $$
  I_{diffuse} = k_d I_l \max(0, \vec{n} \cdot \vec{l})
  $$

- 这里的系数是 `0.4 * diff`。

------

### 3. 镜面反射 (Specular)

```cpp
vec3 r = normalized(n * (n * l) * 2 - l);
double spec = std::pow(std::max(r.z, 0.), 35);
```

- **原理**：光照到物体表面后，反射方向与视线方向的夹角决定镜面高光强度。

- 反射公式：
  $$
  \vec{r} = 2(\vec{n} \cdot \vec{l}) \vec{n} - \vec{l}
  $$

- 由于相机位于 z 轴正方向 `(0,0,1)`，视线方向就是 z 轴，所以只取 `r.z`。

- `35` → 高光指数，值越大高光越“尖锐”。

- 数学公式：
  $$
  I_{specular} = k_s I_l (\max(0, \vec{r} \cdot \vec{v}))^\alpha
  $$

------

### 4. 最终颜色合成

```cpp
gl_FragColor[channel] *= std::min(1., ambient + .4 * diff + .9 * spec);
```

- 环境光 + 漫反射 + 镜面反射
- 各分量系数分别是：
  - 环境光：0.3
  - 漫反射：0.4
  - 镜面反射：0.9
- 最终乘以基础颜色（255,255,255），然后写入 framebuffer。

------

## 四、完整渲染流程总结

1. **顶点阶段**：把物体空间坐标变换到裁剪空间，同时保存眼空间三角形坐标。
2. **光栅化**：将三角形切分为像素片段。
3. **片段阶段 (Phong 光照)**：
   - 计算法线
   - 计算光源与法线夹角（漫反射）
   - 计算反射方向与视线方向（镜面反射）
   - 加上环境光，得到最终颜色
4. **写入 framebuffer + z-buffer 测试**
5. **输出 TGA 文件**

------

## 五、和真实 GPU Phong Shading 的对比

- 这里实现的是 **Flat Shading**（整片三角形使用一个法线）
- 实际 GPU **Phong Shading** 会在像素级插值法线 → 每个像素计算更平滑的高光
- 若改用 Gouraud Shading（顶点计算光照并插值），高光会失真

------

这段代码实现了一个简易 **Phong 光照模型的光栅化渲染器**，其中环境光、漫反射和镜面反射分别由常数、点积和反射向量与视线夹角的幂次计算得到，最终输出三维模型的带高光渲染效果。



---

```cpp
//
// Created by 25190 on 2025/9/26.
//

#include "gl_mine.h"

#include <algorithm>
#include <vector>
#include "geometry.h"
#include "tgaimage.h"

struct TGAImage;
mat<4, 4> ModelView, Viewport, Perspective;
std::vector<double> zbuffer; // depth buffer

// 视口变换矩阵
void init_viewport(const int x, const int y, const int w, const int h)
{
    Viewport = {{{w / 2., 0, 0, x + w / 2.}, {0, h / 2., 0, y + h / 2.}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
}

// 透视投影矩阵 projection matrix (f是焦距, f越大, 视野越窄)
void init_perspective(const double f)
{
    Perspective = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, -1 / f, 1}}};
}

// 视图变换矩阵 ModelView matrix
void lookat(const vec3 eye, const vec3 center, const vec3 up)
{
    vec3 n = normalized(eye - center);
    vec3 l = normalized(cross(up, n));
    vec3 m = normalized(cross(n, l));
    ModelView = mat<4, 4>{{{l.x, l.y, l.z, 0}, {m.x, m.y, m.z, 0}, {n.x, n.y, n.z, 0}, {0, 0, 0, 1}}} *
                mat<4, 4>{{{1, 0, 0, -center.x}, {0, 1, 0, -center.y}, {0, 0, 1, -center.z}, {0, 0, 0, 1}}};
}

void init_zbuffer(const int width, const int height)
{
    zbuffer = std::vector(width * height, -1000.); // 初始化zbuffer设置为负无穷（无限远远）
}

void rasterize(const Triangle &clip, const IShader &shader, TGAImage &framebuffer)
{
    vec4 ndc[3] = {clip[0] / clip[0].w, clip[1] / clip[1].w, clip[2] / clip[2].w}; // normalized device coordinates
    vec2 screen[3] = {(Viewport * ndc[0]).xy(), (Viewport * ndc[1]).xy(), (Viewport * ndc[2]).xy()};
    // screen coordinates

    mat<3, 3> ABC = {{{screen[0].x, screen[0].y, 1.}, {screen[1].x, screen[1].y, 1.}, {screen[2].x, screen[2].y, 1.}}};
    if (ABC.det() < 1) return; // backface culling + discarding triangles that cover less than a pixel
    // 三角形面积：1/2*det(ABC)

    auto [bbminx,bbmaxx] = std::minmax({screen[0].x, screen[1].x, screen[2].x}); // bounding box for the triangle
    auto [bbminy,bbmaxy] = std::minmax({screen[0].y, screen[1].y, screen[2].y});
    // defined by its top left and bottom right corners
#pragma omp parallel for
    for (int x = std::max<int>(bbminx, 0); x <= std::min<int>(bbmaxx, framebuffer.width() - 1); x++)
    {
        // clip the bounding box by the screen
        for (int y = std::max<int>(bbminy, 0); y <= std::min<int>(bbmaxy, framebuffer.height() - 1); y++)
        {
            vec3 bc = ABC.invert_transpose() * vec3{static_cast<double>(x), static_cast<double>(y), 1.};
            // barycentric coordinates of {x,y} w.r.t the triangle
            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;
            // negative barycentric coordinate => the pixel is outside the triangle

            double z = bc * vec3{ndc[0].z, ndc[1].z, ndc[2].z}; // linear interpolation of the depth
            if (z <= zbuffer[x + y * framebuffer.width()]) continue;
            // discard fragments that are too deep w.r.t the z-buffer
            auto [discard, color] = shader.fragment(bc);
            if (discard) continue; // fragment shader can discard current fragment
            zbuffer[x + y * framebuffer.width()] = z; // update the z-buffer
            framebuffer.set(x, y, color); // update the framebuffer
        }
    }
}

```

---

这一段代码里面的 **深度计算方法** 很关键，实际上就是软件实现了 GPU 光栅化管线里的 **透视除法 + 重心坐标插值 + Z-buffer 深度测试**。我来帮你逐行拆解：

------

## 1. 顶点经过裁剪空间 → NDC

```cpp
vec4 ndc[3] = {clip[0] / clip[0].w, clip[1] / clip[1].w, clip[2] / clip[2].w}; 
```

- `clip[i]` 是裁剪空间 (Clip Space) 坐标，即投影矩阵乘过后的结果。
- `/ clip[0].w` 是 **透视除法**，得到 **归一化设备坐标 (NDC)**。
- 所以 `ndc[i] = (x_ndc, y_ndc, z_ndc, 1)`，范围一般在 `[-1, 1]`。

------

## 2. 转换到屏幕坐标

```cpp
vec2 screen[3] = {(Viewport * ndc[0]).xy(), (Viewport * ndc[1]).xy(), (Viewport * ndc[2]).xy()};
```

- 通过 `Viewport` 矩阵，把 `[-1,1]` 的坐标映射到屏幕像素坐标 `[0,width] × [0,height]`。
- 这里只取了 `x,y` 用来判断像素是否落在三角形里面。

------

## 3. 计算重心坐标 (Barycentric Coordinates)

```cpp
vec3 bc = ABC.invert_transpose() * vec3{static_cast<double>(x), static_cast<double>(y), 1.};
```

- `ABC` 是三角形的 2D 坐标矩阵：

  ```
  | x0 y0 1 |
  | x1 y1 1 |
  | x2 y2 1 |
  ```

- `bc = (α, β, γ)` 就是点 `(x,y)` 相对于三角形的 **重心坐标**。

- 性质：
  $$
  P = \alpha A + \beta B + \gamma C, \quad \alpha + \beta + \gamma = 1
  $$
  如果 α,β,γ 都 ≥ 0，则点在三角形内部。

------

## 4. 插值计算深度

```cpp
double z = bc * vec3{ndc[0].z, ndc[1].z, ndc[2].z};
```

这里是核心部分：

- `ndc[0].z, ndc[1].z, ndc[2].z` 分别是三个顶点的 **深度值**（在 NDC 空间下）。

- `bc` 就是对应的重心系数 `(α, β, γ)`。

- 所以：
  $$
  z = \alpha z_0 + \beta z_1 + \gamma z_2
  $$

- 这就是对三角形顶点深度值的 **线性插值**，得到当前像素的深度。

⚠️ 注意：

- 在真正的 GPU 里，由于透视投影，重心插值并不是严格线性的，而是 **透视校正插值 (perspective-correct interpolation)**。
- 这里的实现是直接 NDC 下的线性插值（省略了 w 分量校正），所以是简化版。

------

## 5. Z-buffer 深度测试

```cpp
if (z <= zbuffer[x + y * framebuffer.width()]) continue;
```

- 如果当前像素的 z 小于等于 zbuffer 里存的值，说明它更远 → 舍弃。
- 如果更近，则更新：

```cpp
zbuffer[x + y * framebuffer.width()] = z;
```

------

## 6. 总结流程

1. **顶点投影** → 得到 `ndc`，含有 z 深度信息。
2. **像素重心坐标** → 计算出 `(α, β, γ)`。
3. **插值深度** → `z = αz0 + βz1 + γz2`。
4. **深度测试** → 比较 z 与 zbuffer 中的值，决定是否绘制。

------

## 7. 类比 GPU 的做法

- GPU 硬件里，z 值通常是 **透视校正插值**：
  $$
  z = \frac{\alpha z_0/w_0 + \beta z_1/w_1 + \gamma z_2/w_2}{\alpha /w_0 + \beta /w_1 + \gamma /w_2}
  $$
  这里的实现只取了 `ndc.z`，所以相当于 **已提前做了除法**，因此可以直接线性插值。

------

**一句话总结**：
 代码里深度的计算是用 **重心坐标 (barycentric coordinates)** 对三角形三个顶点的 NDC.z 值做线性插值，得到像素的深度值，再与 z-buffer 比较，实现了“谁近谁显示”的遮挡消隐。

---

## 1. 数学上：Inverse 和 Transpose

- **逆矩阵 (inverse)**：
   如果 MM 是可逆矩阵，那么
  $$
  M^{-1} M = I
  $$

- **转置 (transpose)**：
   矩阵转置就是行列互换，记作 $M^T$。

- **逆转置 (inverse transpose)**：
  $$
  (M^{-1})^T = (M^T)^{-1}
  $$
  这是个数学恒等式。

所以 `invert_transpose()` 按字面理解就是 **先求逆，再取转置**。

------

## 2. 在你的代码里的含义

在这段代码：

```cpp
vec3 bc = ABC.invert_transpose() * vec3{x, y, 1.};
```

这里的 `ABC` 是三角形顶点坐标矩阵：
$$
ABC =  \begin{bmatrix} x_0 & y_0 & 1 \\ x_1 & y_1 & 1 \\ x_2 & y_2 & 1 \end{bmatrix}
$$
我们要求解$(\alpha, \beta, \gamma)$，使得：
$$
\alpha (x_0,y_0,1) + \beta (x_1,y_1,1) + \gamma (x_2,y_2,1) = (x,y,1)
$$
写成矩阵形式就是：
$$
ABC^T \cdot  \begin{bmatrix} \alpha \\ \beta \\ \gamma \end{bmatrix} = \begin{bmatrix} x \\ y \\ 1 \end{bmatrix}
$$
因此：
$$
\begin{bmatrix} \alpha \\ \beta \\ \gamma \end{bmatrix} = (ABC^T)^{-1}  \cdot  \begin{bmatrix} x \\ y \\ 1 \end{bmatrix}
$$

------

## 3. 为什么用 `invert_transpose`

- **正常写法**应该是 $(ABC^T)^{-1}$。

- 由于恒等式：

  $(ABC^T)^{-1} = (ABC^{-1})^T$

- 所以代码里直接写成 `ABC.invert_transpose()`，等价于“先求逆再转置”，结果一样。

换句话说：

🔹 这里的 `invert_transpose()` 不是为了法线变换（图形学里确实有用到 normal matrix = modelview 的 inverse transpose），
 而是一个编程上的 **小技巧**，用来解重心坐标的线性方程组。

------

## 4. 类比总结

- 数学意义：
   `invert_transpose()` = $(M^{-1})^T$，也等价于 $(M^T)^{-1}$。
- 在这段代码里：
   它用来快速算出 **重心系数 (barycentric coordinates)**。
- 在光照/法线变换中：
   “inverse transpose” 通常用来修正非正交矩阵对法线的影响。

------

✅ **一句话总结**：
 `invert_transpose()` 在这里就是“矩阵先取逆再转置”，因为解重心坐标需要用到 $(ABC^T)^{-1}$，而编程里写成 `invert_transpose()` 更方便。
