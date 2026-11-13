#include "websocket_manager.h"
#include "sensor_manager.h"
#include <ArduinoJson.h>

WebSocketManager::WebSocketManager()
    : server(WEBSOCKET_PORT),
      sensorManager(nullptr) {}

void WebSocketManager::begin() {
    server.begin();
    server.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        this->handleEvent(num, type, payload, length);
    });
    server.enableHeartbeat(WEBSOCKET_HEARTBEAT, 3000, 2);
    Serial.println("[WebSocketManager] Server started on port 81 [OK]");
}

void WebSocketManager::loop() {
    server.loop();
}

void WebSocketManager::setSensorManager(SensorManager* mgr) {
    sensorManager = mgr;
}

void WebSocketManager::broadcastRaw(const String& jsonText) {
    String copy = jsonText;
    server.broadcastTXT(copy);
}

WebSocketsServer& WebSocketManager::getServer() {
    return server;
}

void WebSocketManager::handleEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_CONNECTED) {
        IPAddress ip = server.remoteIP(num);
        Serial.printf("[WebSocketManager] Client #%u connected from %s\n", num, ip.toString().c_str());
        registerClientId(num, "WS-CLIENT-" + String(num));
        clientAuthorized[num] = false;

    } else if (type == WStype_DISCONNECTED) {
        Serial.printf("[WebSocketManager] Client #%u disconnected\n", num);
        removeClientId(num);
        clientAuthorized.erase(num);

    } else if (type == WStype_TEXT && length > 0) {
        if (!sensorManager) return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, payload, length);
        if (err) return;

        String msgType = doc["type"] | "";
        String sensorId = doc["sensorId"] | doc["id"] | getClientId(num);
        String key = doc["key"] | doc["auth"] | "";
        String model = doc["model"] | "WebSocket-Client";

        if (msgType == "auth") {
            // Explicit handshake
            bool ok = sensorManager->ingestSensorData(sensorId, "ws", model, "{\"status\":\"auth_request\"}", key);
            clientAuthorized[num] = ok;

            JsonDocument ack;
            ack["type"] = "auth_ack";
            ack["status"] = ok ? "AUTHORIZED" : "REJECTED";
            String ackStr;
            serializeJson(ack, ackStr);
            server.sendTXT(num, ackStr);

        } else if (msgType == "sensor_data" || msgType == "telemetry" || doc["metrics"].is<JsonObject>()) {
            String metricsJson = "{}";
            if (doc["metrics"].is<JsonObject>()) {
                serializeJson(doc["metrics"], metricsJson);
            } else if (doc["data"].is<JsonObject>()) {
                serializeJson(doc["data"], metricsJson);
            } else {
                serializeJson(doc, metricsJson);
            }

            IPAddress ip = server.remoteIP(num);
            sensorManager->ingestSensorData(sensorId, "ws", model, metricsJson, key, 0, 0.0f, ip.toString());
        }
    }
}

void WebSocketManager::registerClientId(uint8_t num, const String& id) {
    clientIds[num] = id;
}

String WebSocketManager::getClientId(uint8_t num) {
    if (clientIds.find(num) != clientIds.end()) return clientIds[num];
    return "WS-" + String(num);
}

void WebSocketManager::removeClientId(uint8_t num) {
    clientIds.erase(num);
}

IPAddress WebSocketManager::getClientIp(uint8_t num) {
    return server.remoteIP(num);
}
