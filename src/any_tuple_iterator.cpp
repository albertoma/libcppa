#include "cppa/util/any_tuple_iterator.hpp"

namespace cppa { namespace util {

any_tuple_iterator::any_tuple_iterator(any_tuple const& data, size_t pos)
    : m_data(data), m_pos(pos)
{
}

} } // namespace cppa::util
