#ifndef OBJECT_IMPL_HPP
#define OBJECT_IMPL_HPP

#include "cppa/object.hpp"

namespace cppa {

template<typename T>
utype const& uniform_type_info();

}

namespace cppa { namespace detail {

template<typename T>
struct obj_impl : object
{
	T m_value;
	obj_impl() : m_value() { }
	obj_impl(T const& v) : m_value(v) { }
	virtual object* copy() const { return new obj_impl(m_value); }
	virtual utype const& type() const { return uniform_type_info<T>(); }
	virtual void* mutable_value() { return &m_value; }
	virtual void const* value() const { return &m_value; }
	virtual void serialize(serializer& s) const
	{
		s << m_value;
	}
	virtual void deserialize(deserializer& d)
	{
		d >> m_value;
	}
};

} } // namespace cppa::detail

#endif // OBJECT_IMPL_HPP
