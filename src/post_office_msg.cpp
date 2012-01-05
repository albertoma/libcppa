#include "cppa/detail/post_office_msg.hpp"

namespace cppa { namespace detail {

post_office_msg::add_peer::add_peer(native_socket_t peer_socket,
                                    process_information_ptr const& peer_ptr,
                                    actor_proxy_ptr const& peer_actor_ptr,
                                    std::unique_ptr<attachable>&& peer_observer)
    : sockfd(peer_socket)
    , peer(peer_ptr)
    , first_peer_actor(peer_actor_ptr)
    , attachable_ptr(std::move(peer_observer))
{
}

post_office_msg::add_server_socket::add_server_socket(native_socket_t ssockfd,
                                                      actor_ptr const& whom)
    : server_sockfd(ssockfd)
    , published_actor(whom)
{
}

post_office_msg::post_office_msg(native_socket_t arg0,
                                 process_information_ptr const& arg1,
                                 actor_proxy_ptr const& arg2,
                                 std::unique_ptr<attachable>&& arg3)
    : next(nullptr)
    , m_type(add_peer_type)
{
    new (&m_add_peer_msg) add_peer(arg0, arg1, arg2, std::move(arg3));
}

post_office_msg::post_office_msg(native_socket_t arg0, actor_ptr const& arg1)
    : next(nullptr)
    , m_type(add_server_socket_type)
{
    new (&m_add_server_socket) add_server_socket(arg0, arg1);
}

post_office_msg::post_office_msg(actor_proxy_ptr const& proxy_ptr)
    : next(nullptr)
    , m_type(proxy_exited_type)
{
    new (&m_proxy_exited) proxy_exited(proxy_ptr);
}

post_office_msg::~post_office_msg()
{
    switch (m_type)
    {
        case add_peer_type:
        {
            m_add_peer_msg.~add_peer();
            break;
        }
        case add_server_socket_type:
        {
            m_add_server_socket.~add_server_socket();
            break;
        }
        case proxy_exited_type:
        {
            m_proxy_exited.~proxy_exited();
            break;
        }
        default: throw std::logic_error("invalid post_office_msg type");
    }
}

} } // namespace cppa::detail
