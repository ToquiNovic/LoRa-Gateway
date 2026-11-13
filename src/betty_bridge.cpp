#include "betty_bridge.h"
#include "config.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

BettyBridge::BettyBridge()
    : xBridgeQueue(NULL),
      enabled(true),
      host(BETTY_DEFAULT_HOST),
      port(BETTY_DEFAULT_HTTP_PORT),
      sensorId(BETTY_DEFAULT_SENSOR_ID),
      apiKey(BETTY_DEFAULT_API_KEY),
      staSsid(DEFAULT_STA_SSID),
      staPass(DEFAULT_STA_PASS),
      totalSent(0),
      totalFailed(0),
      lastHttpCode(0),
      lastSentTime(0) {}

void BettyBridge::begin() {
    prefs.begin("betty_bridge", false);
    enabled = prefs.getBool("enabled", true);
    host = prefs.getString("host", BETTY_DEFAULT_HOST);
    port = prefs.getUShort("port", BETTY_DEFAULT_HTTP_PORT);

    // Auto-migrar IP dummy anterior
    if (host == "192.168.1.100") {
        host = BETTY_DEFAULT_HOST;
        port = BETTY_DEFAULT_HTTP_PORT;
        prefs.putString("host", host);
        prefs.putUShort("port", port);
    }

    sensorId = prefs.getString("sensor_id", BETTY_DEFAULT_SENSOR_ID);
    apiKey = prefs.getString("api_key", BETTY_DEFAULT_API_KEY);
    staSsid = prefs.getString("sta_ssid", DEFAULT_STA_SSID);
    staPass = prefs.getString("sta_pass", DEFAULT_STA_PASS);

    xBridgeQueue = xQueueCreate(32, sizeof(BettyBridgePacket));
    if (!xBridgeQueue) {
        Serial.println("[BettyBridge] ERROR: Could not create FreeRTOS bridge queue!");
    } else {
        Serial.printf("[BettyBridge] Initialized. Target: %s:%u/api/mqtt/webhook | Gateway ID: %s\n",
                      host.c_str(), port, sensorId.c_str());
    }

    // Launch Bridge Task on Core 0
    xTaskCreatePinnedToCore(
        Task_Betty_Bridge,
        "Task_Betty_Bridge",
        4096,
        this,
        1, // Priority 1 (Background worker)
        NULL,
        0 // Core 0
    );
}

bool BettyBridge::enqueueTelemetry(const String& targetSensorId, const String& interface,
                                   const String& payloadJson, int rssi, float snr) {
    if (!enabled || !xBridgeQueue) return false;

    BettyBridgePacket pkt;
    memset(&pkt, 0, sizeof(pkt));

    strncpy(pkt.sensorId, targetSensorId.c_str(), sizeof(pkt.sensorId) - 1);
    strncpy(pkt.interface, interface.c_str(), sizeof(pkt.interface) - 1);
    strncpy(pkt.payloadJson, payloadJson.c_str(), sizeof(pkt.payloadJson) - 1);
    pkt.rssi = rssi;
    pkt.snr = snr;

    if (xQueueSend(xBridgeQueue, &pkt, 0) != pdTRUE) {
        Serial.println("[BettyBridge] Warning: Queue full! Dropping oldest telemetry packet.");
        totalFailed++;
        return false;
    }
    return true;
}

bool BettyBridge::postToBetty(const BettyBridgePacket& pkt) {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    // Determine target sensor ID: use the sensor's own ID/UUID from telemetry
    String targetId = (strlen(pkt.sensorId) > 0) ? String(pkt.sensorId) : "gateway-node";
    String topic = "botuto/sensor/" + targetId + "/data";

    // Build unified Betty payload
    JsonDocument bodyDoc;
    bodyDoc["topic"] = topic;

    JsonObject payloadObj = bodyDoc["payload"].to<JsonObject>();
    payloadObj["origin_type"] = "sensor";
    payloadObj["gateway_id"] = (sensorId.length() > 0) ? sensorId : "gateway-esp32-01";
    payloadObj["source_sensor"] = targetId;
    payloadObj["interface"] = String(pkt.interface);
    if (pkt.rssi != 0) payloadObj["rssi"] = pkt.rssi;
    if (pkt.snr != 0.0f) payloadObj["snr"] = pkt.snr;

    // Parse metric payload
    JsonDocument metricsDoc;
    DeserializationError err = deserializeJson(metricsDoc, pkt.payloadJson);
    if (!err && metricsDoc.is<JsonObject>()) {
        payloadObj["metrics"] = metricsDoc.as<JsonObject>();
    } else {
        payloadObj["raw_data"] = String(pkt.payloadJson);
    }

    String jsonString;
    serializeJson(bodyDoc, jsonString);

    // Clean host
    String cleanHost = host;
    cleanHost.trim();
    if (cleanHost.startsWith("http://")) cleanHost = cleanHost.substring(7);
    if (cleanHost.startsWith("https://")) cleanHost = cleanHost.substring(8);
    if (cleanHost.endsWith("/")) cleanHost = cleanHost.substring(0, cleanHost.length() - 1);

    bool isHttps = (port == 443) || host.startsWith("https://") || 
                   (cleanHost.indexOf('.') != -1 && !cleanHost.endsWith(".local") && !isdigit(cleanHost[0]));

    HTTPClient http;
    int httpCode = 0;
    String url;

    if (isHttps) {
        if (port == 443 || port == 0) {
            url = "https://" + cleanHost + "/api/mqtt/webhook";
        } else {
            url = "https://" + cleanHost + ":" + String(port) + "/api/mqtt/webhook";
        }

        WiFiClientSecure secClient;
        secClient.setInsecure(); // No certificate chain validation required for embedded IoT
        secClient.setTimeout(5);

        if (!http.begin(secClient, url)) {
            Serial.printf("[BettyBridge] Failed to init HTTPS connection to %s\n", url.c_str());
            totalFailed++;
            return false;
        }

        http.addHeader("Content-Type", "application/json");
        http.setTimeout(5000);
        httpCode = http.POST(jsonString);
    } else {
        url = "http://" + cleanHost + ":" + String(port) + "/api/mqtt/webhook";
        WiFiClient client;
        if (!http.begin(client, url)) {
            Serial.printf("[BettyBridge] Failed to init HTTP connection to %s\n", url.c_str());
            totalFailed++;
            return false;
        }

        http.addHeader("Content-Type", "application/json");
        http.setTimeout(3000);
        httpCode = http.POST(jsonString);
    }

    lastHttpCode = httpCode;

    if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED) {
        totalSent++;
        lastSentTime = millis();
        http.end();
        Serial.printf("[BettyBridge] Telemetry dispatched successfully to %s [HTTP %d]\n", url.c_str(), httpCode);
        return true;
    } else {
        Serial.printf("[BettyBridge] Uplink error to %s: HTTP %d\n", url.c_str(), httpCode);
        totalFailed++;
        http.end();
        return false;
    }
}

void BettyBridge::processBridgeQueue() {
    BettyBridgePacket pkt;
    if (xQueueReceive(xBridgeQueue, &pkt, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool ok = postToBetty(pkt);
        if (ok) {
            Serial.printf("[BettyBridge] >>> Dispatched to Betty PaaS: Sensor '%s' via %s [OK]\n",
                          pkt.sensorId, pkt.interface);
        }
    }
}

void Task_Betty_Bridge(void* pvParameters) {
    BettyBridge* bridge = static_cast<BettyBridge*>(pvParameters);
    for (;;) {
        bridge->processBridgeQueue();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void BettyBridge::setEnabled(bool en) {
    enabled = en;
    prefs.putBool("enabled", enabled);
}

bool BettyBridge::isEnabled() const {
    return enabled;
}

void BettyBridge::setHost(const String& h) {
    if (h.length() == 0) return;
    host = h;
    prefs.putString("host", host);
}

String BettyBridge::getHost() const {
    return host;
}

void BettyBridge::setPort(uint16_t p) {
    if (p == 0) return;
    port = p;
    prefs.putUShort("port", port);
}

uint16_t BettyBridge::getPort() const {
    return port;
}

void BettyBridge::setSensorId(const String& sid) {
    if (sid.length() == 0) return;
    sensorId = sid;
    prefs.putString("sensor_id", sensorId);
}

String BettyBridge::getSensorId() const {
    return sensorId;
}

void BettyBridge::setApiKey(const String& key) {
    if (key.length() == 0) return;
    apiKey = key;
    prefs.putString("api_key", apiKey);
}

String BettyBridge::getApiKey() const {
    return apiKey;
}

void BettyBridge::setStaWifi(const String& ssid, const String& pass) {
    staSsid = ssid;
    staPass = pass;
    prefs.putString("sta_ssid", staSsid);
    prefs.putString("sta_pass", staPass);
}

String BettyBridge::getStaSsid() const {
    return staSsid;
}

String BettyBridge::getStaPass() const {
    return staPass;
}

uint32_t BettyBridge::getTotalSent() const {
    return totalSent;
}

uint32_t BettyBridge::getTotalFailed() const {
    return totalFailed;
}

int BettyBridge::getLastHttpCode() const {
    return lastHttpCode;
}

unsigned long BettyBridge::getLastSentTime() const {
    return lastSentTime;
}

bool BettyBridge::isConnectedToBetty() const {
    return (lastHttpCode == 200 || lastHttpCode == 201);
}
