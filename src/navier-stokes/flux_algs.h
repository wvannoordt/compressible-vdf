#pragma once

#include "core/grid.h"

#include "navier-stokes/convective.h"

namespace cvdf::flux_algs
{
    template <
        grid::multiblock_array array_t,
        convective::lr_conv_flux_function<fluid_state::prim_t<typename array_t::value_type>> flux_func_t>
    requires
        grid::has_centering_type<array_t, grid::cell_centered> &&
        (dims::rank_eval<typename array_t::array_minor_dim_t>::value==1) &&
        (dims::rank_eval<typename array_t::array_major_dim_t>::value==0)
    static void flux_lr_diff(const array_t& prims, array_t& rhs, const flux_func_t& flux_func)
    {
        typedef typename array_t::value_type real_type;
        const grid::multiblock_grid auto& ar_grid = prims.get_grid();
        const parallel::parallel_group auto& group = ar_grid.group();
        auto grid_range = range(-1,ar_grid.get_num_cells(0))*range(-1,ar_grid.get_num_cells(1))*range(-1,ar_grid.get_num_cells(2))*range(0,ar_grid.get_num_local_blocks());
        int ct = 0;
        for (auto idx: grid_range*range(0, cvdf_dim))
        {
            int idir = idx[4];
            ctrs::array<grid::cell_t<int>, 4> il(idx[0], idx[1], idx[2], idx[3]);
            ctrs::array<grid::cell_t<int>, 4> ir(idx[0], idx[1], idx[2], idx[3]);
            ir[idir] += 1;
            const ctrs::array<real_type,3> xyz_l = ar_grid.get_coords(il[0], il[1], il[2], il[3]);
            const ctrs::array<real_type,3> xyz_r = ar_grid.get_coords(ir[0], ir[1], ir[2], ir[3]);
            ctrs::array<real_type,3> nvec_l = coords::calc_normal_vector(ar_grid.coord_sys(), xyz_l, il, idir);
            ctrs::array<real_type,3> nvec_r = coords::calc_normal_vector(ar_grid.coord_sys(), xyz_r, ir, idir);
            const real_type jac_l = coords::calc_jacobian(ar_grid.coord_sys(), xyz_l, il);
            const real_type jac_r = coords::calc_jacobian(ar_grid.coord_sys(), xyz_r, ir);
            nvec_l /= jac_l;
            nvec_r /= jac_r;
            if (il == ctrs::array<grid::cell_t<int>, 4>(2, 2, 2, 1))
            {
                print("===================================");
                print(il[0],     il[1],     il[2]);
                print(xyz_l[0],  xyz_l[1],  xyz_l[2]);
                print(nvec_l[0], nvec_l[1], nvec_l[2]);
                print(jac_l);
                print("===================================");
                print(ir[0],     ir[1],     ir[2]);
                print(xyz_r[0],  xyz_r[1],  xyz_r[2]);
                print(nvec_r[0], nvec_r[1], nvec_r[2]);
                print(jac_r);
                print("===================================");
                group.pause();
            }
            fluid_state::prim_t<real_type> ql, qr;
            for (int n = 0; n < ql.size(); ++n) ql[n] = prims(n, il[0], il[1], il[2], il[3]);
            for (int n = 0; n < qr.size(); ++n) qr[n] = prims(n, ir[0], ir[1], ir[2], ir[3]);
            fluid_state::flux_t<real_type> flux = flux_func.calc_flux(ql, qr, nvec_l, nvec_r);
            const real_type dx = ar_grid.get_dx(idir);
            for (int n = 0; n < flux.size(); ++n)
            {
                rhs(n, il[0], il[1], il[2], idx[3])-=jac_l*flux[n]/dx;
                rhs(n, ir[0], ir[1], ir[2], idx[3])+=jac_r*flux[n]/dx;
            }
        }
    }
}