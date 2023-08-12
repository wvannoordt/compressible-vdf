#pragma once

#include <concepts>

#include "grid/grid.h"
#include "omni/omni.h"

namespace spade::pde_algs
{
    template <typename flux_func_t> concept is_flux_functor =
    requires(const flux_func_t& f)
    {
        //TODO
        f;
    };
    
    template <typename arr_t, typename flux_eval_t> concept has_flux_compatibility =
    requires(const arr_t& t0, const flux_eval_t& t1)
    {
        //TODO
        t0;
    };

    enum block_flux_policy
    {
        block_flux_all,
        block_flux_interior
    };

    template <
        grid::multiblock_array sol_arr_t,
        grid::multiblock_array rhs_arr_t,
        typename... flux_funcs_t>
    requires
        grid::has_centering_type<sol_arr_t, grid::cell_centered>
    static void flux_div(
        const sol_arr_t& prims,
        rhs_arr_t& rhs,
        const bound_box_t<bool,sol_arr_t::grid_type::dim()>& domain_boundary_flux,
        const flux_funcs_t&... flux_funcs)
    {
        using real_type  = sol_arr_t::value_type;
        using alias_type = sol_arr_t::alias_type;
        const grid::multiblock_grid auto& ar_grid = prims.get_grid();
        
        const auto prims_img = prims.image();
        auto rhs_img   = rhs.image();

        auto block_range = range(0,ar_grid.get_num_local_blocks());

        using omni_union_type = omni::combine_omni_stencils<flux_funcs_t...>;

        for (auto lb_idir: block_range*range(0, ar_grid.dim()))
        {
            int  lb_loc  = lb_idir[0];
            auto lb_glob = ar_grid.get_partition().to_global(utils::tag[partition::local](lb_loc));
            int idir = lb_idir[1];
            bound_box_t<int,3> flux_bounds;
            for (auto d:range(0,3))
            {
                flux_bounds.min(d) = (ar_grid.dim()==2 && d==2)?0:-1;
                flux_bounds.max(d) = ar_grid.get_num_cells(d);
            }
            const auto& iboundary = ar_grid.is_domain_boundary(lb_glob);
            if (iboundary.min(idir) && !domain_boundary_flux.min(idir)) flux_bounds.min(idir) = 0;
            if (iboundary.max(idir) && !domain_boundary_flux.max(idir)) flux_bounds.max(idir) = ar_grid.get_num_cells(idir) - 1;
            auto g0 = range(flux_bounds.min(0),flux_bounds.max(0));
            auto g1 = range(flux_bounds.min(1),flux_bounds.max(1));
            auto g2 = range(flux_bounds.min(2),flux_bounds.max(2));

            auto grid_range = g0*g1*g2;
            for (auto idx: grid_range)
            {
                auto lb_loc_des = utils::tag[partition::local](lb_loc);
                const real_type dx = ar_grid.get_dx(idir, lb_loc_des);
                grid::cell_idx_t il(idx[0_c], idx[1_c], idx[2_c], lb_loc);
                grid::cell_idx_t ir(idx[0_c], idx[1_c], idx[2_c], lb_loc);
                ir[idir] += 1;
                grid::face_idx_t iface = grid::cell_to_face(il, idir, 1);
                const auto xyz_comp_l = ar_grid.get_comp_coords(il);
                const auto xyz_comp_r = ar_grid.get_comp_coords(ir);
                const real_type jac_l = coords::calc_jacobian(ar_grid.coord_sys(), xyz_comp_l, il);
                const real_type jac_r = coords::calc_jacobian(ar_grid.coord_sys(), xyz_comp_r, ir);
                
                using omni_type = omni_union_type;
                using data_type = omni::stencil_data_t<omni_type, sol_arr_t>;
                data_type data;
                omni::retrieve(ar_grid, prims_img, iface, data);
                using flux_out_t = rhs_arr_t::alias_type;
                flux_out_t accum(0.0);
                utils::foreach_param([&](const auto& flux_func)
                {   
                    using flux_func_t = decltype(flux_func);
                    using omni_type   = utils::remove_all<flux_func_t>::type::omni_type;

                    auto flux_data = omni::interpret_stencil<omni_type>(data);
                    accum += flux_func(flux_data);
                    
                }, flux_funcs...);

                //todo: fix this garbage
                if constexpr (ctrs::basic_array<alias_type>)
                {
                    for (int n = 0; n < accum.size(); ++n)
                    {
                        rhs_img(n, il) -= jac_l*accum[n]/(dx);
                        rhs_img(n, ir) += jac_r*accum[n]/(dx);
                    }
                }
                else
                {
                    rhs_img(il) -= jac_l*accum/(dx);
                    rhs_img(ir) += jac_r*accum/(dx);
                }
            }
        }
    }

        template <
        grid::multiblock_array sol_arr_t,
        grid::multiblock_array rhs_arr_t,
        typename... flux_funcs_t>
    requires
        grid::has_centering_type<sol_arr_t, grid::cell_centered>
    static void flux_div(
        const sol_arr_t& prims,
        rhs_arr_t& rhs,
        const flux_funcs_t&... flux_funcs)
    {
        const std::size_t dim = sol_arr_t::grid_type::dim();
        bound_box_t<bool,dim> domain_boundary_flux;
        for (auto i: range(0,dim))
        {
            domain_boundary_flux.max(i) = true;
            domain_boundary_flux.min(i) = true;
        }
        flux_div(prims, rhs, domain_boundary_flux, flux_funcs...);
    }
}