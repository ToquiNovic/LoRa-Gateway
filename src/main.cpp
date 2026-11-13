/*
 * ====================================================================
 * BETTY IoT SENSOR GATEWAY (NODO BASE)
 * Hardware: ESP32 DevKit V1 + SX1278 (Ra-02) 433 MHz
 * Architecture: FreeRTOS SMP Dual-Core
 * Capabilities: Multi-Transport Authenticated Ingestion & Betty Uplink
 * ====================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_ota_ops.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "config.h"
#include "sensor_device.h"
#include "sensor_auth.h"
#include "sensor_registry.h"
#include "betty_bridge.h"
#include "sensor_manager.h"
#include "lora_manager.h"
#include "websocket_manager.h"
#include "http_server.h"

// ====================================================================
// GLOBAL SUBSYSTEM INSTANCES
// ====================================================================

SensorAuthGuard authGuard;
SensorRegistry registry;
BettyBridge bettyBridge;
SensorManager sensorManager(registry, authGuard, bettyBridge);
LoRaManager lora;
WebSocketManager wsManager;
HttpServer httpServer(registry, authGuard, bettyBridge, sensorManager);

// FreeRTOS Inter-Task Communication Queues
QueueHandle_t xLoRaTxQueue = NULL;
QueueHandle_t xLoRaRxQueue = NULL;

// Enqueue packet for LoRa radio transmission without blocking network tasks
void enqueueLoRaTx(const uint8_t* data, size_t length, uint8_t qosClass = QOS_CLASS_ROUTINE) {
    if (!xLoRaTxQueue || length == 0) return;
    LoRaTxPacket pkt;
    if (length > sizeof(pkt.data)) length = sizeof(pkt.data);
    memcpy(pkt.data, data, length);
    pkt.length = length;
    pkt.qosClass = qosClass;
    xQueueSend(xLoRaTxQueue, &pkt, 0);
}

// --------------------------------------------------------------------
// Core 1 Task: Real-Time LoRa Radio MAC (Priority 4)
// --------------------------------------------------------------------
void Task_LoRa_MAC(void* pvParameters) {
    LoRaTxPacket txPkt;
    LoRaRxPacket rxPkt;

    for (;;) {
        // 1. Service pending outgoing LoRa transmissions with persistent Listen-Before-Talk
        if (xQueueReceive(xLoRaTxQueue, &txPkt, 0) == pdTRUE) {
            lora.sendWithPersistentLbt(txPkt.data, txPkt.length, 10000);
        }

        // 2. Listen for incoming packets over the air
        int packetSize = lora.available();
        if (packetSize > 0) {
            rxPkt.length = lora.readRawPacket(rxPkt.data, sizeof(rxPkt.data), rxPkt.rssi, rxPkt.snr);
            if (rxPkt.length > 0) {
                xQueueSend(xLoRaRxQueue, &rxPkt, 0);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// --------------------------------------------------------------------
// Core 0 Task: Network, Ingestion & Web Services (Priority 2)
// --------------------------------------------------------------------
void Task_Network(void* pvParameters) {
    LoRaRxPacket rxPkt;
    unsigned long lastRegistryPrint = 0;
    unsigned long lastWifiCheck = 0;

    for (;;) {
        // 1. Service HTTP and WebSocket servers
        httpServer.handleClient();
        wsManager.loop();

        // 2. Process incoming LoRa packets from FreeRTOS queue
        if (xQueueReceive(xLoRaRxQueue, &rxPkt, 0) == pdTRUE) {
            sensorManager.ingestLoRaFrame(rxPkt.data, rxPkt.length, rxPkt.rssi, rxPkt.snr);
        }

        // 3. Ingest Serial stream (if external board or USB PC sends JSON lines)
        if (Serial.available()) {
            String serialLine = Serial.readStringUntil('\n');
            sensorManager.ingestSerialLine(serialLine);
        }

        // 5. Sample on-board physical sensors (I2C / ADC)
        sensorManager.samplePhysicalSensors();

        // 6. Periodic Station Wi-Fi reconnect monitor (ONLY when no user is connected to SoftAP!)
        if (millis() - lastWifiCheck >= 30000) {
            lastWifiCheck = millis();
            String staSsid = bettyBridge.getStaSsid();
            uint8_t connectedStations = WiFi.softAPgetStationNum();

            // If a client (phone or PC) is connected to the SoftAP, NEVER interrupt with channel hopping!
            if (connectedStations > 0) {
                // Keep radio pinned to Channel 1 for stable captive portal / inspector operation
            } else if (staSsid.length() > 0 && WiFi.status() != WL_CONNECTED) {
                Serial.printf("[Wi-Fi STA] Background reconnect attempt to '%s' (Timeout 6s)...\n", staSsid.c_str());
                WiFi.begin(staSsid.c_str(), bettyBridge.getStaPass().c_str());

                unsigned long startTry = millis();
                while (WiFi.status() != WL_CONNECTED && millis() - startTry < 6000) {
                    vTaskDelay(pdMS_TO_TICKS(100));
                }

                if (WiFi.status() != WL_CONNECTED) {
                    Serial.println("[Wi-Fi STA] Reconnect timed out -> Resetting to IDLE to protect SoftAP");
                    WiFi.disconnect(false, false);
                } else {
                    Serial.printf("[Wi-Fi STA] Connected to facility network! IP: %s\n", WiFi.localIP().toString().c_str());
                }
            }
        }

        // 7. Periodic Sensor Registry summary logging
        if (millis() - lastRegistryPrint >= 30000) {
            lastRegistryPrint = millis();
            registry.printRegistry();
        }

        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

// ====================================================================
// ARDUINO SETUP
// ====================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");
    Serial.println("   BETTY IoT SENSOR GATEWAY (NODO BASE)           ");
    Serial.println("   FreeRTOS SMP Dual-Core + Authenticated Bridge  ");
    Serial.println("==================================================");

    // Initialize Security & Ingestion Subsystems
    authGuard.begin();
    bettyBridge.begin();
    sensorManager.begin();
    wsManager.setSensorManager(&sensorManager);

    // Wi-Fi Setup: Start in dual AP+STA mode so AP serves the portal while STA scans and connects
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false); // Disable modem sleep to prevent SoftAP beacon dropping
    String staSsid = bettyBridge.getStaSsid();

    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    bool apOk = WiFi.softAP(GATEWAY_AP_SSID, GATEWAY_AP_PASS, 1, 0, 4);
    IPAddress apIP = WiFi.softAPIP();

    Serial.printf("[Wi-Fi AP] Started '%s' on Channel 1 (%s) | Gateway IP: http://%s\n",
                  GATEWAY_AP_SSID, apOk ? "OK" : "FAILED", apIP.toString().c_str());

    if (staSsid.length() > 0) {
        Serial.printf("[Wi-Fi STA] Attempting initial connection to '%s' (Timeout 8s)...\n", staSsid.c_str());
        WiFi.begin(staSsid.c_str(), bettyBridge.getStaPass().c_str());

        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < 8000)) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[Wi-Fi STA] Connected to '%s'! IP: %s\n", staSsid.c_str(), WiFi.localIP().toString().c_str());
        } else {
            Serial.printf("[Wi-Fi STA] Could not connect to '%s' -> Aborting STA to stabilize SoftAP on Channel 1 [OK]\n", staSsid.c_str());
            WiFi.disconnect(false, false);
        }
    } else {
        Serial.println("[Wi-Fi STA] No station credentials configured yet (Running in pure AP mode)");
    }

    // Start Web & WebSocket Servers
    httpServer.begin();
    wsManager.begin();

    // Start LoRa Radio (SX1278 433 MHz)
    if (!lora.begin()) {
        Serial.println("[LoRa] FATAL ERROR: SX1278 transceiver not found or failed to initialize!");
        while (1) { delay(1000); }
    }
    Serial.println("[LoRa] SX1278 433 MHz transceiver initialized [OK]");

    // Create FreeRTOS Inter-Task Queues
    xLoRaTxQueue = xQueueCreate(16, sizeof(LoRaTxPacket));
    xLoRaRxQueue = xQueueCreate(16, sizeof(LoRaRxPacket));

    // Spawn Core 0 Task: Network, Ingestion & Web Services
    xTaskCreatePinnedToCore(
        Task_Network,
        "Task_Network",
        8192,
        NULL,
        2,
        NULL,
        0
    );

    // Spawn Core 1 Task: Real-Time LoRa MAC & SPI
    xTaskCreatePinnedToCore(
        Task_LoRa_MAC,
        "Task_LoRa_MAC",
        4096,
        NULL,
        4, // High Priority Real-Time
        NULL,
        1
    );

    // OTA Boot Verification & Anti-Brick Rollback Guard
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    if (running && esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            Serial.printf("[OTA] New firmware boot verified on '%s' -> Cancelling rollback [OK]\n", running->label);
            esp_ota_mark_app_valid_cancel_rollback();
        } else {
            Serial.printf("[OTA] Running stable partition: '%s' (Address: 0x%06X)\n", running->label, running->address);
        }
    }

    Serial.println("\nGateway is fully online and ready.");
    Serial.printf(">>> Open Sensor Inspector at: http://%s/sensors\n", apIP.toString().c_str());
    Serial.println("--------------------------------------------------\n");
}

void loop() {
    // Main loop yields; FreeRTOS handles dedicated tasks on Core 0 & Core 1
    vTaskDelay(pdMS_TO_TICKS(1000));
}
