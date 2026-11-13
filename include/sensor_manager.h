#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include "sensor_device.h"
#include "sensor_registry.h"
#include "sensor_auth.h"
#include "betty_bridge.h"

// Forward declaration
class WebSocketManager;

// ====================================================================
// SENSOR MANAGER - Ingestion Coordinator & Pipeline Controller
// ====================================================================

class SensorManager {
public:
    SensorManager(SensorRegistry& registry, SensorAuthGuard& authGuard, BettyBridge& bridge);

    void begin();
    void setWebSocketManager(WebSocketManager* wsMgr);

    // Primary Ingestion Entry Point for all interfaces
    bool ingestSensorData(const String& sensorId,
                          const String& interface,
                          const String& model,
                          const String& metricsJson,
                          const String& authKey = "",
                          int rssi = 0,
                          float snr = 0.0f,
                          const String& ip = "");

    // Interface-specific parsers
    bool ingestLoRaFrame(const uint8_t* payload, size_t length, int rssi, float snr);
    bool ingestSerialLine(const String& line);

    // Periodic physical sensor sampling (I2C / ADC)
    void samplePhysicalSensors();

private:
    SensorRegistry& registry;
    SensorAuthGuard& auth;
    BettyBridge& bridge;
    WebSocketManager* wsManager;

    unsigned long lastPhysicalSample;
    bool hasI2cSensor;
};

#endif // SENSOR_MANAGER_H
