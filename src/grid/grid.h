#pragma once

#include <vector>
#include <type_traits>

#include "core/config.h"
#include "core/attribs.h"
#include "core/ctrs.h"
#include "core/typedef.h"
#include "core/static_for.h"
#include "core/bounding_box.h"
#include "core/range.h"
#include "core/coord_system.h"
#include "core/parallel.h"
#include "core/static_math.h"
#include "core/tag_val.h"

#include "grid/partition.h"
#include "grid/grid_index_types.h"

namespace spade::grid
{    
    template <class T> concept multiblock_grid = requires(T t, const cell_idx_t& i_c, const face_idx_t& i_f, const node_idx_t& i_n)
    {
        // todo: write this
        { t.get_coords(i_c) } -> ctrs::basic_array;
        { t.get_coords(i_f) } -> ctrs::basic_array;
        { t.get_coords(i_n) } -> ctrs::basic_array;
        { t.get_comp_coords(i_c) } -> ctrs::basic_array;
        { t.get_comp_coords(i_f) } -> ctrs::basic_array;
        { t.get_comp_coords(i_n) } -> ctrs::basic_array;
    };
    
    template <class T, const array_centering ct> concept has_centering_type = (T::centering_type() == ct);
    
    template <class T> concept multiblock_array = requires(T t, int a, int i, int j, int k, int lb, int b)
    {
        t;
        t.var_map();
        t.block_map();
    };
    
    template <typename T0, typename T1> concept elementwise_compatible = multiblock_array<T0> && multiblock_array<T1> &&
    requires(const T0& t0, const T1& t1)
    {
        //TODO: write this
        t0;
    };
    
    enum exchange_inclusion_e
    {
        exclude_exchanges=0,
        include_exchanges=1
    };
    
    // struct neighbor_relationship_t
    // {
    //     ctrs::array<int, 3> edge_vec;
    //     int rank_end, rank_start;
    //     int lb_glob_end, lb_glob_start;
    // };
    
    template
    <
        ctrs::basic_array array_descriptor_t,
        coords::coordinate_system coord_t,
        typename block_arrangement_t,
        parallel::parallel_group par_group_t
    >
    class cartesian_grid_t
    {
        public:
            
            using dtype            = coord_t::coord_type;
            using coord_type       = coord_t::coord_type;
            using coord_sys_type   = coord_t;
            using coord_point_type = coords::point_t<coord_type>;
            using group_type       = par_group_t;
        
        private:
            partition::block_partition_t grid_partition;
            coord_t coord_system;
            ctrs::array<int, 3> cells_in_block;
            ctrs::array<int, 3> exchange_cells;
            block_arrangement_t block_arrangement;
            const par_group_t& grid_group;

        public:

            constexpr static std::size_t grid_dim = array_descriptor_t::size();
            
            cartesian_grid_t(
                const array_descriptor_t& cells_in_block_in,
                const array_descriptor_t& exchange_cells_in,
                const block_arrangement_t& block_arrangement_in,
                const coord_t& coord_system_in,
                const par_group_t& group_in
                )
            : block_arrangement{block_arrangement_in},
              grid_group{group_in},
              coord_system{coord_system_in},
              grid_partition(block_arrangement_in, group_in)
            {
                //Initialize class members
                ctrs::copy_array(cells_in_block_in, cells_in_block, 1);
                ctrs::copy_array(exchange_cells_in, exchange_cells, 0);
                
                
                // send_size_elems.resize(group_in.size(), 0);
                // recv_size_elems.resize(group_in.size(), 0);
                
                // send_bufs.resize(group_in.size());
                // recv_bufs.resize(group_in.size());
                
                // requests.resize(group_in.size());
                // statuses.resize(group_in.size());
                
                // //compute neighbor relationships
                // for (auto lb: range(0, this->get_num_global_blocks()))
                // {
                //     std::size_t lb_glob = lb;
                //     ctrs::array<int, 3> delta_lb = 0;
                //     int i3d = this->is_3d()?1:0;
                //     auto delta_lb_range = range(-1, 2)*range(-1, 2)*range(-i3d, 1+i3d);
                //     for (auto dlb: delta_lb_range)
                //     {
                //         ctrs::copy_array(dlb, delta_lb);
                //         ctrs::array<int, 3> lb_nd;
                //         ctrs::copy_array(expand_index(lb_glob, this->num_blocks), lb_nd);
                //         lb_nd += delta_lb;
                //         lb_nd += this->num_blocks;
                //         lb_nd %= this->num_blocks;
                //         std::size_t lb_glob_neigh = ctrs::collapse_index(lb_nd, this->num_blocks);
                //         int rank_here  = grid_partition.get_global_rank(lb_glob);
                //         int rank_neigh = grid_partition.get_global_rank(lb_glob_neigh);
                        
                //         neighbor_relationship_t neighbor_relationship;
                //         neighbor_relationship.edge_vec      = delta_lb;
                //         neighbor_relationship.rank_end      = rank_neigh;
                //         neighbor_relationship.lb_glob_end   = lb_glob_neigh;
                //         neighbor_relationship.rank_start    = rank_here;
                //         neighbor_relationship.lb_glob_start = lb_glob;
                //         if (neighbor_relationship.edge_vec[0] != 0 || neighbor_relationship.edge_vec[1] != 0 || neighbor_relationship.edge_vec[2] != 0)
                //         {
                //             if ((rank_here==group_in.rank()) || (rank_neigh==group_in.rank()))
                //             {
                //                 neighbors.push_back(neighbor_relationship);
                                
                //                 //Here sends, neigh receives
                //                 if (group_in.rank()==rank_here)
                //                 {
                //                     send_size_elems[rank_neigh] += this->get_send_index_bounds(delta_lb).volume();
                //                 }
                //                 if (group_in.rank()==rank_neigh)
                //                 {
                //                     recv_size_elems[rank_here]  += this->get_recv_index_bounds(delta_lb).volume();
                //                 }
                //             }
                //         }
                //     }
                // }
            }
            
            cartesian_grid_t(){}
            
            constexpr static int dim() {return grid_dim;}
            
            template <typename idx_t>_finline_ coord_point_type get_coords(const idx_t& i) const
            {
                return coord_system.map(this->get_comp_coords(i));
            }
            
            template <typename idx_t>_finline_ coord_point_type get_comp_coords(const idx_t& i) const
            {
                const auto  idx_r = get_index_coord(i);
                const auto  lb    = grid_partition.to_global(utils::tag[partition::local](i.lb()));
                const auto& bnd   = block_arrangement.get_bounding_box(lb.value);
                coord_point_type output;
                if constexpr (dim() == 2) output[2] = 0.0;
                for (int d = 0; d < dim(); ++d)
                {
                    output[d] = bnd.min(d)+idx_r[d]*this->get_dx(d, lb);
                }
                return output;
            }
            
            md_range_t<int,4> get_range(const array_centering& centering_in, const exchange_inclusion_e& do_guards=exclude_exchanges) const
            {
                int iexchg = 0;
                int i3d = 0;
                if (dim()==3) i3d = 1;
                if (do_guards==include_exchanges) iexchg = 1;
                switch (centering_in)
                {
                    case cell_centered:
                    {
                        return md_range_t<int,4>(
                            -iexchg*exchange_cells[0],cells_in_block[0]+iexchg*exchange_cells[0],
                            -iexchg*exchange_cells[1],cells_in_block[1]+iexchg*exchange_cells[1],
                            -i3d*iexchg*exchange_cells[2],cells_in_block[2]+i3d*iexchg*exchange_cells[2],
                            0,grid_partition.get_num_local_blocks());
                    }
                    case node_centered:
                    {
                        return md_range_t<int,4>(
                            -iexchg*exchange_cells[0],1+cells_in_block[0]+iexchg*exchange_cells[0],
                            -iexchg*exchange_cells[1],1+cells_in_block[1]+iexchg*exchange_cells[1],
                            -i3d*iexchg*exchange_cells[2],(1-i3d)+cells_in_block[2]+i3d*iexchg*exchange_cells[2],
                            0,grid_partition.get_num_local_blocks());
                    }
                    default: return md_range_t<int,4>(0,0,0,0,0,0,0,0);
                }
            }
            
            std::size_t get_grid_size()                         const { return get_num_cells(0)*get_num_cells(1)*get_num_cells(2)*get_num_global_blocks(); }
            std::size_t  get_num_local_blocks()                 const { return grid_partition.get_num_local_blocks(); }
            std::size_t get_num_global_blocks()                 const { return grid_partition.get_num_global_blocks(); }
            int get_num_cells(const std::size_t& i)             const { return cells_in_block[i]; }
            int get_num_exchange(const std::size_t& i)          const { return exchange_cells[i]; }
            const auto& is_domain_boundary(const auto& lb)      const { return block_arrangement.is_domain_boundary(lb); }
            bound_box_t<dtype,  3> get_bounds()                 const { return block_arrangement.get_bounds(); }
            const par_group_t& group()                          const { return grid_group; }
            const coord_t& coord_sys()                          const { return coord_system; }
            const auto& get_blocks()                            const { return block_arrangement; }
            
            constexpr static bool is_3d()                             { return dim()==3; }
            
            ctrs::array<dtype,  3> get_dx(const std::size_t& lb) const
            { 
                return this->get_dx(utils::tag[partition::local](lb));
            }
            
            dtype get_dx(const int i, const std::size_t& lb)  const
            {
                return this->get_dx(i, utils::tag[partition::local](lb));
            }
            
            ctrs::array<dtype,  3> get_dx(const partition::partition_tagged auto& lb) const
            { 
                auto size = block_arrangement.get_size(grid_partition.to_global(lb).value);
                for (int i = 0; i < cells_in_block.size(); ++i) size[i] /= cells_in_block[i];
                return size;
            }
            
            dtype get_dx(const int i, const partition::partition_tagged auto& lb)  const
            {
                return block_arrangement.get_size(i, grid_partition.to_global(lb).value)/cells_in_block[i];
            }
            
            bound_box_t<dtype, 3> get_block_box(const partition::partition_tagged auto& lb) const
            {
                return block_arrangement.get_bounding_box(grid_partition.to_global(lb).value);
            }
            
            const partition::block_partition_t& get_partition() const { return grid_partition; }
            const coord_t& get_coord_sys() const { return coord_system; }
    };
}
