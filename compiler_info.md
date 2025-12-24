# 🔧 Guía de Compilación y Carga del Firmware

Esta guía te ayudará a compilar, cargar y monitorear el firmware del checador NFC en tu ESP8266 NodeMCU.

---

## 🚀 Método Rápido (Script Automático)

El proyecto incluye un script automatizado que realiza todo el proceso en un solo comando:

```bash
./compile_and_upload.sh
```

### ✨ Características del Script

#### 1. **Verificación de Estado del Dispositivo** 🔌
- ✅ Detecta automáticamente si el ESP8266 está conectado
- ✅ Muestra información detallada del puerto y tipo de placa
- ✅ Detecta automáticamente el puerto si el configurado no está disponible
- ✅ Verifica el estado antes de cada operación crítica

#### 2. **Barras de Progreso y Spinners** 📊
- ✅ Spinner animado durante la compilación
- ✅ Indicadores de progreso durante la carga del firmware
- ✅ Etapas visuales: "Iniciando", "Escribiendo", "Verificando", "Completando"
- ✅ Feedback visual claro en cada paso

#### 3. **Interfaz Mejorada** 🎨
- ✅ Encabezado visual con bordes
- ✅ Colores diferenciados para cada tipo de mensaje
- ✅ Indicadores de progreso paso a paso [1/4], [2/4], etc.
- ✅ Mensajes de error más descriptivos con sugerencias

#### 4. **Detección Automática** 🔍
- ✅ Busca automáticamente dispositivos disponibles si el puerto configurado no funciona
- ✅ Pregunta al usuario si desea usar el puerto detectado
- ✅ Muestra todos los puertos disponibles si no encuentra ninguno

### 📋 Ejemplo de Salida del Script

```
╔══════════════════════════════════════════════════════════╗
║     Checador NFC - Compilación y Carga Automática      ║
╚══════════════════════════════════════════════════════════╝

[0/4] Verificando configuración...
✅ secrets.h encontrado

[1/4] Verificando conexión del dispositivo...
   Puerto configurado: /dev/ttyUSB0
   Estado: ✅ Conectado (NodeMCU 1.0)
   Puerto seleccionado: /dev/ttyUSB0

[2/4] Compilando el sketch...
   FQBN: esp8266:esp8266:nodemcuv2
   Sketch: soul23_time_attendance.ino
   [|] Compilando... |  ← Spinner animado
   [✓] Compilación completada
✅ Compilación exitosa

[3/4] Cargando firmware al dispositivo...
   Puerto: /dev/ttyUSB0
   Estado: ✅ Conectado (NodeMCU 1.0)
   [|] Escribiendo... |  ← Spinner con etapas
   [✓] Carga completada
✅ Firmware cargado exitosamente

[4/4] Abriendo monitor serial...
   Puerto: /dev/ttyUSB0
   Baud rate: 115200
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   Monitor Serial (Presiona Ctrl+C para salir)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### 🛠️ Personalización del Script

Puedes modificar estas variables al inicio del script `compile_and_upload.sh`:

```bash
PORT="/dev/ttyUSB0"                    # Puerto del dispositivo
FQBN="esp8266:esp8266:nodemcuv2"       # Tipo de placa
BAUD_RATE="115200"                     # Velocidad del monitor serial
```

---

## 🔧 Comandos Individuales

Si prefieres ejecutar los comandos manualmente o necesitas más control:

### 1. Verificar puertos disponibles

```bash
arduino-cli board list
```

### 2. Compilar el sketch

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 soul23_time_attendance.ino
```

**Nota:** El FQBN `esp8266:esp8266:nodemcuv2` es para NodeMCU 1.0 (ESP-12E Module).

### 3. Cargar al dispositivo

```bash
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp8266:esp8266:nodemcuv2 soul23_time_attendance.ino
```

**⚠️ Importante:** Reemplaza `/dev/ttyUSB0` con el puerto correcto de tu dispositivo (verifica con `arduino-cli board list`)

### 4. Abrir monitor serial

```bash
arduino-cli monitor -p /dev/ttyUSB0 --config baudrate=115200
```

**Para salir del monitor:** Presiona `Ctrl+C`

### 5. Compilar sin cargar (solo verificar errores)

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 soul23_time_attendance.ino
```

### 6. Ver información detallada de compilación

```bash
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 --verbose soul23_time_attendance.ino
```

### 7. Limpiar archivos de compilación

```bash
rm -rf build/
```

---

## 📦 Instalación de Dependencias

### Instalar Core de ESP8266

```bash
arduino-cli core install esp8266:esp8266
```

### Instalar Librerías Necesarias

```bash
# Librerías principales
arduino-cli lib install "MFRC522"
arduino-cli lib install "Adafruit SSD1306"
arduino-cli lib install "Adafruit GFX Library"
arduino-cli lib install "ArduinoJson"
arduino-cli lib install "Time"
```

---

## 🔍 Verificar Instalación

### Ver librerías instaladas

```bash
arduino-cli lib list
```

### Ver cores instalados

```bash
arduino-cli core list
```

### Verificar FQBN correcto

```bash
arduino-cli board listall | grep -i nodemcu
```

Salida esperada:
```
NodeMCU 0.9 (ESP-12 Module)                 esp8266:esp8266:nodemcu
NodeMCU 1.0 (ESP-12E Module)                esp8266:esp8266:nodemcuv2
```

---

## 🐛 Solución de Problemas

### Error: "No se encuentra secrets.h"

```bash
cp secrets.h.example secrets.h
# Luego edita secrets.h con tus credenciales
```

### Error: "Puerto no encontrado"

1. Verifica que el ESP8266 esté conectado:
   ```bash
   arduino-cli board list
   ```

2. Si no aparece, verifica permisos:
   ```bash
   sudo usermod -a -G dialout $USER
   ```
   Luego reinicia sesión o ejecuta:
   ```bash
   newgrp dialout
   ```

### Error: "FQBN no válido"

Verifica el modelo exacto de tu NodeMCU y ajusta el FQBN:
- **NodeMCU 1.0 (ESP-12E):** `esp8266:esp8266:nodemcuv2`
- **NodeMCU 0.9:** `esp8266:esp8266:nodemcu`

### Monitor serial no muestra datos

1. Verifica el baud rate (debe ser **115200**)
2. Presiona el botón **RESET** del ESP8266
3. Verifica que el puerto sea correcto
4. Asegúrate de que no haya otro programa usando el puerto serial

### Error durante la carga del firmware

1. **Desconecta el buzzer** temporalmente (desconecta el GND) durante la carga
2. Verifica que no haya otro programa usando el puerto
3. Presiona el botón **RESET** del ESP8266 antes de cargar
4. Si es necesario, mantén presionado el botón **BOOT** mientras cargas

### El script no encuentra el dispositivo

1. El script buscará automáticamente puertos disponibles
2. Si encuentra uno, te preguntará si deseas usarlo
3. Si no encuentra ninguno, verifica:
   - Que el cable USB esté bien conectado
   - Que el driver USB-Serial esté instalado
   - Que tengas permisos para acceder al puerto

---

## 💡 Tips y Mejores Prácticas

### Uso del Script Automático

- El script detecta automáticamente el puerto si el configurado no está disponible
- Los spinners muestran que el proceso está en ejecución
- Los mensajes de error incluyen sugerencias para solucionarlos
- El estado del dispositivo se verifica antes de cada operación crítica

### Desarrollo y Depuración

- **Mantén el monitor serial abierto** mientras pruebas para ver los mensajes de debug
- El código imprime información útil en Serial a 115200 baud
- Si cambias el código, solo necesitas recompilar y cargar (no necesitas cerrar el monitor)
- Para depuración, busca mensajes que empiezan con `--- Checador IOT (Public Version) ---`

### Flujo de Trabajo Recomendado

1. **Primera vez:**
   ```bash
   # Instalar dependencias
   arduino-cli core install esp8266:esp8266
   arduino-cli lib install "MFRC522" "Adafruit SSD1306" "Adafruit GFX Library" "ArduinoJson" "Time"
   
   # Configurar secrets.h
   cp secrets.h.example secrets.h
   # Editar secrets.h con tus credenciales
   
   # Compilar y cargar
   ./compile_and_upload.sh
   ```

2. **Desarrollo iterativo:**
   ```bash
   # Solo necesitas ejecutar el script
   ./compile_and_upload.sh
   ```

3. **Solo verificar compilación:**
   ```bash
   arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 soul23_time_attendance.ino
   ```

### Optimización

- Si tienes problemas de espacio, puedes limpiar archivos de compilación:
  ```bash
  rm -rf build/
  ```
- Para compilaciones más rápidas, el script ya optimiza el proceso automáticamente

---

## 📝 Resumen de Comandos Rápidos

```bash
# Método rápido (recomendado)
./compile_and_upload.sh

# Comandos individuales
arduino-cli board list                                    # Ver puertos
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 soul23_time_attendance.ino
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp8266:esp8266:nodemcuv2 soul23_time_attendance.ino
arduino-cli monitor -p /dev/ttyUSB0 --config baudrate=115200

# Verificación
arduino-cli lib list                                      # Ver librerías
arduino-cli core list                                     # Ver cores
arduino-cli board listall | grep -i nodemcu              # Ver placas disponibles
```

---

## 🎯 Próximos Pasos

Una vez que hayas compilado y cargado el firmware exitosamente:

1. ✅ Verifica que el dispositivo se conecte a WiFi
2. ✅ Revisa el monitor serial para confirmar la sincronización de tiempo
3. ✅ Prueba acercando una tarjeta NFC para verificar la lectura
4. ✅ Verifica que los registros se envíen correctamente al webhook

Para más información sobre el hardware y la configuración, consulta el [README.md](README.md).


