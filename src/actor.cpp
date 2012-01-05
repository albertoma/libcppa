#include "cppa/config.hpp"

#include <map>
#include <mutex>
#include <atomic>
#include <stdexcept>

#include "cppa/actor.hpp"

#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"

#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/singleton_manager.hpp"

namespace {

inline cppa::detail::actor_registry& registry()
{
    return *(cppa::detail::singleton_manager::get_actor_registry());
}

} // namespace <anonymous>

namespace cppa {

actor::actor(std::uint32_t aid, process_information_ptr const& pptr)
    : m_is_proxy(true), m_id(aid), m_parent_process(pptr)
{
    if (!pptr)
    {
        throw std::logic_error("parent == nullptr");
    }
}

actor::actor(process_information_ptr const& pptr)
    : m_is_proxy(false), m_id(registry().next_id()), m_parent_process(pptr)
{
    if (!pptr)
    {
        throw std::logic_error("parent == nullptr");
    }
}

actor::~actor()
{
}

void actor::join(group_ptr& what)
{
    if (!what) return;
    attach(what->subscribe(this));
}

void actor::leave(group_ptr const& what)
{
    if (!what) return;
    attachable::token group_token(typeid(group::unsubscriber), what.get());
    detach(group_token);
}

void actor::link_to(intrusive_ptr<actor>&& other)
{
    intrusive_ptr<actor> tmp(std::move(other));
    link_to(tmp);
}

void actor::unlink_from(intrusive_ptr<actor>&& other)
{
    intrusive_ptr<actor> tmp(std::move(other));
    unlink_from(tmp);
}

bool actor::remove_backlink(intrusive_ptr<actor>&& to)
{
    intrusive_ptr<actor> tmp(std::move(to));
    return remove_backlink(tmp);
}

bool actor::establish_backlink(intrusive_ptr<actor>&& to)
{
    intrusive_ptr<actor> tmp(std::move(to));
    return establish_backlink(tmp);
}

} // namespace cppa
