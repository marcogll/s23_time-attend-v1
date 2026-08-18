<p align="center">
  <a href="https://soul23.mx">
    <picture>
      <source
        media="(prefers-color-scheme: dark)"
        srcset="https://raw.githubusercontent.com/marcogll/mg_data_storage/refs/heads/main/soul23/logo/soul23_logo_wh.png">
      <source
        media="(prefers-color-scheme: light)"
        srcset="https://raw.githubusercontent.com/marcogll/mg_data_storage/refs/heads/main/soul23/logo/soul23_logo_blk.png">
      <img
        src="https://raw.githubusercontent.com/marcogll/mg_data_storage/refs/heads/main/soul23/logo/soul23_logo_blk.png"
        width="110"
        alt="Soul:23">
    </picture>
  </a>
</p>

<h1 align="center">S23 Time Attend V1</h1>

<p align="center">
  MVP del sistema de checador de puntualidad con n8n, webhooks y ESP8266.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/C++-3a3a3a?style=flat-square&logo=cpp&logoColor=white">
  <img src="https://img.shields.io/badge/ESP8266-3a3a3a?style=flat-square&logo=espressif&logoColor=white">
  <img src="https://img.shields.io/badge/n8n-3a3a3a?style=flat-square&logo=n8n&logoColor=white">
</p>

---

## Description

MVP del sistema de checador de puntualidad basado en n8n, webhooks y ESP8266: guía rápida, estructura del repositorio, funcionalidades, arquitectura del firmware y flujo de operación.

## ⚡️ Guía Rápida (TL;DR)

1. Instala dependencias (ESP8266 core + librerías listadas más abajo) en Arduino IDE.
2. Duplica `secrets.h.example` → `secrets.h` y rellena Wi‑Fi, URLs y zona horaria.
3. Carga el sketch `soul23_time_attendance.ino` en tu NodeMCU 1.0 (ESP‑12E).
4. Cablea RC522 y OLED siguiendo el **pinout seguro** descrito en este documento.
5. Graba tarjetas NFC con **JSON válido** usando NFC Tools.
6. Abre el monitor serie (115200) para verificar conexión, hora y peticiones HTTP.

---

## 📁 Estructura del Repositorio

```
soul23_time_attendance/
├── soul23_time_attendance.ino
├── README.md
├── secrets.h.example
├── secrets.h
└── test/
```

> Solo es necesario editar `secrets.h` para credenciales y constantes sensibles.

---

## 🚀 Funcionalidades

* **Lectura NFC/RFID** (Mifare / NTAG 13.56 MHz)
* **Decodificación NDEF en JSON**
* **Sincronización de hora en la nube** (sin RTC físico)
* **Feedback visual en OLED**
* **Integración vía Webhook HTTP (POST JSON)**

---

## 🧠 Arquitectura del Firmware

| Módulo                 | Función                                            |
| ---------------------- | -------------------------------------------------- |
| `setup()`              | Inicializa Wi‑Fi, OLED, SPI, NFC y sincroniza hora |
| `loop()`               | Muestra hora, refresca estado y espera tarjeta     |
| `processTag()`         | Lee UID + JSON NFC y dispara envío                 |
| `syncTimeWithServer()` | Obtiene `unixtime` desde servidor                  |
| `sendToWebhook()`      | Envía payload JSON vía POST                        |

---

## 🔬 Flujo de Operación

1. **Arranque:** conexión Wi‑Fi + sincronización de hora.
2. **Espera:** muestra reloj y mensaje *ACERQUE TARJETA*.
3. **Lectura NFC:** UID + payload JSON.
4. **Validación:** deserialización segura.
5. **Envío:** POST al webhook.
6. **Feedback:** confirmación visual/sonora.

---

## 🛠️ Hardware Requerido

| Componente      | Descripción       |
| --------------- | ----------------- |
| NodeMCU ESP8266 | MCU con Wi‑Fi     |
| RC522           | Lector NFC SPI    |
| OLED 0.96"      | SSD1306 I2C       |
| LED             | Indicador visual  |
| Buzzer          | Indicador audible |
| Tags NFC        | NTAG213 / Mifare  |

---

## 🔌 Pinout Seguro (Producción)

### RC522 (SPI)

| RC522 | NodeMCU | GPIO   | Color Cable | Nota                               |
| ----- | ------- | ------ | ----------- | ---------------------------------- |
| SDA   | D8      | GPIO15 | Azul        | Pull‑down interno (seguro como SS) |
| SCK   | D5      | GPIO14 | Amarillo    | SPI                                |
| MOSI  | D7      | GPIO13 | Morado      | SPI                                |
| MISO  | D6      | GPIO12 | Blanco      | SPI                                |
| IRQ   | NC      | —      | Naranja     | No conectado                       |
| GND   | GND     | —      | Verde       | Tierra común                       |
| RST   | D0      | GPIO16 | Negro       | Seguro para reset                  |
| 3.3V  | 3.3V    | —      | Rojo        | ⚠️ No usar 5V                      |

### OLED (I2C)

| OLED | NodeMCU | Color Cable |
| ---- | ------- | ----------- |
| SDA  | D2      | Azul        |
| SCL  | D1      | Naranja     |
| VCC  | 3.3V    | Rojo        |
| GND  | GND     | Negro       |

### LED y Buzzer

| Componente | NodeMCU | GPIO  | Uso             |
| ---------- | ------- | ----- | --------------- |
| LED        | D3      | GPIO0 | Estado visual   |
| Buzzer     | D4      | GPIO2 | Feedback sonoro |

---

## 🔧 Pull‑Up Resistors y Pines de Boot (Explicación Clave)

El **ESP8266 tiene pines críticos durante el arranque**. Si alguno está en el nivel incorrecto, el microcontrolador entra en **modo programación** o **falla el boot**.

### GPIO 0 (D3) – LED

* **Requisito:** debe estar en **HIGH** durante el boot.
* **Riesgo:** un LED mal cableado puede forzar LOW.
* **Solución aplicada:**

  * El firmware inicializa primero:

    ```cpp
    pinMode(LED_PIN, INPUT_PULLUP);
    ```
  * Luego se configura como salida cuando el sistema ya arrancó.
* **Resultado:** se evita entrar en modo flash accidentalmente.

### GPIO 2 (D4) – Buzzer

* Tiene **pull‑up interno por defecto**.
* **Problema común:** un buzzer pasivo conectado durante la carga puede generar interferencia.
* **Buenas prácticas:**

  * Inicializar el buzzer **al final del `setup()`**.
  * Desconectar GND del buzzer durante la carga del firmware.
  * Opcional: resistencia serie de **220 Ω**.

### GPIO 15 (D8)

* Tiene **pull‑down interno**.
* Ideal para **SS (SDA) del RC522**.
* **Nunca usar** para señales que requieran HIGH en boot.

### GPIO 16 (D0)

* No tiene pull‑up/pull‑down.
* Seguro para RST del RC522.
* No soporta PWM.

---

## 💳 Formato del Payload NFC (Entrada)

El tag NFC debe contener un **registro NDEF de texto** con JSON válido.

Ejemplo recomendado:

```json
{
  "name": "Juan",
  "num_emp": "XXXX250117",
  "sucursal": "sucursal_1",
  "telegram_id": "123456789"
}
```

Campos adicionales son ignorados de forma segura si no se usan.

---

## 📡 Payload Enviado al Webhook (Salida)

Ejemplo real del POST generado por el dispositivo:

```json
{
  "card_uid": "04A1B2C3",
  "name": "Juan",
  "num_emp": "XXXX250117",
  "sucursal": "sucursal_1",
  "telegram_id": "123456789",
  "timestamp": 1765913424,
  "device": "Checador_Oficina_1"
}
```

---

## ⚙️ Configuración (`secrets.h`)

```cpp
const char* ssid = "TU_WIFI";
const char* password = "TU_PASSWORD";

const char* webhookUrl = "https://api.tudominio.com/webhook";
const char* timeServerUrl = "https://soul23.cloud/time-server";

#define DEVICE_NAME "Checador_Oficina_1"
#define TIMEZONE_OFFSET -21600
```

---

## 🐛 Troubleshooting Rápido

* **No arranca:** revisar GPIO0 y GPIO2.
* **No lee NFC:** revisar 3.3 V y RST.
* **No carga firmware:** desconectar buzzer temporalmente.
* **JSON inválido:** validar comillas rectas y formato.

---

## 📌 Estado

**MVP funcional**, listo para pilotos, dashboards y automatización RH.

---

## 👤 Autor

**Marco** — Inventor · Automatización · IoT

---

## 📄 Licencia

MIT




