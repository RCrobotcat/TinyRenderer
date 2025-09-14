//
// Created by 25190 on 2025/9/14.
//

#ifndef GEOMETRY_H
#define GEOMETRY_H

#pragma once
#include <cmath>
#include <cassert>
#include <iostream>

// 模板类，2d向量，用法是Vec2<int>(2d整形向量)、Vec2<float>(2d浮点数向量)
template<int n>
struct vec
{
    double data[n] = {0};

    double &operator[](const int i)
    {
        assert(i>=0 && i<n);
        return data[i];
    }

    double operator[](const int i) const
    {
        assert(i>=0 && i<n);
        return data[i];
    }
};

template<int n>
std::ostream &operator<<(std::ostream &out, const vec<n> &v)
{
    for (int i = 0; i < n; i++) out << v[i] << " ";
    return out;
}

// 模板类，3d向量，用法是Vec3<int>(3d整形向量)、Vec3<float>(3d浮点数向量)
template<>
struct vec<3>
{
    double x = 0, y = 0, z = 0;

    double &operator[](const int i)
    {
        assert(i>=0 && i<3);
        return i ? (1 == i ? y : z) : x;
    }

    double operator[](const int i) const
    {
        assert(i>=0 && i<3);
        return i ? (1 == i ? y : z) : x;
    }
};

typedef vec<3> vec3;


#endif //GEOMETRY_H
