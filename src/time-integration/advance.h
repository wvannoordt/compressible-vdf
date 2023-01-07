#pragma once

#include <concepts>
#include <ratio>

#include "time-integration/time_integration_utils.h"
#include "time-integration/explicit.h"
#include "time-integration/implicit.h"

namespace spade::time_integration
{
    namespace detail
    {
        using ratio_integral_type = decltype(std::deci::num);
        
        template <typename val_t> struct nonzero_t
        {
            constexpr static bool value = false;
        };
        
        template <const ratio_integral_type num, const ratio_integral_type denum> struct nonzero_t<std::ratio<num, denum>>
        {
            constexpr static bool value = (num!=0);
        };
        
        template <const ratio_integral_type num> struct nonzero_t<std::ratio<num>>
        {
            constexpr static bool value = (num!=0);
        };
        
        template <typename numeric_t, typename type_t> struct coeff_value_t
        {
            //invalid
        };
        
        template <typename numeric_t, const ratio_integral_type num, const ratio_integral_type denum> struct coeff_value_t<numeric_t, std::ratio<num, denum>>
        {
            constexpr static numeric_t value() {return numeric_t(num)/numeric_t(denum);}
        };
        
        template <typename numeric_t, const ratio_integral_type num> struct coeff_value_t<numeric_t, std::ratio<num>>
        {
            constexpr static numeric_t value() {return numeric_t(num);}
        };
        
        template <typename idx_t> const int idx_val = utils::remove_all<idx_t>::type::value;
    }
    
    //explicit RK scheme (TODO: implement concept for this)
    template <typename axis_t, typename data_t, typename scheme_t, typename rhs_t, typename trans_t>
    void integrate_advance(axis_t& axis, data_t& data, const scheme_t& scheme, const rhs_t& rhs, const trans_t& trans)
    {
        //Number of RHS evaluations
        const int num_stages = scheme_t::table_type::rows();
        
        //Get the basic numeric type from the solution
        using numeric_type = typename axis_t::value_type;
        
        //Timestep
        const auto& dt = axis.timestep();
        
        //Computing the residuals
        algs::static_for<0, num_stages>([&](const auto& ii) -> void
        {
            const int i = ii.value;
            
            //We won't update this copy of the solution during the RHS evaluations
            const auto& sol_base = data.solution(0);
            
            //The solution used when we evaluate the RHS
            auto& sol = data.solution(1);
            sol = sol_base; //expensive!
            
            //solution augmentation loop
            //Begin by applying the forward transform
            trans.transform_forward(sol);
            algs::static_for<0, i>([&](const auto& jj) -> void
            {
                const int j = jj.value;
                using table_t = scheme_t::table_type;
                
                //Note that this is a temporary workaround owing to a bug in GCC 10.2
                using coeff_value_t_1 = typename table_t::elem_t<detail::idx_val<decltype(ii)>>;
                using coeff_value_t = typename coeff_value_t_1::elem_t<detail::idx_val<decltype(jj)>>;
                
                //Using GCC 11+, below is valid
                //using coeff_value_t = typename table_t::elem_t<i>::elem_t<j>;
                
                if constexpr (detail::nonzero_t<coeff_value_t>::value)
                {
                    constexpr numeric_type coeff = detail::coeff_value_t<numeric_type, coeff_value_t>::value(); //get the coefficient
                    auto& resid_j = data.residual(j); //"k_j"
                    resid_j *= (dt*coeff); //This requires that "dt" is cheap to multiply, otherwise there is a more efficient way of doing this!
                    sol += resid_j;
                    resid_j /= (dt*coeff);
                }
            });
            trans.transform_inverse(sol);
            
            //By now, the solution has been augmented to accommodate the
            //evaluation of the ith residual
            auto& cur_resid = data.residual(i);
            using dt_coeff_value_t = typename scheme_t::dt_type::elem_t<i>;
            constexpr numeric_type time_coeff = detail::coeff_value_t<numeric_type, dt_coeff_value_t>::value();
            
            //Evaluate the residual at t + c*dt
            axis.time() += time_coeff*dt;
            rhs(cur_resid, sol, axis.time());
            axis.time() -= time_coeff*dt;
        });
        
        auto& new_solution = data.solution(0); //solution is updated in place
        
        //Residual accumulation loop
        algs::static_for<0, num_stages>([&](const auto& ii) -> void
        {
            const int i = ii.value;
            using accum_coeff_value_t = typename scheme_t::accum_type::elem_t<i>;
            
            //We skip over any zero coefficients
            if constexpr (detail::nonzero_t<accum_coeff_value_t>::value)
            {
                constexpr numeric_type coeff = detail::coeff_value_t<numeric_type, accum_coeff_value_t>::value();
                auto& resid = data.residual(i);
                
                //Multiply the residual by the accumulation coefficient
                resid *= (coeff*dt);
                
                //Update the transformed solution
                trans.transform_forward(new_solution);
                new_solution += resid;
                trans.transform_inverse(new_solution);
                
                //Divide to avoid modifying the residual unduly (may be unnecessary!)
                resid /= (coeff*dt);
            }
        });
        
        axis.time() += dt;
    }
}