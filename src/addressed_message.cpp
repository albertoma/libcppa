#include "cppa/detail/addressed_message.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace cppa { namespace detail {

addressed_message::addressed_message(actor_ptr const& from,
                 channel_ptr const& to,
                 any_tuple const& ut)
    : m_sender(from), m_receiver(to), m_content(ut)
{
}

addressed_message::addressed_message(actor_ptr const& from,
                 channel_ptr const& to,
                 any_tuple&& ut)
    : m_sender(from), m_receiver(to), m_content(std::move(ut))
{
}

bool operator==(addressed_message const& lhs, addressed_message const& rhs)
{
    return    lhs.sender() == rhs.sender()
           && lhs.receiver() == rhs.receiver()
           && lhs.content() == rhs.content();
}

} } // namespace cppa
