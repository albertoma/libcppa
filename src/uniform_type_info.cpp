#include "cppa/config.hpp"

#include <map>
#include <set>
#include <locale>
#include <string>
#include <atomic>
#include <limits>
#include <cstring>
#include <cstdint>
#include <sstream>
#include <iostream>
#include <type_traits>

#include "cppa/atom.hpp"
#include "cppa/actor.hpp"
#include "cppa/object.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/announce.hpp"
#include "cppa/any_tuple.hpp"
#include "cppa/intrusive_ptr.hpp"
#include "cppa/uniform_type_info.hpp"

#include "cppa/util/duration.hpp"
#include "cppa/util/void_type.hpp"
#include "cppa/util/enable_if.hpp"
#include "cppa/util/disable_if.hpp"

#include "cppa/detail/demangle.hpp"
#include "cppa/detail/object_array.hpp"
#include "cppa/detail/actor_registry.hpp"
#include "cppa/detail/to_uniform_name.hpp"
#include "cppa/detail/addressed_message.hpp"
#include "cppa/detail/singleton_manager.hpp"
#include "cppa/detail/actor_proxy_cache.hpp"
#include "cppa/detail/uniform_type_info_map.hpp"
#include "cppa/detail/default_uniform_type_info_impl.hpp"

using std::cout;
using std::endl;
using cppa::util::void_type;

namespace std {

ostream& operator<<(ostream& o, cppa::actor_ptr const&) { return o; }
istream& operator>>(istream& i, cppa::actor_ptr&) { return i; }

ostream& operator<<(ostream& o, cppa::util::void_type const&) { return o; }
istream& operator>>(istream& i, cppa::util::void_type&) { return i; }

} // namespace std

namespace {

inline cppa::detail::uniform_type_info_map& uti_map()
{
    return *(cppa::detail::singleton_manager::get_uniform_type_info_map());
}

inline char const* raw_name(std::type_info const& tinfo)
{
#ifdef CPPA_WINDOWS
    return tinfo.raw_name();
#else
    return tinfo.name();
#endif
}

template<typename T>
struct is_signed
        : std::integral_constant<bool, std::numeric_limits<T>::is_signed>
{
};

template<typename T>
inline char const* raw_name()
{
    return raw_name(typeid(T));
}

typedef std::set<std::string> string_set;

template<typename Int>
void push(std::map<int, std::pair<string_set, string_set>>& ints,
          std::integral_constant<bool, true>)
{
    ints[sizeof(Int)].first.insert(raw_name<Int>());
}

template<typename Int>
void push(std::map<int, std::pair<string_set, string_set>>& ints,
          std::integral_constant<bool, false>)
{
    ints[sizeof(Int)].second.insert(raw_name<Int>());
}

template<typename Int>
void push(std::map<int, std::pair<string_set, string_set>>& ints)
{
    push<Int>(ints, is_signed<Int>());
}

template<typename Int0, typename Int1, typename... Ints>
void push(std::map<int, std::pair<string_set, string_set>>& ints)
{
    push<Int0>(ints, is_signed<Int0>());
    push<Int1, Ints...>(ints);
}

} // namespace <anonymous>

namespace cppa { namespace detail { //namespace {

const std::string nullptr_type_name = "@0";

void serialize_nullptr(serializer* sink)
{
    sink->begin_object(nullptr_type_name);
    sink->end_object();
}

void deserialize_nullptr(deserializer* source)
{
    source->begin_object(nullptr_type_name);
    source->end_object();
}

class void_type_tinfo : public util::abstract_uniform_type_info<void_type>
{

 protected:

    void serialize(void const*, serializer* sink) const
    {
        serialize_nullptr(sink);
    }

    void deserialize(void*, deserializer* source) const
    {
        std::string cname = source->seek_object();
        if (cname != name())
        {
            throw std::logic_error("wrong type name found");
        }
        deserialize_nullptr(source);
    }

};

class actor_ptr_tinfo : public util::abstract_uniform_type_info<actor_ptr>
{

 public:

    static void s_serialize(actor_ptr const& ptr,
                            serializer* sink,
                            std::string const& name)
    {
        if (!ptr)
        {
            serialize_nullptr(sink);
        }
        else
        {
            singleton_manager::get_actor_registry()->put(ptr->id(), ptr);
            primitive_variant ptup[3];
            ptup[0] = ptr->id();
            ptup[1] = ptr->parent_process().process_id();
            ptup[2] = to_string(ptr->parent_process().node_id());
            sink->begin_object(name);
            sink->write_tuple(3, ptup);
            sink->end_object();
        }
    }

    static void s_deserialize(actor_ptr& ptrref,
                              deserializer* source,
                              std::string const& name)
    {
        std::string cname = source->seek_object();
        if (cname != name)
        {
            if (cname == nullptr_type_name)
            {
                deserialize_nullptr(source);
                ptrref.reset();
            }
            else
            {
                throw std::logic_error("wrong type name found");
            }
        }
        else
        {
            primitive_variant ptup[3];
            primitive_type ptypes[] = { pt_uint32, pt_uint32, pt_u8string };
            source->begin_object(cname);
            source->read_tuple(3, ptypes, ptup);
            source->end_object();
            std::string const& nstr = get<std::string>(ptup[2]);
            // local actor?
            auto pinf = process_information::get();
            if (   pinf->process_id() == get<std::uint32_t>(ptup[1])
                && cppa::equal(nstr, pinf->node_id()))
            {
                auto id = get<std::uint32_t>(ptup[0]);
                ptrref = singleton_manager::get_actor_registry()->get(id);
                //ptrref = actor::by_id(get<std::uint32_t>(ptup[0]));
            }
            else
            {
                actor_proxy_cache::key_tuple key;
                std::get<0>(key) = get<std::uint32_t>(ptup[0]);
                std::get<1>(key) = get<std::uint32_t>(ptup[1]);
                node_id_from_string(nstr, std::get<2>(key));
                ptrref = detail::get_actor_proxy_cache().get(key);
            }
        }
    }

 protected:

    void serialize(void const* ptr, serializer* sink) const
    {
        s_serialize(*reinterpret_cast<actor_ptr const*>(ptr),//->get(),
                    sink,
                    name());
    }

    void deserialize(void* ptr, deserializer* source) const
    {
        s_deserialize(*reinterpret_cast<actor_ptr*>(ptr), source, name());
    }

};

class group_ptr_tinfo : public util::abstract_uniform_type_info<group_ptr>
{

 public:

    static void s_serialize(group_ptr const& ptr,
                            serializer* sink,
                            std::string const& name)
    {
        if (!ptr)
        {
            serialize_nullptr(sink);
        }
        else
        {
            sink->begin_object(name);
            sink->write_value(ptr->module_name());
            sink->write_value(ptr->identifier());
            sink->end_object();
        }
    }

    static void s_deserialize(group_ptr& ptrref,
                              deserializer* source,
                              std::string const& name)
    {
        std::string cname = source->seek_object();
        if (cname != name)
        {
            if (cname == nullptr_type_name)
            {
                deserialize_nullptr(source);
                ptrref.reset();
            }
            else
            {
                throw std::logic_error("wrong type name found");
            }
        }
        else
        {
            source->begin_object(name);
            auto modname = source->read_value(pt_u8string);
            auto groupid = source->read_value(pt_u8string);
            ptrref = group::get(get<std::string>(modname),
                                get<std::string>(groupid));
            source->end_object();
        }
    }

 protected:

    void serialize(void const* ptr, serializer* sink) const
    {
        s_serialize(*reinterpret_cast<group_ptr const*>(ptr),
                    sink,
                    name());
    }

    void deserialize(void* ptr, deserializer* source) const
    {
        s_deserialize(*reinterpret_cast<group_ptr*>(ptr), source, name());
    }

};

class channel_ptr_tinfo : public util::abstract_uniform_type_info<channel_ptr>
{

    std::string group_ptr_name;
    std::string actor_ptr_name;

 public:

    static void s_serialize(channel_ptr const& ptr,
                            serializer* sink,
                            std::string const& channel_type_name,
                            std::string const& actor_ptr_type_name,
                            std::string const& group_ptr_type_name)
    {
        sink->begin_object(channel_type_name);
        if (!ptr)
        {
            serialize_nullptr(sink);
        }
        else
        {
            auto aptr = ptr.downcast<actor>();
            group_ptr gptr;
            if (aptr)
            {
                actor_ptr_tinfo::s_serialize(aptr, sink, actor_ptr_type_name);
            }
            else if ((gptr = ptr.downcast<group>()))
            {
                group_ptr_tinfo::s_serialize(gptr, sink, group_ptr_type_name);
            }
            else
            {
                throw std::logic_error("channel is neither "
                                       "an actor nor a group");
            }
        }
        sink->end_object();
    }

    static void s_deserialize(channel_ptr& ptrref,
                              deserializer* source,
                              std::string const& name,
                              std::string const& actor_ptr_type_name,
                              std::string const& group_ptr_type_name)
    {
        std::string cname = source->seek_object();
        if (cname != name)
        {
            throw std::logic_error("wrong type name found");
        }
        source->begin_object(cname);
        std::string subobj = source->peek_object();
        if (subobj == actor_ptr_type_name)
        {
            actor_ptr tmp;
            actor_ptr_tinfo::s_deserialize(tmp, source, actor_ptr_type_name);
            ptrref = tmp;
        }
        else if (subobj == group_ptr_type_name)
        {
            group_ptr tmp;
            group_ptr_tinfo::s_deserialize(tmp, source, group_ptr_type_name);
            ptrref = tmp;
        }
        else if (subobj == nullptr_type_name)
        {
            (void) source->seek_object();
            deserialize_nullptr(source);
            ptrref.reset();
        }
        else
        {
            throw std::logic_error("unexpected type name: " + subobj);
        }
        source->end_object();
    }

 protected:

    void serialize(void const* instance, serializer* sink) const
    {
        s_serialize(*reinterpret_cast<channel_ptr const*>(instance),
                    sink,
                    name(),
                    actor_ptr_name,
                    group_ptr_name);
    }

    void deserialize(void* instance, deserializer* source) const
    {
        s_deserialize(*reinterpret_cast<channel_ptr*>(instance),
                      source,
                      name(),
                      actor_ptr_name,
                      group_ptr_name);
    }

 public:

    channel_ptr_tinfo() : group_ptr_name(to_uniform_name(typeid(group_ptr)))
                        , actor_ptr_name(to_uniform_name(typeid(actor_ptr)))
    {
    }

};

class any_tuple_tinfo : public util::abstract_uniform_type_info<any_tuple>
{

 public:

    static void s_serialize(any_tuple const& atup,
                            serializer* sink,
                            std::string const& name)
    {
        sink->begin_object(name);
        sink->begin_sequence(atup.size());
        for (size_t i = 0; i < atup.size(); ++i)
        {
            atup.utype_info_at(i).serialize(atup.at(i), sink);
        }
        sink->end_sequence();
        sink->end_object();
    }

    static void s_deserialize(any_tuple& atref,
                              deserializer* source,
                              std::string const& name)
    {
        auto result = new detail::object_array;
        auto str = source->seek_object();
        if (str != name) throw std::logic_error("invalid type found: " + str);
        source->begin_object(str);
        size_t tuple_size = source->begin_sequence();
        for (size_t i = 0; i < tuple_size; ++i)
        {
            auto tname = source->peek_object();
            auto utype = uniform_type_info::from(tname);
            result->push_back(utype->deserialize(source));
        }
        source->end_sequence();
        source->end_object();
        atref = any_tuple(result);
    }

 protected:

    void serialize(void const* instance, serializer* sink) const
    {
        s_serialize(*reinterpret_cast<any_tuple const*>(instance),sink,name());
    }

    void deserialize(void* instance, deserializer* source) const
    {
        s_deserialize(*reinterpret_cast<any_tuple*>(instance), source, name());
    }

};

class addr_msg_tinfo : public util::abstract_uniform_type_info<addressed_message>
{

    std::string any_tuple_name;
    std::string actor_ptr_name;
    std::string group_ptr_name;
    std::string channel_ptr_name;

 public:

    virtual void serialize(void const* instance, serializer* sink) const
    {
        addressed_message const& msg = *reinterpret_cast<addressed_message const*>(instance);
        any_tuple const& data = msg.content();
        sink->begin_object(name());
        actor_ptr_tinfo::s_serialize(msg.sender(), sink, actor_ptr_name);
        channel_ptr_tinfo::s_serialize(msg.receiver(),
                                       sink,
                                       channel_ptr_name,
                                       actor_ptr_name,
                                       group_ptr_name);
        any_tuple_tinfo::s_serialize(data, sink, any_tuple_name);
        sink->end_object();
    }

    virtual void deserialize(void* instance, deserializer* source) const
    {
        auto tname = source->seek_object();
        if (tname != name()) throw 42;
        source->begin_object(tname);
        addressed_message& msg = *reinterpret_cast<addressed_message*>(instance);
        //actor_ptr sender;
        //channel_ptr receiver;
        //any_tuple content;
        actor_ptr_tinfo::s_deserialize(msg.sender(), source, actor_ptr_name);
        channel_ptr_tinfo::s_deserialize(msg.receiver(),
                                         source,
                                         channel_ptr_name,
                                         actor_ptr_name,
                                         group_ptr_name);
        any_tuple_tinfo::s_deserialize(msg.content(), source, any_tuple_name);
        source->end_object();
        //*reinterpret_cast<any_tuple*>(instance) = any_tuple(sender,
        //                                                    receiver,
        //                                                    content);
    }

    addr_msg_tinfo() : any_tuple_name(to_uniform_name(typeid(any_tuple)))
                     , actor_ptr_name(to_uniform_name(typeid(actor_ptr)))
                     , group_ptr_name(to_uniform_name(typeid(group_ptr)))
                     , channel_ptr_name(to_uniform_name(typeid(channel_ptr)))
    {
    }

};

class atom_value_tinfo : public util::abstract_uniform_type_info<atom_value>
{

 public:

    virtual void serialize(void const* instance, serializer* sink) const
    {
        auto val = reinterpret_cast<atom_value const*>(instance);
        sink->begin_object(name());
        sink->write_value(static_cast<std::uint64_t>(*val));
        sink->end_object();
    }

    virtual void deserialize(void* instance, deserializer* source) const
    {
        auto val = reinterpret_cast<atom_value*>(instance);
        auto tname = source->seek_object();
        if (tname != name()) throw 42;
        source->begin_object(tname);
        auto ptval = source->read_value(pt_uint64);
        source->end_object();
        *val = static_cast<atom_value>(get<std::uint64_t>(ptval));
    }

};

class duration_tinfo : public util::abstract_uniform_type_info<util::duration>
{

    virtual void serialize(void const* instance, serializer* sink) const
    {
        auto val = reinterpret_cast<util::duration const*>(instance);
        sink->begin_object(name());
        sink->write_value(static_cast<std::uint32_t>(val->unit));
        sink->write_value(val->count);
        sink->end_object();
    }

    virtual void deserialize(void* instance, deserializer* source) const
    {
        auto val = reinterpret_cast<util::duration*>(instance);
        auto tname = source->seek_object();
        if (tname != name()) throw 42;
        source->begin_object(tname);
        auto unit_val = source->read_value(pt_uint32);
        auto count_val = source->read_value(pt_uint32);
        source->end_object();
        switch (get<std::uint32_t>(unit_val))
        {
            case 1:
                val->unit = util::time_unit::seconds;
                break;

            case 1000:
                val->unit = util::time_unit::milliseconds;
                break;

            case 1000000:
                val->unit = util::time_unit::microseconds;
                break;

            default:
                val->unit = util::time_unit::none;
                break;
        }
        val->count = get<std::uint32_t>(count_val);
    }

};

template<typename T>
class int_tinfo : public detail::default_uniform_type_info_impl<T>
{

 public:

    bool equals(std::type_info const& tinfo) const
    {
        // TODO: string comparsion sucks & is slow; find a nicer solution
        auto map_iter = uti_map().int_names().find(sizeof(T));
        string_set const& st = is_signed<T>::value ? map_iter->second.first
                                                   : map_iter->second.second;
        auto end = st.end();
        for (auto i = st.begin(); i != end; ++i)
        {
            if ((*i) == raw_name(tinfo)) return true;
        }
        return false;
    }

};

//} } } // namespace cppa::detail::<anonymous>
//namespace cppa { namespace detail {

using std::is_integral;
using util::enable_if;
using util::disable_if;

class uniform_type_info_map_helper
{

    friend class uniform_type_info_map;

    typedef uniform_type_info_map* this_ptr;

    static void insert(this_ptr d, uniform_type_info* uti,
                       std::set<std::string> const& tnames)
    {
        if (tnames.empty())
        {
            throw std::logic_error("tnames.empty()");
        }
        for (std::string const& tname : tnames)
        {
            d->m_by_rname.insert(std::make_pair(tname, uti));
        }
        d->m_by_uname.insert(std::make_pair(uti->name(), uti));
    }

    template<typename T>
    static inline void insert(this_ptr d,
                              std::set<std::string> const& tnames,
                              typename enable_if<is_integral<T>>::type* = 0)
    {
        //insert(new default_uniform_type_info_impl<T>(), tnames);
        insert(d, new int_tinfo<T>, tnames);
    }

    template<typename T>
    static inline void insert(this_ptr d,
                              std::set<std::string> const& tnames,
                              typename disable_if<is_integral<T>>::type* = 0)
    {
        insert(d, new default_uniform_type_info_impl<T>(), tnames);
    }

    template<typename T>
    static inline void insert(this_ptr d)
    {
        insert<T>(d, { std::string(raw_name<T>()) });
    }

    static void init(this_ptr d)
    {
        insert<std::string>(d);
        insert<std::u16string>(d);
        insert<std::u32string>(d);
        insert(d, new duration_tinfo, { raw_name<util::duration>() });
        insert(d, new any_tuple_tinfo, { raw_name<any_tuple>() });
        insert(d, new actor_ptr_tinfo, { raw_name<actor_ptr>() });
        insert(d, new group_ptr_tinfo, { raw_name<actor_ptr>() });
        insert(d, new channel_ptr_tinfo, { raw_name<channel_ptr>() });
        //insert(d, new message_tinfo, { raw_name<any_tuple>() });
        insert(d, new atom_value_tinfo, { raw_name<atom_value>() });
        insert(d, new addr_msg_tinfo, {raw_name<detail::addressed_message>() });
        insert(d, new void_type_tinfo, { raw_name<void_type>() });
        insert<float>(d);
        if (sizeof(double) == sizeof(long double))
        {
            std::string dbl = raw_name<double>();
            std::string ldbl = raw_name<long double>();
            insert<double>(d, { dbl, ldbl });
        }
        else
        {
            insert<double>(d);
            insert<long double>(d);
        }
        // first: signed
        // second: unsigned
        push<char,
             signed char,
             unsigned char,
             short,
             signed short,
             unsigned short,
             short int,
             signed short int,
             unsigned short int,
             int,
             signed int,
             unsigned int,
             long int,
             signed long int,
             unsigned long int,
             long,
             signed long,
             unsigned long,
             long long,
             signed long long,
             unsigned long long,
             wchar_t,
             char16_t,
             char32_t>(d->m_ints);
        insert<std::int8_t>(d, d->m_ints[sizeof(std::int8_t)].first);
        insert<std::uint8_t>(d, d->m_ints[sizeof(std::uint8_t)].second);
        insert<std::int16_t>(d, d->m_ints[sizeof(std::int16_t)].first);
        insert<std::uint16_t>(d, d->m_ints[sizeof(std::uint16_t)].second);
        insert<std::int32_t>(d, d->m_ints[sizeof(std::int32_t)].first);
        insert<std::uint32_t>(d, d->m_ints[sizeof(std::uint32_t)].second);
        insert<std::int64_t>(d, d->m_ints[sizeof(std::int64_t)].first);
        insert<std::uint64_t>(d, d->m_ints[sizeof(std::uint64_t)].second);
    }

};

uniform_type_info_map::uniform_type_info_map()
{
    uniform_type_info_map_helper::init(this);
}

uniform_type_info_map::~uniform_type_info_map()
{
    m_by_rname.clear();
    for (auto& kvp : m_by_uname)
    {
        delete kvp.second;
        kvp.second = nullptr;
    }
    m_by_uname.clear();
}

uniform_type_info* uniform_type_info_map::by_raw_name(std::string const& name) const
{
    auto i = m_by_rname.find(name);
    if (i != m_by_rname.end())
    {
        return i->second;
    }
    return nullptr;
}

uniform_type_info* uniform_type_info_map::by_uniform_name(std::string const& name) const
{
    auto i = m_by_uname.find(name);
    if (i != m_by_uname.end())
    {
        return i->second;
    }
    return nullptr;
}

bool uniform_type_info_map::insert(std::set<std::string> const& raw_names,
                                   uniform_type_info* what)
{
    if (m_by_uname.count(what->name()) > 0)
    {
        delete what;
        return false;
    }
    m_by_uname.insert(std::make_pair(what->name(), what));
    for (std::string const& plain_name : raw_names)
    {
        if (!m_by_rname.insert(std::make_pair(plain_name, what)).second)
        {
            std::string error_str = plain_name;
            error_str += " already mapped to an uniform_type_info";
            throw std::runtime_error(error_str);
        }
    }
    return true;
}

std::vector<uniform_type_info*> uniform_type_info_map::get_all() const
{
    std::vector<uniform_type_info*> result;
    result.reserve(m_by_uname.size());
    for (uti_map::value_type const& i : m_by_uname)
    {
        result.push_back(i.second);
    }
    return std::move(result);
}

} } // namespace cppa::detail

namespace cppa {

bool announce(std::type_info const& tinfo, uniform_type_info* utype)
{
    return uti_map().insert({ raw_name(tinfo) }, utype);
}

uniform_type_info::uniform_type_info(std::string const& uname) : m_name(uname)
{
}

uniform_type_info::~uniform_type_info()
{
}

object uniform_type_info::create() const
{
    return { new_instance(), this };
}

uniform_type_info const*
uniform_type_info::from(std::type_info const& tinf)
{
    auto result = uti_map().by_raw_name(raw_name(tinf));
    if (!result)
    {
        std::string error = "uniform_type_info::by_type_info(): ";
        error += detail::to_uniform_name(tinf);
        error += " is an unknown typeid name";
        throw std::runtime_error(error);
    }
    return result;
}

uniform_type_info* uniform_type_info::from(std::string const& name)
{
    auto result = uti_map().by_uniform_name(name);
    if (!result)
    {
        throw std::runtime_error(name + " is an unknown typeid name");
    }
    return result;
}

object uniform_type_info::deserialize(deserializer* from) const
{
    auto ptr = new_instance();
    deserialize(ptr, from);
    return { ptr, this };
}

std::vector<uniform_type_info*> uniform_type_info::instances()
{
    return uti_map().get_all();
}

uniform_type_info const* uniform_typeid(std::type_info const& tinfo)
{
    return uniform_type_info::from(tinfo);
}

} // namespace cppa
