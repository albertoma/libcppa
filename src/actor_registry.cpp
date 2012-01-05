#include <mutex>
#include <limits>
#include <stdexcept>

#include "cppa/attachable.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

namespace {

typedef cppa::detail::lock_guard<cppa::util::shared_spinlock> exclusive_guard;
typedef cppa::util::shared_lock_guard<cppa::util::shared_spinlock> shared_guard;
typedef cppa::util::upgrade_lock_guard<cppa::util::shared_spinlock> upgrade_guard;

} // namespace <anonymous>

namespace cppa { namespace detail {

actor_registry::actor_registry() : m_running(0), m_ids(1)
{
}

actor_ptr actor_registry::get(actor_id key) const
{
    shared_guard guard(m_instances_mtx);
    auto i = m_instances.find(key);
    if (i != m_instances.end())
    {
        return i->second;
    }
    return nullptr;
}

void actor_registry::put(actor_id key, actor_ptr const& value)
{
    bool add_attachable = false;
    if (value != nullptr)
    {
        shared_guard guard(m_instances_mtx);
        auto i = m_instances.find(key);
        if (i == m_instances.end())
        //if (i == m_map.end() || value != i->second)
        {
            upgrade_guard uguard(guard);
            m_instances.insert(std::make_pair(key, value));
            add_attachable = true;
        }
    }
    if (add_attachable)
    {
        struct eraser : attachable
        {
            actor_id m_id;
            actor_registry* m_singleton;
            eraser(actor_id id, actor_registry* s) : m_id(id), m_singleton(s)
            {
            }
            void detach(std::uint32_t)
            {
                m_singleton->erase(m_id);
            }
        };
        const_cast<actor_ptr&>(value)->attach(new eraser(value->id(), this));
    }
}

void actor_registry::erase(actor_id key)
{
    exclusive_guard guard(m_instances_mtx);
    m_instances.insert(std::pair<actor_id, actor_ptr>(key, nullptr));
    //m_instances.erase(key);
}

std::uint32_t actor_registry::next_id()
{
    return m_ids.fetch_add(1);
}

void actor_registry::inc_running()
{
    ++m_running;
}

size_t actor_registry::running() const
{
    return m_running.load();
}

void actor_registry::dec_running()
{
    size_t new_val = --m_running;
    if (new_val == std::numeric_limits<size_t>::max())
    {
        throw std::underflow_error("actor_count::dec()");
    }
    else if (new_val <= 1)
    {
        unique_lock<mutex> guard(m_running_mtx);
        m_running_cv.notify_all();
    }
}

void actor_registry::await_running_count_equal(size_t expected)
{
    unique_lock<mutex> guard(m_running_mtx);
    while (m_running.load() != expected)
    {
        m_running_cv.wait(guard);
    }
}

} } // namespace cppa::detail
