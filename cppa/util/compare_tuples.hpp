#ifndef COMPARE_TUPLES_HPP
#define COMPARE_TUPLES_HPP

#include "cppa/get.hpp"
#include "cppa/util/at.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/eval_first_n.hpp"
#include "cppa/util/is_comparable.hpp"
#include "cppa/util/eval_type_lists.hpp"

namespace cppa { namespace detail {

template<size_t N, template<typename...> class Tuple, typename... Types>
const typename util::at<N, Types...>::type&
do_get(Tuple<Types...> const& t)
{
    return ::cppa::get<N, Types...>(t);
}

template<size_t N, typename LhsTuple, typename RhsTuple>
struct cmp_helper
{
    inline static bool cmp(LhsTuple const& lhs, RhsTuple const& rhs)
    {
        return    do_get<N>(lhs) == do_get<N>(rhs)
               && cmp_helper<N-1, LhsTuple, RhsTuple>::cmp(lhs, rhs);
    }
};

template<typename LhsTuple, typename RhsTuple>
struct cmp_helper<0, LhsTuple, RhsTuple>
{
    inline static bool cmp(LhsTuple const& lhs, RhsTuple const& rhs)
    {
        return do_get<0>(lhs) == do_get<0>(rhs);
    }
};

} } // namespace cppa::detail

namespace cppa { namespace util {

template<template<typename...> class LhsTuple, typename... LhsTypes,
         template<typename...> class RhsTuple, typename... RhsTypes>
bool compare_tuples(LhsTuple<LhsTypes...> const& lhs,
                    RhsTuple<RhsTypes...> const& rhs)
{
    static_assert(sizeof...(LhsTypes) == sizeof...(RhsTypes),
                  "could not compare tuples of different size");
    static_assert(util::eval_type_lists<util::type_list<LhsTypes...>,
                                        util::type_list<RhsTypes...>,
                                        util::is_comparable>::value,
                  "types of lhs are not comparable to the types of rhs");
    return detail::cmp_helper<(sizeof...(LhsTypes) - 1),
                              LhsTuple<LhsTypes...>,
                              RhsTuple<RhsTypes...>>::cmp(lhs, rhs);
}

template<template<typename...> class LhsTuple, typename... LhsTypes,
         template<typename...> class RhsTuple, typename... RhsTypes>
bool compare_first_elements(LhsTuple<LhsTypes...> const& lhs,
                            RhsTuple<RhsTypes...> const& rhs)
{
    typedef util::type_list<LhsTypes...> lhs_types;
    typedef util::type_list<RhsTypes...> rhs_types;

    static constexpr size_t lhs_size = sizeof...(LhsTypes);
    static constexpr size_t rhs_size = sizeof...(RhsTypes);
    static constexpr size_t cmp_size = (lhs_size < rhs_size)
                                       ? lhs_size
                                       : rhs_size;

    static_assert(util::eval_first_n<cmp_size,
                                     lhs_types,
                                     rhs_types,
                                     util::is_comparable>::value,
                  "types of lhs are not comparable to the types of rhs");

    return detail::cmp_helper<(cmp_size - 1),
                              LhsTuple<LhsTypes...>,
                              RhsTuple<RhsTypes...>>::cmp(lhs, rhs);
}

} } // namespace cppa::util

#endif // COMPARE_TUPLES_HPP
