# 🌐 Betty IoT Sensor Gateway (ESP32 + LoRa 433MHz)
## Guía de Conexión, Credenciales y Manual de Ingesta de Sensores

Este módulo actúa como **Puente de Enlace (Edge Gateway)** universal. Recibe telemetría de cualquier sensor (físico o remoto) mediante múltiples interfaces, valida sus **credenciales a nivel de enlace** y retransmite los datos automáticamente a la plataforma **Betty (Botuto)** para guardarlos en TimescaleDB y proyectarlos en tiempo real a los dashboards y metaversos.

---

## 🔑 1. Credenciales y Parámetros por Defecto

| Parámetro | Valor por Defecto | Descripción |
|---|---|---|
| **Red Wi-Fi AP (SSID)** | `Gateway-IoT-AP` | Red Wi-Fi local emitida por el ESP32 para configuración y monitoreo. |
| **Contraseña Wi-Fi AP** | `12345678` | Clave WPA2 del punto de acceso del gateway. |
| **IP del Gateway (AP)** | `192.168.4.1` | Dirección IP local del servidor web y WebSocket. |
| **Clave Maestra de Sensores (PSK)** | `betty1234` | **Credencial obligatoria** que cada sensor debe incluir para ser aceptado. |
| **Puerto Web / REST** | `80` | Servidor HTTP del inspector y endpoint REST de ingesta. |
| **Puerto WebSockets** | `81` | Servidor WebSocket para dashboard en vivo e ingesta rápida. |
| **Frecuencia LoRa** | `433.0 MHz` | Frecuencia de radio SX1278 (SF7, BW 125 kHz, CR 4/5, Sync 0x12). |
| **Nombre BLE** | `Gateway_IoT_Base` | Nombre publicitado por Bluetooth Low Energy. |
| **Uplink Betty PaaS** | `http://192.168.1.100:3000` | Endpoint destino (`/api/mqtt/webhook`) configurable en la web. |

> [!NOTE]
> Puedes cambiar la **Clave Maestra (PSK)**, las credenciales de tu Wi-Fi de laboratorio/hogar y la IP del servidor de Betty en cualquier momento ingresando a la interfaz de administración en la pestaña **⚙️ Gateway Settings**. Se guardan de forma permanente en la memoria NVS del ESP32.

---

## 🛠️ 2. Panel de Inspección y Herramientas de Monitoreo

### A. Portal Cautivo e Inspector Web en Vivo
Al conectarte a la red Wi-Fi `Gateway-IoT-AP` (contraseña `12345678`), el dispositivo móvil o computadora detectará automáticamente la red y abrirá la ventana del **Portal Cautivo** sin necesidad de escribir la dirección manual.

Si deseas acceder manualmente desde cualquier navegador:
👉 **`http://192.168.4.1`** (o `http://192.168.4.1/sensors`)

* **Servidor DNS Cautivo (Puerto 53 UDP)**: Intercepta cualquier consulta de dominio y redirige hacia el panel de administración.
* **Sondas de Detección**: Intercepta automáticamente las comprobaciones de conectividad de Android (`/generate_204`), Apple iOS/macOS (`/hotspot-detect.html`) y Windows (`/ncsi.txt`).

Allí verás en tiempo real:
* Lista de sensores activos, su identificador y modelo.
* Interfaz por la que se conectaron (`lora`, `wifi_http`, `ws`, `ble`, `adc`, `serial`).
* Intensidad de señal (RSSI en dBm y SNR).
* Frecuencia de paquetes y tiempo transcurrido desde la última lectura.
* Muestra de las últimas métricas recibidas (`temperatura`, `humedad`, `voltaje`, etc.).
* Contador de paquetes descartados por credenciales inválidas (auditoría de seguridad).
* Botón **🔐 Pair Sensor (60s WPS)** para emparejar nuevos sensores en caliente.

### B. Inspector CLI en Consola (Python)
Desde una computadora conectada al Gateway (o en la misma red LAN):
```bash
python tools/sensor_inspector.py --host 192.168.4.1
```
* **Verificar autenticación**:
  ```bash
  python tools/sensor_inspector.py --host 192.168.4.1 --test-auth
  ```
  *(Envía un paquete válido con `betty1234` y uno con clave errónea para comprobar que la seguridad lo rechaza).*
* **Activar modo emparejamiento (WPS)**:
  ```bash
  python tools/sensor_inspector.py --host 192.168.4.1 --pair
  ```

---

## 📡 3. Guía Paso a Paso: Cómo Enviar Datos desde Cualquier Sensor

Cualquier sensor o microcontrolador puede conectarse al Gateway mediante cualquiera de las siguientes interfaces:

---

### Opción 1: Vía Wi-Fi HTTP REST (La más sencilla para ESP8266 / ESP32 / Raspberry Pi / PC)

Cualquier dispositivo con Wi-Fi conectado a la red `Gateway-IoT-AP` (o que esté en la misma red local que el Gateway) puede enviar peticiones HTTP POST.

* **Endpoint**: `http://192.168.4.1/api/sensor/data`
* **Método**: `POST`
* **Cabecera obligatoria**: `X-Sensor-Key: betty1234` (o `Authorization: Bearer betty1234`)
* **Content-Type**: `application/json`

#### Payload JSON:
```json
{
  "sensorId": "DHT22-CAMPUS",
  "model": "DHT22",
  "metrics": {
    "temperature": 23.5,
    "humidity": 62.1,
    "heat_index": 24.1
  }
}
```

#### Ejemplo con cURL (Terminal):
```bash
curl -X POST http://192.168.4.1/api/sensor/data \
  -H "Content-Type: application/json" \
  -H "X-Sensor-Key: betty1234" \
  -d '{"sensorId":"CLIMA-01","model":"BME280","metrics":{"temperatura":22.8,"humedad":58.4,"presion":1013.2}}'
```
* **Respuesta exitosa (`200 OK`)**:
  ```json
  {"status":"ACCEPTED","sensorId":"CLIMA-01"}
  ```
* **Respuesta si la clave es incorrecta (`401 Unauthorized`)**:
  ```json
  {"error":"Unauthorized","message":"Invalid sensor link credentials"}
  ```

#### Ejemplo en Arduino / ESP32 (C++):
```cpp
#include <WiFi.h>
#include <HTTPClient.h>

void sendTelemetry() {
  HTTPClient http;
  http.begin("http://192.168.4.1/api/sensor/data");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Sensor-Key", "betty1234"); // Clave requerida

  String body = "{\"sensorId\":\"ESTACION-01\",\"metrics\":{\"temp\":24.2,\"hum\":60.5}}";
  int httpCode = http.POST(body);

  if (httpCode == 200) {
    Serial.println("Telemetría aceptada por el Gateway!");
  } else {
    Serial.printf("Error o rechazo de autenticación: HTTP %d\n", httpCode);
  }
  http.end();
}
```

#### Ejemplo en Python:
```python
import requests

url = "http://192.168.4.1/api/sensor/data"
headers = {
    "Content-Type": "application/json",
    "X-Sensor-Key": "betty1234"
}
payload = {
    "sensorId": "SUELO-INVERNADERO",
    "model": "CapacitiveSoil_v2",
    "metrics": {
        "humedad_suelo": 45.8,
        "conductividad": 1.2,
        "ph": 6.8
    }
}

r = requests.post(url, json=payload, headers=headers)
print(r.status_code, r.json())
```

---

### Opción 2: Vía Radio LoRa 433 MHz (Para nodos de campo remotos sin internet)

Los nodos de campo remotos (con Arduino, ESP32 o Heltec + SX1278 a 433 MHz) transmiten directamente por radio.

#### Parámetros LoRa del Gateway:
* **Frecuencia:** `433.0 MHz`
* **Spreading Factor (SF):** `7`
* **Ancho de Banda (BW):** `125 kHz`
* **Coding Rate (CR):** `4/5` (valor 5)
* **Sync Word:** `0x12`

El Gateway soporta dos formatos de trama por LoRa:

#### Formato A: Texto Estructurado (Ligero y de ultra bajo consumo):
```text
$DATA,ID=<SENSOR_ID>,KEY=<CLAVE_PSK>,<METRICA_1>=<VALOR_1>,<METRICA_2>=<VALOR_2>*
```
* **Ejemplo**:
  ```text
  $DATA,ID=LORA-TEMP-01,KEY=betty1234,TEMP=21.4,HUM=55.0,BAT=98*
  ```

#### Formato B: JSON Abierto por Radio:
```json
{"id":"LORA-CLIMA-02","key":"betty1234","metrics":{"t":22.1,"h":58.2,"p":1012.8}}
```

#### Ejemplo en Arduino para Nodo LoRa Transmisor con LBT (Listen-Before-Talk):
Para evitar colisiones cuando varios nodos intentan transmitir simultáneamente, los nodos deben sensar el canal antes de emitir. Si el canal está ocupado, el nodo **no transmite de inmediato**, sino que espera con un retroceso exponencial aleatorio (*backoff*) hasta que el medio esté disponible:

```cpp
#include <SPI.h>
#include <LoRa.h>
#include "lora_lbt_node.h" // Driver LBT con CAD persistente

LoRaLbtNode lbtNode(5, 14, 2); // Pines SS, RST, DIO0

void setup() {
  Serial.begin(115200);
  // Inicializa a 433 MHz, SF7, BW 125kHz, CR 4/5, Sync 0x12
  if (!lbtNode.begin(433E6, 7, 125E3, 5, 0x12)) {
    Serial.println("Error iniciando chip LoRa");
    while (1);
  }
  Serial.println("Nodo LoRa iniciado con Listen-Before-Talk (LBT) activo");
}

void loop() {
  float temp = 23.4;
  float hum = 61.2;

  // Trama formateada con clave de enlace 'betty1234'
  String packet = "$DATA,ID=CAMPO-01,KEY=betty1234,TEMP=" + String(temp, 1) + ",HUM=" + String(hum, 1) + "*";

  // Transmisión con LBT: si el canal está ocupado, espera y reintenta hasta que esté libre
  bool ok = lbtNode.sendPacketLBT(packet, 15000); // 15s de timeout máximo

  if (ok) {
    Serial.println("Paquete transmitido con éxito (canal verificado libre)");
  } else {
    Serial.println("No se pudo transmitir: canal saturado durante la ventana de espera");
  }

  delay(10000); // Enviar cada 10s
}
```

> [!TIP]
> Al usar `sendPacketLBT`, si otro sensor está transmitiendo en ese preciso instante, el nodo detecta los preámbulos por **CAD**, posterga su envío sin interferir en el aire, y transmite automáticamente apenas el canal queda despejado.

> [!WARNING]
> Si el paquete LoRa **no incluye `KEY=betty1234`** o si la clave no coincide, el Gateway **descarta el paquete silenciosamente** en el aire, protegiendo a Betty de inyecciones de datos no autorizadas.

---

### Opción 3: Vía WebSockets (Para aplicaciones Web, móviles o agentes de alta velocidad)

El Gateway ejecuta un servidor WebSocket en el puerto `81`: `ws://192.168.4.1:81`.

#### Paso 1: Handshake de Autenticación
Al abrir el socket, enviar:
```json
{
  "type": "auth",
  "id": "WS-NODE-01",
  "key": "betty1234"
}
```
El Gateway responderá:
```json
{"type":"auth_ack","status":"AUTHORIZED"}
```

#### Paso 2: Enviar Telemetría en Vivo
Una vez conectado o en cada mensaje:
```json
{
  "type": "sensor_data",
  "sensorId": "ROBOT-AGV-01",
  "key": "betty1234",
  "metrics": {
    "velocidad": 1.4,
    "bateria": 88,
    "distancia_obstaculo": 42
  }
}
```

---

### Opción 4: Vía Puerto Serie USB (Arduino / Raspberry Pi conectada por cable)

Conecta un microcontrolador o PC al puerto USB del Gateway a `115200 baudios`.
Envía líneas JSON terminadas en salto de línea `\n`:
```json
{"id":"ARDUINO-SERIAL","key":"betty1234","metrics":{"luz_lux":650,"co2_ppm":420}}
```

---

## 🔗 4. Cómo se Conecta el Gateway con Betty (Botuto)

El Gateway opera en modo dual Wi-Fi (`WIFI_AP_STA`):
1. **Punto de Acceso (`AP`)**: Emite `Gateway-IoT-AP` para que los sensores y el técnico se conecten directamente.
2. **Estación Wi-Fi (`STA`)**: Se conecta a la red Wi-Fi de tu casa, laboratorio o campus donde está corriendo Betty.

### Configuración del Enlace a Betty en 3 Pasos:
1. Conéctate a la red `Gateway-IoT-AP` (se abrirá el Portal Cautivo o visita `http://192.168.4.1`) y haz clic en **⚙️ Gateway Settings**.
2. Completa los campos:
   * **Wi-Fi STA SSID**: El nombre de tu red Wi-Fi local.
   * **Wi-Fi STA Password**: La contraseña de tu red Wi-Fi.
   * **Betty Host**: La IP de la máquina donde corre `betty-api` (ej. `192.168.1.150`).
   * **Betty Port**: `3000` (puerto por defecto del backend).
   * **Gateway Sensor UUID**: Copia el UUID de tu sensor registrado en Betty (ej. `d8c83e1c-54a8-4c6e-8cb5-e2f9d50b4412`).
   * **Link Master Key (PSK)**: La clave de enlace para los sensores (ej. `betty1234`).
3. Haz clic en **💾 Save Configuration**.

El Gateway se conectará automáticamente a tu red y cada vez que ingrese un dato de cualquier sensor (por LoRa, Wi-Fi HTTP, WebSockets o Serie), lo colocará en una cola FreeRTOS asíncrona y lo enviará vía webhook a:
```http
POST http://<betty_host>:3000/api/mqtt/webhook
```
Betty validará el payload y lo guardará en la tabla `sensor_data` de TimescaleDB, actualizando en milisegundos los tacómetros y gemelos digitales del dashboard.
