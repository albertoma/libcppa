#include <sstream>
#include "cppa/exception.hpp"

#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>

namespace {

std::string ae_what(std::uint32_t reason)
{
    std::ostringstream oss;
    oss << "actor exited with reason " << reason;
    return oss.str();
}

std::string be_what(int err_code)
{
    switch (err_code)
    {
    case EACCES: return "EACCES: address is protected; root access needed";
    case EADDRINUSE: return "EADDRINUSE: address is already in use";
    case EBADF: return "EBADF: no valid socket descriptor";
    case EINVAL: return "EINVAL: socket already bound to an address";
    case ENOTSOCK: return "ENOTSOCK: file descriptor given";
    default: break;
    }
    std::stringstream oss;
    oss << "an unknown error occurred (code: " << err_code << ")";
    return oss.str();
}

} // namespace <anonymous>


namespace cppa {

exception::exception(const std::string &what_str) : m_what(what_str)
{
}

exception::exception(std::string&& what_str) : m_what(std::move(what_str))
{
}

exception::~exception() throw()
{
}

char const* exception::what() const throw()
{
    return m_what.c_str();
}

actor_exited::actor_exited(std::uint32_t reason) : exception(ae_what(reason))
{
    m_reason = reason;
}

network_error::network_error(std::string const& str) : exception(str)
{
}

network_error::network_error(std::string&& str)
    : exception(std::move(str))
{
}

bind_failure::bind_failure(int err_code) : network_error(be_what(err_code))
{
    m_errno = err_code;
}

} // namespace cppa
