#include "sensor_registry.h"
#include <ArduinoJson.h>

void SensorRegistry::updateSensor(const String& id, const String& interface, const String& model,
                                  int rssi, float snr, const String& metricsJson,
                                  const String& ip, bool authorized) {
    if (id.length() == 0) return;

    SensorDevice& s = activeSensors[id];
    s.id = id;
    s.interface = interface;
    if (model.length() > 0) s.model = model;
    if (ip.length() > 0) s.ip = ip;
    s.rssi = rssi;
    s.snr = snr;
    s.authorized = authorized;
    if (metricsJson.length() > 0) s.lastMetricsJson = metricsJson;
    s.packetCount++;
    s.lastSeen = millis();
    s.online = true;
}

SensorDevice& SensorRegistry::getSensor(const String& id) {
    return activeSensors[id];
}

bool SensorRegistry::hasSensor(const String& id) const {
    return activeSensors.find(id) != activeSensors.end();
}

std::map<String, SensorDevice>& SensorRegistry::getAllSensors() {
    return activeSensors;
}

void SensorRegistry::updateOnlineStatuses(unsigned long timeoutMillis) {
    unsigned long now = millis();
    for (auto& pair : activeSensors) {
        if (now - pair.second.lastSeen > timeoutMillis) {
            pair.second.online = false;
        }
    }
}

size_t SensorRegistry::countOnline() const {
    size_t count = 0;
    for (const auto& pair : activeSensors) {
        if (pair.second.online) count++;
    }
    return count;
}

size_t SensorRegistry::countTotal() const {
    return activeSensors.size();
}

String SensorRegistry::toJsonSummary() {
    updateOnlineStatuses();

    JsonDocument doc;
    JsonArray arr = doc["sensors"].to<JsonArray>();

    for (const auto& pair : activeSensors) {
        const SensorDevice& s = pair.second;
        JsonObject obj = arr.add<JsonObject>();
        obj["id"] = s.id;
        obj["interface"] = s.interface;
        obj["model"] = s.model;
        obj["ip"] = s.ip;
        obj["rssi"] = s.rssi;
        obj["snr"] = s.snr;
        obj["packets"] = s.packetCount;
        obj["online"] = s.online;
        obj["authorized"] = s.authorized;
        obj["ageSec"] = (millis() - s.lastSeen) / 1000;

        // Parse metrics JSON into nested object if valid
        JsonDocument metricsDoc;
        DeserializationError err = deserializeJson(metricsDoc, s.lastMetricsJson);
        if (!err) {
            obj["metrics"] = metricsDoc.as<JsonObject>();
        } else {
            obj["raw"] = s.lastMetricsJson;
        }
    }

    doc["total"] = activeSensors.size();
    doc["online"] = countOnline();
    doc["uptimeSec"] = millis() / 1000;

    String output;
    serializeJson(doc, output);
    return output;
}

void SensorRegistry::printRegistry() {
    updateOnlineStatuses();
    Serial.println("\n==================================================");
    Serial.printf("        LIVE SENSOR REGISTRY (Total: %u, Online: %u)\n",
                  (unsigned)activeSensors.size(), (unsigned)countOnline());
    Serial.println("==================================================");
    for (const auto& pair : activeSensors) {
        const SensorDevice& s = pair.second;
        Serial.printf("  [%s] %s | Model: %s | Interface: %s | RSSI: %d dBm | Age: %lus | Packets: %u\n",
                      s.online ? "ONLINE" : "OFFLINE",
                      s.id.c_str(), s.model.c_str(), s.interface.c_str(),
                      s.rssi, (millis() - s.lastSeen) / 1000, s.packetCount);
        Serial.printf("         Metrics: %s\n", s.lastMetricsJson.c_str());
    }
    Serial.println("==================================================\n");
}
