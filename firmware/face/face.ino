/*
 * Hermes Embodied — Cara OLED
 * Pantalla: SSD1306 128x64 I2C
 * Pines I2C: SDA = GPIO21, SCL = GPIO22 (default ESP32)
 *
 * Librerías necesarias (instalar en Arduino IDE):
 *   - Adafruit SSD1306
 *   - Adafruit GFX Library
 *
 * Expresiones disponibles:
 *   NEUTRAL, HAPPY, SAD, ANGRY, SURPRISED, THINKING, BLINK
 */

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1   // sin pin reset
#define OLED_ADDR     0x3C // dirección I2C (0x3C o 0x3D)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ─── Tipos de expresión ───────────────────────────────────────────────────────
enum Expression {
  NEUTRAL,
  HAPPY,
  SAD,
  ANGRY,
  SURPRISED,
  THINKING,
  BLINK
};

Expression currentExpression = NEUTRAL;
Expression nextExpression     = NEUTRAL;

// ─── Parámetros de los ojos ───────────────────────────────────────────────────
// Ojo izquierdo: centro en (32, 32), ojo derecho: (96, 32)
#define EYE_L_X  32
#define EYE_R_X  96
#define EYE_Y    32

// Tamaños por expresión {ancho, alto}
struct EyeSize { int w; int h; };

EyeSize eyeSizes[] = {
  {20, 24},  // NEUTRAL
  {22, 20},  // HAPPY  — ojos un poco achatados (sonrisa)
  {18, 22},  // SAD    — ojos normales, cejas caídas
  {20, 18},  // ANGRY  — ojos entrecerrados
  {24, 28},  // SURPRISED — ojos muy abiertos
  {20, 24},  // THINKING
  { 0,  0},  // BLINK  — cerrados (se maneja aparte)
};

// Apertura del parpadeo (0.0 = cerrado, 1.0 = abierto)
float blinkState = 1.0;
bool  isBlinking = false;
unsigned long lastBlinkTime = 0;
unsigned long blinkInterval = 3000; // ms entre parpadeos automáticos

// Animación de transición
bool  transitioning = false;
float transProgress = 0.0; // 0.0 → 1.0
EyeSize fromSize, toSize;

// Pupila (movimiento sutil)
int  pupilOffsetX = 0;
int  pupilOffsetY = 0;
long lastPupilMove = 0;

// ─── Dibujar un ojo ──────────────────────────────────────────────────────────
void drawEye(int cx, int cy, int w, int h, int pupilDx, int pupilDy, Expression expr) {
  if (h <= 0) return; // ojo cerrado (blink)

  // Marco exterior del ojo (rectángulo redondeado)
  display.fillRoundRect(cx - w/2, cy - h/2, w, h, h/3, SSD1306_WHITE);

  // Pupila (círculo negro dentro)
  int px = cx + pupilDx;
  int py = cy + pupilDy;
  int pr = min(w, h) / 4 + 1;
  display.fillCircle(px, py, pr, SSD1306_BLACK);

  // Brillo (punto blanco en la pupila)
  display.drawPixel(px - pr/2, py - pr/2, SSD1306_WHITE);

  // Cejas según expresión
  int browY = cy - h/2 - 4;
  int browW = w + 4;
  switch (expr) {
    case ANGRY:
      // Cejas inclinadas hacia adentro
      if (cx < 64) { // ojo izquierdo
        display.drawLine(cx - browW/2, browY - 3, cx + browW/2, browY + 1, SSD1306_WHITE);
      } else {       // ojo derecho
        display.drawLine(cx - browW/2, browY + 1, cx + browW/2, browY - 3, SSD1306_WHITE);
      }
      break;
    case SAD:
      // Cejas inclinadas hacia afuera (triste)
      if (cx < 64) {
        display.drawLine(cx - browW/2, browY + 1, cx + browW/2, browY - 3, SSD1306_WHITE);
      } else {
        display.drawLine(cx - browW/2, browY - 3, cx + browW/2, browY + 1, SSD1306_WHITE);
      }
      break;
    case SURPRISED:
      // Cejas muy arriba
      display.drawLine(cx - browW/2, browY - 5, cx + browW/2, browY - 5, SSD1306_WHITE);
      break;
    case HAPPY:
      // Sin cejas (cara feliz)
      break;
    case THINKING:
      // Una ceja levantada (la derecha)
      if (cx > 64) {
        display.drawLine(cx - browW/2, browY - 4, cx + browW/2, browY, SSD1306_WHITE);
      } else {
        display.drawLine(cx - browW/2, browY, cx + browW/2, browY, SSD1306_WHITE);
      }
      break;
    default:
      // NEUTRAL: cejas rectas
      display.drawLine(cx - browW/2, browY, cx + browW/2, browY, SSD1306_WHITE);
      break;
  }

  // Boca según expresión (solo se dibuja desde el ojo izquierdo para no duplicar)
  if (cx < 64) {
    int mouthY = 54;
    switch (expr) {
      case HAPPY:
        display.drawCircleHelper(64, mouthY - 6, 10, 0b0110, SSD1306_WHITE);
        break;
      case SAD:
        display.drawCircleHelper(64, mouthY + 2, 8, 0b1001, SSD1306_WHITE);
        break;
      case SURPRISED:
        display.drawCircle(64, mouthY, 5, SSD1306_WHITE);
        break;
      case ANGRY:
        display.drawLine(54, mouthY, 74, mouthY - 2, SSD1306_WHITE);
        break;
      case THINKING:
        display.drawLine(58, mouthY, 70, mouthY, SSD1306_WHITE);
        display.drawPixel(72, mouthY - 2, SSD1306_WHITE);
        display.drawPixel(72, mouthY,     SSD1306_WHITE);
        display.drawPixel(72, mouthY + 2, SSD1306_WHITE);
        break;
      default: // NEUTRAL
        display.drawLine(56, mouthY, 72, mouthY, SSD1306_WHITE);
        break;
    }
  }
}

// ─── Dibujar frame completo ───────────────────────────────────────────────────
void drawFace(Expression expr, float blink) {
  display.clearDisplay();

  EyeSize sz = eyeSizes[expr];

  // Aplicar parpadeo (reducir altura)
  int lh = (int)(sz.h * blink);
  int rh = lh;

  // Ligero movimiento de pupila
  drawEye(EYE_L_X, EYE_Y, sz.w, lh, pupilOffsetX, pupilOffsetY, expr);
  drawEye(EYE_R_X, EYE_Y, sz.w, rh, pupilOffsetX, pupilOffsetY, expr);

  display.display();
}

// ─── Parpadeo automático ──────────────────────────────────────────────────────
void updateBlink() {
  unsigned long now = millis();

  if (!isBlinking && (now - lastBlinkTime > blinkInterval)) {
    isBlinking  = true;
    blinkState  = 1.0;
    lastBlinkTime = now;
    // Intervalo aleatorio 2–5 segundos
    blinkInterval = 2000 + random(3000);
  }

  if (isBlinking) {
    blinkState -= 0.15;
    if (blinkState <= 0.0) {
      blinkState  = 0.0;
      isBlinking  = false;
    }
  } else if (blinkState < 1.0) {
    blinkState += 0.2;
    if (blinkState > 1.0) blinkState = 1.0;
  }
}

// ─── Movimiento sutil de pupila ───────────────────────────────────────────────
void updatePupil() {
  unsigned long now = millis();
  if (now - lastPupilMove > 2000 + random(3000)) {
    pupilOffsetX = random(-3, 4);
    pupilOffsetY = random(-2, 3);
    lastPupilMove = now;
  }
}

// ─── API pública: cambiar expresión ──────────────────────────────────────────
void setExpression(Expression expr) {
  currentExpression = expr;
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("ERROR: OLED SSD1306 no encontrado");
    while (true); // detener si no hay pantalla
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 28);
  display.print("Hermes iniciando...");
  display.display();
  delay(1500);

  randomSeed(analogRead(0));
  Serial.println("Hermes OLED listo");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
  updateBlink();
  updatePupil();
  drawFace(currentExpression, blinkState);

  // Demo de expresiones por serial
  if (Serial.available()) {
    char cmd = Serial.read();
    switch (cmd) {
      case 'n': setExpression(NEUTRAL);   Serial.println("NEUTRAL");   break;
      case 'h': setExpression(HAPPY);     Serial.println("HAPPY");     break;
      case 's': setExpression(SAD);       Serial.println("SAD");       break;
      case 'a': setExpression(ANGRY);     Serial.println("ANGRY");     break;
      case 'u': setExpression(SURPRISED); Serial.println("SURPRISED"); break;
      case 't': setExpression(THINKING);  Serial.println("THINKING");  break;
    }
  }

  delay(40); // ~25 fps
}
