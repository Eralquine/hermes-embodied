/*
 * Hermes Embodied — WiFi + OmniRoute
 * Conecta el ESP32 al servidor y manda/recibe mensajes del LLM.
 *
 * Dependencias (instalar en Arduino IDE):
 *   - ArduinoJson (v6)
 *   - WiFi (incluida en ESP32)
 *   - HTTPClient (incluida en ESP32)
 *
 * Flujo:
 *   1. ESP32 se conecta al WiFi
 *   2. Manda texto al servidor Python (bridge.py)
 *   3. bridge.py lo envía a OmniRoute y devuelve la respuesta + emoción
 *   4. ESP32 actualiza la cara y reproduce el audio
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ─── Configuración — EDITAR ESTO ─────────────────────────────────────────────
const char* WIFI_SSID     = "TU_RED_WIFI";
const char* WIFI_PASSWORD = "TU_PASSWORD_WIFI";

// IP de tu servidor Linux (donde corre OmniRoute)
// Cámbiala por la IP de tu máquina: ejecuta `hostname -I` en el servidor
const char* SERVER_IP     = "192.168.1.100";
const int   SERVER_PORT   = 5000;
// ─────────────────────────────────────────────────────────────────────────────

String serverURL;
bool wifiConnected = false;

// ─── Conectar WiFi ────────────────────────────────────────────────────────────
void connectWifi() {
  Serial.printf("Conectando a %s", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("\nWiFi OK — IP: %s\n", WiFi.localIP().toString().c_str());
    serverURL = String("http://") + SERVER_IP + ":" + SERVER_PORT + "/chat";
  } else {
    Serial.println("\nERROR: No se pudo conectar al WiFi");
  }
}

// ─── Enviar mensaje al servidor ───────────────────────────────────────────────
// Devuelve un JSON con {text, emotion}
// emotion puede ser: neutral, happy, sad, angry, surprised, thinking
String sendMessage(String userText) {
  if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
    return "{\"text\":\"Sin conexión\",\"emotion\":\"sad\"}";
  }

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(10000); // 10 segundos máximo

  // Construir body
  StaticJsonDocument<512> reqDoc;
  reqDoc["message"] = userText;
  String reqBody;
  serializeJson(reqDoc, reqBody);

  int httpCode = http.POST(reqBody);

  if (httpCode == 200) {
    String response = http.getString();
    http.end();
    return response;
  } else {
    Serial.printf("Error HTTP: %d\n", httpCode);
    http.end();
    return "{\"text\":\"Error de conexión\",\"emotion\":\"sad\"}";
  }
}

// ─── Parsear respuesta del servidor ──────────────────────────────────────────
struct HermesResponse {
  String text;
  String emotion;
  String audioUrl; // URL del archivo de audio TTS (opcional)
};

HermesResponse parseResponse(String json) {
  HermesResponse res;
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, json);

  if (err) {
    res.text     = "Error al parsear respuesta";
    res.emotion  = "neutral";
    return res;
  }

  res.text     = doc["text"].as<String>();
  res.emotion  = doc["emotion"].as<String>();
  res.audioUrl = doc["audio_url"].as<String>();
  return res;
}

// ─── Mapear emoción a expresión OLED ─────────────────────────────────────────
// (se incluye en face.ino también, aquí como referencia)
int emotionToExpression(String emotion) {
  if (emotion == "happy")     return 1; // HAPPY
  if (emotion == "sad")       return 2; // SAD
  if (emotion == "angry")     return 3; // ANGRY
  if (emotion == "surprised") return 4; // SURPRISED
  if (emotion == "thinking")  return 5; // THINKING
  return 0;                             // NEUTRAL
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  connectWifi();

  // Test de conexión al servidor
  if (wifiConnected) {
    Serial.println("Probando conexión al servidor...");
    String res = sendMessage("ping");
    Serial.printf("Respuesta: %s\n", res.c_str());
  }
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  // Reconectar si se pierde WiFi
  if (WiFi.status() != WL_CONNECTED) {
    wifiConnected = false;
    Serial.println("WiFi perdido, reconectando...");
    connectWifi();
  }

  // Leer mensaje por serial (para pruebas manuales)
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() > 0) {
      Serial.printf("Enviando: %s\n", input.c_str());
      String raw = sendMessage(input);
      HermesResponse res = parseResponse(raw);
      Serial.printf("Hermes [%s]: %s\n", res.emotion.c_str(), res.text.c_str());
    }
  }

  delay(100);
}
