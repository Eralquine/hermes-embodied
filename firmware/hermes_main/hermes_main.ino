/*
 * Hermes Embodied — Main (integración completa)
 * Combina cara OLED + WiFi → OmniRoute
 *
 * Copiar face.ino y wifi.ino en la misma carpeta que este archivo,
 * o compilarlos como un único sketch con múltiples tabs en Arduino IDE.
 *
 * Flujo completo:
 *   1. Inicia OLED (expresión THINKING mientras conecta)
 *   2. Conecta WiFi
 *   3. Espera mensaje por Serial
 *   4. Muestra THINKING en OLED mientras espera respuesta
 *   5. Actualiza expresión según emoción recibida
 *   6. [Futuro] Reproduce audio por bocina
 */

// Las constantes y funciones de face.ino y wifi.ino se compilan juntas.
// Este archivo solo define el setup() y loop() principales.

// ─── Forward declarations (definidas en face.ino) ────────────────────────────
extern void    setExpression(Expression expr);
extern void    drawFace(Expression expr, float blink);
extern void    updateBlink();
extern void    updatePupil();
extern float   blinkState;
extern Expression currentExpression;

// ─── Forward declarations (definidas en wifi.ino) ────────────────────────────
extern void            connectWifi();
extern String          sendMessage(String text);
extern HermesResponse  parseResponse(String json);
extern int             emotionToExpression(String emotion);

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Iniciar OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ERROR: OLED no encontrado");
    while (true);
  }

  // Pantalla de bienvenida
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("  Hermes");
  display.setCursor(10, 35);
  display.println("  iniciando...");
  display.display();
  delay(1000);

  setExpression(THINKING);

  // Conectar WiFi
  connectWifi();

  // Si conectó, cara neutral de bienvenida
  if (WiFi.status() == WL_CONNECTED) {
    setExpression(HAPPY);
    delay(800);
    setExpression(NEUTRAL);
  } else {
    setExpression(SAD);
  }

  randomSeed(analogRead(0));
  Serial.println("Listo. Escribe algo:");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  // Mantener cara animada
  updateBlink();
  updatePupil();
  drawFace(currentExpression, blinkState);

  // Reconectar si se pierde WiFi
  if (WiFi.status() != WL_CONNECTED) {
    setExpression(SAD);
    connectWifi();
    if (WiFi.status() == WL_CONNECTED) setExpression(NEUTRAL);
  }

  // Esperar input por Serial
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    Serial.printf("Tú: %s\n", input.c_str());

    // Cara de "pensando" mientras espera
    setExpression(THINKING);

    // Llamar al servidor
    String raw = sendMessage(input);
    HermesResponse res = parseResponse(raw);

    // Actualizar expresión
    setExpression((Expression)emotionToExpression(res.emotion));

    Serial.printf("Hermes [%s]: %s\n", res.emotion.c_str(), res.text.c_str());

    // Volver a neutral después de 3 segundos
    delay(3000);
    setExpression(NEUTRAL);
  }

  delay(40); // ~25 fps
}
