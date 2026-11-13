#include "sensor_manager.h"
#include "websocket_manager.h"
#include <ArduinoJson.h>
#include <Wire.h>

SensorManager::SensorManager(SensorRegistry& r, SensorAuthGuard& a, BettyBridge& b)
    : registry(r),
      auth(a),
      bridge(b),
      wsManager(nullptr),
      lastPhysicalSample(0),
      hasI2cSensor(false) {}

void SensorManager::begin() {
    Serial.println("[SensorManager] Initializing Multi-Transport Ingestion Layer...");

    // Initialize I2C on standard ESP32 pins (SDA=21, SCL=22)
    Wire.begin(21, 22);
    Wire.setTimeOut(50); // Short timeout

    // Simple I2C Bus Scan
    byte error, address;
    int nDevices = 0;
    for (address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.printf("[SensorManager] Discovered physical I2C device at 0x%02X\n", address);
            nDevices++;
            hasI2cSensor = true;
        }
    }
    if (nDevices == 0) {
        Serial.println("[SensorManager] No physical I2C sensors found on SDA=21, SCL=22");
    }
}

void SensorManager::setWebSocketManager(WebSocketManager* wsMgr) {
    wsManager = wsMgr;
}

bool SensorManager::ingestSensorData(const String& sensorId,
                                    const String& interface,
                                    const String& model,
                                    const String& metricsJson,
                                    const String& authKey,
                                    int rssi,
                                    float snr,
                                    const String& ip) {
    if (sensorId.length() == 0) {
        Serial.println("[SensorManager] Ingestion error: Empty sensorId");
        return false;
    }

    // 1. Verify link-level credentials
    if (!auth.isSensorAuthorized(sensorId, authKey)) {
        return false;
    }

    // 2. Update Live Sensor Registry
    registry.updateSensor(sensorId, interface, model, rssi, snr, metricsJson, ip, true);

    // 3. Broadcast to local WebSockets clients (Web Dashboard / Inspector)
    if (wsManager) {
        JsonDocument wsDoc;
        wsDoc["type"] = "sensor_update";
        wsDoc["sensorId"] = sensorId;
        wsDoc["interface"] = interface;
        wsDoc["model"] = model;
        wsDoc["rssi"] = rssi;
        wsDoc["snr"] = snr;
        wsDoc["metrics"] = metricsJson;

        String wsOut;
        serializeJson(wsDoc, wsOut);
        wsManager->broadcastRaw(wsOut);
    }

    // 4. Enqueue to Betty PaaS Bridge
    bridge.enqueueTelemetry(sensorId, interface, metricsJson, rssi, snr);

    Serial.printf("[SensorManager] Ingested [%s] '%s' (%s) | Metrics: %s\n",
                  interface.c_str(), sensorId.c_str(), model.c_str(), metricsJson.c_str());
    return true;
}

bool SensorManager::ingestLoRaFrame(const uint8_t* payload, size_t length, int rssi, float snr) {
    if (length == 0) return false;

    // Case 1: JSON frame
    if (payload[0] == '{') {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload, length);
        if (!err) {
            String sensorId = doc["id"] | doc["sensorId"] | "LORA-NODE";
            String key = doc["key"] | doc["auth"] | "";
            String model = doc["model"] | "LoRa-Node";

            String metricsJson = "{}";
            if (doc["metrics"].is<JsonObject>()) {
                serializeJson(doc["metrics"], metricsJson);
            } else if (doc["data"].is<JsonObject>()) {
                serializeJson(doc["data"], metricsJson);
            } else {
                serializeJson(doc, metricsJson);
            }

            return ingestSensorData(sensorId, "lora", model, metricsJson, key, rssi, snr);
        }
    }

    // Case 2: Structured text frame ($DATA,ID=...,KEY=...,TEMP=25.0,HUM=60.0*)
    String raw = "";
    raw.reserve(length);
    for (size_t i = 0; i < length; i++) raw += (char)payload[i];

    if (raw.startsWith("$") && raw.indexOf('*') != -1) {
        int end = raw.indexOf('*');
        String content = raw.substring(1, end);

        String sensorId = "LORA-SENSOR";
        String key = "";
        String model = "LoRa-Param";
        JsonDocument metrics;

        int start = 0;
        int sep = content.indexOf(',');
        while (sep != -1 || start < content.length()) {
            String part = (sep != -1) ? content.substring(start, sep) : content.substring(start);
            int eq = part.indexOf('=');
            if (eq != -1) {
                String k = part.substring(0, eq);
                String v = part.substring(eq + 1);
                k.trim(); v.trim();

                if (k.equalsIgnoreCase("ID")) sensorId = v;
                else if (k.equalsIgnoreCase("KEY")) key = v;
                else if (k.equalsIgnoreCase("MODEL")) model = v;
                else {
                    // Try parsing as float or int
                    char* endptr;
                    float fVal = strtof(v.c_str(), &endptr);
                    if (*endptr == '\0') {
                        metrics[k] = fVal;
                    } else {
                        metrics[k] = v;
                    }
                }
            }
            if (sep == -1) break;
            start = sep + 1;
            sep = content.indexOf(',', start);
        }

        String metricsJson;
        serializeJson(metrics, metricsJson);
        return ingestSensorData(sensorId, "lora", model, metricsJson, key, rssi, snr);
    }

    return false;
}

bool SensorManager::ingestSerialLine(const String& line) {
    String trimmed = line;
    trimmed.trim();
    if (trimmed.length() == 0) return false;

    if (trimmed.startsWith("{")) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, trimmed);
        if (!err) {
            String sensorId = doc["id"] | doc["sensorId"] | "SERIAL-DEVICE";
            String key = doc["key"] | doc["auth"] | "";
            String model = doc["model"] | "Serial-Device";

            String metricsJson = "{}";
            if (doc["metrics"].is<JsonObject>()) {
                serializeJson(doc["metrics"], metricsJson);
            } else {
                serializeJson(doc, metricsJson);
            }
            return ingestSensorData(sensorId, "serial", model, metricsJson, key, 0, 0.0f);
        }
    }
    return false;
}

void SensorManager::samplePhysicalSensors() {
    unsigned long now = millis();
    if (now - lastPhysicalSample < 10000) return; // Sample every 10 seconds
    lastPhysicalSample = now;

    // 1. Read on-board ADC channel (e.g. GPIO 34 analog sensor)
    int adcRaw = analogRead(34);
    float adcVoltage = (adcRaw / 4095.0f) * 3.3f;

    JsonDocument doc;
    doc["adc_raw"] = adcRaw;
    doc["voltage"] = round(adcVoltage * 100.0f) / 100.0f;
    doc["free_heap"] = ESP.getFreeHeap();

    String metricsJson;
    serializeJson(doc, metricsJson);

    // Physical sensors on the gateway are pre-authorized
    ingestSensorData("BASE-ONBOARD", "adc", "ESP32-ADC", metricsJson, auth.getPsk(), 0, 0.0f);
}
