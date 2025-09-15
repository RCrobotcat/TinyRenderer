- `main.cpp` (除了main.cpp有变化，其他文件和Lesson_1完全一样)
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
void line(int ax, int ay, int bx, int by, TGAImage &framebuffer, TGAColor color) {
    bool steep = std::abs(ax - bx) < std::abs(ay - by);
    if (steep) {
        // if the line is steep, we transpose the image
        std::swap(ax, ay);
        std::swap(bx, by);
    }
    if (ax > bx) {
        // make it left?to?right
        std::swap(ax, bx);
        std::swap(ay, by);
    }
    int y = ay;
    int ierror = 0;
    for (int x = ax; x <= bx; x++) {
        if (steep) // if transposed, de?transpose
            framebuffer.set(y, x, color);
        else
            framebuffer.set(x, y, color);
        ierror += 2 * std::abs(by - ay);
        if (ierror > bx - ax) {
            y += by > ay ? 1 : -1;
            ierror -= 2 * (bx - ax);
        }
    }
}

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

// 把三维模型的顶点坐标转换为屏幕上的像素点位置 (视口变换 => NDC to screen space)
std::tuple<int, int> project(vec3 v) {
    // First of all, (x,y) is an orthogonal projection of the vector (x,y,z).
    return {
            (v.x + 1.) * width / 2,
            // Second, since the input models are scaled to have fit in the [-1,1]^3 world coordinates,
            (v.y + 1.) * height / 2
    }; // we want to shift the vector (x,y) and then scale it to span the entire screen.
}

int main() {
    Model model("../Obj/african_head.obj");
    TGAImage framebuffer(width, height, TGAImage::RGB);

    for (int i = 0; i < model.nfaces(); i++) { // iterate through all triangles
//        auto [ax, ay] = project(model.vert(i, 0));
//        auto [bx, by] = project(model.vert(i, 1));
//        auto [cx, cy] = project(model.vert(i, 2));
        int ax, ay, bx, by, cx, cy;
        std::tie(ax, ay) = project(model.vert(i, 0));
        std::tie(bx, by) = project(model.vert(i, 1));
        std::tie(cx, cy) = project(model.vert(i, 2));

        TGAColor rnd;
        for (int c = 0; c < 3; c++) rnd[c] = std::rand() % 255; // random color
        // draw the triangle
        triangle(ax, ay, bx, by, cx, cy, framebuffer, rnd);
    }

    framebuffer.write_tga_file("framebuffer.tga");
    return 0;
}

```