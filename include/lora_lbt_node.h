#ifndef LORA_LBT_NODE_H
#define LORA_LBT_NODE_H

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

/**
 * ==============================================================================
 * BETTY IoT - LoRa Listen-Before-Talk (LBT) & Persistent CSMA/CAD Node Driver
 * ==============================================================================
 * Técnica de Acceso al Medio (MAC Layer) para Nodos Sensores y Enlaces LoRa.
 * 
 * Problema resuelto:
 * En transmisiones LoRa clásicas (Pure ALOHA), si dos o más nodos transmiten al 
 * mismo tiempo, sus paquetes colisionan en el aire y se destruyen mutuamente.
 * 
 * Principio de funcionamiento (LBT + CAD + Backoff Adaptativo):
 * 1. Antes de transmitir, el nodo activa el modo Channel Activity Detection (CAD)
 *    en el chip SX1278 para verificar si el canal de radio está en uso.
 * 2. Si el canal está ocupado: NO TRANSMITE ("no acepta el envío inmediato").
 *    Calcula un tiempo de retroceso aleatorio (Truncated Binary Exponential Backoff)
 *    y entra en espera activa/diferida.
 * 3. Repite el sensado hasta que el canal esté completamente libre por un tiempo
 *    de guarda (DIFS).
 * 4. Solo cuando el canal está disponible, abre la portadora y envía el paquete.
 * ==============================================================================
 */

class LoRaLbtNode {
public:
    // Registros internos del chip Semtech SX1276/7/8
    static const uint8_t REG_OP_MODE       = 0x01;
    static const uint8_t REG_IRQ_FLAGS     = 0x12;
    static const uint8_t MODE_STANDBY      = 0x01;
    static const uint8_t MODE_CAD          = 0x07;
    static const uint8_t IRQ_CAD_DONE_MASK = 0x04;
    static const uint8_t IRQ_CAD_DET_MASK  = 0x01;

    LoRaLbtNode(int csPin = 5, int resetPin = 14, int irqPin = 2)
        : _cs(csPin), _reset(resetPin), _irq(irqPin),
          _slotTimeMs(5), _cwMin(7), _cwMax(63), _difsMs(5) {}

    /**
     * Inicializa el módem LoRa con los parámetros estándar de Betty Gateway.
     */
    bool begin(long frequency = 433E6, int sf = 7, long bw = 125E3, int cr = 5, int syncWord = 0x12) {
        LoRa.setPins(_cs, _reset, _irq);
        if (!LoRa.begin(frequency)) {
            return false;
        }
        LoRa.setSpreadingFactor(sf);
        LoRa.setSignalBandwidth(bw);
        LoRa.setCodingRate4(cr);
        LoRa.setSyncWord(syncWord);
        return true;
    }

    /**
     * Sensa el espectro mediante Channel Activity Detection (CAD).
     * Retorna TRUE si el canal está LIBRE de preámbulos y símbolos LoRa.
     */
    bool isChannelFreeCAD() {
        // 1. Limpiar flags anteriores
        writeRegister(REG_IRQ_FLAGS, 0xFF);

        // 2. Activar modo CAD en el módem LoRa (Bit 7: LongRangeMode | MODE_CAD)
        writeRegister(REG_OP_MODE, 0x80 | MODE_CAD);

        // 3. Esperar finalización del barrido CAD (típicamente 1.28 ms a SF7/125kHz)
        unsigned long start = micros();
        while ((readRegister(REG_IRQ_FLAGS) & IRQ_CAD_DONE_MASK) == 0) {
            if (micros() - start > 5000) {
                // Timeout de seguridad: asumir canal ocupado para evitar colisiones
                writeRegister(REG_OP_MODE, 0x80 | MODE_STANDBY);
                return false;
            }
            delayMicroseconds(50);
        }

        uint8_t flags = readRegister(REG_IRQ_FLAGS);
        bool detected = (flags & IRQ_CAD_DET_MASK) != 0;

        // 4. Limpiar interrupciones y regresar a Standby
        writeRegister(REG_IRQ_FLAGS, 0xFF);
        writeRegister(REG_OP_MODE, 0x80 | MODE_STANDBY);

        return !detected; // Libre si NO hubo detección
    }

    /**
     * Retorna TRUE si el canal está ocupado por otra transmisión en el aire.
     */
    bool isChannelBusy() {
        return !isChannelFreeCAD();
    }

    /**
     * Envía una trama de texto estructurado o JSON aplicando Listen-Before-Talk persistente.
     * Si el canal está ocupado, espera y reintenta con backoff adaptativo hasta que se libere.
     */
    bool sendPacketLBT(const String& data, uint32_t timeoutMs = 15000) {
        return sendRawPacketLBT((const uint8_t*)data.c_str(), data.length(), timeoutMs);
    }

    /**
     * Envía un búfer binario crudo aplicando Listen-Before-Talk persistente.
     */
    bool sendRawPacketLBT(const uint8_t* buffer, size_t length, uint32_t timeoutMs = 15000) {
        if (!buffer || length == 0) return false;

        const uint32_t startTime = millis();
        uint8_t currentCw = _cwMin;

        while (millis() - startTime < timeoutMs) {
            // Paso 1: Sensar el canal
            if (isChannelFreeCAD()) {
                // Período de guarda DIFS para confirmar estabilidad del medio
                delay(_difsMs);
                if (isChannelFreeCAD()) {
                    // ¡Canal completamente disponible! Proceder con el envío
                    LoRa.beginPacket();
                    LoRa.write(buffer, length);
                    LoRa.endPacket();
                    return true;
                }
            }

            // Paso 2: El canal está ocupado.
            // NO se acepta el envío inmediato. Se calcula retroceso aleatorio (Backoff).
            uint32_t slots = random(1, currentCw + 1);
            uint32_t waitMs = slots * _slotTimeMs;

            delay(waitMs);

            // Incrementar ventana de contención para descongestionar el medio
            currentCw = min((uint8_t)(currentCw * 2 + 1), _cwMax);
        }

        // Si se excedió el timeout sin encontrar canal libre, rechazar para no colisionar
        return false;
    }

    // Configuración de temporización CSMA/CA
    void setSlotTimeMs(uint32_t slotMs) { _slotTimeMs = slotMs; }
    void setContentionWindow(uint8_t minCw, uint8_t maxCw) { _cwMin = minCw; _cwMax = maxCw; }
    void setDifsGuardMs(uint32_t difsMs) { _difsMs = difsMs; }

private:
    int _cs;
    int _reset;
    int _irq;

    uint32_t _slotTimeMs;
    uint8_t  _cwMin;
    uint8_t  _cwMax;
    uint32_t _difsMs;

    uint8_t readRegister(uint8_t address) {
        digitalWrite(_cs, LOW);
        SPI.transfer(address & 0x7F);
        uint8_t value = SPI.transfer(0x00);
        digitalWrite(_cs, HIGH);
        return value;
    }

    void writeRegister(uint8_t address, uint8_t value) {
        digitalWrite(_cs, LOW);
        SPI.transfer(address | 0x80);
        SPI.transfer(value);
        digitalWrite(_cs, HIGH);
    }
};

#endif // LORA_LBT_NODE_H
