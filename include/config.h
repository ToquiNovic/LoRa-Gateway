#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ====================================================================
// GATEWAY HARDWARE CONFIGURATION - ESP32 DevKit V1 + SX1278 Ra-02
// ====================================================================

// LoRa SPI Pins (SX1278)
#define LORA_SCK         18
#define LORA_MISO        19
#define LORA_MOSI        23
#define LORA_SS          5
#define LORA_RST         14
#define LORA_DIO0        2

// LoRa 433 MHz Radio Configuration
#define LORA_BAND        433E6
#define LORA_SF          7
#define LORA_BW          125E3
#define LORA_CR          5
#define LORA_SYNC_WORD   0x12
#define LORA_MAX_PACKET  256

// LoRa-CAD-EDCA QoS Priority Classes
#define QOS_CLASS_EMERGENCY       0
#define QOS_CLASS_VOICE_EMERGENCY 0
#define QOS_CLASS_MEDIA           1
#define QOS_CLASS_IMAGE           1
#define QOS_CLASS_ROUTINE         2
#define QOS_CLASS_CHAT_ROUTINE    2

// Default Wi-Fi Access Point (Local management & inspector)
#define GATEWAY_AP_SSID      "Gateway-IoT-AP"
#define GATEWAY_AP_PASS      "12345678"

// Default Wi-Fi Station (Local facility network where Betty resides)
#define DEFAULT_STA_SSID     ""
#define DEFAULT_STA_PASS     ""

// Default Link-Level Pre-Shared Key (Wi-Fi style credentials for sensors)
#define DEFAULT_GATEWAY_PSK  "betty1234"

// Betty PaaS (Botuto) Bridge Defaults
#define BETTY_DEFAULT_HOST       "192.168.1.100"
#define BETTY_DEFAULT_HTTP_PORT  3000
#define BETTY_DEFAULT_SENSOR_ID  "gateway-esp32-01"
#define BETTY_DEFAULT_API_KEY    "betty_live_secret"

// Networking Ports
#define HTTP_PORT            80
#define WEBSOCKET_PORT       81
#define WEBSOCKET_HEARTBEAT  15000

// Inter-Task FreeRTOS Packet Definitions
struct LoRaTxPacket {
    uint8_t data[LORA_MAX_PACKET];
    size_t length;
    uint8_t qosClass;
};

struct LoRaRxPacket {
    uint8_t data[LORA_MAX_PACKET];
    size_t length;
    int rssi;
    float snr;
};

#endif // CONFIG_H
