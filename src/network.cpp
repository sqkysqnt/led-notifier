#include "network.h"
#include <WiFi.h>
#include <WiFiManager.h>

#ifdef BOARD_ESP32_S3_ETH
  // The khoih-prog library expects these #defines before its include.
  // We map them from our platformio.ini flags.
  #define INT_GPIO        WS_W5500_INT_GPIO
  #define MISO_GPIO       WS_W5500_MISO_GPIO
  #define MOSI_GPIO       WS_W5500_MOSI_GPIO
  #define SCK_GPIO        WS_W5500_SCK_GPIO
  #define CS_GPIO         WS_W5500_CS_GPIO
  #define ETH_SPI_HOST    SPI3_HOST
  #define SPI_CLOCK_MHZ   WS_W5500_SPI_CLOCK_MHZ
  #include <WebServer_ESP32_SC_W5500.h>
#elif defined(BOARD_WT32_ETH01)
  #include <ETH.h>
  // LAN8720 RMII pins for WT32-ETH01
  #define ETH_PHY_ADDR_WT    1
  #define ETH_PHY_POWER_WT   16
  #define ETH_PHY_MDC_WT     23
  #define ETH_PHY_MDIO_WT    18
  #define ETH_PHY_TYPE_WT    ETH_PHY_LAN8720
  #define ETH_CLK_MODE_WT    ETH_CLOCK_GPIO0_IN
#endif

static enum { MODE_NONE, MODE_ETH, MODE_WIFI } activeMode = MODE_NONE;

#if defined(BOARD_WT32_ETH01) || defined(BOARD_ESP32_S3_ETH)
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
      Serial.printf("[ETH] IP: %s\n", ETH.localIP().toString().c_str());
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

#if defined(BOARD_WT32_ETH01)
  ETH.begin(ETH_PHY_ADDR_WT, ETH_PHY_POWER_WT, ETH_PHY_MDC_WT,
            ETH_PHY_MDIO_WT, ETH_PHY_TYPE_WT, ETH_CLK_MODE_WT);
#elif defined(BOARD_ESP32_S3_ETH)
  // Pulse W5500 reset line so a warm reset starts the chip clean
  pinMode(WS_W5500_RST_GPIO, OUTPUT);
  digitalWrite(WS_W5500_RST_GPIO, LOW);
  delay(20);
  digitalWrite(WS_W5500_RST_GPIO, HIGH);
  delay(150);
  // khoih-prog API: ETH.begin(miso, mosi, sck, cs, irq, spi_clock_mhz, spi_host)
  ETH.begin(WS_W5500_MISO_GPIO, WS_W5500_MOSI_GPIO, WS_W5500_SCK_GPIO,
            WS_W5500_CS_GPIO, WS_W5500_INT_GPIO,
            WS_W5500_SPI_CLOCK_MHZ, SPI3_HOST);
#endif

  uint32_t start = millis();
  while (!eth_got_ip && millis() - start < timeoutMs) {
    delay(50);
  }
  return eth_got_ip;
}
#endif

bool initNetwork(const char* apName, bool forcePortal) {
#if defined(BOARD_WT32_ETH01) || defined(BOARD_ESP32_S3_ETH)
  // Try Ethernet first (WiFi falls back if no link or DHCP fails)
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
#if defined(BOARD_WT32_ETH01) || defined(BOARD_ESP32_S3_ETH)
  if (activeMode == MODE_ETH) return ETH.localIP();
#endif
  return WiFi.localIP();
}
