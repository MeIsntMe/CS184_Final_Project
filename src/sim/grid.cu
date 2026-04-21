// grid.cu
#include "grid.h"
#include <thrust/fill.h>

MACGrid::MACGrid(int nx, int ny, int nz, float dx)
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

void MACGrid::clear() {
    thrust::fill(d_vel_u.begin(), d_vel_u.end(), 0.f);
    thrust::fill(d_vel_v.begin(), d_vel_v.end(), 0.f);
    thrust::fill(d_vel_w.begin(), d_vel_w.end(), 0.f);
    thrust::fill(d_weight_u.begin(), d_weight_u.end(), 0.f);
    thrust::fill(d_weight_v.begin(), d_weight_v.end(), 0.f);
    thrust::fill(d_weight_w.begin(), d_weight_w.end(), 0.f);
}

DeviceMACGrid MACGrid::get_device_grid() {
    DeviceMACGrid dg;
    dg.nx = nx; dg.ny = ny; dg.nz = nz; dg.dx = dx;

    // pack raw GPU pointers into struct
    dg.vel_u = thrust::raw_pointer_cast(d_vel_u.data());
    dg.vel_v = thrust::raw_pointer_cast(d_vel_v.data());
    dg.vel_w = thrust::raw_pointer_cast(d_vel_w.data());

    dg.weight_u = thrust::raw_pointer_cast(d_weight_u.data());
    dg.weight_v = thrust::raw_pointer_cast(d_weight_v.data());
    dg.weight_w = thrust::raw_pointer_cast(d_weight_w.data());

    dg.pressure = thrust::raw_pointer_cast(d_pressure.data());
    dg.cell_type = thrust::raw_pointer_cast(d_cell_type.data());

    return dg;
}