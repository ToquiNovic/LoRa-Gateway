#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include "config.h"

// Forward declarations
class SensorRegistry;
class SensorAuthGuard;
class BettyBridge;
class SensorManager;

// ====================================================================
// HTTP SERVER & CAPTIVE PORTAL - Web Dashboard, REST API & Ingestion
// ====================================================================

class HttpServer {
public:
    HttpServer(SensorRegistry& reg, SensorAuthGuard& a, BettyBridge& b, SensorManager& sMgr);

    void begin();
    void handleClient();
    WebServer& getServer();

private:
    WebServer server;
    DNSServer dnsServer;
    SensorRegistry& registry;
    SensorAuthGuard& auth;
    BettyBridge& bridge;
    SensorManager& sensorManager;

    void setupRoutes();

    // Route handlers
    void handleRoot();
    void handleGetSensorsConnected();
    void handlePostSensorData();
    void handleGetStatus();
    void handleStartPairing();
    void handlePostGatewayConfig();
    void handleWifiScan();
    void handleOtaPage();
    void handleOtaUpload();
    void handleOtaUploadResult();
    void handleNotFound();

    bool updatingFirmware = false;
};

#endif // HTTP_SERVER_H
