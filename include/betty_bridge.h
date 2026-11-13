#ifndef BETTY_BRIDGE_H
#define BETTY_BRIDGE_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <Preferences.h>

// ====================================================================
// BETTY BRIDGE - Uplink Gateway to Betty (Botuto) PaaS
// ====================================================================

struct BettyBridgePacket {
    char sensorId[48];
    char interface[16];
    char payloadJson[512];
    int rssi;
    float snr;
};

class BettyBridge {
public:
    BettyBridge();

    void begin();

    // Enqueue sensor reading for asynchronous dispatch to Betty
    bool enqueueTelemetry(const String& sensorId, const String& interface,
                          const String& payloadJson, int rssi = 0, float snr = 0.0f);

    // Configuration
    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setHost(const String& host);
    String getHost() const;

    void setPort(uint16_t port);
    uint16_t getPort() const;

    void setSensorId(const String& sensorId);
    String getSensorId() const;

    void setApiKey(const String& apiKey);
    String getApiKey() const;

    void setStaWifi(const String& ssid, const String& pass);
    String getStaSsid() const;
    String getStaPass() const;

    // Diagnostics & Statistics
    uint32_t getTotalSent() const;
    uint32_t getTotalFailed() const;
    int getLastHttpCode() const;
    unsigned long getLastSentTime() const;
    bool isConnectedToBetty() const;

    // Task runner called from Core 0
    void processBridgeQueue();

private:
    Preferences prefs;
    QueueHandle_t xBridgeQueue;

    bool enabled;
    String host;
    uint16_t port;
    String sensorId;
    String apiKey;
    String staSsid;
    String staPass;

    uint32_t totalSent;
    uint32_t totalFailed;
    int lastHttpCode;
    unsigned long lastSentTime;

    bool postToBetty(const BettyBridgePacket& pkt);
};

// Global FreeRTOS Task function
void Task_Betty_Bridge(void* pvParameters);

#endif // BETTY_BRIDGE_H
