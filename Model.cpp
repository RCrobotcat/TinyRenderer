//
// Created by 25190 on 2025/9/14.
//

#include "Model.h"
#include <fstream>
#include <sstream>

// ���캯�������������.obj�ļ�·��
Model::Model(const std::string filename) {
    std::ifstream in;
    in.open(filename, std::ifstream::in); // ��.obj�ļ�
    if (in.fail()) return;
    std::string line;
    while (!in.eof()) // û�е��ļ�ĩβ�Ļ�
    {
        std::getline(in, line); // ����һ��
        std::istringstream iss(line.c_str());
        char trash;
        if (!line.compare(0, 2, "v ")) // �����һ�е�ǰ�����ַ��ǡ�v ���Ļ��������Ƕ�������
        {
            iss >> trash; // ʡ�Ե�"v "
            vec3 v; // ���붥������
            for (int i: {0, 1, 2}) iss >> v[i];
            verts.push_back(v); // ���붥�㼯
        } else if (!line.compare(0, 3, "vn ")) {
            iss >> trash >> trash;
            vec3 n;
            for (int i: {0, 1, 2}) iss >> n[i];
            norms.push_back(normalized(n));
        } else if (!line.compare(0, 2, "f ")) // �����һ�е�ǰ�����ַ��ǡ�f ���Ļ�����������Ƭ����
        {
            int f, t, n, cnt = 0;
            iss >> trash; // ʡ�Ե�"f "
            // ��ȡ��Ƭ�Ķ���������������������������
            while (iss >> f >> trash >> t >> trash >> n) // ��ȡx/x/x��ʽ
            {
                facet_vrt.push_back(--f); // ֻ���涥��������obj������1��ʼ�����1
                facet_nrm.push_back(--n); // ֻ���淨��������obj������1��ʼ�����1
                cnt++;
            }
            if (3 != cnt) // ����Ƿ�Ϊ������
            {
                std::cerr << "Error: the obj file is supposed to be triangulated" << std::endl;
                return;
            }
        }
    }
    // ���������������
    std::cerr << "# v# " << nverts() << " f# " << nfaces() << std::endl;
}

int Model::nverts() const { return verts.size(); }

int Model::nfaces() const { return facet_vrt.size() / 3; }

vec3 Model::vert(const int i) const {
    return verts[i];
}

vec3 Model::vert(const int iface, const int nthvert) const {
    return verts[facet_vrt[iface * 3 + nthvert]];
}

vec3 Model::normal(const int iface, const int nthvert) const {
    return norms[facet_nrm[iface * 3 + nthvert]];
}
