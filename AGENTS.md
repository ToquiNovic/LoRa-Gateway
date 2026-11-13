# 🤖 AGENTS.md — Reglas y Guía de Arquitectura para Asistentes y Agentes de IA

Este archivo define las instrucciones, restricciones de hardware, estándares de código y políticas de seguridad que todo **Agente de IA** (Google Antigravity, Cursor, Claude Code, GitHub Copilot) debe seguir al trabajar en este repositorio.

---

## 🧭 1. Misión y Alcance del Proyecto

El repositorio `LoRa-Gateway` contiene el firmware y herramientas para el **Botuto IoT Sensor Gateway (ESP32 + LoRa 433 MHz)**.
Su función principal es actuar como un **puente de borde (Edge Bridge)** transparente y seguro entre sensores físicos/remotos y la plataforma en la nube **Botuto Platform** (`botuto.laboratorio3d.online`).

---

## 🧱 2. Restricciones Críticas de Hardware y Arquitectura

Todo agente que proponga modificaciones o cree código debe respetar estrictamente estas directrices de hardware:

1. **Arquitectura FreeRTOS SMP Dual-Core (ESP32)**:
   * **Core 0 (Networking & Uplink)**: Ejecuta la pila Wi-Fi, servidor HTTP (puerto 80), servidor WebSockets (puerto 81), servidor DNS Cautivo (puerto 53 UDP) y la tarea de despacho HTTPS `BettyBridge`.
   * **Core 1 (Sensing & RF)**: Dedicado a la radio LoRa SX1278 (CAD/EDCA), muestreo de sensores físicos (I2C, ADC) y procesamiento de interrupciones sin bloquear las comunicaciones de red.
   * **Desacoplamiento asíncrono**: La comunicación entre cores se realiza exclusivamente mediante colas FreeRTOS (`xQueue`). Nunca uses variables globales sin protección o delays bloqueantes dentro de las tareas críticas.

2. **Prohibición del Stack BLE**:
   * El soporte BLE fue removido deliberadamente para evitar colisiones de RF con el Wi-Fi en el módem del ESP32 (abortos por coexistencia en ESP-IDF) y para liberar más de 140 KB de memoria RAM (Heap libre > 180 KB).
   * **Regla**: NO reintroducir librerías BLE (`BLEDevice.h`, `BLEServer.h`) en el Gateway base.

3. **Parámetros de Radio LoRa 433 MHz**:
   * **Transceptor**: Semtech SX1278 (Ra-02).
   * **Frecuencia**: `433E6` (433.0 MHz).
   * **Spreading Factor**: `7` (SF7).
   * **Ancho de Banda**: `125E3` (125 kHz).
   * **Coding Rate**: `5` (CR 4/5).
   * **Sync Word**: `0x12` (Palabra de sincronización privada).
   * **Acceso al Medio**: Uso obligatorio de Listen-Before-Talk (LBT) mediante Channel Activity Detection (CAD).

4. **Uplink Seguro hacia Botuto Cloud**:
   * El destino predeterminado es `https://betty-api.laboratorio3d.online/api/mqtt/webhook` en el puerto `443` con TLS/HTTPS.
   * Las peticiones salientes deben utilizar `WiFiClientSecure` con `setInsecure()` para operar de forma resiliente en microcontroladores embebidos sin carga excesiva de certificados raíz.

---

## 🛡️ 3. Reglas de Seguridad y Validación de Datos

1. **Pre-Shared Key Obligatoria (`betty1234`)**:
   * Todo sensor que ingrese por cualquier interfaz (LoRa, Wi-Fi HTTP, WebSockets o Serie) DEBE incluir la clave de enlace configurada en el Gateway (`betty1234` por defecto).
   * La validación la ejecuta `SensorAuthGuard`. Si la clave es inválida o ausente, el paquete debe ser descartado inmediatamente sin enviarse a Botuto ni procesarse en la cola.
   * **Regla**: Nunca deshabilites la validación de `SensorAuthGuard` para "facilitar pruebas". La seguridad a nivel de enlace es innegociable.

2. **Identificador del Sensor (`sensorId` = UUID)**:
   * En Botuto Platform, cada lectura se almacena en la tabla `sensor_data` de TimescaleDB, la cual tiene una restricción de clave foránea (`FOREIGN KEY`) hacia la tabla `sensors.id` (UUID v4).
   * Al generar ejemplos, firmas o pruebas, el `sensorId` SIEMPRE debe ser un UUID válido generado en la plataforma (ejemplo: `55f1a34e-a2ab-4ece-ade4-6d4c5808c08f`).
   * **Regla**: Nunca uses identificadores aleatorios que no sean UUIDs si el destino es la ingesta en TimescaleDB.

---

## 📁 4. Estructura del Repositorio y Habilidades

Al navegar o extender este proyecto, ubica los componentes según este mapa:

```text
├── .agents/
│   └── skills/
│       └── botuto-sensor-integration/   # Skill oficial para conectar sensores
├── examples/
│   └── heltec_v2_lora_led/              # Proyecto de ejemplo para Heltec LoRa V2
├── include/
│   ├── config.h                         # Parámetros por defecto de hardware y red
│   ├── betty_bridge.h                   # Tarea y cola de enlace HTTPS a Botuto
│   ├── lora_manager.h                   # Driver LoRa con CAD-EDCA y LBT
│   ├── sensor_auth.h                    # Motor SensorAuthGuard (validación PSK)
│   └── sensor_manager.h                 # Registro en vivo y multiplexor de ingesta
├── src/                                 # Implementación modular en C++
├── platformio.ini                       # Configuración de compilación para ESP32
├── README.md                            # Documentación principal con diagramas
└── AGENTS.md                            # Este archivo de reglas para agentes
```

---

## 💻 5. Estándares de Código y Flujo de Trabajo

1. **Framework y Herramientas**:
   * Plataforma: PlatformIO (`platform = espressif32`, `board = esp32dev`, `framework = arduino`).
   * Manejo de JSON: `bblanchon/ArduinoJson @ ^7.0.4` (utilizar `JsonDocument` estilo v7, evitando la sintaxis deprecada de `DynamicJsonDocument` de v6).
   * Driver LoRa: `sandeepmistry/LoRa @ ^0.8.0`.

2. **Control de Versiones (Git)**:
   * Utilizar **Conventional Commits** estrictamente:
     * `feat(...)`: Nuevas funcionalidades o interfaces.
     * `fix(...)`: Corrección de errores en drivers o puente de red.
     * `docs(...)`: Mejoras en diagramas o guías.
     * `refactor(...)`: Mejoras en la arquitectura sin cambio de comportamiento.
   * **Regla**: NUNCA agregar trailers de atribución artificiales como `Co-Authored-By` o menciones de autoría por IA en los mensajes de commit.

3. **Verificación Obligatoria antes de Dar por Concluida una Tarea**:
   * Toda modificación en el código debe compilarse con PlatformIO (`pio run`) garantizando cero errores (`[SUCCESS]`).
   * Si se crean nuevos ejemplos de sensores, deben incluir su propio `platformio.ini`, `src/main.cpp` y `README.md` documentado.
