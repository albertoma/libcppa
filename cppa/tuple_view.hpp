#ifndef TUPLE_VIEW_HPP
#define TUPLE_VIEW_HPP

#include <vector>

#include "cppa/get.hpp"
#include "cppa/tuple.hpp"

#include "cppa/util/at.hpp"
#include "cppa/util/type_list.hpp"
#include "cppa/util/a_matches_b.hpp"
#include "cppa/util/fixed_vector.hpp"
#include "cppa/util/compare_tuples.hpp"

#include "cppa/detail/decorated_tuple.hpp"

namespace cppa {

template<size_t N, typename... Types>
typename util::at<N, Types...>::type& get_ref(tuple_view<Types...>& t);

/**
 * @brief Describes a view of an fixed-length tuple.
 */
template<typename... ElementTypes>
class tuple_view
{

    //static_assert(sizeof...(ElementTypes) > 0,
    //              "could not declare an empty tuple_view");

    template<size_t N, typename... Types>
    friend typename util::at<N, Types...>::type& get_ref(tuple_view<Types...>&);

 public:

    typedef cow_ptr<detail::abstract_tuple> vals_t;

    static constexpr size_t num_elements = sizeof...(ElementTypes);

    typedef util::fixed_vector<size_t, num_elements> mapping_vector;

    tuple_view() : m_vals(tuple<ElementTypes...>().vals()) { }

    static tuple_view from(vals_t const& vals)
    {
        return tuple_view(vals);
    }

    static tuple_view from(vals_t&& vals)
    {
        return tuple_view(std::move(vals));
    }

    tuple_view(vals_t const& vals, mapping_vector& mapping)
        : m_vals(new detail::decorated_tuple<ElementTypes...>(vals, mapping))
    {
    }

    tuple_view(tuple_view&& other) : m_vals(std::move(other.m_vals))
    {
    }

    tuple_view& operator=(tuple_view const& other)
    {
        m_vals = other.m_vals;
        return *this;
    }

    tuple_view& operator=(tuple_view&& other)
    {
        m_vals = std::move(other.m_vals);
        return *this;
    }

    tuple_view(tuple_view const&) = default;

    inline vals_t const& vals() const
    {
        return m_vals;
    }

    inline size_t size() const
    {
        return sizeof...(ElementTypes);
        //return m_vals->size();
    }

 private:

    explicit tuple_view(vals_t const& vals) : m_vals(vals)
    {
    }

    explicit tuple_view(vals_t&& vals) : m_vals(std::move(vals))
    {
    }

    vals_t m_vals;

};

template<size_t N, typename... Types>
const typename util::at<N, Types...>::type& get(tuple_view<Types...> const& t)
{
    static_assert(N < sizeof...(Types), "N >= t.size()");
    typedef typename util::at<N, Types...>::type result_t;
    return *reinterpret_cast<result_t const*>(t.vals()->at(N));
}

template<size_t N, typename... Types>
typename util::at<N, Types...>::type& get_ref(tuple_view<Types...>& t)
{
    static_assert(N < sizeof...(Types), "N >= t.size()");
    typedef typename util::at<N, Types...>::type result_t;
    return *reinterpret_cast<result_t*>(t.m_vals->mutable_at(N));
}

template<typename TypeList>
struct tuple_view_type_from_type_list;

template<typename... Types>
struct tuple_view_type_from_type_list<util::type_list<Types...>>
{
    typedef tuple_view<Types...> type;
};

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(tuple_view<LhsTypes...> const& lhs,
                       tuple_view<RhsTypes...> const& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(tuple<LhsTypes...> const& lhs,
                       tuple_view<RhsTypes...> const& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator==(tuple_view<LhsTypes...> const& lhs,
                       tuple<RhsTypes...> const& rhs)
{
    return util::compare_tuples(lhs, rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(tuple_view<LhsTypes...> const& lhs,
                       tuple_view<RhsTypes...> const& rhs)
{
    return !(lhs == rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(tuple<LhsTypes...> const& lhs,
                       tuple_view<RhsTypes...> const& rhs)
{
    return !(lhs == rhs);
}

template<typename... LhsTypes, typename... RhsTypes>
inline bool operator!=(tuple_view<LhsTypes...> const& lhs,
                       tuple<RhsTypes...> const& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

#endif // TUPLE_VIEW_HPP
