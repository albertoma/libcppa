#ifndef ACTOR_REGISTRY_HPP
#define ACTOR_REGISTRY_HPP

#include <map>
#include <atomic>
#include <cstdint>

#include "cppa/actor.hpp"
#include "cppa/attachable.hpp"
#include "cppa/detail/thread.hpp"
#include "cppa/util/shared_spinlock.hpp"

namespace cppa { namespace detail {

class actor_registry
{

 public:

    actor_registry();

    // return nullptr if the actor wasn't put *or* finished execution
    actor_ptr get(actor_id key) const;

    void put(actor_id key, actor_ptr const& value);

    void erase(actor_id key);

    // gets the next free actor id
    actor_id next_id();

    // increases running-actors-count by one
    void inc_running();

    // decreases running-actors-count by one
    void dec_running();

    size_t running() const;

    // blocks the caller until running-actors-count becomes @p expected
    void await_running_count_equal(size_t expected);

 private:

    std::atomic<size_t> m_running;
    std::atomic<std::uint32_t> m_ids;

    mutex m_running_mtx;
    condition_variable m_running_cv;

    mutable util::shared_spinlock m_instances_mtx;
    std::map<std::uint32_t, actor_ptr> m_instances;

};

} } // namespace cppa::detail

#endif // ACTOR_REGISTRY_HPP
