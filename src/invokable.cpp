#include "cppa/detail/invokable.hpp"

namespace cppa { namespace detail {

invokable_base::~invokable_base()
{
}

timed_invokable::timed_invokable(util::duration const& d) : m_timeout(d)
{
}

} } // namespace cppa::detail
