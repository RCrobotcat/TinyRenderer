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

// ����������������з���
// ͨ����������������
// Area = 1/2 * |AB x AC|
double signed_triangle_area(int ax, int ay, int bx, int by, int cx, int cy)
{
    return .5 * ((by - ay) * (bx + ax) + (cy - by) * (cx + bx) + (ay - cy) * (ax + cx));
}

// ���ϱ����޳���΢С�������޳��İ汾
void triangle(int ax, int ay, int az, int bx, int by, int bz, int cx, int cy, int cz, TGAImage &zbuffer,
              TGAImage &framebuffer, TGAColor color)
{
    // ����AABB������Χ��
    int bbminx = std::min(std::min(ax, bx), cx); // bounding box for the triangle
    int bbminy = std::min(std::min(ay, by), cy); // defined by its top left and bottom right corners
    int bbmaxx = std::max(std::max(ax, bx), cx);
    int bbmaxy = std::max(std::max(ay, by), cy);
    double total_area = signed_triangle_area(ax, ay, bx, by, cx, cy);
    if (total_area < 1) return; // backface culling + discarding triangles that cover less than a pixel

    // ������Χ���ڵ��������أ��������������ж��Ƿ����������ڲ�������ڣ��ͻ���������أ�����ͺ�����
#pragma omp parallel for
    for (int x = bbminx; x <= bbmaxx; x++)
    {
        for (int y = bbminy; y <= bbmaxy; y++)
        {
            double alpha = signed_triangle_area(x, y, bx, by, cx, cy) / total_area;
            double beta = signed_triangle_area(x, y, cx, cy, ax, ay) / total_area;
            double gamma = signed_triangle_area(x, y, ax, ay, bx, by) / total_area;
            if (alpha < 0 || beta < 0 || gamma < 0) // �������������ⲿ
                continue; // negative barycentric coordinate => the pixel is outside the triangle

            unsigned char z = static_cast<unsigned char>(alpha * az + beta * bz + gamma * cz);
            if (zbuffer.get(x, y)[0] >= z) continue; // z-buffer test
            // zԽ�󣬴���Խ�����۲���

            zbuffer.set(x, y, {z}); // write the z value in the z-buffer
            // {z} uses aggregate initialization(�ۺ����� => û���Զ��幹�캯��, �������б��ʼ��) to create a TGAColor with only the first channel set to z and the rest to 0
            // �����ǻҶ�ͼ(ֻ�е�һ��ͨ������ʾͼƬ�ĻҶ�)������ֻ��Ҫ���õ�һ��ͨ������
            framebuffer.set(x, y, color);
        }
    }
}

// ����άģ�͵Ķ�������ת��Ϊ��Ļ�ϵ����ص�λ�� (�ӿڱ任 => NDC to screen space)
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
