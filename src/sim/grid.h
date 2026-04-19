#pragma once
#include <vector>
#include <algorithm>

enum CellType { FLUID, AIR, SOLID };

struct MACGrid {
    int nx, ny, nz;
    float dx;

    std::vector<float> vel_u;
    std::vector<float> vel_v;
    std::vector<float> vel_w;

    std::vector<float> weight_u;
    std::vector<float> weight_v;
    std::vector<float> weight_w;


    std::vector<float> pressure;
    std::vector<CellType> cell_type;

    MACGrid(int nx, int ny, int nz, float dx)
        : nx(nx), ny(ny), nz(nz), dx(dx),
        vel_u((nx + 1)* ny* nz, 0.f),
        vel_v(nx* (ny + 1)* nz, 0.f),
        vel_w(nx* ny* (nz + 1), 0.f),

        weight_u((nx + 1)* ny* nz, 0.f),  
        weight_v(nx* (ny + 1)* nz, 0.f),  
        weight_w(nx* ny* (nz + 1), 0.f),  

        pressure(nx* ny* nz, 0.f),
        cell_type(nx* ny* nz, AIR)
    {
    }

    int idx(int x, int y, int z) const {
        return x + nx * (y + ny * z);
    }

    void clear() {
        std::fill(vel_u.begin(), vel_u.end(), 0.f);
        std::fill(vel_v.begin(), vel_v.end(), 0.f);
        std::fill(vel_w.begin(), vel_w.end(), 0.f);
        std::fill(weight_u.begin(), weight_u.end(), 0.f);
        std::fill(weight_v.begin(), weight_v.end(), 0.f);
        std::fill(weight_w.begin(), weight_w.end(), 0.f);
    }

};