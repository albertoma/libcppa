#ifndef DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
#define DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP

#include "cppa/anything.hpp"

#include "cppa/util/rm_ref.hpp"
#include "cppa/util/void_type.hpp"
#include "cppa/util/conjunction.hpp"
#include "cppa/util/disjunction.hpp"
#include "cppa/util/is_iterable.hpp"
#include "cppa/util/is_primitive.hpp"
#include "cppa/util/is_forward_iterator.hpp"
#include "cppa/util/abstract_uniform_type_info.hpp"

#include "cppa/detail/map_member.hpp"
#include "cppa/detail/list_member.hpp"
#include "cppa/detail/type_to_ptype.hpp"
#include "cppa/detail/primitive_member.hpp"

namespace cppa { namespace detail {

template<typename T>
class is_stl_compatible_list
{

    template<class C>
    static bool cmp_help_fun
    (
        // mutable pointer
        C* mc,
        // check if there's a 'void push_back()' that takes C::value_type
        decltype(mc->push_back(typename C::value_type()))*            = nullptr,
        // check if there's a 'size()' member function returning an integer
        typename util::enable_if<std::is_integral<decltype(mc->size())>>::type* = nullptr
    )
    {
        return true;
    }

    // SFNINAE default
    static void cmp_help_fun(void*) { }

    typedef decltype(cmp_help_fun(static_cast<T*>(nullptr))) result_type;

 public:

    static const bool value =    util::is_iterable<T>::value
                              && std::is_same<bool, result_type>::value;

};

template<typename T>
class is_stl_compatible_map
{

    template<class C>
    static bool cmp_help_fun
    (
        // mutable pointer
        C* mc,
        // check if there's an 'insert()' that takes C::value_type
        decltype(mc->insert(typename C::value_type()))*               = nullptr,
            // check if there's a 'size()' member function returning an integer
        typename util::enable_if<std::is_integral<decltype(mc->size())>>::type* = nullptr
    )
    {
        return true;
    }

    static void cmp_help_fun(...) { }

    typedef decltype(cmp_help_fun(static_cast<T*>(nullptr))) result_type;

 public:

    static const bool value =    util::is_iterable<T>::value
                              && std::is_same<bool, result_type>::value;

};

template<typename T>
class default_uniform_type_info_impl : public util::abstract_uniform_type_info<T>
{

    template<typename X>
    struct is_invalid
    {
        static const bool value =    !util::is_primitive<X>::value
                                  && !is_stl_compatible_map<X>::value
                                  && !is_stl_compatible_list<X>::value;
    };

    class member
    {

        uniform_type_info* m_meta;

        std::function<void (uniform_type_info const*,
                            void const*,
                            serializer*              )> m_serialize;

        std::function<void (uniform_type_info const*,
                            void*,
                            deserializer*            )> m_deserialize;

        member(member const&) = delete;
        member& operator=(member const&) = delete;

        void swap(member& other)
        {
            std::swap(m_meta, other.m_meta);
            std::swap(m_serialize, other.m_serialize);
            std::swap(m_deserialize, other.m_deserialize);
        }

        template<typename S, class D>
        member(uniform_type_info* mtptr, S&& s, D&& d)
            : m_meta(mtptr)
            , m_serialize(std::forward<S>(s))
            , m_deserialize(std::forward<D>(d))
        {
        }

     public:

        template<typename R, class C>
        member(uniform_type_info* mtptr, R C::*mem_ptr) : m_meta(mtptr)
        {
            m_serialize = [mem_ptr] (uniform_type_info const* mt,
                                     void const* obj,
                                     serializer* s)
            {
                mt->serialize(&(*reinterpret_cast<C const*>(obj).*mem_ptr), s);
            };
            m_deserialize = [mem_ptr] (uniform_type_info const* mt,
                                       void* obj,
                                       deserializer* d)
            {
                mt->deserialize(&(*reinterpret_cast<C*>(obj).*mem_ptr), d);
            };
        }

        template<typename GRes, typename SRes, typename SArg, class C>
        member(uniform_type_info* mtptr,
               GRes (C::*getter)() const,
               SRes (C::*setter)(SArg)) : m_meta(mtptr)
        {
            typedef typename util::rm_ref<GRes>::type getter_result;
            typedef typename util::rm_ref<SArg>::type setter_arg;
            static_assert(std::is_same<getter_result, setter_arg>::value,
                          "getter result doesn't match setter argument");
            m_serialize = [getter] (uniform_type_info const* mt,
                                    void const* obj,
                                    serializer* s)
            {
                GRes v = (*reinterpret_cast<C const*>(obj).*getter)();
                mt->serialize(&v, s);
            };
            m_deserialize = [setter] (uniform_type_info const* mt,
                                      void* obj,
                                      deserializer* d)
            {
                setter_arg value;
                mt->deserialize(&value, d);
                (*reinterpret_cast<C*>(obj).*setter)(value);
            };
        }

        member(member&& other) : m_meta(nullptr)
        {
            swap(other);
        }

        // a member that's not a member at all, but "forwards"
        // the 'self' pointer to make use of *_member implementations
        static member fake_member(uniform_type_info* mtptr)
        {
            return {
                mtptr,
                [] (uniform_type_info const* mt, void const* obj, serializer* s)
                {
                    mt->serialize(obj, s);
                },
                [] (uniform_type_info const* mt, void* obj, deserializer* d)
                {
                    mt->deserialize(obj, d);
                }
            };
        }

        ~member()
        {
            delete m_meta;
        }

        member& operator=(member&& other)
        {
            swap(other);
            return *this;
        }

        inline void serialize(void const* parent, serializer* s) const
        {
            m_serialize(m_meta, parent, s);
        }

        inline void deserialize(void* parent, deserializer* d) const
        {
            m_deserialize(m_meta, parent, d);
        }

    };

    std::vector<member> m_members;

    // terminates recursion
    inline void push_back() { }

    // pr.first = member pointer
    // pr.second = meta object to handle pr.first
    template<typename R, class C, typename... Args>
    void push_back(std::pair<R C::*, util::abstract_uniform_type_info<R>*> pr,
                   Args&&... args)
    {
        m_members.push_back({ pr.second, pr.first });
        push_back(std::forward<Args>(args)...);
    }

    // pr.first = getter / setter pair
    // pr.second = meta object to handle pr.first
    template<typename GRes, typename SRes, typename SArg, typename C, typename... Args>
    void push_back(const std::pair<std::pair<GRes (C::*)() const, SRes (C::*)(SArg)>,
                   util::abstract_uniform_type_info<typename util::rm_ref<GRes>::type>*>& pr,
                   Args&&... args)
    {
        m_members.push_back({ pr.second, pr.first.first, pr.first.second });
        push_back(std::forward<Args>(args)...);
    }

    // pr.first = getter member const function pointer
    // pr.second = setter member function pointer
    template<typename GRes, typename SRes, typename SArg, class C, typename... Args>
    typename util::enable_if<util::is_primitive<typename util::rm_ref<GRes>::type> >::type
    push_back(const std::pair<GRes (C::*)() const, SRes (C::*)(SArg)>& pr,
              Args&&... args)
    {
        typedef typename util::rm_ref<GRes>::type memtype;
        m_members.push_back({ new primitive_member<memtype>(), pr.first, pr.second });
        push_back(std::forward<Args>(args)...);
    }

    template<typename R, class C, typename... Args>
    typename util::enable_if<util::is_primitive<R> >::type
    push_back(R C::*mem_ptr, Args&&... args)
    {
        m_members.push_back({ new primitive_member<R>(), mem_ptr });
        push_back(std::forward<Args>(args)...);
    }

    template< typename R, class C,typename... Args>
    typename util::enable_if<is_stl_compatible_list<R> >::type
    push_back(R C::*mem_ptr, Args&&... args)
    {
        m_members.push_back({ new list_member<R>(), mem_ptr });
        push_back(std::forward<Args>(args)...);
    }

    template<typename R, class C, typename... Args>
    typename util::enable_if<is_stl_compatible_map<R> >::type
    push_back(R C::*mem_ptr, Args&&... args)
    {
        m_members.push_back({ new map_member<R>(), mem_ptr });
        push_back(std::forward<Args>(args)...);
    }

    template<typename R, class C, typename... Args>
    typename util::enable_if<is_invalid<R>>::type
    push_back(R C::*mem_ptr, Args&&... args)
    {
        static_assert(util::is_primitive<R>::value,
                      "T is neither a primitive type nor a "
                      "stl-compliant list/map");
    }

    template<typename P>
    void init_(typename util::enable_if<util::is_primitive<P>>::type* = 0)
    {
        m_members.push_back(member::fake_member(new primitive_member<P>()));
    }

    template<typename Map>
    void init_(typename util::enable_if<is_stl_compatible_map<Map>>::type* = 0)
    {
        m_members.push_back(member::fake_member(new map_member<Map>));
    }

    template<typename List>
    void init_(typename util::enable_if<is_stl_compatible_list<List>>::type* = 0)
    {
        m_members.push_back(member::fake_member(new list_member<List>));
    }

    template<typename X>
    void init_(typename util::enable_if<is_invalid<X>>::type* = 0)
    {
        // T is neither primitive nor a STL compliant list/map,
        // so it has to be an announced type
        static_assert(util::is_primitive<X>::value,
                      "T is neither a primitive type nor a "
                      "stl-compliant list/map");
    }

 public:

    template<typename... Args>
    default_uniform_type_info_impl(Args&&... args)
    {
        push_back(std::forward<Args>(args)...);
    }

    default_uniform_type_info_impl()
    {
        init_<T>();
        if (m_members.size() != 1)
        {
            throw std::logic_error("no fake member added");
        }
    }

    void serialize(void const* obj, serializer* s) const
    {
        s->begin_object(this->name());
        for (auto& m : m_members)
        {
            m.serialize(obj, s);
        }
        s->end_object();
    }

    void deserialize(void* obj, deserializer* d) const
    {
        std::string cname = d->seek_object();
        if (cname != this->name())
        {
            throw std::logic_error("wrong type name found");
        }
        d->begin_object(this->name());
        for (auto& m : m_members)
        {
            m.deserialize(obj, d);
        }
        d->end_object();
    }

};

} } // namespace detail

#endif // DEFAULT_UNIFORM_TYPE_INFO_IMPL_HPP
