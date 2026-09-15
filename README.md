# Hermes Embodied 🤖

Un robot de escritorio con IA — basado en ESP32, pantalla OLED con cara animada, servos y voz.

Conectado a [OmniRoute](https://github.com/diegosouzapw/OmniRoute) para enrutar entre múltiples proveedores de LLM de forma gratuita.

---

## Hardware

| Componente | Descripción | Estado |
|---|---|---|
| ESP32 | Microcontrolador principal, WiFi integrado | 🛒 Comprado |
| OLED SSD1306 128x64 | Cara animada (ojos, expresiones) | 🛒 Comprado |
| Servo MG90S x4 | Movimiento de cabeza / reacciones | 🛒 Comprado |
| Cables Dupont H-H + M-H | Conexiones | 🛒 Comprado |
| INMP441 | Micrófono I2S omnidireccional | 🚚 En camino |
| PAM8403 (OKY3462-5) | Amplificador de audio Clase D 2x3W | 🚚 En camino |
| Bocina 8Ω 1W | Salida de audio | 🚚 En camino |

---

## Arquitectura

```
[INMP441 mic] → ESP32 → WiFi → Servidor (Whisper STT)
                                        ↓
                                  Hermes / LLM
                                  (via OmniRoute)
                                        ↓
                               gTTS genera audio
                                        ↓
              ESP32 ← respuesta + emoción ← servidor
                ↓                ↓
          OLED (cara)     PAM8403 → bocina
                ↓
           Servo (movimiento)
```

---

## Software

- `firmware/` — Código Arduino para ESP32
  - `face/` — Animaciones OLED (ojos, expresiones)
  - `servo/` — Control de servos
  - `audio/` — Micrófono I2S + reproducción
  - `wifi/` — Conexión y comunicación con el servidor
- `server/` — Scripts del servidor Linux
  - `stt.py` — Speech-to-text con Whisper
  - `tts.py` — Text-to-speech con gTTS
  - `bridge.py` — Puente ESP32 ↔ OmniRoute

---

## Estado del proyecto

- [x] Hardware seleccionado y en pedido
- [x] OmniRoute configurado con 6 providers (Groq, Gemini, Mistral, LLM7, OpenRouter, NVIDIA NIM)
- [ ] Firmware: cara animada en OLED
- [ ] Firmware: conexión WiFi → OmniRoute
- [ ] Firmware: servo reactivo
- [ ] Server: pipeline STT → LLM → TTS
- [ ] Integración completa

---

## Proveedores LLM (OmniRoute)

| Provider | Tokens gratis/mes |
|---|---|
| Groq | 14.4K req/día |
| Gemini | 60M tokens/mes |
| Mistral | 1B tokens/mes |
| LLM7 | 150M tokens/mes |
| OpenRouter | 50 req/día |
| NVIDIA NIM | ~1,000 calls/modelo |
