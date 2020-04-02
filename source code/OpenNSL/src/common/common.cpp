#include "common.h"

extern "C" {
#include <opennsl/error.h>
#include <opennsl/port.h>
#include <opennsl/link.h>
#include <opennsl/vlan.h>
}

#include <stddef.h>
#include <stdint.h>
#include <iostream>
#include <iomanip>


// Add all ports to different VLANs. Each ports is configured to not add a VLAn tag to
// outgoing packets and treat all incoming packets as belonging to its VLAN.
// The CPU port is added to all VLANs including the default VLAN.
int configureAllPorts(int unit)
{
    int result = 0;

    // get some information on the available ports
    opennsl_port_config_t pcfg;
    result = opennsl_port_config_get(unit, &pcfg);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_port_config_get failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }
    std::cout << "All ports mask     = " << pcfg.all << '\n';
    std::cout << "Ethernet port mask = " << pcfg.e << '\n';
    std::cout << "CPU port mask      = " << pcfg.cpu << '\n';

    // get the default VLAN
    opennsl_vlan_t defVlanId = 1;
    result = opennsl_vlan_default_get(unit, &defVlanId);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_vlan_default_get failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }

    std::cout << "Add CPU port to default VLAN " << defVlanId << '\n';
    result = opennsl_vlan_port_add(unit, defVlanId, pcfg.cpu, pcfg.cpu);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_vlan_port_add failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }

    // add ethernet ports to different VLANs
    int port = 1, vlanId = defVlanId;
    OPENNSL_PBMP_ITER(pcfg.e, port)
    {
        result = configurePort(unit, port, pcfg.cpu, vlanId, defVlanId);
        if (OPENNSL_FAILURE(result))
        {
            std::cout << "Tried to add ethernet port " << port << " to VLAN " << vlanId << std::endl;
            return result;
        }
        ++vlanId;
    }
}

// Create VLAN 'vlanId' and configure port 'port' to use it.
// The CPU ports are added to every VLAN.
int configurePort(int unit, int port, opennsl_pbmp_t cpuPort, opennsl_vlan_t vlanId, opennsl_vlan_t defVlanId)
{
    int result;

    if (vlanId != defVlanId)
    {
        // create VLAN
        opennsl_vlan_create(unit, vlanId);
        if (OPENNSL_FAILURE(result))
        {
            std::cout << "opennsl_vlan_create failed: " << opennsl_errmsg(result) << std::endl;
            return result;
        }

        // add CPU port to VLAN
        result = opennsl_vlan_port_add(unit, vlanId, cpuPort, cpuPort);
        if (OPENNSL_FAILURE(result))
        {
            std::cout << "Adding CPU port to VLAN " << vlanId << " failed: ";
            std::cout << opennsl_errmsg(result) << std::endl;
            opennsl_vlan_destroy(unit, vlanId);
            return result;
        }
    }

    // assign port to VLAN and set egress packets to untagged
    opennsl_pbmp_t portBitmap;
    OPENNSL_PBMP_CLEAR(portBitmap);
    OPENNSL_PBMP_PORT_SET(portBitmap, port);

    result = opennsl_vlan_port_add(unit, vlanId, portBitmap, portBitmap);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_vlan_port_add failed: " << opennsl_errmsg(result) << std::endl;
        opennsl_vlan_destroy(unit, vlanId);
        return result;
    }

    // treat untagged ingress packets as belonging to the port's VLAN
    result = opennsl_port_untagged_vlan_set(unit, port, vlanId);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_port_untagged_vlan_set failed: " << opennsl_errmsg(result) << std::endl;
        opennsl_vlan_destroy(unit, vlanId);
        return result;
    }
}

int getPortInfo(int unit, int port)
{
    int result;

    // check whether the port is enabled
    int enabled;
    result = opennsl_port_enable_get(unit, port, &enabled);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_port_enable_get failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }
    std::cout << "Port " << port << " is " << (enabled ? "enabled" : "disabled") << '\n';

    // get connection speed
    int speed;
    result = opennsl_port_speed_get(unit, port, &speed);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_port_speed_get failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }
    std::cout << "Speed: " << speed << " Mbps\n";

    return result;
}

// Print list of all ports.
void enumeratePorts(int unit)
{
    int enabled, speed;
    for (int port = 1;
        OPENNSL_SUCCESS(opennsl_port_enable_get(unit, port, &enabled))
        && OPENNSL_SUCCESS(opennsl_port_speed_get(unit, port, &speed));
        ++port)
    {
        std::cout << "Port " << std::setw(2) << port << ": " << (enabled ? "enabled" : "disabled");
        std::cout << ", speed: " << speed << " Mbps\n";
    }
}

// List all VLANs and their port bitmaps.
int enumerateVlans(int unit)
{
    int result;

    opennsl_vlan_data_t *list;
    int count;
    result = opennsl_vlan_list(unit, &list, &count);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_vlan_list failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }

    for (int i = 0; i < count; ++i)
    {
        std::cout << "VLAN " << std::setw(2) << list[i].vlan_tag;
        std::cout << ": ports = " << list[i].port_bitmap << '\n';
    }

    opennsl_vlan_list_destroy(unit, list, count);
}

int getPortStats(int unit, int port)
{
    int result;

    // synchronize with hardware counters
    result = opennsl_stat_sync(unit);
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_stat_sync failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }

    // get counter values
    constexpr int NUM_STATS = 12;
    opennsl_stat_val_t stats[NUM_STATS] = {
        opennsl_spl_snmpIfInUcastPkts,
        opennsl_spl_snmpIfInNUcastPkts,
        opennsl_spl_snmpIfInDiscards,
        opennsl_spl_snmpIfInErrors,
        opennsl_spl_snmpIfInMulticastPkts,
        opennsl_spl_snmpIfInBroadcastPkts,
        opennsl_spl_snmpIfOutUcastPkts,
        opennsl_spl_snmpIfOutNUcastPkts,
        opennsl_spl_snmpIfOutDiscards,
        opennsl_spl_snmpIfOutErrors,
        opennsl_spl_snmpIfOutMulticastPkts,
        opennsl_spl_snmpIfOutBroadcastPkts,
    };
    uint64_t values[NUM_STATS] = {};
    result = opennsl_stat_multi_get(unit, port, NUM_STATS, stats, reinterpret_cast<uint64*>(values));
    if (OPENNSL_FAILURE(result))
    {
        std::cout << "opennsl_stat_multi_get failed: " << opennsl_errmsg(result) << std::endl;
        return result;
    }

    std::cout << "Ingress Unicast packets........ " << values[0] << '\n';
    std::cout << "Ingress Non-unicast packets.... " << values[1] << '\n';
    std::cout << "Ingress Discard packets........ " << values[2] << '\n';
    std::cout << "Ingress Error packets.......... " << values[3] << '\n';
    std::cout << "Ingress Multicast packets...... " << values[4] << '\n';
    std::cout << "Ingress Broadcast packets...... " << values[5] << '\n';
    std::cout << "Egress Unicast packets......... " << values[6] << '\n';
    std::cout << "Egress Non-unicast packets..... " << values[7] << '\n';
    std::cout << "Egress Discard packets......... " << values[8] << '\n';
    std::cout << "Egress Error packets........... " << values[9] << '\n';
    std::cout << "Egress Multicast packets....... " << values[10] << '\n';
    std::cout << "Egress Broadcast packets....... " << values[11] << '\n';

    return result;
}

int resetPortStats(int unit, int port)
{
    int result = opennsl_stat_clear(unit, port);
    if (OPENNSL_FAILURE(result))
        std::cout << "opennsl_stat_multi_get failed: " << opennsl_errmsg(result) << std::endl;
    return result;
}

std::ostream& operator<<(std::ostream &stream, const opennsl_pbmp_t &mask)
{
    auto oldFlags = stream.flags(std::ios::hex);

    constexpr size_t N = sizeof(mask.pbits) / sizeof(*mask.pbits);
    for (size_t i = 0; i < N; ++i)
        stream << std::setw(8) << mask.pbits[i];

    stream.setf(oldFlags);
    return stream;
}

std::ostream& operator<<(std::ostream &stream, const opennsl_mac_t &mac)
{
    auto oldFlags = stream.flags(std::ios::hex);
    auto oldFillChar = stream.fill('0');

    for (size_t i = 0; i < 5; ++i)
        stream << std::setw(2) << +mac[i] << ':';
    stream << std::setw(2) << +mac[5];

    stream.fill(oldFillChar);
    stream.flags(oldFlags);
    return stream;
}

// Parse a string of up to ceil(length/2) hexadecimal characters and put the resulting bytes in
// the array 'bytes'. Returns the number of bytes written.
size_t parseHexString(const char *str, uint8_t *bytes, size_t length)
{
    size_t i = 0;
    for (; str[i] && i < length; ++i)
    {
        uint8_t nibble;
        if (str[i] >= '0' && str[i] <= '9')
            nibble = str[i] - '0';
        else if (str[i] >= 'a' && str[i] <= 'f')
            nibble = 10 + (str[i] - 'a');
        else if (str[i] >= 'A' && str[i] <= 'F')
            nibble = 10 + (str[i] - 'A');
        else
            break;
        if (i % 2 == 0)
            bytes[i/2] = nibble << 4;
        else
            bytes[i/2] |= nibble;
    }
    return (i + 1) / 2;
}
