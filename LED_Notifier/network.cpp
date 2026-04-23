#include "network.h"
#include <WiFi.h>
#include <WiFiManager.h>

#ifdef BOARD_WT32_ETH01
  #include <ETH.h>
  // LAN8720 pins for WT32-ETH01
  #define ETH_PHY_ADDR_WT    1
  #define ETH_PHY_POWER_WT   16
  #define ETH_PHY_MDC_WT     23
  #define ETH_PHY_MDIO_WT    18
  #define ETH_PHY_TYPE_WT    ETH_PHY_LAN8720
  #define ETH_CLK_MODE_WT    ETH_CLOCK_GPIO0_IN
#endif

static enum { MODE_NONE, MODE_ETH, MODE_WIFI } activeMode = MODE_NONE;

#ifdef BOARD_WT32_ETH01
static volatile bool eth_got_ip = false;

static void onNetEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println("[ETH] Started");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println("[ETH] Link up");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      Serial.printf("[ETH] IP: %s  (%s %s)\n",
                    ETH.localIP().toString().c_str(),
                    ETH.fullDuplex() ? "FULL" : "HALF",
                    (ETH.linkSpeed() == 100) ? "100Mbps" : "10Mbps");
      eth_got_ip = true;
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      Serial.println("[ETH] Link down");
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println("[ETH] Stopped");
      break;
    default:
      break;
  }
}

static bool tryEthernet(uint32_t timeoutMs) {
  eth_got_ip = false;
  WiFi.onEvent(onNetEvent);
  ETH.begin(ETH_PHY_ADDR_WT, ETH_PHY_POWER_WT, ETH_PHY_MDC_WT,
            ETH_PHY_MDIO_WT, ETH_PHY_TYPE_WT, ETH_CLK_MODE_WT);
  uint32_t start = millis();
  while (!eth_got_ip && millis() - start < timeoutMs) {
    delay(50);
  }
  return eth_got_ip;
}
#endif

bool initNetwork(const char* apName, bool forcePortal) {
#ifdef BOARD_WT32_ETH01
  // Try Ethernet first
  Serial.println("[Net] Attempting Ethernet...");
  if (tryEthernet(8000)) {
    activeMode = MODE_ETH;
    return true;
  }
  Serial.println("[Net] Ethernet unavailable, falling back to WiFi");
#endif

  // WiFi via WiFiManager
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  wm.setAPCallback([](WiFiManager* w) {
    Serial.println("[WiFi] Config portal started");
  });
  if (forcePortal) {
    Serial.println("[WiFi] Reset WiFi credentials");
    wm.resetSettings();
  }
  if (!wm.autoConnect(apName)) {
    Serial.println("[WiFi] Failed to connect");
    activeMode = MODE_NONE;
    return false;
  }
  Serial.printf("[WiFi] Connected: %s\n", WiFi.localIP().toString().c_str());
  WiFi.setSleep(false);
  activeMode = MODE_WIFI;
  return true;
}

const char* currentNetworkMode() {
  switch (activeMode) {
    case MODE_ETH:  return "ethernet";
    case MODE_WIFI: return "wifi";
    default:        return "none";
  }
}

IPAddress currentIP() {
#ifdef BOARD_WT32_ETH01
  if (activeMode == MODE_ETH) return ETH.localIP();
#endif
  return WiFi.localIP();
}
