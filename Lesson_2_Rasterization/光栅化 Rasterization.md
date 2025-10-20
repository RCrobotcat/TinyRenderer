- 代码：

```cpp
// 计算三角形面积（有方向）
// 通过向量叉积计算面积
// Area = 1/2 * |AB x AC|
double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy) {
    return .5 * ((by - ay) * (bx + ax) + (cy - by) * (cx + bx) + (ay - cy) * (ax + cx));
}

// 加上背面剔除和微小三角形剔除的版本
void triangle(int ax, int ay, int bx, int by, int cx, int cy, TGAImage &framebuffer, TGAColor color) {
    // 计算AABB轴对齐包围盒
    int bbminx = std::min(std::min(ax, bx), cx); // bounding box for the triangle
    int bbminy = std::min(std::min(ay, by), cy); // defined by its top left and bottom right corners
    int bbmaxx = std::max(std::max(ax, bx), cx);
    int bbmaxy = std::max(std::max(ay, by), cy);
    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    if(total_area < 1) return; // backface culling + discarding triangles that cover less than a pixel

    // 遍历包围盒内的所有像素，根据重心坐标判断是否在三角形内部，如果在，就绘制这个像素，否则就忽略它
#pragma omp parallel for
    for (int x = bbminx; x <= bbmaxx; x++) {
        for (int y = bbminy; y <= bbmaxy; y++) {
            double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            double beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            if (alpha < 0 || beta < 0 || gamma < 0) // 像素在三角形外部
                continue; // negative barycentric coordinate => the pixel is outside the triangle
            framebuffer.set(x, y, color);
        }
    }
}

```

------

# 1. 背面剔除 (Backface Culling)

### 原理

- 在 3D 渲染中，如果一个三角形的**朝向背对摄像机**，它在屏幕上是不可见的。
- 我们可以通过**三角形面积的符号**（顺时针/逆时针顺序）来判断其朝向。

### 实现

```cpp
double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
if(total_area < 1) return;
```

- `signed_triangle_area` 计算带符号的面积：
  - 如果点的顺序是 **逆时针**，面积为正；
  - 如果点的顺序是 **顺时针**，面积为负。
- 如果面积为负，说明是背面 → 可以直接丢弃，不再光栅化。

👉 优点：大约能减少 **一半三角形** 的绘制开销，提升性能。

------

# 2. 微小三角形剔除 (Small Triangle Discard)

### 原理

- 有些三角形经过投影后只覆盖不到一个像素（比如面很小或远处的三角形）。
- 这些三角形即使绘制出来，也几乎没有可见贡献。

### 实现

```cpp
if(total_area < 1) return;
```

- 这里 `area` 的单位就是 **像素²**，当 `< 1` 时，说明三角形面积不足 1 个像素 → 直接跳过。

👉 优点：减少无意义的计算，特别是在高多边形模型里能显著加速。

⚠️ 需要注意：上面的 `if (total_area < 1)` 同时承担了“背面剔除 + 微小三角形剔除”的作用，其实更严谨的写法应该是：

```cpp
if (total_area <= 0) return;              // 背面剔除
if (fabs(total_area) < 1) return;         // 微小三角形剔除
```

------

# 3. `#pragma omp parallel for`

### 作用

这是 **OpenMP** 的并行化指令，告诉编译器把接下来的 `for` 循环分配到多个线程上执行。

你的代码里：

```cpp
#pragma omp parallel for
for (int x = bbminx; x <= bbmaxx; x++) {
    for (int y = bbminy; y <= bbmaxy; y++) {
        ...
        framebuffer.set(x, y, color);
    }
}
```

### 工作机制

- **外层循环 (x 循环)** 被多个线程分担：
  - 比如 4 个线程时，线程 0 处理 x=bbminx..x0，线程 1 处理 x0+1..x1，依次类推。
- **好处**：显著利用多核 CPU，加速像素填充过程。
- **注意**：
  - `framebuffer.set(x, y, color);` 必须是线程安全的（每个线程只写自己独立的像素，不会互相覆盖）。
  - OpenMP 自动处理循环分割和线程同步。

------

# ✅ 总结

1. **背面剔除**：利用面积符号判断三角形是否背对摄像机，丢掉不可见的。
2. **微小三角形剔除**：如果面积 < 1 像素²，丢弃，减少无意义计算。
3. **`#pragma omp parallel for`**：并行化外层循环，让不同线程处理不同范围的 x，提高渲染效率。