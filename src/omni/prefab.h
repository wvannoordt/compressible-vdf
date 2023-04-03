#pragma once

#include "omni/omni.h"

namespace spade::omni
{
    // Here, we provide a list of useful stencils that users can invoke without the tedium of 
    // generating the stencils themselves
    namespace prefab
    {
        template <typename... infos_t> using lr_t = 
            stencil_t<
                grid::face_centered,
                elem_t<offset_t<-1, 0, 0>, info_list_t<infos_t...>>,
                elem_t<offset_t< 1, 0, 0>, info_list_t<infos_t...>>
            >;
        
        template <typename... infos_t> using face_mono_t = 
            stencil_t<grid::face_centered, elem_t<offset_t<0, 0, 0>, info_list_t<infos_t...>>>;

        template <typename... infos_t> using cell_mono_t = 
            stencil_t<grid::cell_centered, elem_t<offset_t<0, 0, 0>, info_list_t<infos_t...>>>;
        
        template <const grid::array_centering center, typename... infos_t> using mono_t = 
            stencil_t<center,              elem_t<offset_t<0, 0, 0>, info_list_t<infos_t...>>>;
    }
}