#ifndef PRIMITIVE_VARIANT_HPP
#define PRIMITIVE_VARIANT_HPP

#include <new>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

#include "cppa/primitive_type.hpp"

#include "cppa/util/pt_token.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"
#include "cppa/util/pt_dispatch.hpp"
#include "cppa/util/is_primitive.hpp"

#include "cppa/detail/type_to_ptype.hpp"
#include "cppa/detail/ptype_to_type.hpp"

namespace cppa { namespace detail {

template<primitive_type FT, class T, class V>
void ptv_set(primitive_type& lhs_type, T& lhs, V&& rhs,
             typename util::disable_if<std::is_arithmetic<T>>::type* = 0);

template<primitive_type FT, class T, class V>
void ptv_set(primitive_type& lhs_type, T& lhs, V&& rhs,
             typename util::enable_if<std::is_arithmetic<T>, int>::type* = 0);

} } // namespace cppa::detail

namespace cppa {

class primitive_variant;

template<typename T>
T const& get(primitive_variant const& pv);

template<typename T>
T& get_ref(primitive_variant& pv);

/**
 * @ingroup TypeSystem
 * @brief An union container for primitive data types.
 */
class primitive_variant
{

    friend bool operator==(primitive_variant const& lhs,
                           primitive_variant const& rhs);

    template<typename T>
    friend T const& get(primitive_variant const& pv);

    template<typename T>
    friend T& get_ref(primitive_variant& pv);

    primitive_type m_ptype;

    union
    {
        std::int8_t i8;
        std::int16_t i16;
        std::int32_t i32;
        std::int64_t i64;
        std::uint8_t u8;
        std::uint16_t u16;
        std::uint32_t u32;
        std::uint64_t u64;
        float fl;
        double db;
        long double ldb;
        std::string s8;
        std::u16string s16;
        std::u32string s32;
    };

    // use static call dispatching to select member
    inline decltype(i8)&    get(util::pt_token<pt_int8>)        { return i8;  }
    inline decltype(i16)&   get(util::pt_token<pt_int16>)       { return i16; }
    inline decltype(i32)&   get(util::pt_token<pt_int32>)       { return i32; }
    inline decltype(i64)&   get(util::pt_token<pt_int64>)       { return i64; }
    inline decltype(u8)&    get(util::pt_token<pt_uint8>)       { return u8;  }
    inline decltype(u16)&   get(util::pt_token<pt_uint16>)      { return u16; }
    inline decltype(u32)&   get(util::pt_token<pt_uint32>)      { return u32; }
    inline decltype(u64)&   get(util::pt_token<pt_uint64>)      { return u64; }
    inline decltype(fl)&    get(util::pt_token<pt_float>)       { return fl;  }
    inline decltype(db)&    get(util::pt_token<pt_double>)      { return db;  }
    inline decltype(ldb)&   get(util::pt_token<pt_long_double>) { return ldb; }
    inline decltype(s8)&    get(util::pt_token<pt_u8string>)    { return s8;  }
    inline decltype(s16)&   get(util::pt_token<pt_u16string>)   { return s16; }
    inline decltype(s32)&   get(util::pt_token<pt_u32string>)   { return s32; }

    // get(...) const overload
    template<primitive_type PT>
    const typename detail::ptype_to_type<PT>::type&
    get(util::pt_token<PT> token) const
    {
        return const_cast<primitive_variant*>(this)->get(token);
    }

    template<class Self, typename Fun>
    struct applier
    {
        Self* m_self;
        Fun& m_f;
        applier(Self* self, Fun& f) : m_self(self), m_f(f) { }
        template<primitive_type FT>
        inline void operator()(util::pt_token<FT> token)
        {
            m_f(m_self->get(token));
        }
    };

    void destroy();

    template<primitive_type PT>
    void type_check() const
    {
        if (m_ptype != PT) throw std::logic_error("type check failed");
    }

    template<primitive_type PT>
    typename detail::ptype_to_type<PT>::type& get_as()
    {
        static_assert(PT != pt_null, "PT == pt_null");
        type_check<PT>();
        util::pt_token<PT> token;
        return get(token);
    }

    template<primitive_type PT>
    const typename detail::ptype_to_type<PT>::type& get_as() const
    {
        static_assert(PT != pt_null, "PT == pt_null");
        type_check<PT>();
        util::pt_token<PT> token;
        return const_cast<primitive_variant*>(this)->get(token);
    }

 public:

    template<typename Fun>
    void apply(Fun&& f)
    {
        util::pt_dispatch(m_ptype, applier<primitive_variant, Fun>(this, f));
    }

    template<typename Fun>
    void apply(Fun&& f) const
    {
        util::pt_dispatch(m_ptype,
                          applier<const primitive_variant, Fun>(this, f));
    }

    /**
     * @brief Creates an empty variant.
     * @post <tt>ptype() == pt_null && type() == typeid(void)</tt>.
     */
    primitive_variant();

    /**
     * @brief Creates a variant from @p value.
     * @param value A primitive value.
     * @pre @p value does have a primitive type.
     */
    template<typename V>
    primitive_variant(V&& value) : m_ptype(pt_null)
    {
        static constexpr primitive_type ptype = detail::type_to_ptype<V>::ptype;
        static_assert(ptype != pt_null, "V is not a primitive type");
        detail::ptv_set<ptype>(m_ptype,
                               get(util::pt_token<ptype>()),
                               std::forward<V>(value));
    }

    /**
     * @brief Creates a variant with type pt.
     * @param pt Requestet type.
     * @post <tt>ptype() == pt</tt>.
     */
    primitive_variant(primitive_type pt);

    /**
     * @brief Creates a copy from @p other.
     * @param other A primitive variant.
     */
    primitive_variant(primitive_variant const& other);

    /**
     * @brief Creates a new variant and move the value from @p other to it.
     * @param other A primitive variant rvalue.
     */
    primitive_variant(primitive_variant&& other);

    /**
     * @brief Moves @p value to this variant if @p value is an rvalue;
     *        otherwise copies the value of @p value.
     * @param value A primitive value.
     * @returns <tt>*this</tt>.
     */
    template<typename V>
    primitive_variant& operator=(V&& value)
    {
        static constexpr primitive_type ptype = detail::type_to_ptype<V>::ptype;
        static_assert(ptype != pt_null, "V is not a primitive type");
        util::pt_token<ptype> token;
        if (ptype == m_ptype)
        {
            get(token) = std::forward<V>(value);
        }
        else
        {
            destroy();
            detail::ptv_set<ptype>(m_ptype, get(token), std::forward<V>(value));
            //set(std::forward<V>(value));
        }
        return *this;
    }

    /**
     * @brief Copies the content of @p other to @p this.
     * @param other A primitive variant.
     * @returns <tt>*this</tt>.
     */
    primitive_variant& operator=(primitive_variant const& other);

    /**
     * @brief Moves the content of @p other to @p this.
     * @param other A primitive variant rvalue.
     * @returns <tt>*this</tt>.
     */
    primitive_variant& operator=(primitive_variant&& other);

    /**
     * @brief Gets the {@link primitive_type type} of @p this.
     * @returns The {@link primitive_type type} of @p this.
     */
    inline primitive_type ptype() const { return m_ptype; }

    /**
     * @brief Gets the RTTI type of @p this.
     * @returns <tt>typeid(void)</tt> if <tt>ptype() == pt_null</tt>;
     *          otherwise typeid(T) is returned, where T is the C++ type
     *          of @p this.
     */
    std::type_info const& type() const;

    ~primitive_variant();

};

/**
 * @ingroup TypeSystem
 * @brief Casts a primitive variant to its C++ type.
 * @relates primitive_variant
 * @param pv A primitive variant of type @p T.
 * @returns A const reference to the value of @p pv of type @p T.
 * @throws <tt>std::logic_error</tt> if @p pv is not of type @p T.
 */
template<typename T>
T const& get(primitive_variant const& pv)
{
    static const primitive_type ptype = detail::type_to_ptype<T>::ptype;
    return pv.get_as<ptype>();
}

/**
 * @ingroup TypeSystem
 * @brief Casts a non-const primitive variant to its C++ type.
 * @relates primitive_variant
 * @param pv A primitive variant of type @p T.
 * @returns A reference to the value of @p pv of type @p T.
 * @throws <tt>std::logic_error</tt> if @p pv is not of type @p T.
 */
template<typename T>
T& get_ref(primitive_variant& pv)
{
    static const primitive_type ptype = detail::type_to_ptype<T>::ptype;
    return pv.get_as<ptype>();
}

#ifdef CPPA_DOCUMENTATION

/**
 * @ingroup TypeSystem
 * @brief Casts a primitive variant to its C++ type.
 * @relates primitive_variant
 * @tparam T C++ type equivalent of @p PT.
 * @param pv A primitive variant of type @p T.
 * @returns A const reference to the value of @p pv of type @p T.
 */
template<primitive_type PT>
T const& get_ref(primitive_variant const& pv);

/**
 * @ingroup TypeSystem
 * @brief Casts a non-const primitive variant to its C++ type.
 * @relates primitive_variant
 * @tparam T C++ type equivalent of @p PT.
 * @param pv A primitive variant of type @p T.
 * @returns A reference to the value of @p pv of type @p T.
 */
template<primitive_type PT>
T& get_ref(primitive_variant& pv);

#else

template<primitive_type PT>
inline const typename detail::ptype_to_type<PT>::type&
get(primitive_variant const& pv)
{
    static_assert(PT != pt_null, "PT == pt_null");
    return get<typename detail::ptype_to_type<PT>::type>(pv);
}

template<primitive_type PT>
inline typename detail::ptype_to_type<PT>::type&
get_ref(primitive_variant& pv)
{
    static_assert(PT != pt_null, "PT == pt_null");
    return get_ref<typename detail::ptype_to_type<PT>::type>(pv);
}

#endif

bool operator==(primitive_variant const& lhs, primitive_variant const& rhs);

inline
bool operator!=(primitive_variant const& lhs, primitive_variant const& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
typename util::enable_if<util::is_primitive<T>, bool>::type
operator==(T const& lhs, primitive_variant const& rhs)
{
    static constexpr primitive_type ptype = detail::type_to_ptype<T>::ptype;
    static_assert(ptype != pt_null, "T is an incompatible type");
    return (rhs.ptype() == ptype) ? lhs == get<ptype>(rhs) : false;
}

template<typename T>
typename util::enable_if<util::is_primitive<T>, bool>::type
operator==(primitive_variant const& lhs, T const& rhs)
{
    return (rhs == lhs);
}

template<typename T>
typename util::enable_if<util::is_primitive<T>, bool>::type
operator!=(primitive_variant const& lhs, T const& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
typename util::enable_if<util::is_primitive<T>, bool>::type
operator!=(T const& lhs, primitive_variant const& rhs)
{
    return !(lhs == rhs);
}

} // namespace cppa

namespace cppa { namespace detail {

template<primitive_type FT, class T, class V>
void ptv_set(primitive_type& lhs_type, T& lhs, V&& rhs,
             typename util::disable_if<std::is_arithmetic<T>>::type*)
{
    if (FT == lhs_type)
    {
        lhs = std::forward<V>(rhs);
    }
    else
    {
        new (&lhs) T(std::forward<V>(rhs));
        lhs_type = FT;
    }
}

template<primitive_type FT, class T, class V>
void ptv_set(primitive_type& lhs_type, T& lhs, V&& rhs,
             typename util::enable_if<std::is_arithmetic<T>, int>::type*)
{
    // don't call a constructor for arithmetic types
    lhs = rhs;
    lhs_type = FT;
}

} } // namespace cppa::detail


#endif // PRIMITIVE_VARIANT_HPP
