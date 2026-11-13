#ifndef LORA_MANAGER_H
#define LORA_MANAGER_H

#include <Arduino.h>
#include <LoRa.h>
#include "config.h"

// ====================================================================
// GESTOR LoRa - Manejo de comunicación LoRa 433MHz
// ====================================================================

class LoRaManager {
public:
    // Inicializar el módulo LoRa
    bool begin(int sck = LORA_SCK, int miso = LORA_MISO, int mosi = LORA_MOSI,
               int ss = LORA_SS, int rst = LORA_RST, int dio0 = LORA_DIO0);
    
    // Verificar si hay paquetes disponibles
    int available();
    
    // Leer un paquete LoRa
    String readPacket(int& rssi, float& snr);
    
    // Leer un paquete LoRa como bytes crudos (necesario para tramas binarias)
    int readRawPacket(uint8_t* buffer, size_t maxLength, int& rssi, float& snr);
    
    // Enviar datos por LoRa (texto simple)
    void sendPacket(const String& data);
    
    // Enviar trama binaria por LoRa
    void sendRawPacket(const uint8_t* buffer, size_t length, bool bypassInterval = false);

    // Sensado físico de portadora mediante Channel Activity Detection (CAD) en SX1278
    bool isChannelFreeCAD();
    bool isChannelBusy();

    // Transmisión asistida por CSMA/CA con CAD y diferenciación de prioridad EDCA
    bool sendWithCadCsma(const uint8_t* buffer, size_t length, uint8_t qosClass = QOS_CLASS_CHAT_ROUTINE);

    // Transmisión persistente Listen-Before-Talk (LBT): espera a canal libre con backoff sin forzar colisión
    bool sendWithPersistentLbt(const uint8_t* buffer, size_t length, uint32_t timeoutMs = 15000);
    
    // Configurar pines
    void setPins();
    
    // Configurar parámetros de radio
    void configureRadio();

private:
    uint8_t readRegister(uint8_t address);
    void writeRegister(uint8_t address, uint8_t value);

    int _sck = LORA_SCK;
    int _miso = LORA_MISO;
    int _mosi = LORA_MOSI;
    int _ss = LORA_SS;
    int _rst = LORA_RST;
    int _dio0 = LORA_DIO0;

    unsigned long lastTxTime = 0;
};

#endif
