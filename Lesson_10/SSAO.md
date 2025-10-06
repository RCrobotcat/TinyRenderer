```cpp
#include <random>
#include <algorithm>

#include "gl_mine.h"
#include "model.h"

extern mat<4, 4> Viewport, ModelView, Perspective; // "OpenGL" state matrices and
extern std::vector<double> zbuffer;     // the depth buffer

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

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " obj/model.obj" << std::endl;
        return 1;
    }

    constexpr int width = 800;      // output image size
    constexpr int height = 800;
    constexpr vec3 eye{-1, 0, 2}; // camera position
    constexpr vec3 center{0, 0, 0}; // camera direction
    constexpr vec3 up{0, 1, 0}; // camera up vector

    // usual rendering pass
    lookat(eye, center, up);
    init_perspective(norm(eye - center));
    init_viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8);
    init_zbuffer(width, height);
    TGAImage framebuffer(width, height, TGAImage::RGB, {177, 195, 209, 255});

    Model model("../Obj/diablo3_pose.obj");
    BlankShader shader{model};
    for (int f = 0; f < model.nfaces(); f++) {      // iterate through all facets
        Triangle clip = {shader.vertex(f, 0),  // assemble the primitive
                         shader.vertex(f, 1),
                         shader.vertex(f, 2)};
        rasterize(clip, shader, framebuffer);   // rasterize the primitive
    }

    constexpr double ao_radius = .1;  // ssao ball radius in normalized device coordinates
    constexpr int nsamples = 128;     // number of samples in the ball
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(-ao_radius, ao_radius);
    auto smoothstep = [](double edge0, double edge1,
                         double x) {         // smoothstep returns 0 if the input is less than the left edge,
        double t = std::clamp((x - edge0) / (edge1 - edge0), 0., 1.);  // 1 if the input is greater than the right edge,
        return t * t * (3 - 2 * t);
        // Hermite interpolation inbetween. The derivative of the smoothstep function is zero at both edges.
    };

#pragma omp parallel for
    for (int x = 0; x < width; x++) {
        for (int y = 0; y < height; y++) {
            double z = zbuffer[x + y * width];
            if (z < -100) continue;
            vec4 fragment = Viewport.invert() * vec4{static_cast<double>(x),
                                                     static_cast<double>(y), z,
                                                     1.}; // for each fragment in the framebuffer
            double vote = 0;
            double voters = 0;
            for (int i = 0; i < nsamples; i++) {
                // compute a very rough approximation of the solid angle
                vec4 p = Viewport * (fragment + vec4{dist(gen), dist(gen), dist(gen), 0.});
                if (p.x < 0 || p.x >= width || p.y < 0 || p.y >= height) continue;
                double d = zbuffer[int(p.x) + int(p.y) * width];
                if (z + 5 * ao_radius < d) continue; // range check to remove the dark halo
                voters++;
                vote += d > p.z;
            }
            double ssao = smoothstep(0, 1, 1 - vote / voters * .4);
            TGAColor c = framebuffer.get(x, y);
            framebuffer.set(x, y, {static_cast<uint8_t>(c[0] * ssao),
                                   static_cast<uint8_t>(c[1] * ssao),
                                   static_cast<uint8_t>(c[2] * ssao), c[3]});
        }
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

```

这段代码其实是一个完整的 **基于深度缓冲的 SSAO（Screen Space Ambient Occlusion）实现**。
 下面我会按照执行顺序、数学原理、坐标系方向（你的是 **z 越大越近**）逐步解释，让你能完整掌握整个算法的意义与实现逻辑。

------

# 一、代码总体结构

这段程序主要分为两个阶段：

| 阶段                 | 功能                                                         |
| -------------------- | ------------------------------------------------------------ |
| **1. 正常渲染 pass** | 渲染模型并生成颜色帧缓冲（`framebuffer`）和深度缓冲（`zbuffer`） |
| **2. SSAO pass**     | 仅使用 `zbuffer`（屏幕空间深度）估算每个像素的环境光遮蔽程度，然后让被遮挡区域变暗 |

输出结果是一张带有自然阴影的灰度或彩色图像，阴影集中在几何体的缝隙、接触面和凹陷处。

------

# 二、前半部分：标准渲染阶段

```cpp
lookat(eye, center, up);
init_perspective(norm(eye - center));
init_viewport(width / 16, height / 16, width * 7 / 8, height * 7 / 8);
init_zbuffer(width, height);
```

- 这几行设置了虚拟相机的观察矩阵（`ModelView`）、透视矩阵（`Perspective`）、视口矩阵（`Viewport`）；

- `zbuffer` 初始化为远平面（通常为 -∞ 或很小的负数）；

- 渲染循环：

  ```cpp
  for (each face f in model)
      rasterize(shader.vertex(f,0), shader.vertex(f,1), shader.vertex(f,2))
  ```

  每个三角形经过顶点变换 → 光栅化 → 更新 `framebuffer` 和 `zbuffer`。

结果：

- `framebuffer` 保存颜色；
- `zbuffer` 保存每个像素的深度（⚠️**你的深度定义是：越大越靠近相机**）。

------

# 三、SSAO 的理论原理

SSAO（屏幕空间环境光遮蔽）是一种快速的近似方法，用来估计一个像素被周围几何体“挡住天空”的程度。

直觉：

- 如果某像素附近的其他像素比它**更靠近相机（z 更大）**，说明这个方向上有遮挡；
- 如果周围都是更远的（z 更小），说明该像素暴露在外，没有遮挡。

遮蔽比例越大 → 光线照不进来 → 更暗。

------

# 四、你的 SSAO 实现细节

------

## (1) 设置参数和随机分布

```cpp
constexpr double ao_radius = .1;   // 采样球体半径（NDC空间）
constexpr int nsamples = 128;      // 采样次数
std::uniform_real_distribution<double> dist(-ao_radius, ao_radius);
```

- 每个像素周围采样一个半径为 `0.1` 的球体；
- 用 `dist(gen)` 生成 `[−0.1, 0.1]` 的随机偏移；
- 共采样 128 个随机方向。

这样就能在屏幕空间的局部区域进行“随机探测”。

------

## (2) 反变换到 NDC 空间

```cpp
vec4 fragment = Viewport.invert() * vec4{x, y, z, 1.};
```

- `(x, y, z)` 是屏幕空间坐标；
- `Viewport.invert()` 把它从屏幕空间变回到 **NDC（Normalized Device Coordinates）空间**；
- `fragment` 表示当前像素在三维空间中的近似位置。

------

## (3) 生成随机采样点并投影回屏幕

```cpp
vec4 p = Viewport * (fragment + vec4{dist(gen), dist(gen), dist(gen), 0.});
```

- 在当前片元周围随机生成一个点 `p`（三维偏移向量）；
- 然后乘上 `Viewport`，把它重新映射回屏幕坐标系（以便能查 zbuffer）。

------

## (4) 深度采样与遮挡判断

```cpp
double d = zbuffer[int(p.x) + int(p.y) * width];
if (z + 5 * ao_radius < d) continue;  // 过滤远处差异过大的点
voters++;
vote += d > p.z;
```

逐行解释：

### ✅ 深度方向含义（你的坐标系）

> **z 越大 = 越近相机，z 越小 = 越远。**

### 判断逻辑

1. `d`：从 zbuffer 取出的屏幕深度值 → 表示屏幕上该位置的可见表面距离相机的深浅；
2. `p.z`：采样点的深度；
3. `d > p.z` → 屏幕上有更近的几何体挡住该方向；
   - ✅ 表示“被遮挡”；
4. `z + 5 * ao_radius < d` → 采样点离当前像素太远（深度差太大），跳过；
   - 用来防止黑边“光晕（halo）”效应；
5. `vote` 统计“被遮挡的次数”，`voters` 统计有效样本数。

------

## (5) 计算遮蔽比例并平滑化

```cpp
double ssao = smoothstep(0, 1, 1 - vote / voters * .4);
```

定义：
$$
\text{occlusion} = \frac{\text{vote}}{\text{voters}}
$$
 → 被遮挡的比例。

平滑处理：
$$
SSAO = \text{smoothstep}(0,1,1 - 0.4\times\text{occlusion})
$$
其中 smoothstep：
$$
\text{smoothstep}(a,b,x)=
 \begin{cases}
 0,&x<a\\
 3t^2-2t^3,&a\le x\le b,\ t=\frac{x-a}{b-a}\\
 1,&x>b
 \end{cases}
$$
作用：

- 让亮度变化更平滑；
- 避免“硬边”阴影。

------

## (6) 根据 SSAO 调整像素亮度

```cpp
TGAColor c = framebuffer.get(x, y);
framebuffer.set(x, y, {
    uint8_t(c[0] * ssao),
    uint8_t(c[1] * ssao),
    uint8_t(c[2] * ssao),
    c[3]
});
```

乘上 `ssao` 系数后：

- 若 `ssao` 接近 1 → 明亮；
- 若 `ssao` 小（被遮挡多）→ 变暗。

最终得到的图像包含局部阴影信息，增强立体感。

------

# 五、整体流程总结

| 阶段      | 公式 / 操作                                | 说明                |
| --------- | ------------------------------------------ | ------------------- |
| 渲染 pass | 生成颜色缓冲 + 深度缓冲                    | 得到每个像素的 z 值 |
| 反投影    | ( fragment = V^{-1}(x,y,z) )               | 屏幕点→NDC 空间     |
| 随机采样  | ( p = fragment + \text{rand}(-r,r) )       | 局部随机扰动        |
| 深度比较  | 若 ( d > p_z )，则方向被遮挡               | 统计被遮挡方向      |
| 遮蔽率    | ( \text{AO} = 1 - 0.4\times(vote/voters) ) | 遮挡越多越暗        |
| 平滑处理  | ( SSAO = smoothstep(0,1,AO) )              | 柔化过渡            |
| 调整亮度  | ( C' = C\times SSAO )                      | 应用阴影效果        |

------

# 六、几何直觉（基于 z 越大越近的定义）

下图概念（文字版）：

```
相机
  ↑
  │
  │ z 增大（更近）
  │
  ├── 点 P (当前像素)
  │ \
  │  \
  │   ·  ← 采样点 (p)
  │    \
  │     ●  ← 前方几何体（d>p.z）
  ↓
```

在该方向上：

- 如果存在“前方几何体” → 该方向被遮挡；
- 如果一直通向远处（z 变小） → 该方向开放。

多个方向采样后即可估算 AO。

------

# ✅ 七、总结一句话

> 这段代码通过对每个像素的深度值进行**局部随机采样和深度比较**，统计被遮挡的比例，并通过 `smoothstep` 平滑化，最终让凹陷处或接触处变暗，实现了**基于屏幕空间的环境光遮蔽（SSAO）效果**。
>  在你的实现中，**z 越大表示越靠近相机**，因此判断 `d > p.z` 表示当前方向被遮挡。