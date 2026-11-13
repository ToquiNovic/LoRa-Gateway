#ifndef SENSOR_REGISTRY_H
#define SENSOR_REGISTRY_H

#include <Arduino.h>
#include <map>
#include "sensor_device.h"

// ====================================================================
// SENSOR REGISTRY - Active Connected Sensor Tracking & State Store
// ====================================================================

class SensorRegistry {
public:
    // Update or register incoming sensor telemetry
    void updateSensor(const String& id, const String& interface, const String& model,
                      int rssi, float snr, const String& metricsJson,
                      const String& ip = "", bool authorized = true);

    // Lookups & Access
    SensorDevice& getSensor(const String& id);
    bool hasSensor(const String& id) const;
    std::map<String, SensorDevice>& getAllSensors();

    // JSON Serialization for Web UI and REST API
    String toJsonSummary();

    // Periodic maintenance
    void updateOnlineStatuses(unsigned long timeoutMillis = 60000);
    size_t countOnline() const;
    size_t countTotal() const;

    // Diagnostic logging
    void printRegistry();

private:
    std::map<String, SensorDevice> activeSensors;
};

#endif // SENSOR_REGISTRY_H
