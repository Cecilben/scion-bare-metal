#pragma once

extern "C" {
    #include <opennsl/types.h>
}

#include <iostream>


int configureAllPorts(int unit);
int configurePort(int unit, int port, opennsl_pbmp_t cpuPort, opennsl_vlan_t vlanId, opennsl_vlan_t defVlanId);

int getPortInfo(int unit, int port);

void enumeratePorts(int unit);
int enumerateVlans(int unit);

int getPortStats(int unit, int port);
int resetPortStats(int unit, int port);

std::ostream& operator<<(std::ostream &stream, const opennsl_pbmp_t &mask);
std::ostream& operator<<(std::ostream &stream, const opennsl_mac_t &mac);

size_t parseHexString(const char *str, uint8_t *bytes, size_t length);
