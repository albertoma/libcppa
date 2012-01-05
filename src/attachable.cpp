#include "cppa/attachable.hpp"

namespace cppa {

attachable::~attachable()
{
}

void attachable::detach(std::uint32_t)
{
}

bool attachable::matches(attachable::token const&)
{
    return false;
}

} // namespace cppa::detail
