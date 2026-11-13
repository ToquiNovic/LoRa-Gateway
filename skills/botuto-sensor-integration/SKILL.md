---
name: botuto-sensor-integration
description: Use when connecting new hardware sensors (ESP32, Heltec, ESP8266, Arduino, Raspberry Pi) or software clients (Python, WebSockets, REST) to the Botuto IoT Gateway and Botuto Cloud Platform. Provides data contracts, packet formatting, authentication protocols (PSK), LoRa 433MHz parameters, and code templates for TimescaleDB ingestion.
---

# 🌐 Skill: Integración de Sensores con Botuto IoT Gateway y Botuto Cloud

Esta habilidad guía a desarrolladores y agentes de inteligencia artificial en el proceso completo para conectar cualquier sensor físico o aplicación de software al **Gateway IoT Base** y enviar datos de forma transparente a la plataforma **Botuto Cloud** (TimescaleDB, WebSockets y Gemelos Digitales).

---

## 🏛️ 1. Contrato del Sistema y Flujo de Datos

El Gateway actúa como un **puente transparente y seguro**:
1. Recibe la telemetría del sensor a través de **LoRa (433 MHz)**, **Wi-Fi HTTP REST**, **WebSockets** o **Puerto Serie USB**.
2. Valida la clave de enlace del sensor (**Link Master Key / PSK**) mediante su motor `SensorAuthGuard`. Si la clave es inválida, el paquete se descarta de inmediato para proteger la red.
3. Despacha automáticamente la telemetría mediante una cola asíncrona FreeRTOS y una petición segura **HTTPS POST (TLS 1.3 en puerto 443)** hacia la API de Botuto (`https://betty-api.laboratorio3d.online/api/mqtt/webhook`).
4. La API de Botuto verifica que el `sensorId` exista como llave foránea en PostgreSQL y almacena el objeto de métricas en la base de datos temporal **TimescaleDB**, emitiendo la actualización en vivo al frontend web.

```
[ Tu Sensor / Nodo ] 
       │ (LoRa 433MHz / HTTP / WebSockets / Serie)
       ▼
[ Gateway IoT Base (ESP32) ] ── (Valida PSK: betty1234)
       │ (HTTPS POST TLS :443)
       ▼
[ Botuto Cloud API ] ── (Valida UUID en tabla 'sensors')
       ├─► [ TimescaleDB (sensor_data) ]
       └─► [ WebSockets Broker ] ──► [ Dashboard Web / Gemelo Digital ]
```

---

## 🔑 2. Regla de Oro: El UUID del Sensor

> [!IMPORTANT]
> **El `sensorId` DEBE ser un UUID válido registrado en Botuto**.
> Botuto API rechaza con error `404 Not Found` cualquier lectura cuyo `sensorId` no esté previamente creado en la plataforma.

### Cómo obtener el UUID:
1. Accede a [botuto.laboratorio3d.online](https://botuto.laboratorio3d.online).
2. Ve a la sección **Sensores** -> **+ Registrar Nuevo Sensor**.
3. Asigna un nombre al sensor (ej. `BME280-Laboratorio`) y haz clic en **Crear**.
4. Copia el **UUID** que aparece en el modal del Topic MQTT:
   `botuto/sensor/xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx/data`
   *(Usa este UUID exacto en la variable `sensorId` de tu sensor).*

---

## 📡 3. Guías de Implementación por Transporte

### Transporte 1: Radio LoRa 433 MHz (Para nodos de campo remotos sin internet)

#### Parámetros de Modulación LoRa Obligatorios:
* **Frecuencia:** `433.0 MHz` (`433E6`)
* **Spreading Factor (SF):** `7`
* **Ancho de Banda (BW):** `125 kHz` (`125E3`)
* **Coding Rate (CR):** `4/5` (valor `5`)
* **Sync Word:** `0x12` *(Palabra de sincronización privada del Gateway)*
* **Potencia de transmisión recomendada:** `17 dBm`
* **CRC por Hardware:** Habilitado

#### Formato del Paquete JSON (Recomendado):
```json
{
  "sensorId": "55f1a34e-a2ab-4ece-ade4-6d4c5808c08f",
  "key": "betty1234",
  "model": "Heltec-V2",
  "metrics": {
    "temperatura": 22.4,
    "humedad": 58.1,
    "bateria_volt": 3.95
  }
}
```

#### Formato Texto Ligero `$DATA` (Ultra bajo consumo / tramas mínimas):
```text
$DATA,ID=55f1a34e-a2ab-4ece-ade4-6d4c5808c08f,KEY=betty1234,TEMP=22.4,HUM=58.1,BAT=3.95*
```

#### Plantilla C++ para Arduino / ESP32 / Heltec LoRa:
```cpp
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

// Definición de pines SPI según tu placa (ejemplo para Heltec V2)
#define LORA_SCK   5
#define LORA_MISO  19
#define LORA_MOSI  27
#define LORA_SS    18
#define LORA_RST   14
#define LORA_DIO0  26

const char* SENSOR_UUID = "PEGA_AQUI_EL_UUID_DE_BOTUTO";
const char* GATEWAY_PSK = "betty1234";

void setup() {
    Serial.begin(115200);
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
    LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(433E6)) {
        Serial.println("[ERROR] Radio LoRa no detectado");
        while (1);
    }

    LoRa.setSpreadingFactor(7);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(5);
    LoRa.setSyncWord(0x12);
    LoRa.setTxPower(17);
    LoRa.enableCrc();
}

void loop() {
    // 1. Leer sensores físicos aquí
    float temp = 24.5;
    float hum = 60.2;

    // 2. Construir payload JSON
    JsonDocument doc;
    doc["sensorId"] = SENSOR_UUID;
    doc["key"]      = GATEWAY_PSK;
    doc["model"]    = "MiSensor-ESP32";
    
    JsonObject metrics = doc["metrics"].to<JsonObject>();
    metrics["temperatura"] = temp;
    metrics["humedad"]     = hum;
    metrics["uptime_s"]    = millis() / 1000;

    String payload;
    serializeJson(doc, payload);

    // 3. Transmitir por LoRa
    LoRa.beginPacket();
    LoRa.print(payload);
    LoRa.endPacket();

    delay(10000); // Enviar cada 10 segundos
}
```

---

### Transporte 2: Wi-Fi HTTP REST (ESP8266 / ESP32 / Raspberry Pi / PC)

Cualquier dispositivo conectado a la red local del Gateway (`Gateway-IoT-AP` o en la misma LAN):

* **Endpoint:** `http://192.168.4.1/api/sensor/data` *(o la IP local del Gateway)*
* **Método:** `POST`
* **Headers:**
  * `Content-Type: application/json`
  * `X-Sensor-Key: betty1234`
* **Body:**
  ```json
  {
    "sensorId": "55f1a34e-a2ab-4ece-ade4-6d4c5808c08f",
    "model": "ESP8266-DHT",
    "metrics": {
      "temperatura": 23.8,
      "humedad": 62.4
    }
  }
  ```

#### Ejemplo en Python:
```python
import requests

GATEWAY_URL = "http://192.168.4.1/api/sensor/data"
SENSOR_UUID = "55f1a34e-a2ab-4ece-ade4-6d4c5808c08f"
GATEWAY_PSK = "betty1234"

payload = {
    "sensorId": SENSOR_UUID,
    "model": "Python-Agent",
    "metrics": {
        "cpu_usage": 15.2,
        "ram_free_mb": 4096,
        "estado": "ACTIVO"
    }
}

headers = {
    "Content-Type": "application/json",
    "X-Sensor-Key": GATEWAY_PSK
}

response = requests.post(GATEWAY_URL, json=payload, headers=headers)
print("Respuesta:", response.status_code, response.json())
```

#### Ejemplo con cURL (Línea de Comandos):
```bash
curl -X POST http://192.168.4.1/api/sensor/data \
  -H "Content-Type: application/json" \
  -H "X-Sensor-Key: betty1234" \
  -d '{"sensorId":"55f1a34e-a2ab-4ece-ade4-6d4c5808c08f","metrics":{"temp":24.1,"hum":59.3}}'
```

---

### Transporte 3: WebSockets (Streaming de alta frecuencia y tiempo real)

El Gateway cuenta con un servidor WebSockets en el puerto `81`: `ws://192.168.4.1:81`.

#### Paso 1: Autenticación inicial
Al abrir la conexión, el cliente debe enviar un paquete de autenticación:
```json
{
  "type": "auth",
  "id": "55f1a34e-a2ab-4ece-ade4-6d4c5808c08f",
  "key": "betty1234"
}
```
El Gateway responderá:
```json
{ "type": "auth_ack", "status": "AUTHORIZED" }
```

#### Paso 2: Transmisión continua de telemetría
```json
{
  "type": "sensor_data",
  "sensorId": "55f1a34e-a2ab-4ece-ade4-6d4c5808c08f",
  "key": "betty1234",
  "metrics": {
    "rpm": 1450,
    "vibracion_g": 0.12,
    "temperatura": 42.1
  }
}
```

---

### Transporte 4: Puerto Serie USB (115200 bps)

Para microcontroladores o computadoras conectadas por cable USB al Gateway:
* Velocidad: `115200 baudios`
* Formato: Una línea JSON terminada en salto de línea `\n`:
```json
{"id":"55f1a34e-a2ab-4ece-ade4-6d4c5808c08f","key":"betty1234","metrics":{"adc_raw":512,"luz_lux":340}}
```

---

## 🔍 4. Protocolo de Diagnóstico y Verificación

Cuando un nuevo sensor empiece a transmitir:

1. **Monitoreo Local en el Inspector Web del Gateway:**
   * Conéctate a la red Wi-Fi `Gateway-IoT-AP`.
   * Abre `http://192.168.4.1/sensors`.
   * Tu sensor debe aparecer en la lista **Live Connected Sensors** con su interfaz de origen (`lora`, `wifi_http`, `ws`, `serial`), su RSSI/SNR y los valores de métricas recibidos.

2. **Monitoreo en el Puerto Serial del Gateway:**
   * En la consola del Gateway verás:
     ```text
     [SensorManager] Ingested [lora] '55f1a34e-...' (Heltec-V2) | Metrics: {"temp":24.5,...}
     [BettyBridge] Telemetry dispatched successfully to https://betty-api.laboratorio3d.online/api/mqtt/webhook [HTTP 200]
     ```

3. **Verificación en Botuto Cloud:**
   * Entra a `https://botuto.laboratorio3d.online/sensors/<UUID>`.
   * El estado del sensor debe indicar **Activo** y los datos deben aparecer en la gráfica en vivo y en la tabla histórica de TimescaleDB.

---

## ⚠️ 5. Errores Comunes y Cómo Evitarlos

| Problema | Causa Raíz | Solución Inmediata |
| :--- | :--- | :--- |
| **`HTTP 401 Unauthorized`** o paquete ignorado | La clave `key` o header `X-Sensor-Key` no coincide con la configurada en el Gateway. | Usa `betty1234` (o la clave que hayas configurado en el Portal Cautivo). |
| **`HTTP 404: Sensor not found` en Botuto** | El UUID no existe en la base de datos de Botuto. | Crea primero el sensor en el panel web y copia el UUID exacto sin espacios. |
| **`Uplink error: HTTP -1` en el Gateway** | El Gateway no está conectado a una red Wi-Fi con salida a Internet. | Conéctate a `Gateway-IoT-AP`, abre `http://192.168.4.1`, y configura tu red Wi-Fi en **Gateway Settings**. |
| **No se reciben paquetes LoRa** | Parámetros de radio desincronizados. | Comprueba que la frecuencia sea `433E6`, `SF7`, `BW 125kHz`, `CR 4/5` y `Sync Word 0x12`. |
