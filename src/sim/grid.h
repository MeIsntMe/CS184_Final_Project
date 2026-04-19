#pragma once
#include <vector>

enum CellType { FLUID, AIR, SOLID };

struct MACGrid {
    int nx, ny, nz;
    float dx;

    std::vector<float> vel_u;
    std::vector<float> vel_v;
    std::vector<float> vel_w;
    std::vector<float> pressure;
    std::vector<CellType> cell_type;

    MACGrid(int nx, int ny, int nz, float dx)
        : nx(nx), ny(ny), nz(nz), dx(dx),
        vel_u((nx + 1)* ny* nz, 0.f),
        vel_v(nx* (ny + 1)* nz, 0.f),
        vel_w(nx* ny* (nz + 1), 0.f),
        pressure(nx* ny* nz, 0.f),
        cell_type(nx* ny* nz, AIR)
    {
    }

    int idx(int x, int y, int z) const {
        return x + nx * (y + ny * z);
    }
};