#include "common.h"

extern "C" {
#include "ofdpa_api.h"
#include "ofdpa_datatypes.h"
}

#include <iostream>
#include <string>
#include <iomanip>


std::ostream& operator<<(std::ostream &stream, const ofdpaMacAddr_t &mac)
{
    auto oldFlags = stream.flags(std::ios::hex);
    auto oldFillChar = stream.fill('0');

    for (size_t i = 0; i < 5; ++i)
        stream << std::setw(2) << +mac.addr[i] << ':';
    stream << std::setw(2) << +mac.addr[5];

    stream.fill(oldFillChar);
    stream.flags(oldFlags);
    return stream;
}

std::ostream& printPacket(std::ostream &stream, char *buffer, size_t length)
{
    auto oldFlags = stream.flags(std::ios::hex);
    auto oldFillChar = stream.fill('0');

    constexpr size_t lineLength = 32;
    for (size_t i = 0; i < length; i += lineLength)
    {
        stream << "0x" << std::setw(4) << i << "  ";
        for (size_t j = 0; j < lineLength && (i+j) < length; ++j)
            stream << std::setw(2) << +buffer[i + j] << ' ';
        stream << '\n';
    }

    stream.fill(oldFillChar);
    stream.flags(oldFlags);
    return stream;
}

// Print detailed information on the state of a physical port.
OFDPA_ERROR_t printPhysicalPortInfo(uint32_t portNum)
{
    OFDPA_ERROR_t result;

    ofdpaMacAddr_t mac;
    result = ofdpaPortMacGet(portNum, &mac);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaPortMacGet failed\n";
        return result;
    }

    uint32_t currSpeed;
    result = ofdpaPortCurrSpeedGet(portNum, &currSpeed);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaPortCurrSpeedGet failed\n";
        return result;
    }

    uint32_t maxSpeed;
    result = ofdpaPortMaxSpeedGet(portNum, &maxSpeed);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaPortMaxSpeedGet failed\n";
        return result;
    }

    std::cout << "MAC: " << mac << ", speed: " << currSpeed << " kbps";
    std::cout << " (max: " << maxSpeed << " kbps )\n";
}

// Print some information on a port.
OFDPA_ERROR_t printPortInfo(uint32_t portNum)
{
    // get port index and type
    uint32_t portIndex, portType;
    ofdpaPortIndexGet(portNum, &portIndex);
    ofdpaPortTypeGet(portNum, &portType);

    // get the port name
    char name[OFDPA_PORT_NAME_STRING_SIZE] = {};
    ofdpa_buffdesc bufferDesc = {};
    bufferDesc.size = sizeof(name);
    bufferDesc.pstart = name;
    ofdpaPortNameGet(portNum, &bufferDesc);

    std::cout << "Port " << portIndex << " (" << name << ") is type " << portType << '\n';
    if (portType == OFDPA_PORT_TYPE_PHYSICAL)
        return printPhysicalPortInfo(portNum);

    return OFDPA_E_NONE;
}

// Enumerate all ports and print state information on each.
void enumeratePorts()
{
    OFDPA_ERROR_t result;
    uint32_t portNum = 0, nextPortNum;

    while ((result = ofdpaPortNextGet(portNum, &nextPortNum)) == OFDPA_E_NONE)
    {
        portNum = nextPortNum;
        printPortInfo(portNum);
    }
    if (result != OFDPA_E_FAIL)
    {
        std::cout << "ofdpaPortNextGet failed: " << result << std::endl;
    }
}

// Print statistics of a port and its CoS queues
OFDPA_ERROR_t printPortStatistics(uint32_t portNum)
{
    OFDPA_ERROR_t result;
    ofdpaPortStats_t stats;
    result = ofdpaPortStatsGet(portNum, &stats);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaPortStatsGet failed: " << result << std::endl;
        return result;
    }

    std::cout << "Received Packets.................. " << stats.rx_packets << '\n';
    std::cout << "Transmitted Packets............... " << stats.tx_packets << '\n';
    std::cout << "Received Bytes.................... " << stats.rx_bytes << '\n';
    std::cout << "Transmitted Bytes................. " << stats.tx_bytes << '\n';
    std::cout << "Received Errors................... " << stats.rx_errors << '\n';
    std::cout << "Transmitted Errors................ " << stats.tx_errors << '\n';
    std::cout << "Received Packets Dropped.......... " << stats.rx_drops << '\n';
    std::cout << "Transmit Packets Dropped.......... " << stats.tx_drops << '\n';
    std::cout << "Received Frame Errors............. " << stats.rx_frame_err << '\n';
    std::cout << "Received Frame Overrun Errors..... " << stats.rx_over_err << '\n';
    std::cout << "Received Packets with CRC Errors.. " << stats.rx_crc_err << '\n';
    std::cout << "Transmit collisions............... " << stats.collisions << '\n';
    std::cout << "Time port has been alive.......... " << stats.duration_seconds << '\n';

    uint32_t numQueues;
    result = ofdpaNumQueuesGet(portNum, &numQueues);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaNumQueuesGet failed: " << result << std::endl;
        return result;
    }

    std::cout << "Queues:\n";
    for (uint32_t queue = 0; queue < numQueues; ++queue)
    {
        ofdpaPortQueueStats_t queueStats;
        result = ofdpaQueueStatsGet(portNum, queue, &queueStats);
        if (result != OFDPA_E_NONE)
        {
            std::cout << "ofdpaQueueStatsGet failed: " << result << std::endl;
            return result;
        }
        std::cout << "Queue " << queue;
        std::cout << " txBytes: " << queueStats.txBytes;
        std::cout << ", txPkts: " << queueStats.txPkts << '\n';
    }

    return result;
}

// Reset the statistics of a port and its CoS queues
OFDPA_ERROR_t clearPortStatistics(uint32_t portNum)
{
    OFDPA_ERROR_t result;

    result = ofdpaPortStatsClear(portNum);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaPortStatsClear failed: " << result << std::endl;
        return result;
    }

    uint32_t numQueues;
    result = ofdpaNumQueuesGet(portNum, &numQueues);
    if (result != OFDPA_E_NONE)
    {
        std::cout << "ofdpaNumQueuesGet failed: " << result << std::endl;
        return result;
    }

    for (uint32_t queue = 0; queue < numQueues; ++queue)
    {
        result = ofdpaQueueStatsClear(portNum, queue);
        if (result != OFDPA_E_NONE)
        {
            std::cout << "ofdpaQueueStatsClear failed: " << result << std::endl;
            return result;
        }
    }

    return result;
}
