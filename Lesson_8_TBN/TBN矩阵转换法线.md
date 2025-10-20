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

## 1. ΪʲôҪ�� TBN

�� Blinn-Phong ��������������ͼ�Ĺ���ģ���У�**������ͼ��ķ��������Ǵ洢�����߿ռ� (Tangent Space)** �ģ�������������ռ�����ۿռ䡣

���߿ռ�����������������ǣ�

* **T (Tangent)** ���߷���
* **B (Bitangent �� Binormal)** �����߷���
* **N (Normal)** ���߷���

�������������������ܴ����߿ռ�ת��������/�ۿռ䡣������ƬԪ��ɫ�׶Σ����ܰ���ͼ��ķ�����ȷӦ�õ����ռ����С�

---

## 2. �����е� TBN �Ƶ�

����� `fragment` �

```cpp
// TBN matrix
mat<2, 4> E = {tri[1] - tri[0], tri[2] - tri[0]};  
mat<2, 2> U = {varying_uv[1] - varying_uv[0], varying_uv[2] - varying_uv[0]};  
mat<2, 4> T = U.invert() * E;
```

������߼��ǣ�

1. **E**�����������������������ӿռ��У�

    * `tri[1] - tri[0]`
    * `tri[2] - tri[0]`

   �������������������ڿռ���ıߡ�

2. **U**��UV �����

    * `uv1 - uv0`
    * `uv2 - uv0`

   ����ʾ���������������������߷����ϵĲ�ֵ��

3. **�����Է���������/������**
   ��ʽ��Դ�ǰѿռ������ **E** �� UV ��ֵ **U** �������Թ�ϵ��

   ```
   E = U * [T, B]^T
   ```

   ���õ���

   ```
   [T, B]^T = U^-1 * E
   ```

   ���� `T[0]` ���� **��������**��`T[1]` ���� **����������**��

---

## 3. ���� Darboux Frame (TBN ����)

```cpp
mat<4, 4> D = { normalized(T[0]),     // Tangent
                normalized(T[1]),     // Bitangent
                normalized(varying_nrm[0] * bar[0] +
                           varying_nrm[1] * bar[1] +
                           varying_nrm[2] * bar[2]), // Normal
                {0, 0, 0, 1} };
```

* **T**������
* **B**��������
* **N**����ֵ��Ķ��㷨�ߣ��ٹ�һ����
* �������γ�һ���ֲ�����ϵ��Darboux Frame�������������߿ռ���ķ��߱任���ӿռ䡣

---

## 4. Ӧ�� TBN

```cpp
vec2 uv = varying_uv[0] * bar.x + varying_uv[1] * bar.y + varying_uv[2] * bar.z;
vec4 n = normalized(D.transpose() * model.normal(uv));
```

* `model.normal(uv)` ���� **������ͼ**�������ķ��ߣ��������߿ռ�ġ�
* `D.transpose()` �������߿ռ� �� �ӿռ䡣

  > ע�������� `D.transpose()` ������ `D`����Ϊ��������������ʽ�洢�ģ�����ת�õȼ��ڰ� `[T, B, N]` ��������������

�õ��� `n` ���������� **�ۿռ���ķ���**��

---

## 5. ��������

```cpp
vec4 h = normalized(l + eye); // �������
double ambient = .5;
double diff = std::max(0., n * l);
double spec = std::pow(std::max(n * h, 0.), 70);
```

* **diff** = �������ȡ `max(0, n��l)`
* **spec** = �߹���� `n��h`��Blinn-Phong��
* **ambient** = �����ⳣ��

�����ǵ��Ӻ��ٳ�����������ͼ��������ͼ���͵õ�������������ɫ��

---

## 6. �ܽ�һ�仰

��δ���� TBN �߼�����������ǣ�
**ͨ�� UV �����������α�������������/�����ߣ��ټ��ϲ�ֵ������� TBN ���󣬰ѷ�����ͼ������߿ռ䷨��ת�����ۿռ���� Blinn-Phong ���ռ��㡣**
