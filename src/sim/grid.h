#pragma once
#include <vector>
#include <algorithm>

#ifndef __CUDACC__
#define __device__
#define __host__
#endif

enum CellType { FLUID, AIR, SOLID };

struct DeviceMACGrid {
    int nx, ny, nz;
    float dx;

    float* vel_u;
    float* vel_v;
    float* vel_w;

    // Pre-pressure-solve snapshot for FLIP
    float* vel_u_old;
    float* vel_v_old;
    float* vel_w_old;

    float* weight_u;
    float* weight_v;
    float* weight_w;

    float* pressure;
    float* pressure_tmp;
    float* divergence;
    CellType* cell_type;

    __host__ __device__ int idx(int x, int y, int z) const {
        return x + nx * (y + ny * z);
    }
};

#ifdef USE_CUDA

#include <thrust/device_vector.h>
class MACGrid {
public:
    int nx, ny, nz;
    float dx;

    thrust::device_vector<float> d_vel_u;
    thrust::device_vector<float> d_vel_v;
    thrust::device_vector<float> d_vel_w;

    // Pre-pressure-solve snapshot for FLIP
    thrust::device_vector<float> d_vel_u_old;
    thrust::device_vector<float> d_vel_v_old;
    thrust::device_vector<float> d_vel_w_old;

    thrust::device_vector<float> d_weight_u;
    thrust::device_vector<float> d_weight_v;
    thrust::device_vector<float> d_weight_w;

    thrust::device_vector<float> d_pressure;
    thrust::device_vector<float> d_pressure_tmp;
    thrust::device_vector<float> d_divergence;
    thrust::device_vector<CellType> d_cell_type;

    MACGrid(int nx, int ny, int nz, float dx);
    void clear();
    DeviceMACGrid get_device_grid();
};

#else

// CPU-only version for builds without CUDA
class MACGrid {
public:
    int nx, ny, nz;
    float dx;

    std::vector<float> d_vel_u;
    std::vector<float> d_vel_v;
    std::vector<float> d_vel_w;

    std::vector<float> d_weight_u;
    std::vector<float> d_weight_v;
    std::vector<float> d_weight_w;

    std::vector<float> d_pressure;
    std::vector<CellType> d_cell_type;

    MACGrid(int nx, int ny, int nz, float dx)
        : nx(nx), ny(ny), nz(nz), dx(dx),
        d_vel_u((nx + 1)* ny* nz, 0.f),
        d_vel_v(nx* (ny + 1)* nz, 0.f),
        d_vel_w(nx* ny* (nz + 1), 0.f),
        d_weight_u((nx + 1)* ny* nz, 0.f),
        d_weight_v(nx* (ny + 1)* nz, 0.f),
        d_weight_w(nx* ny* (nz + 1), 0.f),
        d_pressure(nx* ny* nz, 0.f),
        d_cell_type(nx* ny* nz, AIR)
    {
    }

    void clear() {
        std::fill(d_vel_u.begin(), d_vel_u.end(), 0.f);
        std::fill(d_vel_v.begin(), d_vel_v.end(), 0.f);
        std::fill(d_vel_w.begin(), d_vel_w.end(), 0.f);
        std::fill(d_weight_u.begin(), d_weight_u.end(), 0.f);
        std::fill(d_weight_v.begin(), d_weight_v.end(), 0.f);
        std::fill(d_weight_w.begin(), d_weight_w.end(), 0.f);
    }
};

#endif