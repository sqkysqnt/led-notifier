#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>
#include <IPAddress.h>

// Initialize networking.
// On BOARD_WT32_ETH01: tries Ethernet first (blocks briefly for link+DHCP),
// falls back to WiFiManager if Ethernet isn't available.
// On other boards: WiFiManager only.
// forcePortal: if true, resets stored WiFi credentials first.
// Returns true on successful network connection, false otherwise.
bool initNetwork(const char* apName, bool forcePortal);

// Which transport the device is currently using, for display purposes.
const char* currentNetworkMode();

// IP of the active interface.
IPAddress currentIP();

#endif
