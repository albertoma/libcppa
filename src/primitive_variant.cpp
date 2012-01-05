#include <typeinfo>

#include "cppa/primitive_variant.hpp"
#include "cppa/detail/type_to_ptype.hpp"

namespace cppa {

namespace {

template<class T>
void ptv_del(T&,
             typename util::enable_if<std::is_arithmetic<T>, int>::type* = 0)
{
    // arithmetic types don't need destruction
}

template<class T>
void ptv_del(T& what,
             typename util::disable_if< std::is_arithmetic<T> >::type* = 0)
{
    what.~T();
}

struct destroyer
{
    template<typename T>
    inline void operator()(T& what) const
    {
        ptv_del(what);
    }
};

struct type_reader
{
    std::type_info const* tinfo;
    type_reader() : tinfo(nullptr) { }
    template<typename T>
    void operator()(T const&)
    {
        tinfo = &typeid(T);
    }
};

struct comparator
{
    bool result;
    primitive_variant const& lhs;
    primitive_variant const& rhs;

    comparator(primitive_variant const& pv1, primitive_variant const& pv2)
        : result(false), lhs(pv1), rhs(pv2)
    {
    }

    template<primitive_type PT>
    void operator()(util::pt_token<PT>)
    {
        if (rhs.ptype() == PT)
        {
            result = (get<PT>(lhs) == get<PT>(rhs));
        }
    }
};

struct initializer
{
    primitive_variant& lhs;

    inline initializer(primitive_variant& pv) : lhs(pv) { }

    template<primitive_type PT>
    inline void operator()(util::pt_token<PT>)
    {
        typedef typename detail::ptype_to_type<PT>::type T;
        lhs = T();
    }
};

struct setter
{
    primitive_variant& lhs;

    setter(primitive_variant& pv) : lhs(pv) { }

    template<typename T>
    inline void operator()(T const& rhs)
    {
        lhs = rhs;
    }
};

struct mover
{
    primitive_variant& lhs;

    mover(primitive_variant& pv) : lhs(pv) { }

    template<typename T>
    inline void operator()(T& rhs)
    {
        lhs = std::move(rhs);
    }
};

} // namespace <anonymous>

primitive_variant::primitive_variant() : m_ptype(pt_null) { }

primitive_variant::primitive_variant(primitive_type ptype) : m_ptype(pt_null)
{
    util::pt_dispatch(ptype, initializer(*this));
}

primitive_variant::primitive_variant(primitive_variant const& other)
    : m_ptype(pt_null)
{
    other.apply(setter(*this));
}

primitive_variant::primitive_variant(primitive_variant&& other)
    : m_ptype(pt_null)
{
    other.apply(mover(*this));
}

primitive_variant& primitive_variant::operator=(primitive_variant const& other)
{
    other.apply(setter(*this));
    return *this;
}

primitive_variant& primitive_variant::operator=(primitive_variant&& other)
{
    other.apply(mover(*this));
    return *this;
}

bool operator==(primitive_variant const& lhs, primitive_variant const& rhs)
{
    comparator cmp(lhs, rhs);
    util::pt_dispatch(lhs.m_ptype, cmp);
    return cmp.result;
}

std::type_info const& primitive_variant::type() const
{
    type_reader tr;
    apply(tr);
    return (tr.tinfo == nullptr) ? typeid(void) : *tr.tinfo;
}

primitive_variant::~primitive_variant()
{
    destroy();
}

void primitive_variant::destroy()
{
    apply(destroyer());
    m_ptype = pt_null;
}

} // namespace cppa
