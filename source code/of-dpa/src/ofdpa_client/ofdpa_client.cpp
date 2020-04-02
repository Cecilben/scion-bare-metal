#include "common.h"

extern "C" {
#include "ofdpa_api.h"
#include "ofdpa_datatypes.h"
}

#include <stddef.h>
#include <stdint.h>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstring>


// List all valid flow tables.
void enumerateFlowTables()
{
    for (unsigned int i = 0; i < 256; ++i)
    {
        auto tableId = static_cast<OFDPA_FLOW_TABLE_ID_t>(i);
        if (ofdpaFlowTableSupported(tableId) == OFDPA_E_NONE)
        {
            std::cout << "Table " << std::setw(3) << i << ": ";
            std::cout << ofdpaFlowTableNameGet(tableId);

            ofdpaFlowTableInfo_t info;
            if (ofdpaFlowTableInfoGet(tableId, &info) == OFDPA_E_NONE)
                std::cout << " (" << info.numEntries << " / " << info.maxEntries << ')';

            std::cout << '\n';
        }
    }
}

// Print the first 'count' entries of a flow table.
OFDPA_ERROR_t dumpFlowTable(OFDPA_FLOW_TABLE_ID_t tableId, uint32_t count)
{
    OFDPA_ERROR_t result;

    std::cout << "Table " << std::setw(3) << tableId << ": ";
    std::cout << ofdpaFlowTableNameGet(tableId) << '\n';

    ofdpaFlowTableInfo_t info;
    result = ofdpaFlowTableInfoGet(tableId, &info);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaFlowTableInfoGet failed: " << result << std::endl;
        return result;
    }

    std::cout << "Entries: " << info.numEntries << " / " << info.maxEntries << '\n';

    ofdpaFlowEntry_t entry;
    result = ofdpaFlowEntryInit(tableId, &entry);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaFlowEntryInit failed: " << result << std::endl;
        return result;
    }

    char buffer[512];
    unsigned int i = 0;
    while (ofdpaFlowNextGet(&entry, &entry) == OFDPA_E_NONE && i < count)
    {
        std::cout << "Entry " << i << ": ";
        result = ofdpaFlowEntryDecode(&entry, buffer, sizeof(buffer));
        if (result == OFDPA_E_NONE || result == OFDPA_E_FULL)
        {
            std::cout << buffer;
            if (result == OFDPA_E_FULL) std::cout << "...\n";
            else std::cout << '\n';
        }
        else
        {
            std::cout << "ofdpaFlowEntryDecode failed: " << result << std::endl;
            return result;
        }
        ++i;
    }

    return OFDPA_E_NONE;
}

// Add or remove a flow entry forwarding packets arriving at port 'inPortNum' to the controller.
OFDPA_ERROR_t addFlowEntryReceive(uint32_t inPortNum, bool add)
{
    OFDPA_ERROR_t result;

    ofdpaFlowEntry_t entry;
    result = ofdpaFlowEntryInit(OFDPA_FLOW_TABLE_ID_ACL_POLICY, &entry);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaFlowEntryInit failed: " << result << std::endl;
        return result;
    }

    entry.flowData.policyAclFlowEntry.match_criteria.inPort = inPortNum;
    entry.flowData.policyAclFlowEntry.match_criteria.inPortMask = OFDPA_INPORT_EXACT_MASK;
    entry.flowData.policyAclFlowEntry.match_criteria.etherTypeMask = OFDPA_ETHERTYPE_ALL_MASK;
    entry.flowData.policyAclFlowEntry.outputPort = OFDPA_PORT_CONTROLLER;

    if (add)
    {
        result = ofdpaFlowAdd(&entry);
        if (result == OFDPA_E_EXISTS)
            std::cout << "Entry already exists\n";
        else if (result != OFDPA_E_NONE)
            std::cout << "ofdpaFlowAdd failed: " << result << std::endl;
    }
    else
    {
        result = ofdpaFlowDelete(&entry);
        if (result == OFDPA_E_NOT_FOUND)
            std::cout << "Entry not found\n";
        else if (result != OFDPA_E_NONE)
            std::cout << "ofdpaFlowDelete failed: " << result << std::endl;
    }

    return result;
}

// Send count packets from the given port.
OFDPA_ERROR_t sendPacket(uint32_t port, uint32_t count)
{
    OFDPA_ERROR_t result;

    uint8_t buffer[] = {
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
        // 0x00, 0x00, 0x00, 0x00,             // CRC
    };

    ofdpa_buffdesc pktData = {};
    pktData.pstart = reinterpret_cast<char*>(buffer);
    pktData.size = sizeof(buffer);

    for (uint32_t i = 0; i < count; ++i)
    {
        result = ofdpaPktSend(&pktData, 0, port, 0);
        if (result != OFDPA_E_NONE)
        {
            std::cout << "ofdpaPktSend failed: " << result << std::endl;
            return result;
        }
    }

    std::cout << "Packet(s) sent\n";
    return result;
}

// Receive up to 'count' packets.
OFDPA_ERROR_t receivePackets(uint32_t count)
{
    OFDPA_ERROR_t result;

    timeval timeout = {};
    timeout.tv_sec = 1;

    for (uint32_t i = 0; i < count; ++i)
    {
        char buffer[8192];
        ofdpaPacket_t packet = {};
        packet.pktData.pstart = buffer;
        packet.pktData.size = sizeof(buffer);

        result = ofdpaPktReceive(&timeout, &packet);
        if (result == OFDPA_E_TIMEOUT)
        {
            std::cout << "No more packets available\n";
            return OFDPA_E_NONE;
        }
        else if (result != OFDPA_E_NONE)
        {
            std::cout << "ofdpaPktReceive failed: " << result << std::endl;
            return result;
        }

        std::cout << "Received packet on port " << packet.inPortNum;
        std::cout << ", reason: " << packet.reason;
        std::cout << ", size: " << packet.pktData.size << '\n';
        printPacket(std::cout, buffer, std::min(packet.pktData.size, 128u));

        // don't wait for more packets, if no more are pending
        timeout.tv_sec = 0;
    }
}

bool executeCommand(const std::string &inputStr)
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
            enumeratePorts();
            return true;
        }
        else if (command == "table")
        {
            enumerateFlowTables();
            return true;
        }
    }
    else if (command == "status")
    {
        uint32_t id;
        input >> command;
        input >> id;
        if (input)
        {
            if (command == "port")
            {
                printPortInfo(id);
                return true;
            }
        }
    }
    else if (command == "stats")
    {
        uint32_t id;
        input >> command;
        input >> id;
        if (input)
        {
            if (command == "port")
            {
                printPortStatistics(id);
                return true;
            }
        }
    }
    else if (command == "reset")
    {
        input >> command;
        if (command == "stats")
        {
            uint32_t id;
            input >> command;
            input >> id;
            if (input)
            {
                if (command == "port")
                {
                    clearPortStatistics(id);
                    return true;
                }
            }
        }
    }
    else if (command == "dump")
    {
        uint32_t id, count;
        input >> command;
        input >> id;
        input >> count;
        if (input)
        {
            if (command == "table")
            {
                dumpFlowTable(static_cast<OFDPA_FLOW_TABLE_ID_t>(id), count);
                return true;
            }
        }
    }
    else if (command == "add")
    {
        input >> command;
        if (command == "flow")
        {
            input >> command;
            if (command == "rx")
            {
                uint32_t port;
                input >> port;
                if (input)
                {
                    addFlowEntryReceive(port, true);
                    return true;
                }
            }
        }
    }
    else if (command == "del")
    {
        input >> command;
        if (command == "flow")
        {
            input >> command;
            if (command == "rx")
            {
                uint32_t port;
                input >> port;
                if (input)
                {
                    addFlowEntryReceive(port, false);
                    return true;
                }
            }
        }
    }
    else if (command == "tx")
    {
        uint32_t port, count;
        input >> port;
        input >> count;
        if (input)
        {
            sendPacket(port, count);
            return true;
        }
    }
    else if (command == "rx")
    {
        uint32_t count;
        input >> count;
        if (input)
        {
            receivePackets(count);
            return true;
        }
    }
    else
    {
        std::cout << "Commands:\n"
        "ls [port|table] : List ports or flow tables\n"
        "status port <id> : Show status of an object\n"
        "stats port <id> : Show statistics on an object\n"
        "reset stats port <id> : Reset the statistics of an object\n"
        "dump table <id> <count> : Print the first <count> entries of flow table <id>\n"
        "add flow rx <port> : Add a flow entry to receive packets on <port>\n"
        "del flow rx <port> : Delete the flow entry to receive packets on <port>\n"
        "tx <port> <count> : Send <count> packets on <port>\n"
        "rx <count> : Receive up to <count> packets\n"
        "quit : Quit the application\n";
        return true;
    }

    std::cout << "Invalid command line\n";
    return true;
}

int main(int argc, char* argv[])
{
    OFDPA_ERROR_t result;

    char clientName[] = "ofdpa test client";
    result = ofdpaClientInitialize(clientName);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaClientInitialize failed: " << result << std::endl;
        return result;
    }

    result = ofdpaClientPktSockBind();
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaClientPktSockBind failed: " << result << std::endl;
        return result;
    }

    // menu
    std::string input;
    do {
        std::cout << '>';
        std::getline(std::cin, input);
    } while (executeCommand(input));

    return 0;
}
