#include <SPI.h>
#include <MFRC522.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TimeLib.h> // Requiere instalar: arduino-cli lib install "Time"

#include "secrets.h"

// Intervalo de resincronización del reloj (1 hora)
const unsigned long syncInterval = 3600000; 

// ==========================================
// 🔌 PINES SEGUROS (NO INTERFIEREN CON BOOT)
// ==========================================
#define SS_PIN  D8  // GPIO 15
#define RST_PIN D0  // GPIO 16
#define OLED_SDA D2
#define OLED_SCL D1
#define LED_PIN  D3  // LED de estatus (GPIO 0) - Inicializado con pull-up para evitar problemas de boot
#define BUZZER_PIN D4 // Buzzer (GPIO 2) - ⚠️ IMPORTANTE: Inicializado MUY TARDE en setup() para evitar problemas de boot

MFRC522 mfrc522(SS_PIN, RST_PIN);

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Variables de control
unsigned long lastSyncTime = 0;
int lastMinuteDisplayed = -1;
bool hasError = false;
unsigned long lastBlinkTime = 0;
bool ledState = LOW;

// Control del Watchdog del Lector NFC
const unsigned long nfcCheckInterval = 15000; // 15 segundos (Revisión frecuente para auto-reparación rápida)
unsigned long lastNfcCheck = 0;


// ==========================================
// 📝 DECLARACIONES DE FUNCIONES
// ==========================================
void checkAndResetNFC();
void showStatus(String line1, String line2);
void syncTimeWithServer();
bool sendToWebhook(String uid, String name, String num_emp, String sucursal);
void showClockScreen();
String getFormattedTime();
bool readCardData(byte *data, byte *length);
void processTag();
String generateUUID(byte length);
String getISO8601DateTime(time_t t);
String getFormattedDate();
void beep(int times, int duration);
void alarmSound();
void silenceBuzzer();
void updateStatusLED();
void scanI2C();

// ==========================================
// 🚀 SETUP
// ==========================================
void setup() {
  Serial.println("\n--- Iniciando setup ---");
  // ⚠️ CRÍTICO: Inicializar GPIO 2 (BUZZER) PRIMERO para evitar ruido durante boot
  // GPIO 2 tiene pull-up interno que puede causar problemas con buzzer pasivo
  // Configurarlo como INPUT sin pull-up/pull-down para evitar interferencia
  pinMode(BUZZER_PIN, INPUT); // INPUT sin pull para evitar ruido durante boot
  delay(10); // Pequeña pausa para estabilizar
  
  // ⚠️ INICIALIZAR GPIO 0 (LED) CON PULL-UP para evitar problemas de boot
  // Si GPIO 0 está LOW durante boot, el ESP8266 entra en modo programación
  pinMode(LED_PIN, INPUT_PULLUP); // Primero como INPUT con pull-up interno
  delay(10); // Pequeña pausa para estabilizar
  pinMode(LED_PIN, OUTPUT); // Luego como OUTPUT
  digitalWrite(LED_PIN, LOW); // Apagado inicial
  
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n--- Checador IOT (Public Version) ---");
  
  randomSeed(analogRead(A0));

  // 1. Inicializar los buses de comunicación primero
  // ⚠️ NO inicializar el buzzer aquí para evitar problemas de boot
  // El pin D4 (GPIO 2) tiene pull-up interno, pero el buzzer puede causar problemas si está conectado
  SPI.begin();
  Wire.begin(OLED_SDA, OLED_SCL);

  // 2. Luego, inicializar los dispositivos periféricos
  Serial.println("Escanenado dispositivos I2C...");
  scanI2C(); // Escanear dispositivos I2C conectados
  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial(); // Imprime datos del lector NFC para depuración


  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("Intentando dirección OLED 0x3D..."));
    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
      Serial.println(F("Error: No se encuentra OLED"));
      sos(); // Error crítico, S.O.S.
    }
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  // display.setContrast(255); // Comentado: Método no soportado
  display.dim(false); // Configurar brillo máximo (equivalente a alto contraste)
  Serial.println("OLED inicializado y configurado.");
  
  showStatus("CONECTANDO", "WIFI...");
  WiFi.begin(ssid, password);
  
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) { 
    delay(500); Serial.print("."); timeout++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    showStatus("WIFI", "OK");
    delay(1000); // Pausa para estabilizar la red
  } else {
    showStatus("ERROR", "WIFI"); 
    sos(); // Error crítico, S.O.S.
  }
  
  syncTimeWithServer();
  Serial.println("Sistema Listo.");
  
  // ⚠️ CRÍTICO: Configurar buzzer como OUTPUT SOLO AL FINAL, después de que TODO esté listo
  // El pin ya está configurado como INPUT al inicio para evitar ruido durante boot
  // Ahora lo cambiamos a OUTPUT y lo ponemos en LOW para silenciarlo
  delay(100); // Pausa adicional para asegurar que todo esté estable
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Buzzer apagado inicial - CRÍTICO para evitar ruido
  delay(50); // Pausa más larga para estabilizar
  // Asegurar múltiples veces que el buzzer esté silenciado
  digitalWrite(BUZZER_PIN, LOW);
  delay(10);
  digitalWrite(BUZZER_PIN, LOW);
  
  blinkLed(5, 200); // Boot OK - Parpadeo de 5 veces
  digitalWrite(LED_PIN, LOW); // LED apagado en estado de reposo
  silenceBuzzer(); // Asegurar que el buzzer esté silenciado
}

// ==========================================
// 🔄 LOOP PRINCIPAL
// ==========================================
void loop() {
  Serial.println("Loop ejecutándose..."); // Debug: confirmar que loop se ejecuta
  checkAndResetNFC(); // Comprobar estado del lector NFC

  if (timeStatus() == timeNotSet || (millis() - lastSyncTime > syncInterval)) {
    syncTimeWithServer();
  }

  if (minute() != lastMinuteDisplayed) {
    showClockScreen();
    lastMinuteDisplayed = minute();
  }
  
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    processTag();
    lastMinuteDisplayed = -1;
  }
  
  // Asegurar que el buzzer esté silenciado en cada iteración
  silenceBuzzer();
  
  delay(50); 
}

// ==========================================
// 📋 LÓGICA DE PROCESAMIENTO
// ==========================================
void processTag() {
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
     uid += (mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
     uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  uid.toUpperCase();

  byte buffer[128]; 
  byte bufferSize = sizeof(buffer);
  
  if (readCardData(buffer, &bufferSize)) {
    int jsonStart = -1;
    for (int i = 0; i < bufferSize; i++) { if (buffer[i] == '{') { jsonStart = i; break; } }

    if (jsonStart != -1) {
      DynamicJsonDocument docCard(512);
      DeserializationError error = deserializeJson(docCard, (const char*)(buffer + jsonStart));

      if (error) {
        showStatus("ERROR", "JSON ILEGIBLE");
        signalTemporaryError();
      } else {
        String name = docCard["name"];
        String num_emp = docCard["num_emp"];
        String sucursal = docCard["sucursal"];
        String telegram_id = docCard["telegram_id"];
        
        sendToWebhook(uid, name, num_emp, sucursal, telegram_id);
      }
    } else {
      showStatus("ERROR", "SIN JSON");
      signalTemporaryError();
    }
  } else {
    showStatus("ERROR", "LECTURA FALLO");
    signalTemporaryError();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
  // El delay y el apagado del LED se manejan ahora en las funciones de señalización
}

// ==========================================
// 🚨 WATCHDOG DEL LECTOR NFC
// ==========================================
void checkAndResetNFC() {
  // Solo ejecutar si ha pasado el intervalo de tiempo definido
  if (millis() - lastNfcCheck > nfcCheckInterval) {
    Serial.println("NFC Watchdog: Verificando lector...");

    // Verificación más segura leyendo la versión del firmware
    byte v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
    
    // 0x00 o 0xFF indican error de comunicación
    if (v == 0x00 || v == 0xFF) {
      Serial.println(F("NFC Watchdog: Lector no responde. Intentando Soft Reset..."));
      mfrc522.PCD_Init();
      delay(50);
      
      // Verificar si revivió
      v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
      if (v == 0x00 || v == 0xFF) {
         Serial.println(F("NFC Watchdog: Soft Reset falló. Intentando HARD RESET..."));
         
         // Hard Reset usando el pin RST
         pinMode(RST_PIN, OUTPUT);
         digitalWrite(RST_PIN, LOW);
         delay(100); // Mantener en bajo un momento
         digitalWrite(RST_PIN, HIGH);
         delay(50); // Esperar a que arranque
         
         mfrc522.PCD_Init(); // Re-inicializar registros
         
         v = mfrc522.PCD_ReadRegister(MFRC522::VersionReg);
         if (v == 0x00 || v == 0xFF) {
            Serial.println(F("NFC Watchdog: ¡FALLO CRÍTICO! Hard Reset tampoco funcionó."));
         } else {
            Serial.println(F("NFC Watchdog: Hard Reset exitoso."));
         }
      } else {
         Serial.println(F("NFC Watchdog: Soft Reset exitoso."));
      }
    } else {
      Serial.print(F("NFC Watchdog: Lector OK (v=0x"));
      Serial.print(v, HEX);
      Serial.println(F("). Refrescando configuración..."));
      // Re-inicializar de todos modos para asegurar configuración correcta (Gain, etc.)
      mfrc522.PCD_Init();
    }

    // Actualizar el tiempo de la última comprobación
    lastNfcCheck = millis();
  }
}

// ==========================================
// ☁️ RED Y LECTURA DE TARJETA
// ==========================================
bool readCardData(byte *data, byte *length) {
  MFRC522::MIFARE_Key key;
  byte nfcForumKey[6] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7};
  memcpy(key.keyByte, nfcForumKey, 6);

  MFRC522::StatusCode status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 4, &key, &(mfrc522.uid));
  if (status != MFRC522::STATUS_OK) { return false; }
  
  delay(5);

  byte readBlock[18];
  byte index = 0;
  for (byte block = 4; block < 12; block++) {
    if (block % 4 == 3) {
      status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block + 1, &key, &(mfrc522.uid));
      if (status != MFRC522::STATUS_OK) break;
      continue;
    }
    byte size = sizeof(readBlock);
    status = mfrc522.MIFARE_Read(block, readBlock, &size);
    if (status != MFRC522::STATUS_OK) break;
    memcpy(&data[index], readBlock, 16);
    index += 16;
  }
  *length = index;
  return index > 0;
}

void syncTimeWithServer() {
  if (WiFi.status() != WL_CONNECTED) return;
  std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
  client->setInsecure();
  HTTPClient http;
  if (http.begin(*client, timeServerUrl)) {
    http.setTimeout(10000); // Timeout de 10 segundos para evitar resets por watchdog
    http.setUserAgent(DEVICE_NAME);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, http.getString());
      // Ajuste para el servidor de ejemplo worldtimeapi.org
      setTime(doc["unixtime"].as<unsigned long>() + timeZoneOffset); 
      lastSyncTime = millis();
    }
    http.end();
  }
}

bool sendToWebhook(String uid, String name, String num_emp, String sucursal, String telegram_id) {
  if (timeStatus() == timeNotSet) {
    showStatus("ERROR", "SIN HORA");
    signalTemporaryError();
    return false;
  }
  
  std::unique_ptr<WiFiClientSecure> client(new WiFiClientSecure);
  client->setInsecure();
  HTTPClient http;
  
  unsigned long utcTimestamp = now() - timeZoneOffset;
  String uuid = generateUUID(11);
  String date = getFormattedDate();
  String datetime_utc = getISO8601DateTime(utcTimestamp);

  showStatus("ENVIANDO", "REGISTRO...");

  if (http.begin(*client, webhookUrl)) {
    http.setTimeout(10000); // Timeout de 10 segundos para evitar resets por watchdog
    http.setUserAgent(DEVICE_NAME);
    http.addHeader("Content-Type", "application/json");

    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();

    JsonObject obj = array.createNestedObject();
    obj["uuid"] = uuid;
    obj["timestamp"] = utcTimestamp;
    obj["datetime_utc"] = datetime_utc;
    obj["date"] = date;
    obj["num_empleado"] = num_emp;
    obj["name"] = name;
    obj["branch"] = sucursal;
    obj["telegram_id"] = telegram_id;

    String jsonPayload;
    serializeJson(doc, jsonPayload);
    
    int httpResponseCode = http.POST(jsonPayload);
    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      showStatus("HOLA :)", name);
      http.end();
      blinkLed(5, 200); // Éxito - Parpadeo de 5 veces (mismo comportamiento que boot)
      return true;
    } else {
      showStatus("ERROR", "AL ENVIAR");
      http.end();
      signalTemporaryError();
      return false;
    }
  }
  signalTemporaryError();
  return false;
}

// ==========================================
// 🔊 BUZZER Y LEDS
// ==========================================

void signalTemporaryError() {
  alarmSound();
  digitalWrite(LED_PIN, HIGH);
  delay(5000);
  digitalWrite(LED_PIN, LOW);
}

void blinkLed(int times, int duration) {
  for (int i = 0; i < times; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(duration);
    digitalWrite(LED_PIN, LOW);
    if (i < times - 1) delay(duration);
  }
}

void morseDot() {
  digitalWrite(LED_PIN, HIGH);
  delay(250);
  digitalWrite(LED_PIN, LOW);
  delay(250);
}

void morseDash() {
  digitalWrite(LED_PIN, HIGH);
  delay(750);
  digitalWrite(LED_PIN, LOW);
  delay(250);
}

void sos() {
  while(true) { // Bucle infinito para S.O.S.
    // SOS con la luz: ... --- ... (3 segundos)
    morseDot(); morseDot(); morseDot();
    delay(500);
    morseDash(); morseDash(); morseDash();
    delay(500);
    morseDot(); morseDot(); morseDot();
    delay(3000); // 3 segundos de SOS
    delay(10000); // Esperar 10 segundos antes de repetir
  }
}

void beep(int times, int duration) {
  // Asegurar que el pin esté configurado (por si se llama antes del setup completo)
  static bool buzzerInitialized = false;
  if (!buzzerInitialized) {
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    buzzerInitialized = true;
  }
  
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration);
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(duration / 2);
  }
  
  // Asegurar que el buzzer quede silenciado después de cada beep
  digitalWrite(BUZZER_PIN, LOW);
  delay(10); // Pequeña pausa para estabilizar
}

void alarmSound() {
  beep(3, 150); // 3 beeps cortos y rápidos para el error
  silenceBuzzer(); // Asegurar que quede silenciado después del error
}

void silenceBuzzer() {
  // Función para asegurar que el buzzer esté completamente silenciado
  // Útil para evitar ruido o interferencia electromagnética
  // Asegurar que el pin esté configurado como OUTPUT y en LOW
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
}

// ==========================================
// 🖥️ PANTALLA Y UTILIDADES
// ==========================================
String generateUUID(byte length) {
  String randomString = "";
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  for (byte i = 0; i < length; i++) {
    randomString += charset[random(sizeof(charset) - 1)];
  }
  return randomString;
}

String getISO8601DateTime(time_t t) {
  char buff[21];
  sprintf(buff, "%04d-%02d-%02dT%02d:%02d:%02dZ",
    year(t), month(t), day(t), hour(t), minute(t), second(t));
  return String(buff);
}

String getFormattedDate() {
  String dateStr = String(year()) + "-";
  if (month() < 10) dateStr += "0"; dateStr += String(month());
  dateStr += "-";
  if (day() < 10) dateStr += "0"; dateStr += String(day());
  return dateStr;
}

String getFormattedTime() {
  String timeStr = "";
  int h = hour(); int m = minute(); String ampm = "AM";
  if (h >= 12) { ampm = "PM"; if (h > 12) h -= 12; }
  if (h == 0) h = 12;
  if (h < 10) timeStr += " "; timeStr += String(h); timeStr += ":";
  if (m < 10) timeStr += "0"; timeStr += String(m); timeStr += " " + ampm;
  return timeStr;
}

void showClockScreen() {
  display.clearDisplay();
  display.setTextSize(1); display.setCursor(0, 0); display.println(F("CHECADOR"));
  display.setTextSize(2); display.setCursor(10, 25); 
  if (timeStatus() != timeNotSet) { display.println(getFormattedTime()); }
  else { display.println("--:--"); }
  display.setTextSize(1); display.setCursor(0, 55); display.println(F("ACERQUE TARJETA..."));
  display.display();
}

void showStatus(String line1, String line2) {
  display.clearDisplay();
  display.setTextSize(2); display.setCursor(0, 10); display.println(line1);
  if(line2.length() > 8) display.setTextSize(1); else display.setTextSize(2);
  display.setCursor(0, 35); display.println(line2);
  display.display();
}

void scanI2C() {
  byte error, address;
  int nDevices = 0;
  Serial.println("Dispositivos I2C encontrados:");
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("Encontrado en 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    } else if (error == 4) {
      Serial.print("Error desconocido en 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0) Serial.println("Ningún dispositivo I2C encontrado");
  else Serial.println("Escaneo terminado");
}
