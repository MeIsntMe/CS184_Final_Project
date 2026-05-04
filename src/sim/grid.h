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

    // device so its callable by GPU
    __host__ __device__ int idx(int x, int y, int z) const {
        return x + nx * (y + ny * z);
    }
};

#include <thrust/device_vector.h>
class MACGrid{
public:
    int nx, ny, nz;
    float dx;

    // Device Vectors (GPU VRAM)
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

    // Helper to easily pack the pointers for the kernel
    DeviceMACGrid get_device_grid();
};
