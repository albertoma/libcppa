#include <set>
#include <stdexcept>

#include "cppa/any_tuple.hpp"
#include "cppa/detail/group_manager.hpp"
#include "cppa/util/shared_spinlock.hpp"
#include "cppa/util/shared_lock_guard.hpp"
#include "cppa/util/upgrade_lock_guard.hpp"

namespace {

using namespace cppa;

typedef detail::lock_guard<util::shared_spinlock> exclusive_guard;
typedef util::shared_lock_guard<util::shared_spinlock> shared_guard;
typedef util::upgrade_lock_guard<util::shared_spinlock> upgrade_guard;

class local_group_module;

class local_group : public group
{

    friend class local_group_module;

    util::shared_spinlock m_shared_mtx;
    std::set<channel_ptr> m_subscribers;

    // allow access to local_group_module only
    local_group(std::string&& gname) : group(std::move(gname), "local")
    {
    }

    local_group(std::string const& gname) : group(gname, "local")
    {
    }

 public:

    void enqueue(actor* sender, any_tuple const& msg) /*override*/
    {
        shared_guard guard(m_shared_mtx);
        for (auto i = m_subscribers.begin(); i != m_subscribers.end(); ++i)
        {
            const_cast<channel_ptr&>(*i)->enqueue(sender, msg);
        }
    }

    void enqueue(actor* sender, any_tuple&& msg) /*override*/
    {
        any_tuple tmp(std::move(msg));
        enqueue(sender, tmp);
    }

    virtual group::subscription subscribe(channel_ptr const& who)
    {
        group::subscription result;
        exclusive_guard guard(m_shared_mtx);
        if (m_subscribers.insert(who).second)
        {
            result.reset(new group::unsubscriber(who, this));
        }
        return result;
    }

    virtual void unsubscribe(channel_ptr const& who)
    {
        exclusive_guard guard(m_shared_mtx);
        m_subscribers.erase(who);
    }

};

class local_group_module : public group::module
{

    typedef group::module super;

    util::shared_spinlock m_mtx;
    std::map<std::string, group_ptr> m_instances;

 public:

    local_group_module() : super("local")
    {
    }

    group_ptr get(std::string const& group_name)
    {
        shared_guard guard(m_mtx);
        auto i = m_instances.find(group_name);
        if (i != m_instances.end())
        {
            return i->second;
        }
        else
        {
            group_ptr tmp(new local_group(group_name));
            // lifetime scope of uguard
            {
                upgrade_guard uguard(guard);
                auto p = m_instances.insert(std::make_pair(group_name, tmp));
                // someone might preempt us
                return p.first->second;
            }
        }
    }

};

} // namespace <anonymous>

namespace cppa { namespace detail {

group_manager::group_manager()
{
    std::unique_ptr<group::module> ptr(new local_group_module);
    m_mmap.insert(std::make_pair(std::string("local"), std::move(ptr)));
}

intrusive_ptr<group> group_manager::get(std::string const& module_name,
                                        std::string const& group_identifier)
{
    // lifetime scope of guard
    {
        detail::lock_guard<detail::mutex> guard(m_mmap_mtx);
        auto i = m_mmap.find(module_name);
        if (i != m_mmap.end())
        {
            return (i->second)->get(group_identifier);
        }
    }
    std::string error_msg = "no module named \"";
    error_msg += module_name;
    error_msg += "\" found";
    throw std::logic_error(error_msg);
}

void group_manager::add_module(group::module* mod)
{
    std::string const& mname = mod->name();
    std::unique_ptr<group::module> mptr(mod);
    // lifetime scope of guard
    {
        detail::lock_guard<detail::mutex> guard(m_mmap_mtx);
        if (m_mmap.insert(std::make_pair(mname, std::move(mptr))).second)
        {
            return; // success; don't throw
        }
    }
    std::string error_msg = "module name \"";
    error_msg += mname;
    error_msg += "\" already defined";
    throw std::logic_error(error_msg);
}

} } // namespace cppa::detail
