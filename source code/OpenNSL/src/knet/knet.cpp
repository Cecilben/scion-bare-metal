#define INCLUDE_DIAG_SHELL

#include <common.h>

extern "C" {
#include <sal/driver.h>
#include <opennsl/error.h>
#include <opennsl/types.h>
#include <opennsl/port.h>
#include <opennsl/link.h>
#include <opennsl/rx.h>
#include <opennsl/tx.h>
#include <opennsl/knet.h>
}

#include <stddef.h>
#include <stdint.h>
#include <iostream>
#include <ostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <cstring>
#include <algorithm>


extern "C" int netifTraverseCallback(int unit, opennsl_knet_netif_t *netif, void *userData)
{
    try {
        std::cout << "ID: " << netif->id << ", Port: " << netif->port;
        std::cout << ", VLAN: " << netif->vlan << ", Name: \"" << netif->name << '\"';
        std::cout << ", MAC: " << netif->mac_addr << '\n';
    }
    catch (...) {
        return 1;
    }
    return 0;
}

int listKnetInterfaces(int unit)
{
    int result = opennsl_knet_netif_traverse(unit, &netifTraverseCallback, nullptr);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_knet_netif_traverse failed: " << opennsl_errmsg(result) << std::endl;
    }
    return result;
}

extern "C" int filterTraverseCallback(int unit, opennsl_knet_filter_t *filter, void *userData)
{
    try {
        std::cout << "ID: " << filter->id << ", Priority: " << filter->priority;
        std::cout << ", Ingress port: " << filter->m_ingport << ", DestID: " << filter->dest_id;
        std::cout << ", Description: \"" << filter->desc << "\"\n";
    }
    catch (...) {
        return 1;
    }
    return 0;
}

int listKnetFilters(int unit)
{
    int result = opennsl_knet_filter_traverse(unit, &filterTraverseCallback, nullptr);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_knet_filter_traverse failed: " << opennsl_errmsg(result) << std::endl;
    }
    return result;
}

int createNetif(int unit, int port, const std::string &name, int &interfaceId)
{
    // initialize data structure
    opennsl_knet_netif_t netif;
    opennsl_knet_netif_t_init(&netif);
    netif.type = OPENNSL_KNET_NETIF_T_TX_LOCAL_PORT;
    netif.port = port; // egress port
    std::strncpy(netif.name, name.c_str(), OPENNSL_KNET_NETIF_NAME_MAX-1);
    netif.name[OPENNSL_KNET_NETIF_NAME_MAX-1] = 0;
    opennsl_mac_t mac = { 0x02, 0x10, 0x18, 0x00, 0x00, static_cast<uint8>(port) };
    std::memcpy(netif.mac_addr, mac, 6);

    // create the interface
    int result = opennsl_knet_netif_create(unit, &netif);
    if (OPENNSL_SUCCESS(result))
    {
        std::cout << "Created interface " << name << " ID: " << netif.id << '\n';
        interfaceId = netif.id;
    }
    else
        std::cout << "Failed to create interface " << name << ": " << opennsl_errmsg(result) << std::endl;
    return result;
}

int createFilter(int unit, int port, int destInterface, const std::string &name)
{
    // initialize data structure
    opennsl_knet_filter_t filter;
    opennsl_knet_filter_t_init(&filter);
    filter.type = OPENNSL_KNET_FILTER_T_RX_PKT;
    filter.flags = OPENNSL_KNET_FILTER_F_STRIP_TAG;
    filter.priority = 100;
    filter.dest_type = OPENNSL_KNET_DEST_T_NETIF;
    filter.dest_id = destInterface;
    filter.match_flags = OPENNSL_KNET_FILTER_M_INGPORT;
    filter.m_ingport = port;
    std::strncpy(filter.desc, name.c_str(), OPENNSL_KNET_FILTER_DESC_MAX-1);
    filter.desc[OPENNSL_KNET_FILTER_DESC_MAX-1] = 0;

    // create filter
    int result = opennsl_knet_filter_create(unit, &filter);
    if (OPENNSL_SUCCESS(result))
        std::cout << "Created filter " << filter.desc << " ID: " << filter.id << '\n';
    else
        std::cout << "Failed to create filter " << filter.desc << ": " << opennsl_errmsg(result) << std::endl;
    return result;
}

struct RawPacketData
{
    static constexpr size_t MAX_FILTER_SIZE = 16;
    unsigned int offset;
    unsigned int length;
    uint8_t data[MAX_FILTER_SIZE];
    uint8_t mask[MAX_FILTER_SIZE];
};

std::istream& operator>>(std::istream &stream, RawPacketData &filter)
{
    std::string str;
    std::memset(filter.data, 0, RawPacketData::MAX_FILTER_SIZE);
    std::memset(filter.mask, 0, RawPacketData::MAX_FILTER_SIZE);

    stream >> filter.offset;
    stream >> str;
    auto dataLength = parseHexString(str.c_str(), filter.data, RawPacketData::MAX_FILTER_SIZE);
    stream >> str;
    auto maskLength = parseHexString(str.c_str(), filter.mask, RawPacketData::MAX_FILTER_SIZE);
    filter.length = std::max(dataLength, maskLength);

    return stream;
}

// Create a filter matching a single byte at the given offset.
int createRawDataFilter(
    int unit, int port, int destInterface, const std::string &name,
    const RawPacketData &rawFilter)
{
    // initialize data structure
    opennsl_knet_filter_t filter;
    opennsl_knet_filter_t_init(&filter);
    filter.type = OPENNSL_KNET_FILTER_T_RX_PKT;
    filter.flags = OPENNSL_KNET_FILTER_F_STRIP_TAG;
    filter.priority = 50;
    filter.dest_type = OPENNSL_KNET_DEST_T_NETIF;
    filter.dest_id = destInterface;
    filter.match_flags = OPENNSL_KNET_FILTER_M_INGPORT | OPENNSL_KNET_FILTER_M_RAW;
    filter.m_ingport = port;
    std::strncpy(filter.desc, name.c_str(), OPENNSL_KNET_FILTER_DESC_MAX-1);
    filter.desc[OPENNSL_KNET_FILTER_DESC_MAX-1] = 0;

    if (rawFilter.offset + rawFilter.length > OPENNSL_KNET_FILTER_SIZE_MAX) return OPENNSL_E_PARAM;
    std::memset(filter.m_raw_data, 0, OPENNSL_KNET_FILTER_SIZE_MAX);
    std::memset(filter.m_raw_mask, 0, OPENNSL_KNET_FILTER_SIZE_MAX);
    std::memcpy(filter.m_raw_data + rawFilter.offset, rawFilter.data, rawFilter.length);
    std::memcpy(filter.m_raw_mask + rawFilter.offset, rawFilter.mask, rawFilter.length);
    filter.raw_size = rawFilter.offset + rawFilter.length;

    // create filter
    int result = opennsl_knet_filter_create(unit, &filter);
    if (OPENNSL_SUCCESS(result))
        std::cout << "Created filter " << filter.desc << " ID: " << filter.id << '\n';
    else
        std::cout << "Failed to create filter " << filter.desc << ": " << opennsl_errmsg(result) << std::endl;
    return result;
}

int destroyNetif(int unit, int id)
{
    int result = opennsl_knet_netif_destroy(unit, id);
    if (OPENNSL_SUCCESS(result))
        std::cout << "Interface " << id << " deleted\n";
    else
        std::cout << "opennsl_knet_netif_destroy failed: " << opennsl_errmsg(result) << std::endl;
    return result;
}

int destroyFilter(int unit, int id)
{
    int result = opennsl_knet_filter_destroy(unit, id);
    if (OPENNSL_SUCCESS(result))
        std::cout << "Filter " << id << " deleted\n";
    else
        std::cout << "opennsl_knet_filter_destroy failed: " << opennsl_errmsg(result) << std::endl;
    return result;
}

int sendPacket(int unit, int port, int count)
{
    uint8 data[] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, // dst MAC
        0x82, 0x2f, 0x2e, 0x42, 0x46, 0x74, // src MAC
        0x08, 0x06, // ethertype (ARP)
        0x00, 0x01, // hardware type (Ethernet)
        0x08, 0x00, // protocol type (IPv4)
        0x06, 0x04, // address length
        0x00, 0x01, // operation (request)
        0x82, 0x2f, 0x2e, 0x42, 0x46, 0x74, // sender hardware address
        0x0a, 0x00, 0x00, 0x01,             // sender protocol address
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // target hardware address
        0x0a, 0x00, 0x00, 0x02,             // target protocol address
        0x00, 0x00, 0x00, 0x00,             // CRC
    };
    constexpr int size = static_cast<int>(sizeof(data));

    int result;

    // allocate a packet and set its data
    opennsl_pkt_t *pkt;
    result = opennsl_pkt_alloc(unit, size, 0, &pkt);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_pkt_alloc failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }

    result = opennsl_pkt_memcpy(pkt, 0, data, size);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_pkt_memcpy failed: " << opennsl_errmsg(result) << std::endl;
        opennsl_pkt_free(unit, pkt);
        return result;
    }

    OPENNSL_PBMP_PORT_SET(pkt->tx_pbmp, port); // set egress port

    // send packets
    for (int i = 0; i < count; ++i)
    {
        result = opennsl_tx(unit, pkt, nullptr);
        if (OPENNSL_FAILURE(result))
        {
            std::cout << "opennsl_tx failed: " << opennsl_errmsg(result) << std::endl;
            opennsl_pkt_free(unit, pkt);
            return result;
        }
    }

    opennsl_pkt_free(unit, pkt);
    return result;
}

// Callback for inbound packets.
opennsl_rx_t rxCallback(int unit, opennsl_pkt_t *pkt, void *cookie)
{
    std::cout << "Packet received on port " << +pkt->rx_port << std::endl;
    return OPENNSL_RX_HANDLED;
}

// Register the callback to receive packets.
int startReceive(int unit)
{
    int result = opennsl_rx_register(unit, "test application", &rxCallback, 10, nullptr, OPENNSL_RCO_F_ALL_COS);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_rx_register failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }
}

// Unregister the callback to receive packets.
int stopReceive(int unit)
{
    int result = opennsl_rx_unregister(unit, &rxCallback, 10);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_rx_unregister failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }
}

int getLinkscanInterval(int unit, int *interval)
{
    int result = opennsl_linkscan_enable_get(unit, interval);
    if (OPENNSL_FAILURE(result))
        std::cout << "opennsl_linkscan_enable_get failed: " << opennsl_errmsg(result) << std::endl;
    return result;
}

int setLinkscanInterval(int unit, int interval)
{
    int result = opennsl_linkscan_enable_set(unit, interval);
    if (OPENNSL_FAILURE(result))
        std::cout << "opennsl_linkscan_enable_set failed: " << opennsl_errmsg(result) << std::endl;
    return result;
}

bool executeCommand(int unit, const std::string &inputStr)
{
    std::stringstream input(inputStr);
    std::string command;

    input >> command;
    if (command == "quit")
    {
        return false;
    }
    else if (command == "ls")
    {
        input >> command;
        if (command == "port")
        {
            enumeratePorts(unit);
            return true;
        }
        else if (command == "vlan")
        {
            enumerateVlans(unit);
            return true;
        }
        else if (command == "netif")
        {
            listKnetInterfaces(unit);
            return true;
        }
        else if (command == "filter")
        {
            listKnetFilters(unit);
            return true;
        }
    }
    else if (command == "add")
    {
        input >> command;
        if (command == "netif")
        {
            int port;
            std::string name;
            input >> port;
            input >> name;
            if (input)
            {
                int interfaceId;
                if (OPENNSL_SUCCESS(createNetif(unit, port, name, interfaceId)))
                    createFilter(unit, port, interfaceId, name);
                return true;
            }
        }
        else if (command == "filter")
        {
            int port, interfaceId;
            std::string name;
            input >> port;
            input >> interfaceId;
            input >> name;
            if (input)
            {
                createFilter(unit, port, interfaceId, name);
                return true;
            }
        }
        else if (command == "raw_filter")
        {
            int port, interfaceId;
            std::string name;
            RawPacketData rawFilter;
            input >> port >> interfaceId >> name >> rawFilter;
            if (input)
            {
                createRawDataFilter(unit, port, interfaceId, name, rawFilter);
                return true;
            }
        }
    }
    else if (command == "del")
    {
        int id;
        input >> command;
        input >> id;
        if (input)
        {
            if (command == "netif")
            {
                destroyNetif(unit, id);
                return true;
            }
            else if (command == "filter")
            {
                destroyFilter(unit, id);
                return true;
            }
        }
    }
    else if (command == "status")
    {
        int port;
        input >> port;
        if (input)
        {
            getPortInfo(unit, port);
            return true;
        }
    }
    else if (command == "stats")
    {
        int port;
        input >> port;
        if (input)
        {
            getPortStats(unit, port);
            return true;
        }
    }
    else if (command == "reset")
    {
        input >> command;
        if (command == "stats")
        {
            int port;
            input >> port;
            if (input)
            {
                resetPortStats(unit, port);
                return true;
            }
        }
    }
    else if (command == "tx")
    {
        int port, count;
        input >> port;
        input >> count;
        if (input)
        {
            if (OPENNSL_SUCCESS(sendPacket(unit, port, count)))
                std::cout << "Packet(s) sent\n";
            return true;
        }
    }
    else if (command == "rx")
    {
        input >> command;
        if (command == "start")
        {
            startReceive(unit);
            return true;
        }
        else if (command == "stop")
        {
            stopReceive(unit);
            return true;
        }
    }
    else if (command == "linkscan")
    {
        int interval;
        input >> command;
        if (command == "get")
        {
            if (OPENNSL_SUCCESS(getLinkscanInterval(unit, &interval)))
                std::cout << "Linkscan interval: " << interval << " us\n";
            return true;
        }
        else if (command == "set")
        {
            input >> interval;
            if (input)
            {
                if (OPENNSL_SUCCESS(setLinkscanInterval(unit, interval)))
                    std::cout << "Linkscan interval set\n";
                return true;
            }
        }
    }
    else if (command == "shell")
    {
        opennsl_driver_shell();
        return true;
    }
    else
    {
        std::cout << "Commands:\n"
        "ls [port|vlan|netif|filter] : List ports, VLANs, KNET interfaces or KNET filters\n"
        "add netif <port> <name> : Add KNET interface and corresponding filter\n"
        "add filter <port> <netif id> <name> : Add filter directing inbound traffic on <port> to <netinf id>\n"
        "add raw_filter <port> <netif id> <name> <offset> <data> <mask> : Add filter matching raw packet data\n"
        "del [netif|filter] <id> : Delete KNET interface or filter\n"
        "status <port> : Get status information on a port\n"
        "stats <port> : Get statistics for a port\n"
        "reset stats <port> : Reset the statistics of a port\n"
        "tx <port> <count> : Send <count> packets on <port>\n"
        "rx [start|stop] : Start or stop receiving packets on all ports\n"
        "linkscan [get|set] <interval> : Get or set the linkscan interval in us\n"
        "shell : Launch driver shell\n"
        "quit : Quit the application\n";
        return true;
    }

    std::cout << "Invalid command line\n";
    return true;
}

int main(int argc, char* argv[])
{
    constexpr int unit = 0;
    int result = 0;

    result = opennsl_driver_init(nullptr);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_driver_init failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }

    std::cout << "Using unit " << unit << '\n';

    // RX module must run for KNET to work
    if (opennsl_rx_active(unit))
        std::cout << "RX module is already active\n";
    else
    {
        std::cout << "Starting RX module\n";
        result = opennsl_rx_start(unit, nullptr);
        if (OPENNSL_FAILURE(result))
        {
            std::cout << "opennsl_rx_start failed: " << opennsl_errmsg(result) << std::endl;
            // opennsl_driver_exit();
            return result;
        }
    }

    // apply default configration to all ports
    result = configureAllPorts(unit);
    if (OPENNSL_FAILURE(result))
    {
        // opennsl_driver_exit();
        return result;
    }

    // initialize kernel networking subsystem
    result = opennsl_knet_init(unit);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_knet_init failed: " << opennsl_errmsg(result) << std::endl;
        // opennsl_driver_exit();
        return result;
    }

    // initialize statistics module
    result = opennsl_stat_init(unit);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_stat_init failed: " << opennsl_errmsg(result) << std::endl;
        // opennsl_driver_exit();
        return result;
    }

    // menu
    std::string input;
    do {
        std::cout << '>';
        std::getline(std::cin, input);
    } while (executeCommand(unit, input));

    return opennsl_driver_exit();
}
