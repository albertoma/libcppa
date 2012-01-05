#include <stack>
#include <sstream>
#include <algorithm>

#include "cppa/atom.hpp"
#include "cppa/object.hpp"
#include "cppa/to_string.hpp"
#include "cppa/serializer.hpp"
#include "cppa/from_string.hpp"
#include "cppa/deserializer.hpp"
#include "cppa/primitive_variant.hpp"
#include "cppa/uniform_type_info.hpp"

namespace cppa {

namespace {

class string_serializer : public serializer
{

    std::ostream& out;

    struct pt_writer
    {

        std::ostream& out;

        pt_writer(std::ostream& mout) : out(mout) { }

        template<typename T>
        void operator()(T const& value)
        {
            out << value;
        }

        void operator()(std::string const& str)
        {
            out << "\"";// << str << "\"";
            for (char c : str)
            {
                if (c == '"') out << "\\\"";
                else out << c;
            }
            out << '"';
        }

        void operator()(std::u16string const&) { }

        void operator()(std::u32string const&) { }

    };

    bool m_after_value;
    bool m_obj_just_opened;
    std::stack<std::string> m_open_objects;

    inline void clear()
    {
        if (m_after_value)
        {
            out << ", ";
            m_after_value = false;
        }
        else if (m_obj_just_opened)
        {
            out << " ( ";
            m_obj_just_opened = false;
        }
    }

 public:

    string_serializer(std::ostream& mout)
        : out(mout), m_after_value(false), m_obj_just_opened(false)
    {
    }

    void begin_object(std::string const& type_name)
    {
        clear();
        m_open_objects.push(type_name);
        out << type_name;// << " ( ";
        m_obj_just_opened = true;
    }
    void end_object()
    {
        if (m_obj_just_opened)
        {
            // no open brackets to close
            m_obj_just_opened = false;
        }
        else
        {
            out << (m_after_value ? " )" : ")");
        }
        m_after_value = true;
        if (!m_open_objects.empty())
        {
            m_open_objects.pop();
        }
    }

    void begin_sequence(size_t)
    {
        clear();
        out << "{ ";
    }

    void end_sequence()
    {
        out << (m_after_value ? " }" : "}");
        m_after_value = true;
    }

    void write_value(primitive_variant const& value)
    {
        clear();
        if (m_open_objects.empty())
        {
            throw std::runtime_error("write_value(): m_open_objects.empty()");
        }
        if (m_open_objects.top() == "@atom")
        {
            if (value.ptype() != pt_uint64)
            {
                throw std::runtime_error("expected uint64 value after @atom");
            }
            // write atoms as strings instead of integer values
            auto av = static_cast<atom_value>(get<std::uint64_t>(value));
            (pt_writer(out))(to_string(av));
        }
        else
        {
            value.apply(pt_writer(out));
        }
        m_after_value = true;
    }

    void write_tuple(size_t size, primitive_variant const* values)
    {
        clear();
        out << "{";
        primitive_variant const* end = values + size;
        for ( ; values != end; ++values)
        {
            write_value(*values);
        }
        out << (m_after_value ? " }" : "}");
    }

};

class string_deserializer : public deserializer
{

    std::string m_str;
    std::string::iterator m_pos;
    //size_t m_obj_count;
    std::stack<bool> m_obj_had_left_parenthesis;
    std::stack<std::string> m_open_objects;

    void skip_space_and_comma()
    {
        while (*m_pos == ' ' || *m_pos == ',') ++m_pos;
    }

    void throw_malformed(std::string const& error_msg)
    {
        throw std::logic_error("malformed string: " + error_msg);
    }

    void consume(char c)
    {
        skip_space_and_comma();
        if (*m_pos != c)
        {
            std::string error;
            error += "expected '";
            error += c;
            error += "' found '";
            error += *m_pos;
            error += "'";
            if (m_open_objects.empty() == false)
            {
                error += "during deserialization an instance of ";
                error += m_open_objects.top();
            }
            throw_malformed(error);
        }
        ++m_pos;
    }

    bool try_consume(char c)
    {
        skip_space_and_comma();
        if (*m_pos == c)
        {
            ++m_pos;
            return true;
        }
        return false;
    }

    inline std::string::iterator next_delimiter()
    {
        return std::find_if(m_pos, m_str.end(), [] (char c) -> bool {
            switch (c)
            {
             case '(':
             case ')':
             case '{':
             case '}':
             case ' ':
             case ',': return true;
             default : return false;
            }
        });
    }

    void integrity_check()
    {
        if (m_open_objects.empty() || m_obj_had_left_parenthesis.empty())
        {
            throw_malformed("missing begin_object()");
        }
        if (m_obj_had_left_parenthesis.top() == false)
        {
            throw_malformed("expected left parenthesis after "
                            "begin_object call or void value");
        }
    }

 public:

    string_deserializer(std::string const& str) : m_str(str)
    {
        m_pos = m_str.begin();
    }

    string_deserializer(std::string&& str) : m_str(std::move(str))
    {
        m_pos = m_str.begin();
    }

    std::string seek_object()
    {
        skip_space_and_comma();
        auto substr_end = next_delimiter();
        if (m_pos == substr_end)
        {
            throw_malformed("could not seek object type name");
        }
        std::string result(m_pos, substr_end);
        m_pos = substr_end;
        return result;
    }

    std::string peek_object()
    {
        std::string result = seek_object();
        // restore position in stream
        m_pos -= result.size();
        return result;
    }

    void begin_object(std::string const& type_name)
    {
        m_open_objects.push(type_name);
        //++m_obj_count;
        skip_space_and_comma();
        m_obj_had_left_parenthesis.push(try_consume('('));
        //consume('(');
    }

    void end_object()
    {
        if (m_obj_had_left_parenthesis.empty())
        {
            throw_malformed("missing begin_object()");
        }
        else
        {
            if (m_obj_had_left_parenthesis.top() == true)
            {
                consume(')');
            }
            m_obj_had_left_parenthesis.pop();
        }
        if (m_open_objects.empty())
        {
            throw std::runtime_error("no object to end");
        }
        m_open_objects.pop();
        if (m_open_objects.empty())
        {
            skip_space_and_comma();
            if (m_pos != m_str.end())
            {
                throw_malformed("expected end of of string");
            }
        }
    }

    size_t begin_sequence()
    {
        integrity_check();
        consume('{');
        auto list_end = std::find(m_pos, m_str.end(), '}');
        return std::count(m_pos, list_end, ',') + 1;
    }

    void end_sequence()
    {
        integrity_check();
        consume('}');
    }

    struct from_string
    {
        std::string const& str;
        from_string(std::string const& s) : str(s) { }
        template<typename T>
        void operator()(T& what)
        {
            std::istringstream s(str);
            s >> what;
        }
        void operator()(std::string& what)
        {
            what = str;
        }
        void operator()(std::u16string&) { }
        void operator()(std::u32string&) { }
    };

    primitive_variant read_value(primitive_type ptype)
    {
        integrity_check();
        if (m_open_objects.top() == "@atom")
        {
            if (ptype != pt_uint64)
            {
                throw_malformed("expected read of pt_uint64 after @atom");
            }
            auto str_val = get<std::string>(read_value(pt_u8string));
            if (str_val.size() > 10)
            {
                throw_malformed("atom string size > 10");
            }
            return detail::atom_val(str_val.c_str());
        }
        skip_space_and_comma();
        std::string::iterator substr_end;
        auto find_if_cond = [] (char c) -> bool
        {
            switch (c)
            {
             case ')':
             case '}':
             case ' ':
             case ',': return true;
             default : return false;
            }
        };
        if (ptype == pt_u8string)
        {
            if (*m_pos == '"')
            {
                // skip leading "
                ++m_pos;
                char last_char = '"';
                auto find_if_str_cond = [&last_char] (char c) -> bool
                {
                    if (c == '"' && last_char != '\\')
                    {
                        return true;
                    }
                    last_char = c;
                    return false;
                };
                substr_end = std::find_if(m_pos, m_str.end(), find_if_str_cond);
            }
            else
            {
                substr_end = std::find_if(m_pos, m_str.end(), find_if_cond);
            }
        }
        else
        {
            substr_end = std::find_if(m_pos, m_str.end(), find_if_cond);
        }
        if (substr_end == m_str.end())
        {
            throw std::logic_error("malformed string (unterminated value)");
        }
        std::string substr(m_pos, substr_end);
        m_pos += substr.size();
        if (ptype == pt_u8string)
        {
            // skip trailing "
            if (*m_pos != '"')
            {
                std::string error_msg;
                error_msg  = "malformed string, expected '\"' found '";
                error_msg += *m_pos;
                error_msg += "'";
                throw std::logic_error(error_msg);
            }
            ++m_pos;
            // replace '\"' by '"'
            char last_char = ' ';
            auto cond = [&last_char] (char c) -> bool
            {
                if (c == '"' && last_char == '\\')
                {
                    return true;
                }
                last_char = c;
                return false;
            };
            std::string tmp;
            auto sbegin = substr.begin();
            auto send = substr.end();
            for (auto i = std::find_if(sbegin, send, cond);
                 i != send;
                 i = std::find_if(i, send, cond))
            {
                --i;
                tmp.append(sbegin, i);
                tmp += '"';
                i += 2;
                sbegin = i;
            }
            if (sbegin != substr.begin())
            {
                tmp.append(sbegin, send);
            }
            if (!tmp.empty())
            {
                substr = std::move(tmp);
            }
        }
        primitive_variant result(ptype);
        result.apply(from_string(substr));
        return result;
    }

    void read_tuple(size_t size,
                    primitive_type const* begin,
                    primitive_variant* storage)
    {
        integrity_check();
        consume('{');
        primitive_type const* end = begin + size;
        for ( ; begin != end; ++begin)
        {
            *storage = std::move(read_value(*begin));
            ++storage;
        }
        consume('}');
    }

};

} // namespace <anonymous>

object from_string(std::string const& what)
{
    string_deserializer strd(what);
    std::string uname = strd.peek_object();
    auto utype = uniform_type_info::from(uname);
    if (utype == nullptr)
    {
        throw std::logic_error(uname + " is not announced");
    }
    return utype->deserialize(&strd);
}

namespace detail {

std::string to_string_impl(const void *what, const uniform_type_info *utype)
{
    std::ostringstream osstr;
    string_serializer strs(osstr);
    utype->serialize(what, &strs);
    return osstr.str();
}

} // namespace detail

} // namespace cppa
