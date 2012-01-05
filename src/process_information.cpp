#include "cppa/config.hpp"

#include <cstdio>
#include <cstring>
#include <sstream>

#include <unistd.h>
#include <sys/types.h>


#include "cppa/util/ripemd_160.hpp"
#include "cppa/process_information.hpp"

namespace {

void erase_trailing_newline(std::string& str)
{
    while (!str.empty() && (*str.rbegin()) == '\n')
    {
        str.resize(str.size() - 1);
    }
}

#ifdef CPPA_MACOS
char const* s_get_uuid =
    "/usr/sbin/diskutil info / | "
    "/usr/bin/awk '$0 ~ /UUID/ { print $3 }'";
char const* s_get_mac =
    "/usr/sbin/system_profiler SPNetworkDataType | "
    "/usr/bin/grep -Fw MAC | "
    "/usr/bin/grep -o '[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}' | "
    "/usr/bin/head -n1";
#elif defined(CPPA_LINUX)
char const* s_get_uuid =
    "/bin/egrep -o 'UUID=(([0-9a-fA-F-]+)(-[0-9a-fA-F-]+){3})\\s+/\\s+' "
                  "/etc/fstab | "
    "/bin/egrep -o '([0-9a-fA-F-]+)(-[0-9a-fA-F-]+){3}'";
char const* s_get_mac =
    "/sbin/ifconfig | "
    "/bin/egrep -o '[0-9a-fA-F]{2}(:[0-9a-fA-F]{2}){5}' | "
    "head -n1";
#endif

cppa::process_information* compute_proc_info()
{
    char cbuf[100];
    // fetch hd serial
    std::string hd_serial_and_mac_addr;
    FILE* get_uuid_cmd = popen(s_get_uuid, "r");
    while (fgets(cbuf, 100, get_uuid_cmd) != 0)
    {
        hd_serial_and_mac_addr += cbuf;
    }
    pclose(get_uuid_cmd);
    erase_trailing_newline(hd_serial_and_mac_addr);
    // fetch mac address of first network device
    FILE* get_mac_cmd = popen(s_get_mac, "r");
    while (fgets(cbuf, 100, get_mac_cmd) != 0)
    {
        hd_serial_and_mac_addr += cbuf;
    }
    pclose(get_mac_cmd);
    erase_trailing_newline(hd_serial_and_mac_addr);
    cppa::process_information::node_id_type node_id;
    cppa::util::ripemd_160(node_id, hd_serial_and_mac_addr);
    return new cppa::process_information(getpid(), node_id);
}

cppa::intrusive_ptr<cppa::process_information> s_pinfo;

struct pinfo_manager
{
    pinfo_manager()
    {
        if (!s_pinfo)
        {
            s_pinfo.reset(compute_proc_info());
        }
    }
}
s_pinfo_manager;

std::uint8_t hex_char_value(char c)
{
    if (isdigit(c))
    {
        return static_cast<std::uint8_t>(c - '0');
    }
    else if (isalpha(c))
    {
        if (c >= 'a' && c <= 'f')
        {
            return static_cast<std::uint8_t>((c - 'a') + 10);
        }
        else if (c >= 'A' && c <= 'F')
        {
            return static_cast<std::uint8_t>((c - 'A') + 10);
        }
    }
    throw std::invalid_argument(std::string("illegal character: ") + c);
}

} // namespace <anonymous>

namespace cppa {

void node_id_from_string(std::string const& hash,
                         process_information::node_id_type& node_id)
{
    if (hash.size() != (node_id.size() * 2))
    {
        throw std::invalid_argument("string argument is not a node id hash");
    }
    auto j = hash.c_str();
    for (size_t i = 0; i < node_id.size(); ++i)
    {
        // read two characters, each representing 4 bytes
        auto& val = node_id[i];
        val  = hex_char_value(*j++) << 4;
        val |= hex_char_value(*j++);
    }
}

bool equal(std::string const& hash,
           process_information::node_id_type const& node_id)
{
    if (hash.size() != (node_id.size() * 2))
    {
        return false;
    }
    auto j = hash.c_str();
    try
    {
        for (size_t i = 0; i < node_id.size(); ++i)
        {
            // read two characters, each representing 4 bytes
            std::uint8_t val;
            val  = hex_char_value(*j++) << 4;
            val |= hex_char_value(*j++);
            if (val != node_id[i])
            {
                return false;
            }
        }
    }
    catch (std::invalid_argument&)
    {
        return false;
    }
    return true;
}

process_information::process_information(process_information const& other)
    : super(), m_process_id(other.process_id()), m_node_id(other.node_id())
{
}

process_information::process_information(std::uint32_t a, std::string const& b)
    : m_process_id(a)
{
    node_id_from_string(b, m_node_id);
}

process_information::process_information(std::uint32_t a, node_id_type const& b)
    : m_process_id(a), m_node_id(b)
{
}

std::string to_string(process_information::node_id_type const& node_id)
{
    std::ostringstream oss;
    oss << std::hex;
    oss.fill('0');
    for (size_t i = 0; i < process_information::node_id_size; ++i)
    {
        oss.width(2);
        oss << static_cast<std::uint32_t>(node_id[i]);
    }
    return oss.str();
}

intrusive_ptr<process_information> const& process_information::get()
{
    return s_pinfo;
}

int process_information::compare(process_information const& other) const
{
    int tmp = strncmp(reinterpret_cast<char const*>(node_id().data()),
                      reinterpret_cast<char const*>(other.node_id().data()),
                      node_id_size);
    if (tmp == 0)
    {
        if (m_process_id < other.process_id()) return -1;
        else if (m_process_id == other.process_id()) return 0;
        return 1;
    }
    return tmp;
}

std::string to_string(process_information const& what)
{
    std::ostringstream oss;
    oss << what.process_id() << "@" << to_string(what.node_id());
    return oss.str();
}

} // namespace cppa
