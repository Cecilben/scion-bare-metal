#pragma once

extern "C" {
#include "ofdpa_datatypes.h"
}

#include <stddef.h>
#include <stdint.h>
#include <ostream>


std::ostream& operator<<(std::ostream &stream, const ofdpaMacAddr_t &mac);
std::ostream& printPacket(std::ostream &stream, char *buffer, size_t length);

OFDPA_ERROR_t printPhysicalPortInfo(uint32_t portNum);
OFDPA_ERROR_t printPortInfo(uint32_t portNum);
void enumeratePorts();

OFDPA_ERROR_t printPortStatistics(uint32_t portNum);
OFDPA_ERROR_t clearPortStatistics(uint32_t portNum);
