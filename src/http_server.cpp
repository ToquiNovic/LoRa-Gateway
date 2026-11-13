#include "http_server.h"
#include "gateway_html.h"
#include "sensor_registry.h"
#include "sensor_auth.h"
#include "betty_bridge.h"
#include "sensor_manager.h"
#include <WiFi.h>
#include <ArduinoJson.h>

HttpServer::HttpServer(SensorRegistry& reg, SensorAuthGuard& a, BettyBridge& b, SensorManager& sMgr)
    : server(HTTP_PORT),
      registry(reg),
      auth(a),
      bridge(b),
      sensorManager(sMgr) {}

void HttpServer::begin() {
    setupRoutes();

    // Start DNS Server for Captive Portal (Catch-all DNS redirects all queries to SoftAP IP)
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.printf("[CaptivePortal] DNS Server active on port 53 -> %s [OK]\n", WiFi.softAPIP().toString().c_str());

    server.begin();
    Serial.println("[HttpServer] HTTP Web Server and REST API started on port 80 [OK]");
}

void HttpServer::handleClient() {
    for (int i = 0; i < 3; ++i) {
        dnsServer.processNextRequest();
    }
    server.handleClient();
}

WebServer& HttpServer::getServer() {
    return server;
}

void HttpServer::setupRoutes() {
    // Web Dashboard (Sensor Inspector) & Captive Portal Root
    server.on("/", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/sensors", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/index.html", HTTP_GET, [this]() { this->handleRoot(); });

    // OS Captive Portal Probes (Android, iOS, Windows, ChromeOS)
    server.on("/generate_204", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/gen_204", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/connectivity-check.html", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/hotspot-detect.html", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/canonical.html", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/ncsi.txt", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/connecttest.txt", HTTP_GET, [this]() { this->handleRoot(); });
    server.on("/redirect", HTTP_GET, [this]() { this->handleRoot(); });

    // REST API - Discovery & Inspection
    server.on("/api/sensors/connected", HTTP_GET, [this]() { this->handleGetSensorsConnected(); });
    server.on("/api/status", HTTP_GET, [this]() { this->handleGetStatus(); });

    // REST API - Ingestion
    server.on("/api/sensor/data", HTTP_POST, [this]() { this->handlePostSensorData(); });

    // REST API - Control & Configuration
    server.on("/api/sensors/pair", HTTP_POST, [this]() { this->handleStartPairing(); });
    server.on("/api/gateway/config", HTTP_POST, [this]() { this->handlePostGatewayConfig(); });
    server.on("/api/wifi/scan", HTTP_GET, [this]() { this->handleWifiScan(); });

    // Web OTA Firmware Update
    server.on("/update", HTTP_GET, [this]() { this->handleOtaPage(); });
    server.on("/update", HTTP_POST,
        [this]() { this->handleOtaUploadResult(); },
        [this]() { this->handleOtaUpload(); }
    );

    server.onNotFound([this]() { this->handleNotFound(); });
}

void HttpServer::handleRoot() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send_P(200, "text/html", GATEWAY_HTML);
}

void HttpServer::handleGetSensorsConnected() {
    String json = registry.toJsonSummary();
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

void HttpServer::handlePostSensorData() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"Missing body\"}");
        return;
    }

    String body = server.arg("plain");
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        server.send(400, "application/json", "{\"error\":\"Malformed JSON\"}");
        return;
    }

    String sensorId = doc["sensorId"] | doc["id"] | "";
    String model = doc["model"] | "HTTP-Sensor";

    // Extract authentication key from Header or JSON body
    String authKey = "";
    if (server.hasHeader("X-Sensor-Key")) {
        authKey = server.header("X-Sensor-Key");
    } else if (server.hasHeader("Authorization")) {
        String authHeader = server.header("Authorization");
        if (authHeader.startsWith("Bearer ")) authKey = authHeader.substring(7);
        else authKey = authHeader;
    } else if (doc["key"].is<const char*>()) {
        authKey = doc["key"].as<String>();
    }

    String metricsJson = "{}";
    if (doc["metrics"].is<JsonObject>()) {
        serializeJson(doc["metrics"], metricsJson);
    } else if (doc["data"].is<JsonObject>()) {
        serializeJson(doc["data"], metricsJson);
    } else {
        serializeJson(doc, metricsJson);
    }

    String clientIp = server.client().remoteIP().toString();
    bool accepted = sensorManager.ingestSensorData(sensorId, "wifi_http", model, metricsJson, authKey, 0, 0.0f, clientIp);

    server.sendHeader("Access-Control-Allow-Origin", "*");
    if (accepted) {
        server.send(200, "application/json", "{\"status\":\"ACCEPTED\",\"sensorId\":\"" + sensorId + "\"}");
    } else {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\",\"message\":\"Invalid sensor link credentials\"}");
    }
}

void HttpServer::handleGetStatus() {
    JsonDocument doc;
    doc["status"] = "OK";
    doc["uptimeSec"] = millis() / 1000;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["totalSensors"] = registry.countTotal();
    doc["onlineSensors"] = registry.countOnline();
    doc["sentToBetty"] = bridge.getTotalSent();
    doc["failedBetty"] = bridge.getTotalFailed();
    doc["bettyConnected"] = bridge.isConnectedToBetty();
    doc["authRejections"] = auth.getRejectedCount();
    doc["pairingActive"] = auth.isPairingActive();
    doc["pairingRemaining"] = auth.getPairingRemainingSec();
    doc["gatewayId"] = bridge.getSensorId();
    doc["apiKey"] = bridge.getApiKey();

    String out;
    serializeJson(doc, out);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", out);
}

void HttpServer::handleStartPairing() {
    auth.startPairing(60);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"status\":\"PAIRING_STARTED\",\"durationSec\":60}");
}

void HttpServer::handlePostGatewayConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"Missing body\"}");
        return;
    }

    JsonDocument doc;
    deserializeJson(doc, server.arg("plain"));

    if (doc["ssid"].is<const char*>() && doc["pass"].is<const char*>()) {
        String newSsid = doc["ssid"].as<String>();
        String newPass = doc["pass"].as<String>();
        bridge.setStaWifi(newSsid, newPass);
        if (newSsid.length() > 0) {
            WiFi.mode(WIFI_AP_STA);
            WiFi.begin(newSsid.c_str(), newPass.c_str());
        } else {
            WiFi.disconnect(false, true);
            Serial.println("[Wi-Fi STA] Station credentials cleared -> Running in pure AP mode");
        }
    }
    if (doc["host"].is<const char*>()) bridge.setHost(doc["host"].as<String>());
    if (doc["port"].is<int>()) bridge.setPort(doc["port"].as<uint16_t>());
    if (doc["sensorId"].is<const char*>()) bridge.setSensorId(doc["sensorId"].as<String>());
    if (doc["apiKey"].is<const char*>()) bridge.setApiKey(doc["apiKey"].as<String>());
    if (doc["psk"].is<const char*>()) auth.setPsk(doc["psk"].as<String>());

    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "{\"status\":\"CONFIG_UPDATED\"}");
}

void HttpServer::handleWifiScan() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[Wi-Fi Scan] Disconnecting idle STA to release Wi-Fi state machine...");
        WiFi.disconnect(false, false);
        delay(50);
    }

    Serial.println("[Wi-Fi Scan] Scanning 2.4 GHz channels (Fast 100ms)...");
    WiFi.scanDelete();
    int16_t n = WiFi.scanNetworks(false, false, false, 100);
    Serial.printf("[Wi-Fi Scan] Scan completed: %d networks found\n", n);

    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();

    if (n > 0) {
        for (int i = 0; i < n; ++i) {
            String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) continue;

            bool duplicate = false;
            for (JsonObject net : arr) {
                if (net["ssid"] == ssid) {
                    duplicate = true;
                    if (WiFi.RSSI(i) > net["rssi"].as<int>()) {
                        net["rssi"] = WiFi.RSSI(i);
                    }
                    break;
                }
            }
            if (duplicate) continue;

            JsonObject net = arr.add<JsonObject>();
            net["ssid"] = ssid;
            net["rssi"] = WiFi.RSSI(i);
            net["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            Serial.printf("   -> Discovered SSID: '%s' (%d dBm, %s)\n",
                          ssid.c_str(), WiFi.RSSI(i),
                          WiFi.encryptionType(i) != WIFI_AUTH_OPEN ? "Secured" : "Open");
        }
    }
    WiFi.scanDelete();

    doc["count"] = arr.size();
    doc["status"] = (n >= 0) ? "done" : "error";

    String out;
    serializeJson(doc, out);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", out);
}

void HttpServer::handleOtaPage() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send_P(200, "text/html", GATEWAY_HTML);
}

void HttpServer::handleOtaUploadResult() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Connection", "close");
    if (Update.hasError()) {
        server.send(500, "application/json", "{\"status\":\"ERROR\",\"message\":\"Update failed\"}");
    } else {
        server.send(200, "application/json", "{\"status\":\"SUCCESS\",\"message\":\"Firmware updated. Rebooting now...\"}");
        delay(1000);
        ESP.restart();
    }
}

void HttpServer::handleOtaUpload() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        updatingFirmware = true;
        Serial.printf("\n[OTA] ==================================================\n");
        Serial.printf("[OTA] Receiving remote firmware update: %s\n", upload.filename.c_str());
        Serial.printf("[OTA] Target partition: Next OTA slot\n");
        
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
            Serial.print("[OTA] Error in Update.begin: ");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Serial.print("[OTA] Error in Update.write: ");
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("[OTA] Firmware binary written successfully: %u bytes!\n", upload.totalSize);
            Serial.printf("[OTA] Rebooting gateway into new firmware partition...\n");
            Serial.printf("[OTA] ==================================================\n\n");
        } else {
            Serial.print("[OTA] Error in Update.end: ");
            Update.printError(Serial);
            updatingFirmware = false;
        }
    } else if (upload.status == UPLOAD_FILE_ABORTED) {
        Update.end();
        Serial.println("[OTA] Firmware upload ABORTED by client.");
        updatingFirmware = false;
    }
}

void HttpServer::handleNotFound() {
    String uri = server.uri();
    if (uri.startsWith("/api/")) {
        server.sendHeader("Access-Control-Allow-Origin", "*");
        server.send(404, "application/json", "{\"error\":\"Endpoint not found\"}");
        return;
    }

    // Captive Portal 302 Redirect to root dashboard
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
}
