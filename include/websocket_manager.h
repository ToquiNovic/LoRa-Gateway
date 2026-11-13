#ifndef WEBSOCKET_MANAGER_H
#define WEBSOCKET_MANAGER_H

#include <Arduino.h>
#include <WebSocketsServer.h>
#include "config.h"
#include <map>

// Forward declaration
class SensorManager;

// ====================================================================
// WEBSOCKET MANAGER - Real-Time Dashboard & Telemetry Stream
// ====================================================================

class WebSocketManager {
public:
    WebSocketManager();

    void begin();
    void loop();

    void setSensorManager(SensorManager* mgr);

    // Raw broadcast to all connected web clients
    void broadcastRaw(const String& jsonText);

    // Server access
    WebSocketsServer& getServer();

    // Event handler
    void handleEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);

    // Client tracking
    void registerClientId(uint8_t num, const String& id);
    String getClientId(uint8_t num);
    void removeClientId(uint8_t num);
    IPAddress getClientIp(uint8_t num);

private:
    WebSocketsServer server;
    SensorManager* sensorManager;
    std::map<uint8_t, String> clientIds;
    std::map<uint8_t, bool> clientAuthorized;
};

#endif // WEBSOCKET_MANAGER_H
