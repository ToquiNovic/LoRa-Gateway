#ifndef SENSOR_DEVICE_H
#define SENSOR_DEVICE_H

#include <Arduino.h>

// ====================================================================
// SENSOR DEVICE MODEL - Clean Telemetry Representation
// ====================================================================

struct SensorDevice {
    String id;                  // Unique identifier (e.g., "BME280-01", "SOIL-A0", "NODE-LORA-02")
    String interface = "lora";  // "lora", "wifi_http", "ws", "ble", "i2c", "adc", "serial"
    String model = "Generic";   // Hardware or sensor model
    String ip = "";             // IP address (if applicable)
    int rssi = 0;               // Signal strength in dBm
    float snr = 0.0f;           // Signal-to-noise ratio in dB (LoRa)
    uint32_t packetCount = 0;   // Count of successfully ingested frames
    unsigned long lastSeen = 0; // millis() of last received telemetry
    bool online = true;         // Active connection flag
    bool authorized = true;     // Link-level authentication status
    String lastMetricsJson = "{}"; // Latest dynamic metrics as JSON string
};

#endif // SENSOR_DEVICE_H
