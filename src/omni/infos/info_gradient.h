#pragma once

#include "omni/info.h"

namespace spade::omni
{
    namespace info
    {
        struct gradient : public info_base<gradient> // floating-point types only, and need to be able to specify the order-of-accuracy later
        {

            constexpr static bool requires_direction = false;
            
            template <typename array_t, const grid::array_centering center>
            using array_data_type
            = ctrs::array<typename array_t::alias_type, array_t::grid_type::coord_point_type::size()>;
            
            template <typename array_t, typename index_t>
            requires((index_t::centering_type() == grid::face_centered) && (grid::cell_centered == array_t::centering_type()))
            static void compute(
                const array_t& array,
                const index_t& idx,
                array_data_type<array_t, index_t::centering_type()>& out)
            {
                const auto& ar_grid = array.get_grid();
                using grid_t = utils::remove_all<decltype(ar_grid)>::type;
                const int idir0 = idx.dir();
                const ctrs::array<int,3> idir(idir0, (idir0+1)%ar_grid.dim(), (idir0+2)%ar_grid.dim());
                const ctrs::array<typename grid_t::coord_type, 3> invdx
                (
                    1.0/ar_grid.get_dx(0), //todo: update with block index
                    1.0/ar_grid.get_dx(1),
                    1.0/ar_grid.get_dx(2)
                );
                grid::cell_idx_t ic = grid::face_to_cell(idx, 0);
                
                out = 0.0;
                auto apply_coeff_at = [&](const int& iset, const typename array_t::value_type& coeff, const grid::cell_idx_t& idx)
                {
                    auto q = array.get_elem(ic);
                    for (std::size_t i = 0; i < out[iset].size(); ++i) out[iset][i] += coeff*q[i]*invdx[iset];
                };
                
                apply_coeff_at(idir[0],  -1.0, ic);
                for (int ii = 1; ii < grid_t::dim(); ++ii)
                {
                    ic[idir[ii]] += 1;
                    apply_coeff_at(idir[ii],  0.25, ic);
                    ic[idir[ii]] -= 2;
                    apply_coeff_at(idir[ii], -0.25, ic);
                    ic[idir[ii]] += 1;
                }
                ic[idir[0]] += 1;
                apply_coeff_at(idir[0],   1.0, ic);
                for (int ii = 1; ii < grid_t::dim(); ++ii)
                {
                    ic[idir[ii]] += 1;
                    apply_coeff_at(idir[ii],  0.25, ic);
                    ic[idir[ii]] -= 2;
                    apply_coeff_at(idir[ii], -0.25, ic);
                    ic[idir[ii]] += 1;
                }
                using real_type = typename array_t::value_type;
                static_assert(std::same_as<typename utils::remove_all<decltype(ar_grid.get_coord_sys())>::type, coords::identity<real_type>>, "gradient transformation required for general coordinates");
                // const auto& x_face = ar_grid.get_comp_coords(iface);
                // transform_gradient(ar_grid, iface, out);
            }
        };
    }
}