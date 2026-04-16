#!/usr/bin/env python3
# ============================================================
#  wissen_phi-3_voice.py  –  Expodroide mit STT + TTS
#  STT : faster-whisper (tiny)
#  TTS : piper-tts 1.4.1  (piper.PiperVoice.synthesize)
#  LLM : Ollama (phi-3.5 / gemma3:1b via HTTP)
#  Starten mit: python3.12 wissen_phi_voice.py geht auch mit 3 gkaub ich
#  FRAG MAL CHATTY WELCHE PAKETE VORINSTALIERT WERDEN SOLLEN AUF TERMINAL!!
# ============================================================

import requests
import json
import os
import io
import wave
import tempfile
import numpy as np
import sounddevice as sd
import scipy.io.wavfile as wavfile
from collections import deque
from faster_whisper import WhisperModel
from piper import PiperVoice

# ─────────────────────────────────────────────
#  KONFIGURATION
# ─────────────────────────────────────────────
MODELL           = "phi3.5"      #"phi3.5" oder "gemma3:1b"
MAX_MEMORY       = 3             #anzahl letzter gespräche im gedächtnis (je mehr, desto länger die Antwortzeit, aber besserer Kontext)
MAX_TOKENS       = 150           #die Antwortlaenge in Tokens (höher = längere Antworten, aber langsamere Antwortzeit!!)
TEMPERATUR       = 0.1          #0.0 = konservativ, 0.5 = ausgewogen, 1.0 = kreativ (je höher, desto kreativer, aber auch unverständlicher die Antworten)   
OLLAMA_URL       = "http://localhost:11434/api/generate"

WHISPER_MODEL    = "base"               #"tiny", "base", "small", "medium", "large-v2" (je größer, desto genauer, aber auch langsamer und ressourcenintensiver)
WHISPER_DEVICE   = "cpu"                #"cpu" oder "cuda" (wenn GPU mit CUDA-Unterstützung vorhanden)
WHISPER_COMPUTE  = "int8"               #"int8" (schnell, weniger genau), "float16" (langsamer, genauer) – je nach Modell und Hardware kann int8 manchmal zu schlechteren Ergebnissen führen, teste ggf. beide Modi

SAMPLE_RATE      = 16000
RECORD_SECONDS   = 15            # Maximale zuhörzeit (Sicherheit gegen Endlosschleife und noises)
SILENCE_THRESH   = 0.010         # höhere Schwelle = weniger Falsch-Stille unter diesem zählt als Stille
SILENCE_DURATION = 2.5           # 2.5s Stille nötig bevor Aufnahme stoppt


SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
PIPER_MODEL = os.path.join(SCRIPT_DIR, "de_DE-thorsten-medium.onnx")

# ─────────────────────────────────────────────
#  MODELLE LADEN
# ─────────────────────────────────────────────
print("⏳ Lade Whisper-Modell ...")
stt_model = WhisperModel(
    WHISPER_MODEL,
    device=WHISPER_DEVICE,
    compute_type=WHISPER_COMPUTE,
    download_root=os.path.join(SCRIPT_DIR, "whisper_cache")
)
print(f"✅ Whisper '{WHISPER_MODEL}' geladen.")

print("⏳ Lade Piper-Stimme ...")
if not os.path.exists(PIPER_MODEL):
    raise FileNotFoundError(f"Piper-Modell nicht gefunden: {PIPER_MODEL}")
tts_voice = PiperVoice.load(PIPER_MODEL)
print(f"✅ Piper-Stimme geladen: {PIPER_MODEL}")

# ─────────────────────────────────────────────
#  HILFSFUNKTION: Stille erkennen
# ─────────────────────────────────────────────
def _is_silent(chunk: np.ndarray, threshold: float = SILENCE_THRESH) -> bool:
    return float(np.sqrt(np.mean(chunk ** 2))) < threshold

# ─────────────────────────────────────────────
#  STT – Spracheingabe → Text
# ─────────────────────────────────────────────
def listen() -> str:
    print("🎙️  Bitte sprechen ...")
    chunk_size   = int(SAMPLE_RATE * 0.5)
    silent_limit = int(SILENCE_DURATION / 0.5)
    max_blocks   = int(RECORD_SECONDS / 0.5)

    audio_chunks = []
    silent_count = 0
    speaking     = False

    with sd.InputStream(
        samplerate=SAMPLE_RATE,
        channels=1,
        dtype="float32",
        blocksize=chunk_size
    ) as stream:
        for _ in range(max_blocks):
            chunk, _ = stream.read(chunk_size)
            chunk_1d = chunk[:, 0]
            if not _is_silent(chunk_1d):
                speaking     = True
                silent_count = 0
                audio_chunks.append(chunk_1d)
            elif speaking:
                silent_count += 1
                audio_chunks.append(chunk_1d)
                if silent_count >= silent_limit:
                    break

    if not audio_chunks:
        return ""

    audio_np = np.concatenate(audio_chunks, axis=0)

    with tempfile.NamedTemporaryFile(suffix=".wav", delete=False) as tmp:
        wavfile.write(tmp.name, SAMPLE_RATE, (audio_np * 32767).astype(np.int16))
        tmp_path = tmp.name

    try:
        segments, _ = stt_model.transcribe(
            tmp_path,
            language="de",
            beam_size=1,
            vad_filter=True
        )
        text = " ".join(seg.text for seg in segments).strip()
    finally:
        os.remove(tmp_path)

    print(f"👤 Erkannt: {text}")
    return text

# ─────────────────────────────────────────────
#  TTS – Text → Sprache (piper-tts 1.4.1)
#  synthesize_wav braucht ein wave.Wave_write Objekt
# ─────────────────────────────────────────────
def speak(text: str) -> None:
    if not text.strip():
        return

    print(f"🔊 Spreche: {text[:60]}{'...' if len(text) > 60 else ''}")

    # wave.Wave_write in BytesIO-Buffer schreiben
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wav_file:
        tts_voice.synthesize_wav(text, wav_file)

    # Buffer zurückspulen und lesen
    buf.seek(0)
    sample_rate, audio_data = wavfile.read(buf)
    audio_float = audio_data.astype(np.float32) / 32768.0

    sd.play(audio_float, samplerate=sample_rate)
    sd.wait()

# ─────────────────────────────────────────────
#  WISSENSBASIS LADEN
# ─────────────────────────────────────────────
def lade_wissen():
    json_pfad = os.path.join(SCRIPT_DIR, "wissen.json")
    try:
        with open(json_pfad, "r", encoding="utf-8") as file:
            return json.load(file)
    except FileNotFoundError:
        print("Fehler: wissen.json nicht gefunden!")
        return None

def finde_relevante_fakten(benutzer_eingabe, daten, max_fakten=4): #hier kann man auch die Anzahl der zurückgegebenen Fakten anpassen 
    eingabe_lower = benutzer_eingabe.lower()
    beste_treffer = []
    for eintrag in daten["fakten"]:
        score = 0
        for keyword in eintrag["keywords"]:
            if keyword in eingabe_lower:
                score += 1
            elif len(keyword) > 5 and keyword[:5] in eingabe_lower:
                score += 0.5
        if score > 0 and eintrag["antwort"].strip():
            beste_treffer.append((score, eintrag["antwort"]))
    beste_treffer.sort(key=lambda x: x[0], reverse=True)
    return (
        [antwort for _, antwort in beste_treffer[:max_fakten]],
        beste_treffer
    )

# ─────────────────────────────────────────────
#  LLM – Antwort generieren
# ─────────────────────────────────────────────
def frage_phi(benutzer_eingabe, fakten_liste, system_instructions, verlauf):
    memory_text = ""
    if verlauf:
        memory_text = "Bisheriger Gesprächsverlauf:\n"
        for eintrag in verlauf:
            memory_text += f"Besucher: {eintrag['frage']}\n"
            memory_text += f"Expodroide: {eintrag['antwort']}\n"
        memory_text += "\n"

    if fakten_liste:
        fakten_text    = "\n".join(f"Fakt {i+1}: {f}" for i, f in enumerate(fakten_liste))
        kontext_prompt = (
            f"{memory_text}{fakten_text}\n"
            f"Frage: {benutzer_eingabe}\n"
            "in maximal 2-3 Sätzen NUR basierend auf den Fakten. Erfinde nichts."
        )
    else:
        kontext_prompt = f"{memory_text}Frage: {benutzer_eingabe}"

    payload = {
        "model":   MODELL,
        "system":  system_instructions,
        "prompt":  kontext_prompt,
        "stream":  False,
        "options": {
            "temperature": TEMPERATUR,
            "num_predict": MAX_TOKENS,
            "stop":        ["Besucher:", "Frage des Nutzers:"]
        }
    }

    try:
        response = requests.post(OLLAMA_URL, json=payload, timeout=60)
        antwort  = response.json().get("response", "").strip()
        if not antwort:
            return "Das weiß ich leider nicht – frag einen Kollegen am Stand!"
        return antwort
    except Exception as e:
        return f"Verbindungsfehler: {str(e)}"

# ─────────────────────────────────────────────
#  HAUPTSCHLEIFE
# ─────────────────────────────────────────────
if __name__ == "__main__":
    wissensdatenbank = lade_wissen()
    if not wissensdatenbank:
        exit(1)

    verlauf = deque(maxlen=MAX_MEMORY)

    print("\n🤖 Expodroide bereit! Sprich mit mir (oder tippe 'ende').")
    speak("Hallo! Ich bin der Expodroide. Wie kann ich dir helfen?")

    while True:
        try:
            benutzer_eingabe = listen()
        except Exception as e:
            print(f"[STT-Fehler] {e} – Bitte Text eingeben:")
            benutzer_eingabe = input("Besucher: ").strip()

        if not benutzer_eingabe:
            continue
        if benutzer_eingabe.lower() in ("ende", "beenden", "tschüss"):
            speak("Bis bald!")
            print("Expodroide: Bis bald! 👋")
            break

        tokens_memory = sum(len(e["frage"]) + len(e["antwort"]) for e in verlauf) // 4
        tokens_frage  = len(benutzer_eingabe) // 4
        tokens_system = len(wissensdatenbank.get("systemprompt", "")) // 4
        pi_wartezeit  = round(MAX_TOKENS / 3.4, 1)
        print(f"[DEBUG] Memory:{len(verlauf)}/{MAX_MEMORY} | "
              f"Tokens≈{tokens_memory + tokens_frage + tokens_system} | "
              f"Wartezeit≈{pi_wartezeit}s")

        fakten, alle_treffer = finde_relevante_fakten(benutzer_eingabe, wissensdatenbank)
        antwort = frage_phi(
            benutzer_eingabe,
            fakten,
            wissensdatenbank.get("systemprompt", ""),
            verlauf
        )

        print(f"🤖 Expodroide: {antwort}")
        speak(antwort)

        verlauf.append({"frage": benutzer_eingabe, "antwort": antwort})
