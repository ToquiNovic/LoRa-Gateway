#include "lora_manager.h"
#include <SPI.h>

// ====================================================================
// IMPLEMENTACIÓN - GESTOR LoRa
// ====================================================================

#define REG_OP_MODE       0x01
#define REG_IRQ_FLAGS     0x12
#define MODE_STANDBY      0x01
#define MODE_CAD          0x07
#define IRQ_CAD_DONE_MASK 0x04
#define IRQ_CAD_DET_MASK  0x01

bool LoRaManager::begin(int sck, int miso, int mosi, int ss, int rst, int dio0) {
    _sck = sck;
    _miso = miso;
    _mosi = mosi;
    _ss = ss;
    _rst = rst;
    _dio0 = dio0;

    setPins();
    configureRadio();
    
    Serial.print("Iniciando módem LoRa SX1278 en 433 MHz con protocolo LoRa-CAD-EDCA...");
    if (!LoRa.begin(LORA_BAND)) {
        Serial.println(" [ERROR]");
        Serial.println("¡No se pudo iniciar el chip LoRa!");
        return false;
    }
    
    Serial.println(" [OK]");
    return true;
}

void LoRaManager::setPins() {
    SPI.begin(_sck, _miso, _mosi, _ss);
    LoRa.setPins(_ss, _rst, _dio0);
}

void LoRaManager::configureRadio() {
    LoRa.setSpreadingFactor(LORA_SF);
    LoRa.setSignalBandwidth(LORA_BW);
    LoRa.setCodingRate4(LORA_CR);
    LoRa.setSyncWord(LORA_SYNC_WORD);
}

uint8_t LoRaManager::readRegister(uint8_t address) {
    digitalWrite(_ss, LOW);
    SPI.transfer(address & 0x7F);
    uint8_t value = SPI.transfer(0x00);
    digitalWrite(_ss, HIGH);
    return value;
}

void LoRaManager::writeRegister(uint8_t address, uint8_t value) {
    digitalWrite(_ss, LOW);
    SPI.transfer(address | 0x80);
    SPI.transfer(value);
    digitalWrite(_ss, HIGH);
}

bool LoRaManager::isChannelFreeCAD() {
    // 1. Limpiar flags anteriores escribiendo 1
    writeRegister(REG_IRQ_FLAGS, 0xFF);

    // 2. Activar modo CAD (LongRangeMode | MODE_CAD)
    writeRegister(REG_OP_MODE, 0x80 | MODE_CAD);

    // 3. Esperar finalización del escaneo CAD (típicamente 1.28 ms a SF7/125kHz)
    unsigned long start = micros();
    while ((readRegister(REG_IRQ_FLAGS) & IRQ_CAD_DONE_MASK) == 0) {
        if (micros() - start > 5000) {
            writeRegister(REG_OP_MODE, 0x80 | MODE_STANDBY);
            return false; // Timeout de seguridad: asumir canal ocupado
        }
        delayMicroseconds(50);
    }

    uint8_t flags = readRegister(REG_IRQ_FLAGS);
    bool detected = (flags & IRQ_CAD_DET_MASK) != 0;

    // 4. Limpiar flags y regresar a Standby
    writeRegister(REG_IRQ_FLAGS, 0xFF);
    writeRegister(REG_OP_MODE, 0x80 | MODE_STANDBY);

    return !detected; // Libre si NO fue detectado
}

bool LoRaManager::isChannelBusy() {
    return !isChannelFreeCAD();
}

bool LoRaManager::sendWithPersistentLbt(const uint8_t* buffer, size_t length, uint32_t timeoutMs) {
    if (!buffer || length == 0) return false;

    const uint32_t startTime = millis();
    uint8_t cw = 7;                // Contention window inicial (ranuras)
    const uint8_t cwMin = 7;
    const uint8_t cwMax = 63;
    const uint32_t slotTimeMs = 4; // Ranura base de tiempo para backoff

    Serial.printf("[LBT-MAC] Solicitud de TX (%u bytes). Sensando canal...\n", (unsigned)length);

    while (millis() - startTime < timeoutMs) {
        // 1. Sensar canal usando CAD en hardware
        if (isChannelFreeCAD()) {
            // Período de guarda (DIFS) de 5 ms para confirmar que no emergió otra transmisión
            delay(5);
            if (isChannelFreeCAD()) {
                // Canal libre y estable: transmitir
                LoRa.beginPacket();
                LoRa.write(buffer, length);
                LoRa.endPacket();
                lastTxTime = millis();
                Serial.printf("[LBT-MAC] Canal libre verificado. Transmitido con exito tras %lu ms.\n",
                              millis() - startTime);
                return true;
            }
        }

        // 2. Canal ocupado: no aceptar envío inmediato y diferir con backoff aleatorio
        uint32_t backoffSlots = random(1, cw + 1);
        uint32_t waitMs = backoffSlots * slotTimeMs;

        Serial.printf("[LBT-MAC] Canal ocupado. Transmision en espera: retroceso de %u ms (CW=%u)...\n",
                      waitMs, cw);
        delay(waitMs);

        // Incrementar ventana de contención exponencialmente
        cw = min((uint8_t)(cw * 2 + 1), cwMax);
    }

    Serial.printf("[LBT-MAC] Timeout (%u ms) excedido sin canal libre. Transmision cancelada para proteger el espectro.\n",
                  timeoutMs);
    return false;
}

bool LoRaManager::sendWithCadCsma(const uint8_t* buffer, size_t length, uint8_t qosClass) {
    uint8_t cwMin = (qosClass == QOS_CLASS_VOICE_EMERGENCY) ? 3 : ((qosClass == QOS_CLASS_IMAGE) ? 7 : 15);
    uint8_t cwMax = (qosClass == QOS_CLASS_VOICE_EMERGENCY) ? 7 : ((qosClass == QOS_CLASS_IMAGE) ? 15 : 31);
    uint32_t aifsUs = (qosClass == QOS_CLASS_VOICE_EMERGENCY) ? 4000 : ((qosClass == QOS_CLASS_IMAGE) ? 8000 : 16000);
    uint8_t maxRetries = (qosClass == QOS_CLASS_VOICE_EMERGENCY) ? 4 : ((qosClass == QOS_CLASS_IMAGE) ? 3 : 2);

    uint8_t currentCw = cwMin;

    for (uint8_t retry = 0; retry <= maxRetries; retry++) {
        delayMicroseconds(aifsUs);

        if (isChannelFreeCAD()) {
            LoRa.beginPacket();
            LoRa.write(buffer, length);
            LoRa.endPacket();
            lastTxTime = millis();

            // Espaciado intra-ráfaga opcional si se requiere
            if (length > 0 && qosClass < QOS_CLASS_ROUTINE) {
                delay(random(25, 75));
            }
            return true;
        }

        // Canal ocupado: backoff exponencial en ranuras de 2 ms
        uint32_t slots = random(0, currentCw + 1);
        delay(slots * 2);
        currentCw = min((uint8_t)(currentCw * 2 + 1), cwMax);
    }

    // Forzar envío si es tráfico de emergencia crítico
    if (qosClass == QOS_CLASS_VOICE_EMERGENCY) {
        LoRa.beginPacket();
        LoRa.write(buffer, length);
        LoRa.endPacket();
        lastTxTime = millis();
        return true;
    }

    Serial.println("[MAC LoRa] Paquete descartado por canal ocupado tras reintentos CAD");
    return false;
}

int LoRaManager::available() {
    return LoRa.parsePacket();
}

String LoRaManager::readPacket(int& rssi, float& snr) {
    String data = "";
    while (LoRa.available()) {
        data += (char)LoRa.read();
    }
    rssi = LoRa.packetRssi();
    snr = LoRa.packetSnr();
    return data;
}

int LoRaManager::readRawPacket(uint8_t* buffer, size_t maxLength, int& rssi, float& snr) {
    size_t len = 0;
    while (LoRa.available() && len < maxLength) {
        buffer[len++] = (uint8_t)LoRa.read();
    }
    rssi = LoRa.packetRssi();
    snr = LoRa.packetSnr();
    return (int)len;
}

void LoRaManager::sendPacket(const String& data) {
    sendWithCadCsma((const uint8_t*)data.c_str(), data.length(), QOS_CLASS_CHAT_ROUTINE);
    Serial.printf("[TX LoRa CAD] %s\n", data.c_str());
}

void LoRaManager::sendRawPacket(const uint8_t* buffer, size_t length, bool bypassInterval) {
    uint8_t qos = QOS_CLASS_ROUTINE;
    sendWithCadCsma(buffer, length, qos);
    Serial.printf("[TX LoRa RAW CAD] %u B (Header 0x%02X, QoS=%u)\n", (unsigned)length, length > 0 ? buffer[0] : 0, qos);
}
