#ifndef ACTOR_HPP
#define ACTOR_HPP

#include <memory>
#include <cstdint>
#include <type_traits>

#include "cppa/group.hpp"
#include "cppa/channel.hpp"
#include "cppa/attachable.hpp"
#include "cppa/process_information.hpp"

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/enable_if.hpp"

namespace cppa {

class serializer;
class deserializer;

typedef std::uint32_t actor_id;

/**
 * @brief Base class for all actor implementations.
 */
class actor : public channel
{

    bool m_is_proxy;
    std::uint32_t m_id;
    process_information_ptr m_parent_process;

 protected:

    actor(process_information_ptr const& parent = process_information::get());

    actor(std::uint32_t aid,
          process_information_ptr const& parent = process_information::get());

 public:

    ~actor();

    /**
     * @brief Attaches @p ptr to this actor
     *        (the actor takes ownership of @p ptr).
     *
     * The actor will call <tt>ptr->detach(...)</tt> on exit or immediately
     * if he already exited.
     *
     * @returns @c true if @p ptr was successfully attached to the actor;
     *          otherwise (actor already exited) @p false.
     */
    virtual bool attach(attachable* ptr) = 0;

    /**
     * @brief Attaches the functor or function @p ftor to this actor.
     *
     * The actor executes <tt>ftor()</tt> on exit or immediatley if he
     * already exited.
     * @returns @c true if @p ftor was successfully attached to the actor;
     *          otherwise (actor already exited) @p false.
     */
    template<typename F>
    bool attach_functor(F&& ftor);

    /**
     * @brief Detaches the first attached object that matches @p what.
     */
    virtual void detach(attachable::token const& what) = 0;

    template<typename T>
    bool attach(std::unique_ptr<T>&& ptr,
            typename util::enable_if<std::is_base_of<attachable,T>>::type* = 0);

    /**
     * @brief Forces this actor to subscribe to the group @p what.
     *
     * The group will be unsubscribed if the actor finishes execution.
     * @param what Group instance that should be joined.
     */
    void join(group_ptr& what);

    /**
     * @brief Forces this actor to leave the group @p what.
     * @param what Joined group that should be leaved.
     * @note Groups are leaved automatically if the Actor finishes
     *       execution.
     */
    void leave(group_ptr const& what);

    /**
     * @brief Links this actor to @p other.
     * @param other Actor instance that whose execution is coupled to the
     *              execution of this Actor.
     */
    virtual void link_to(intrusive_ptr<actor>& other) = 0;

    /**
     * @brief Unlinks this actor from @p other.
     * @param oter Linked Actor.
     * @note Links are automatically removed if the Actor finishes execution.
     */
    virtual void unlink_from(intrusive_ptr<actor>& other) = 0;

    /**
     * @brief Establishes a link relation between this actor and @p other.
     * @param other Actor instance that wants to link against this Actor.
     * @returns @c true if this actor is running and added @p other to its
     *          list of linked actors; otherwise @c false.
     */
    virtual bool establish_backlink(intrusive_ptr<actor>& other) = 0;

    /**
     * @brief Removes a link relation between this actor and @p other.
     * @param other Actor instance that wants to unlink from this Actor.
     * @returns @c true if this actor is running and removed @p other
     *          from its list of linked actors; otherwise @c false.
     */
    virtual bool remove_backlink(intrusive_ptr<actor>& other) = 0;

    // rvalue support
    /**
     * @copydoc link_to(intrusive_ptr<actor>&)
     * Support for rvalue references.
     */
    void link_to(intrusive_ptr<actor>&& other);

    /**
     * @copydoc unlink_from(intrusive_ptr<actor>&)
     * Support for rvalue references.
     */
    void unlink_from(intrusive_ptr<actor>&& other);

    /**
     * @copydoc remove_backlink(intrusive_ptr<actor>&)
     * Support for rvalue references.
     */
    bool remove_backlink(intrusive_ptr<actor>&& to);

    /**
     * @copydoc establish_backlink(intrusive_ptr<actor>&)
     * Support for rvalue references.
     */
    bool establish_backlink(intrusive_ptr<actor>&& to);

    /**
     * @brief Gets the {@link process_information} of the parent process.
     * @returns The {@link process_information} of the parent process.
     */
    inline process_information const& parent_process() const;

    /**
     * @brief Gets the {@link process_information} pointer
     *        of the parent process.
     * @returns A pointer to the {@link process_information}
     *          of the parent process.
     */
    inline process_information_ptr parent_process_ptr() const;

    /**
     * @brief Gets an integer value that uniquely identifies this Actor in
     *        the process it's executed in.
     * @returns The unique identifier of this actor.
     */
    inline std::uint32_t id() const;

    inline bool is_proxy() const;

};

/**
 * @brief A smart pointer type that manages instances of {@link actor}.
 */
typedef intrusive_ptr<actor> actor_ptr;

/******************************************************************************
 *             inline and template member function implementations            *
 ******************************************************************************/

inline process_information const& actor::parent_process() const
{
    return *m_parent_process;
}

inline process_information_ptr actor::parent_process_ptr() const
{
    return m_parent_process;
}

inline std::uint32_t actor::id() const
{
    return m_id;
}

inline bool actor::is_proxy() const
{
    return m_is_proxy;
}

template<typename T>
bool actor::attach(std::unique_ptr<T>&& ptr,
                typename util::enable_if<std::is_base_of<attachable,T>>::type*)
{
    return attach(static_cast<attachable*>(ptr.release()));
}

template<class F>
class functor_attachable : public attachable
{

    F m_functor;

 public:

    template<class FArg>
    functor_attachable(FArg&& arg) : m_functor(std::forward<FArg>(arg))
    {
    }

    virtual void detach(std::uint32_t reason)
    {
        m_functor(reason);
    }

};

template<typename F>
bool actor::attach_functor(F&& ftor)
{
    typedef typename util::rm_ref<F>::type f_type;
    return attach(new functor_attachable<f_type>(std::forward<F>(ftor)));
}

} // namespace cppa

#endif // ACTOR_HPP
