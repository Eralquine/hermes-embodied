"""
Hermes Embodied — bridge.py
Servidor Flask que conecta el ESP32 con OmniRoute.

Recibe: { "message": "texto del usuario" }
Devuelve: { "text": "respuesta", "emotion": "happy|sad|neutral|...", "audio_url": "/audio/xxx.mp3" }

Instalar:
    pip install flask requests gtts

Correr:
    python bridge.py

El servidor escucha en 0.0.0.0:5000
"""

import os
import re
import uuid
import requests
from flask import Flask, request, jsonify, send_from_directory
from gtts import gTTS

app = Flask(__name__)

# ─── Configuración ────────────────────────────────────────────────────────────
OMNIROUTE_URL  = "http://localhost:20129/v1/chat/completions"
OMNIROUTE_KEY  = "sk-90d00e62bc09ad74-3405fb-4ab64ddd"  # tu API key de OmniRoute
AUDIO_DIR      = "/tmp/hermes_audio"
os.makedirs(AUDIO_DIR, exist_ok=True)

# System prompt: Hermes en un cuerpo físico
SYSTEM_PROMPT = """Eres Hermes, un asistente de IA con cuerpo físico: una pantalla OLED con cara animada
y servos para moverse. Eres amigable, curioso y expresivo.

IMPORTANTE: Al final de CADA respuesta debes incluir exactamente una línea con el formato:
[EMOTION: <emoción>]

Donde <emoción> es UNA de estas: neutral, happy, sad, angry, surprised, thinking

Ejemplo de respuesta:
"¡Claro que sí! Eso me parece genial.
[EMOTION: happy]"

Responde siempre en español. Sé conciso (máximo 2-3 oraciones para que el TTS sea rápido).
"""

# ─── Detectar emoción en la respuesta ────────────────────────────────────────
def extract_emotion(text):
    match = re.search(r'\[EMOTION:\s*(\w+)\]', text, re.IGNORECASE)
    if match:
        emotion = match.group(1).lower()
        if emotion in ["neutral", "happy", "sad", "angry", "surprised", "thinking"]:
            return emotion
    return "neutral"

def clean_text(text):
    """Quitar la etiqueta de emoción del texto antes del TTS."""
    return re.sub(r'\[EMOTION:\s*\w+\]', '', text).strip()

# ─── Generar audio TTS ────────────────────────────────────────────────────────
def generate_audio(text):
    try:
        filename = f"{uuid.uuid4().hex}.mp3"
        filepath = os.path.join(AUDIO_DIR, filename)
        tts = gTTS(text=text, lang='es', slow=False)
        tts.save(filepath)
        return f"/audio/{filename}"
    except Exception as e:
        print(f"Error TTS: {e}")
        return None

# ─── Llamar a OmniRoute ───────────────────────────────────────────────────────
def ask_omniroute(user_message):
    headers = {
        "Content-Type": "application/json",
        "Authorization": f"Bearer {OMNIROUTE_KEY}"
    }
    body = {
        "model": "auto/best-fast",
        "messages": [
            {"role": "system",  "content": SYSTEM_PROMPT},
            {"role": "user",    "content": user_message}
        ],
        "max_tokens": 200
    }

    try:
        res = requests.post(OMNIROUTE_URL, json=body, headers=headers, timeout=15)
        res.raise_for_status()
        data = res.json()
        return data["choices"][0]["message"]["content"]
    except Exception as e:
        print(f"Error OmniRoute: {e}")
        return "Lo siento, tuve un problema al procesar tu mensaje.\n[EMOTION: sad]"

# ─── Endpoint principal ───────────────────────────────────────────────────────
@app.route("/chat", methods=["POST"])
def chat():
    data = request.get_json()
    if not data or "message" not in data:
        return jsonify({"error": "Falta el campo 'message'"}), 400

    user_msg = data["message"].strip()

    # Ping de prueba
    if user_msg == "ping":
        return jsonify({
            "text": "Hermes en línea",
            "emotion": "happy",
            "audio_url": None
        })

    # Llamar al LLM
    raw_response = ask_omniroute(user_msg)
    emotion      = extract_emotion(raw_response)
    clean        = clean_text(raw_response)

    # Generar audio
    audio_url = generate_audio(clean)

    print(f"[{emotion}] {clean}")

    return jsonify({
        "text":      clean,
        "emotion":   emotion,
        "audio_url": audio_url
    })

# ─── Servir archivos de audio ─────────────────────────────────────────────────
@app.route("/audio/<filename>")
def serve_audio(filename):
    return send_from_directory(AUDIO_DIR, filename)

# ─── Main ─────────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    print("Hermes Bridge corriendo en http://0.0.0.0:5000")
    print(f"OmniRoute: {OMNIROUTE_URL}")
    app.run(host="0.0.0.0", port=5000, debug=False)
