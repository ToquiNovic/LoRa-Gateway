#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

// ====================================================================
// HELTEC WIFI LORA 32 (V2) - CONFIGURACION DE PINES
// ====================================================================
#define LED_PIN         25   // LED blanco de usuario en Heltec V2 (Activo en HIGH)
#define VEXT_PIN        21   // Control de alimentación de periféricos (LOW = encendido)

// Pines de comunicación SPI con el transceptor LoRa SX1276/SX1278
#define LORA_SCK        5
#define LORA_MISO       19
#define LORA_MOSI       27
#define LORA_SS         18
#define LORA_RST        14
#define LORA_DIO0       26

// ====================================================================
// PARAMETROS DE RADIO LORA (Deben coincidir exactamente con el Gateway)
// ====================================================================
// NOTA: Si tu Heltec es versión 433 MHz, usa 433E6. Si es 868/915 MHz, cambia según tu región.
#define LORA_BAND       433E6   
#define LORA_SF         7
#define LORA_BW         125E3
#define LORA_CR         5       // 4/5
#define LORA_SYNC_WORD  0x12    // Sync Word privado del Gateway

// Clave de autenticación con el Gateway (Link Master Key)
#define GATEWAY_PSK     "betty1234"

// ====================================================================
// IDENTIFICADOR DEL SENSOR EN BETTY / BOTUTO
// ====================================================================
// UUID asignado en Botuto para Heltec-LED
const char* SENSOR_UUID = "55f1a34e-a2ab-4ece-ade4-6d4c5808c08f";

// ====================================================================
// VARIABLES DE ESTADO
// ====================================================================
bool ledState = false;
unsigned long lastTransmission = 0;
const unsigned long INTERVALO_MS = 5000; // Transmitir y alternar LED cada 5 segundos
uint32_t packetCounter = 0;

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n==================================================");
    Serial.println("  HELTEC LORA V2 -> BETTY SENSOR NODE (LED DEMO)  ");
    Serial.println("==================================================");

    // 1. Configurar pines del LED y alimentación externa
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Habilitar alimentación externa en Heltec V2 (Vext activo en bajo)
    pinMode(VEXT_PIN, OUTPUT);
    digitalWrite(VEXT_PIN, LOW);
    delay(50);

    // 2. Inicializar bus SPI para LoRa
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    Serial.printf("[LoRa] Iniciando radio en %.1f MHz...\n", LORA_BAND / 1E6);
    if (!LoRa.begin(LORA_BAND)) {
        Serial.println("[ERROR] No se pudo inicializar el radio LoRa. Revisa la frecuencia y conexiones.");
        // Si falla, hacer parpadear el LED rápido como advertencia visual
        while (true) {
            digitalWrite(LED_PIN, HIGH);
            delay(150);
            digitalWrite(LED_PIN, LOW);
            delay(150);
        }
    }

    // 3. Aplicar configuración idéntica a la del Gateway
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setTxPower(17); // 17 dBm de potencia de salida
    LoRa.enableCrc();

    Serial.println("[LoRa] Transceptor listo y configurado.");
    Serial.printf("[Config] Sensor UUID: %s\n", SENSOR_UUID);
    Serial.printf("[Config] Clave Gateway: %s\n", GATEWAY_PSK);
    Serial.println("--------------------------------------------------");
}

void loop() {
    unsigned long currentMillis = millis();

    if (currentMillis - lastTransmission >= INTERVALO_MS) {
        lastTransmission = currentMillis;

        // 1. Alternar el estado del LED (Prender / Apagar)
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState ? HIGH : LOW);
        packetCounter++;

        Serial.printf("\n[Nodo] LED cambiado a: %s (Paquete #%u)\n", 
                      ledState ? "ENCENDIDO [HIGH]" : "APAGADO [LOW]", 
                      packetCounter);

        // 2. Construir el paquete JSON para el Gateway de Betty
        JsonDocument doc;
        doc["sensorId"] = SENSOR_UUID;
        doc["key"]      = GATEWAY_PSK;
        doc["model"]    = "Heltec-V2";

        // Métricas que Betty registrará en su base de datos TimescaleDB
        JsonObject metrics = doc["metrics"].to<JsonObject>();
        metrics["led"]         = ledState ? 1 : 0;
        metrics["state"]       = ledState ? "ON" : "OFF";
        metrics["counter"]     = packetCounter;
        metrics["uptime_sec"]  = currentMillis / 1000;

        String payload;
        serializeJson(doc, payload);

        // 3. Transmitir el paquete vía LoRa
        Serial.printf("[LoRa TX] Enviando: %s\n", payload.c_str());
        
        LoRa.beginPacket();
        LoRa.print(payload);
        LoRa.endPacket();

        Serial.println("[LoRa TX] Paquete enviado con exito.");
    }
}
