#include "cppa/util/duration.hpp"

namespace {

inline std::uint64_t ui64_val(cppa::util::duration const& d)
{
    return static_cast<std::uint64_t>(d.unit) * d.count;
}

} // namespace <anonmyous>

namespace cppa { namespace util {

bool operator==(duration const& lhs, duration const& rhs)
{
    return (lhs.unit == rhs.unit ? lhs.count == rhs.count
                                 : ui64_val(lhs) == ui64_val(rhs));
}

} } // namespace cppa::util
