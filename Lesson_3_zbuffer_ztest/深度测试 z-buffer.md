```cpp
#include <cmath>
#include <tuple>
#include "geometry.h"
#include "model.h"
#include "tgaimage.h"

constexpr int width = 800;
constexpr int height = 800;

constexpr TGAColor white = {255, 255, 255, 255}; // attention, BGRA order
constexpr TGAColor green = {0, 255, 0, 255};
constexpr TGAColor red = {0, 0, 255, 255};
constexpr TGAColor blue = {255, 128, 64, 255};
constexpr TGAColor yellow = {0, 200, 255, 255};

// Bresenham's line algorithm
void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color)
{
    bool steep = std::abs(ax - bx) < std::abs(ay - by);
    if (steep)
    {
        // if the line is steep, we transpose the image
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax > bx)
    {
        // make it left?to?right
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    int y = ay;
    int ierror = 0;
    for (int x = ax; x <= bx; x++)
    {
        if (steep) // if transposed, de?transpose
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
        ierror += 2 * std::abs(by - ay);
        if (ierror > bx - ax)
        {
            y += by > ay ? 1 : -1;
            ierror -= 2 * (bx - ax);
        }
    }
}

// 计算三角形面积（有方向）
// 通过向量叉积计算面积
// Area = 1/2 * |AB x AC|
double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy)
{
    return .5 * ((by - ay) * (bx + ax) + (cy - by) * (cx + bx) + (ay - cy) * (ax + cx));
}

// 加上背面剔除和微小三角形剔除的版本
void triangle(int ax, int ay, int az, int bx, int by, int bz, int cx, int cy, int cz, TGAImage &zbuffer,
              TGAImage &framebuffer, TGAColor color)
{
    // 计算AABB轴对齐包围盒
    int bbminx = std::min(std::min(ax, bx), cx); // bounding box for the triangle
    int bbminy = std::min(std::min(ay, by), cy); // defined by its top left and bottom right corners
    int bbmaxx = std::max(std::max(ax, bx), cx);
    int bbmaxy = std::max(std::max(ay, by), cy);
    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area < 1) return; // backface culling + discarding triangles that cover less than a pixel

    // 遍历包围盒内的所有像素，根据重心坐标判断是否在三角形内部，如果在，就绘制这个像素，否则就忽略它
#pragma omp parallel for
    for (int x = bbminx; x <= bbmaxx; x++)
    {
        for (int y = bbminy; y <= bbmaxy; y++)
        {
            double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            double beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            if (alpha < 0 || beta < 0 || gamma < 0) // 像素在三角形外部
                continue; // negative barycentric coordinate => the pixel is outside the triangle

            unsigned char z = static_cast<unsigned char>(alpha * az + beta * bz + gamma * cz);
            if (zbuffer.get(x, y)[0] >= z) continue; // z-buffer test
            // z越大，代表越靠近观察者

            zbuffer.set(x, y, {z}); // write the z value in the z-buffer
            // {z} uses aggregate initialization(聚合类型 => 没有自定义构造函数, 可以用列表初始化) to create a TGAColor with only the first channel set to z and the rest to 0
            framebuffer.set(x, y, color);
        }
    }
}

// 把三维模型的顶点坐标转换为屏幕上的像素点位置 (视口变换 => NDC to screen space)
std::tuple<int, int, int> project(vec3 v)
{
    // First of all, (x,y) is an orthogonal projection of the vector (x,y,z).
    return {
        (v.x + 1.) * width / 2,
        // Second, since the input models are scaled to have fit in the [-1,1]^3 world coordinates,
        (v.y + 1.) * height / 2,
        (v.z + 1.) * 255. / 2 // z is between -1 and 1
        // with higher z values meaning closer to the camera
    }; // we want to shift the vector (x,y) and then scale it to span the entire screen.
}

int main()
{
    Model model("../Obj/african_head.obj");
    TGAImage framebuffer(width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE); // z-buffer

    for (int i = 0; i < model.nfaces(); i++)
    {
        // iterate through all triangles
        //        auto [ax, ay] = project(model.vert(i, 0));
        //        auto [bx, by] = project(model.vert(i, 1));
        //        auto [cx, cy] = project(model.vert(i, 2));
        int ax, ay, bx, by, cx, cy;
        int az, bz, cz;
        std::tie(ax, ay, az) = project(model.vert(i, 0));
        std::tie(bx, by, bz) = project(model.vert(i, 1));
        std::tie(cx, cy, cz) = project(model.vert(i, 2));

        TGAColor rnd;
        for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255; // random color
        // draw the triangle
        triangle(ax, ay, az, bx, by, bz, cx, cy, cz, zbuffer, framebuffer, rnd);
    }

    framebuffer.write_tga_file("framebuffer.tga");
    zbuffer.write_tga_file("zbuffer.tga");
    return 0;
}

```

# 3D 渲染代码笔记

## 1. 程序目标

这段程序实现了一个简单的 **基于光栅化 (Rasterization)** 的渲染管线，用于加载 `.obj` 模型并通过 **三角形扫描转换 + Z-Buffer 算法** 绘制到帧缓冲区。
最终会输出两张图像：

* **framebuffer.tga**：彩色渲染结果。
* **zbuffer.tga**：深度缓冲可视化结果。

---

## 2. 核心模块

### (1) `line` 函数：Bresenham 直线算法

* 输入：两点 `(ax, ay)` 和 `(bx, by)`。
* 判断是否陡峭 (`steep`)：

    * 如果斜率过大，就交换 x 和 y（转置绘制）。
* 始终保证从左往右绘制，避免重复代码。
* 累积误差 `ierror` 用于决定什么时候调整 y。
* 时间复杂度 O(|x2 - x1|)。

👉 作用：能在像素栅格上高效绘制一条直线。

---

### (2) `signed_triangle_area` 函数：有向三角形面积

* 使用叉积公式计算：

  $$
  Area = \frac{1}{2} | \overrightarrow{AB} \times \overrightarrow{AC} |
  $$
* 返回带符号面积，用于判断三角形方向（顺时针 / 逆时针）。
* 应用：

    * **背面剔除 (Backface Culling)**：如果面积 < 0，说明三角形背对相机，不必绘制。
    * **重心坐标 (Barycentric Coordinates)** 计算时需要用到。

---

### (3) `triangle` 函数：三角形绘制 + Z-Buffer

* **输入**：三角形三个顶点 `(ax, ay, az), (bx, by, bz), (cx, cy, cz)`。
* 步骤：

    1. **计算 AABB（包围盒）**：减少遍历范围，只检查三角形覆盖的像素区域。
    2. **三角形面积 total\_area**：用于 barycentric 坐标归一化。
    3. **遍历像素点**：

        * 计算 barycentric 坐标 `(α, β, γ)`。
        * 若有负数 ⇒ 点在三角形外，跳过。
    4. **深度插值 (z)**：

       $$
       z = \alpha \cdot z_A + \beta \cdot z_B + \gamma \cdot z_C
       $$
    5. **Z-buffer 测试**：

        * 如果 `z` 比 zbuffer 里存的值更大，说明更靠近相机，就更新。
        * 否则丢弃（被遮挡）。
    6. 更新 `framebuffer` 和 `zbuffer`。

👉 实现了最基本的 **遮挡消隐 (Hidden Surface Removal)**。

---

### (4) `project` 函数：视口变换

* 输入：模型顶点 `vec3 v`，范围在 `[-1, 1]`。
* 输出：屏幕坐标 `(x, y)` 和深度 `(z)`。

    * $$
      x_{screen} = \frac{(v.x + 1)}{2} \cdot width
      $$
    * $$
      y_{screen} = \frac{(v.y + 1)}{2} \cdot height
      $$
    * $$
      z_{buffer} = \frac{(v.z + 1)}{2} \cdot 255
      $$
* 映射到屏幕像素坐标，并将 z 压缩到 \[0,255]。

---

### (5) `main` 函数：渲染流程

1. 加载 `.obj` 模型：

   ```cpp
   Model model("../Obj/african_head.obj");
   ```
2. 创建图像缓冲：

    * `framebuffer`：RGB 彩色缓冲。
    * `zbuffer`：灰度图保存深度。
3. 遍历模型所有三角形：

    * 将三维顶点投影到屏幕。
    * 随机生成颜色。
    * 调用 `triangle` 进行光栅化 + Z-Buffer 测试。
4. 写出 `.tga` 文件。

---

## 3. 关键知识点总结

### 光栅化三角形

* 通过重心坐标判断像素是否落在三角形内部。
* 每个像素的深度值由三角形三个顶点插值得到。

### Z-Buffer 原理

* 每个像素存储“最近的深度值”。
* 如果新像素更靠近相机，才会覆盖旧像素。

### 优化点

* 使用 **AABB 裁剪** 避免无效像素。
* 使用 **重心坐标**，方便插值计算。
* 使用 `#pragma omp parallel for` 并行加速三角形扫描。

---

## 4. 渲染结果

* `framebuffer.tga`：带有随机颜色的三角形模型。
* `zbuffer.tga`：每个像素的灰度表示深度，越亮越靠近相机。

---

## 5. 学习收获

1. 了解了 **从模型到屏幕的渲染管线**：

    * 模型空间 → 投影 → 屏幕空间。
2. 掌握了 **Bresenham 算法** 与 **三角形光栅化**。
3. 学会了 **Z-buffer 消隐算法**。
4. 熟悉了 TGA 图像写入，能可视化调试结果。
