#pragma once

#include "core/except.h"
#include "grid/transactions.h"
#include "amr/amr_node.h"

namespace spade::grid
{
    template <typename grid_t>
    auto get_transaction(
        const grid_t& grid,
        const std::size_t&,
        const neighbor_relation_t& relation)
    {
        grid_rect_copy_t output;
        auto lb_ini  = utils::tag[partition::global](relation.lb_ini);
        auto lb_term = utils::tag[partition::global](relation.lb_term);
        
        output.rank_send = grid.get_partition().get_rank(lb_ini);
        output.rank_recv = grid.get_partition().get_rank(lb_term);

        output.source.min(3) = grid.get_partition().to_local(lb_ini).value;
        output.source.max(3) = output.source.min(3)+1;
        output.dest.min(3)   = grid.get_partition().to_local(lb_term).value;
        output.dest.max(3)   = output.dest.min(3)+1;
        
        for (int d = 0; d < 3; ++d)
        {
            const int sign = relation.edge[d];
            const int nx   = grid.get_num_cells(d);
            const int ng   = grid.get_num_exchange(d);
            switch (sign)
            {
                case -1:
                {
                    output.source.min(d) = 0;
                    output.source.max(d) = ng;
                    output.dest.min(d)   = nx;
                    output.dest.max(d)   = nx+ng;
                    break;
                }
                case  0:
                {
                    output.source.min(d) = 0;
                    output.source.max(d) = nx;
                    output.dest.min(d)   = 0;
                    output.dest.max(d)   = nx;
                    break;
                }
                case  1:
                {
                    output.source.min(d) = nx-ng;
                    output.source.max(d) = nx;
                    output.dest.min(d)   = -ng;
                    output.dest.max(d)   = 0;
                    break;
                }
            }
        }
        if constexpr (grid_t::dim() == 2)
        {
            output.source.min(2) = 0;
            output.source.max(2) = 1;
            output.dest.min(2)   = 0;
            output.dest.max(2)   = 1;
        }
        return output;
    };
    
    template <typename grid_t>
    auto get_transaction(
        const grid_t& grid,
        const std::size_t& lb_ini,
        const amr::amr_neighbor_t<grid_t::dim()>& relation)
    {
        patch_fill_t<grid_t::dim()> output;
        neighbor_relation_t base_relation;
        base_relation.edge = -1*relation.edge;
        if constexpr (grid_t::dim() == 2) base_relation.edge[2] = 0;
        base_relation.lb_ini  = relation.endpoint.get().tag;
        base_relation.lb_term = lb_ini;
        
        
        
        //NOTE: here, we ASK the neighbor for data instead of tell it about the data we send.
        auto ptch = get_transaction(grid, lb_ini, base_relation);
        output.patches = ptch;
        
        
        auto& self  = relation.endpoint.get();
        auto& neigh = grid.get_blocks().enumerated_nodes[lb_ini].get();
        
        output.i_coeff = 0;
        output.i_incr  = 0;
        for (int d = 0; d < grid_t::dim(); ++d)
        {
            int level_diff = self.level[d] - neigh.level[d];
            
            //Need to modify:
            
            // [ ] output.patches.source
            // [ ] output.patches.dest
            // [x] output.i_skip
            // [x] output.num_oslot
            // [x] output.delta_i
            
            switch(level_diff)
            {
                case -1: // neighbor is finer, I am coarser in this direction
                {
                    output.i_coeff[d] = -1;
                    // Neighbor is finer, so I only need half the range in this
                    // direction
                    switch(base_relation.edge[d])
                    {
                        case -1:
                        {
                            output.patches.source.max(d) = output.patches.source.min(d) + output.patches.source.size(d)/2;
                            break;
                        }
                        case  0:
                        {
                            output.patches.source.max(d) = output.patches.source.min(d) + output.patches.source.size(d)/2;
                            if (neigh.amr_position.min(d)>self.amr_position.min(d))
                            {
                                const auto sz = output.patches.source.size(d);
                                output.patches.source.min(d) += sz;
                                output.patches.source.max(d) += sz;
                            }
                            break;
                        }
                        case  1:
                        {
                            output.patches.source.min(d) = output.patches.source.max(d) - output.patches.source.size(d)/2;
                            break;
                        }
                    }
                    break;
                }
                case  0: // same level (do nothing)
                {
                    break;
                }
                case  1: // neighbor is coarser, I am finer in this direction
                {
                    output.i_incr[d] = 1;
                    output.i_coeff[d] = 1;
                    switch(base_relation.edge[d])
                    {
                        // neighbor is coarser, so send twice the data
                        case -1:
                        {
                            output.patches.source.max(d) = output.patches.source.min(d) + 2*output.patches.source.size(d);
                            break;
                        }
                        case  0:
                        {
                            //except here, where we only fill half the data!
                            output.patches.dest.max(d) = output.patches.dest.min(d) + output.patches.dest.size(d)/2;
                            if (self.amr_position.min(d) > neigh.amr_position.min(d))
                            {
                                const auto sz = output.patches.dest.size(d);
                                output.patches.dest.min(d) += sz;
                                output.patches.dest.max(d) += sz;
                            }
                            break;
                        }
                        case  1:
                        {
                            output.patches.source.min(d) = output.patches.source.max(d) - 2*output.patches.source.size(d);
                            break;
                        }
                    }
                    
                    break;
                }
                default:
                {
                    throw except::sp_exception("illegal grid configuration");
                }
            }
        }
        
        // bool aa = (base_relation.lb_ini == 10) && (base_relation.lb_term == 21);
        // bool bb = (base_relation.lb_ini == 21) && (base_relation.lb_term == 10);
        // if (aa || bb)
        // {
        //     print("found");
        //     print("SENDER", grid.get_bounding_box(utils::tag[partition::global](base_relation.lb_ini)));
        //     print("RECVER", grid.get_bounding_box(utils::tag[partition::global](base_relation.lb_term)));
        //     print(base_relation.lb_ini, base_relation.lb_term);
        //     print(base_relation.edge);
        //     print("source --> ", ptch.source);
        //     print("dest   --> ", ptch.dest);
        //     print("num_oslot", output.num_oslot);
        //     print("i_skip", output.i_skip);
        //     print("delta_i", output.delta_i);
        //     if (aa) std::cin.get();
        // }
        
        return output;
    };
}