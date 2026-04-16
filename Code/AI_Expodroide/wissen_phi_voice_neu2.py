#!/usr/bin/env python3
# ============================================================
#  wissen_phi-3_voice.py  –  Expodroide mit STT + TTS + Filler-System
#  STT : faster-whisper
#  TTS : piper-tts 1.4.1
#  LLM : Ollama (phi3.5 / gemma3:1b via HTTP)
#  NEU : Filler-Sätze + DHBW-Fakten/Witze während LLM rechnet,
#        LLM läuft parallel in einem Thread, Filler werden beim
#        Start einmal vor-synthetisiert (Cache im RAM) → robust
#        und ressourcenschonend für Raspberry Pi auf Messen.
# ============================================================

import requests
import json
import os
import io
import wave
import tempfile
import random
import threading
import queue
import time
import numpy as np
import sounddevice as sd
import scipy.io.wavfile as wavfile
from collections import deque
from faster_whisper import WhisperModel
from piper import PiperVoice

# ─────────────────────────────────────────────
#  KONFIGURATION
# ─────────────────────────────────────────────
MODELL           = "phi3.5"
MAX_MEMORY       = 3
MAX_TOKENS       = 150
TEMPERATUR       = 0.1
OLLAMA_URL       = "http://localhost:11434/api/generate"
OLLAMA_TIMEOUT   = 90            # Sicherheitsnetz falls LLM hängt

WHISPER_MODEL    = "base"
WHISPER_DEVICE   = "cpu"
WHISPER_COMPUTE  = "int8"

SAMPLE_RATE      = 16000
RECORD_SECONDS   = 15
SILENCE_THRESH   = 0.010
SILENCE_DURATION = 2.5

# Filler-Verhalten
FILLER_WAIT_BEFORE_START = 1.2   # nicht mehr relevant (Filler spielt immer),
                                 # aber behalten für eventuelle spätere Nutzung
USE_FACT_AFTER_FILLER    = True  # True = nach dem Filler folgt immer mindestens ein Fakt
PAUSE_ZWISCHEN_FAKTEN    = 2   # Sekunden Pause zwischen zwei Fakten (Atempause)
PAUSE_VOR_ANTWORT        = 0.9   # Sekunden Pause zwischen Überleitung und KI-Antwort

SCRIPT_DIR  = os.path.dirname(os.path.abspath(__file__))
PIPER_MODEL = os.path.join(SCRIPT_DIR, "de_DE-thorsten-medium.onnx")

# ─────────────────────────────────────────────
#  FILLER-INHALTE
#  Entschuldigungs-/Überleitungs-Sätze, die den User informieren,
#  dass der Roboter gleich etwas Interessantes erzählen wird.
#  Die Überleitung am Ende jedes Satzes ist variiert, damit es
#  nicht monoton wirkt.
# ─────────────────────────────────────────────
FILLER_SAETZE = [
    "Ich überlege kurz. Währenddessen erzähle ich dir gerne etwas Interessantes.",
    "Das ist eine gute Frage, ich brauche einen Moment. In der Zwischenzeit, ein kleiner Fakt für dich.",
    "Hmm, lass mich kurz nachdenken. Damit dir nicht langweilig wird, hier etwas Spannendes.",
    "Bitte hab einen Augenblick Geduld. Soll ich dir solange etwas Lustiges erzählen?",
    "Ich sortiere gerade meine Gedanken. Darf ich dir dazu etwas erzählen?",
    "Gib mir bitte einen kurzen Moment. In der Zwischenzeit habe ich etwas für dich.",
    "Ich prozessiere deine Frage. Zur Überbrückung, hier eine kleine Anekdote.",
    "Einen Moment bitte, ich denke nach. Damit du nicht wartest, ein Fakt vorab.",
    "Das muss ich kurz durchdenken. Inzwischen teile ich gerne etwas mit dir.",
    "Moment, das überlege ich mir gut. Hier nebenbei etwas Interessantes für dich.",
]

DHBW_FAKTEN_UND_WITZE = [
    # DHBW-Fakten
    "Die DHBW ist die erste duale Hochschule Deutschlands und wurde 2009 offiziell als Hochschule anerkannt.",
    "Die DHBW hat neun Standorte in Baden-Württemberg, darunter Ravensburg, Stuttgart und Mannheim.",
    "An der DHBW studieren über 34.000 Studierende gleichzeitig in Theorie- und Praxisphasen.",
    "Der Campus Friedrichshafen gehört zur DHBW Ravensburg und liegt direkt am Bodensee.",
    "Die DHBW kooperiert mit über 9000 Partnerunternehmen in ganz Deutschland.",
    "Das duale Prinzip der DHBW geht auf die Berufsakademie Baden-Württemberg von 1974 zurück.",
    "Ein duales Studium an der DHBW dauert in der Regel drei Jahre und endet mit einem Bachelor.",
    "Die DHBW bietet über 100 Studienrichtungen in den Bereichen Technik, Wirtschaft und Sozialwesen an.",
    "Viele DHBW-Studierende übernehmen nach dem Abschluss direkt eine Stelle in ihrem Partnerunternehmen.",
    "Die DHBW Ravensburg ist besonders bekannt für ihre Studiengänge im Bereich Maschinenbau und Mechatronik.",
    # Technik- und Ingenieurs-Witze (harmlos, messetauglich)
    "Warum mögen Ingenieure keine Natur? Zu viele Bugs!",
    "Es gibt zehn Arten von Menschen. Die, die Binär verstehen, und die, die es nicht verstehen.",
    "Warum programmiert kein Informatiker im Wald? Zu viele logs.",
    "Ein Roboter geht zum Arzt und sagt, ich habe einen Virus. Der Arzt sagt, da kann ich nichts machen, ich bin kein Techniker.",
    "Wie viele Ingenieure braucht man, um eine Glühbirne zu wechseln? Keinen, das ist ein Hardware-Problem.",
    "Warum war der Computer kalt? Er hatte sein Windows offen gelassen.",
    "Was sagt ein Regler zum anderen? Bleib stabil, mein Freund.",
    "Warum lieben Mechatroniker den Regen? Endlich Feedback von oben.",
    "Ein Algorithmus betritt eine Bar. Der Barkeeper fragt, was möchten Sie? Der Algorithmus antwortet, das kommt darauf an.",
    "Warum hat der Roboter den Job bekommen? Er hatte die besten Referenzen, alle im ROM gespeichert.",
]

# Überleitungs-Sätze: werden gesprochen, sobald die KI-Antwort bereit ist,
# BEVOR die eigentliche Antwort vorgelesen wird. Signalisiert dem User klar,
# dass jetzt die echte Antwort kommt und nicht ein weiterer Fakt.
UEBERLEITUNGS_SAETZE = [
    "Okay, ich habe deine Antwort.",
    "Gut, jetzt kann ich dir antworten.",
    "Perfekt, hier kommt meine Antwort.",
    "Alles klar, ich weiß es jetzt.",
    "So, jetzt habe ich es.",
    "Fertig! Hier ist meine Antwort.",
    "Okay, kommen wir zurück zu deiner Frage.",
    "Gut, ich habe nachgedacht. Hör zu.",
]

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
        try:
            os.remove(tmp_path)
        except OSError:
            pass

    print(f"👤 Erkannt: {text}")
    return text

# ─────────────────────────────────────────────
#  TTS-Kern: Text → Roboter-Audio (NumPy-Array)
#  Trennung zwischen "Synthese" und "Abspielen" erlaubt Caching
#  der Filler-Sätze im RAM.
# ─────────────────────────────────────────────
def synthesize_robot_audio(text: str):
    """Erzeugt (sample_rate, audio_float32) mit Roboter-Effekt.
    Gibt (None, None) zurück, wenn Text leer ist."""
    if not text or not text.strip():
        return None, None

    # Piper → WAV in Buffer
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wav_file:
        tts_voice.synthesize_wav(text, wav_file)
    buf.seek(0)
    sample_rate, audio_data = wavfile.read(buf)
    audio = audio_data.astype(np.float32) / 32768.0

    # ── ROBOT-EFFEKT (3 Schichten) ────────────────────────────
    t = np.arange(len(audio)) / sample_rate

    # 1) Bit-Crusher → digitaler, harter 8-Bit-Sound
    bits = 8
    audio = np.round(audio * (2**bits)) / (2**bits)

    # 2) Ring-Modulation bei 120 Hz → klassischer Dalek/Roboter-Klang
    carrier = np.sin(2 * np.pi * 120 * t)
    audio = audio * carrier

    # 3) Tanh-Sättigung → fügt Oberwellen & Schärfe hinzu
    audio = np.tanh(audio * 2.5) / np.tanh(np.float32(2.5))

    # Normalisieren
    peak = np.max(np.abs(audio))
    if peak > 0:
        audio = (audio / peak * 0.85).astype(np.float32)

    return sample_rate, audio

def play_audio(sample_rate, audio):
    """Spielt vor-synthetisiertes Audio ab. Fehler werden abgefangen."""
    if audio is None or sample_rate is None:
        return
    try:
        sd.play(audio, samplerate=sample_rate)
        sd.wait()
    except Exception as e:
        print(f"[Audio-Fehler] {e}")

def speak(text: str) -> None:
    """Synthese + sofortiges Abspielen (für die eigentliche KI-Antwort)."""
    if not text or not text.strip():
        return
    print(f"🔊 Spreche: {text[:60]}{'...' if len(text) > 60 else ''}")
    try:
        sr, audio = synthesize_robot_audio(text)
        play_audio(sr, audio)
    except Exception as e:
        print(f"[TTS-Fehler] {e}")

# ─────────────────────────────────────────────
#  FILLER-CACHE
#  Beim Start werden alle Filler und Fakten EINMAL synthetisiert
#  und im RAM gehalten. So ist das Abspielen später instant und
#  die CPU kann sich während des Dialogs auf Ollama konzentrieren.
# ─────────────────────────────────────────────
class FillerCache:
    def __init__(self):
        self.filler_audios       = []   # (sr, audio) für "Ich überlege kurz..."
        self.fakt_audios         = []   # (sr, audio) für DHBW-Fakten/Witze
        self.ueberleitungs_audios = []  # (sr, audio) für "Okay, hier ist die Antwort"

    def build(self):
        print("⏳ Synthetisiere Filler-Sätze (einmalig, bitte warten) ...")
        for i, satz in enumerate(FILLER_SAETZE, 1):
            try:
                sr, audio = synthesize_robot_audio(satz)
                if audio is not None:
                    self.filler_audios.append((sr, audio))
            except Exception as e:
                print(f"[Filler-Synth-Fehler bei #{i}] {e}")
            print(f"   Filler {i}/{len(FILLER_SAETZE)} fertig.")

        print("⏳ Synthetisiere DHBW-Fakten und Witze ...")
        for i, fakt in enumerate(DHBW_FAKTEN_UND_WITZE, 1):
            try:
                sr, audio = synthesize_robot_audio(fakt)
                if audio is not None:
                    self.fakt_audios.append((sr, audio))
            except Exception as e:
                print(f"[Fakt-Synth-Fehler bei #{i}] {e}")
            print(f"   Fakt {i}/{len(DHBW_FAKTEN_UND_WITZE)} fertig.")

        print("⏳ Synthetisiere Überleitungs-Sätze ...")
        for i, ueb in enumerate(UEBERLEITUNGS_SAETZE, 1):
            try:
                sr, audio = synthesize_robot_audio(ueb)
                if audio is not None:
                    self.ueberleitungs_audios.append((sr, audio))
            except Exception as e:
                print(f"[Überleitungs-Synth-Fehler bei #{i}] {e}")
            print(f"   Überleitung {i}/{len(UEBERLEITUNGS_SAETZE)} fertig.")

        print(f"✅ Filler-Cache bereit: {len(self.filler_audios)} Filler, "
              f"{len(self.fakt_audios)} Fakten/Witze, "
              f"{len(self.ueberleitungs_audios)} Überleitungen.")

    def play_random_filler(self):
        if not self.filler_audios:
            return None
        idx = random.randrange(len(self.filler_audios))
        sr, audio = self.filler_audios[idx]
        play_audio(sr, audio)
        return idx

    def play_random_fakt(self, exclude: set = None):
        """Spielt einen zufälligen Fakt, möglichst nicht aus 'exclude'.
        Gibt den Index des gespielten Fakts zurück (oder None wenn leer/alle ausgeschlossen)."""
        if not self.fakt_audios:
            return None
        exclude = exclude or set()
        verfuegbar = [i for i in range(len(self.fakt_audios)) if i not in exclude]
        if not verfuegbar:
            return None
        idx = random.choice(verfuegbar)
        sr, audio = self.fakt_audios[idx]
        play_audio(sr, audio)
        return idx

    def play_random_ueberleitung(self):
        """Spielt eine zufällige Überleitung ('Okay, hier ist deine Antwort').
        Wenn keine Überleitung gecached ist, passiert nichts – die Antwort folgt direkt."""
        if not self.ueberleitungs_audios:
            return None
        idx = random.randrange(len(self.ueberleitungs_audios))
        sr, audio = self.ueberleitungs_audios[idx]
        play_audio(sr, audio)
        return idx

filler_cache = FillerCache()

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

def finde_relevante_fakten(benutzer_eingabe, daten, max_fakten=4):
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
#  LLM – Antwort generieren (blockierend; wird im Thread aufgerufen)
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
        response = requests.post(OLLAMA_URL, json=payload, timeout=OLLAMA_TIMEOUT)
        antwort  = response.json().get("response", "").strip()
        if not antwort:
            return "Das weiß ich leider nicht – frag einen Kollegen am Stand!"
        return antwort
    except Exception as e:
        return f"Verbindungsfehler: {str(e)}"

def frage_phi_threaded(args, result_queue: queue.Queue):
    """Wrapper: ruft frage_phi auf und legt Ergebnis in Queue.
    Fängt ALLE Exceptions ab, damit der Haupt-Thread nie hängt."""
    try:
        antwort = frage_phi(*args)
        result_queue.put(("ok", antwort))
    except Exception as e:
        result_queue.put(("error", f"Interner Fehler: {e}"))

# ─────────────────────────────────────────────
#  DIALOG-TURN mit Filler-System
#
#  Ablauf:
#  1) LLM-Anfrage in Thread starten
#  2) Kurz warten (FILLER_WAIT_BEFORE_START). Ist die Antwort in
#     dieser Zeit schon da → direkt sprechen, kein Filler.
#  3) Sonst: Filler abspielen (+ optional DHBW-Fakt).
#  4) Dann auf LLM-Antwort warten (bis OLLAMA_TIMEOUT).
#  5) KI-Antwort sprechen.
#
#  Robustheit:
#  - Filler-Audio kommt aus Cache → kein CPU-Spike
#  - LLM läuft im Thread → Filler blockiert nie die Antwort
#  - Wenn LLM-Thread crasht, gibt es eine Fallback-Nachricht
#  - KI-Antwort wird IMMER gesprochen, egal wie langsam/schnell
# ─────────────────────────────────────────────
def dialog_turn(benutzer_eingabe, wissensdatenbank, verlauf):
    """
    Ablauf pro User-Frage:
      1) LLM-Anfrage startet sofort in Hintergrund-Thread.
      2) Filler-Satz wird IMMER abgespielt (Pi ist nie schnell genug).
      3) Mindestens EIN Fakt/Witz wird IMMER abgespielt.
      4) Danach in Schleife: Wenn Antwort da → Überleitung + Antwort.
         Wenn nicht → kurze Pause + nächster Fakt → erneut prüfen.
      5) Sicherheitsnetz: nach OLLAMA_TIMEOUT wird abgebrochen.

    Robustheit:
      - Laufende Audio-Wiedergabe wird NIE unterbrochen (sd.wait blockiert).
        Dadurch hört der User immer jeden Satz vollständig, es gibt nie
        abgeschnittene Wörter oder überlappende Stimmen.
      - Jeder Audio-Schritt ist in try/except, damit ein Einzelfehler
        den Turn nicht tötet.
      - Die KI-Antwort wird IMMER gesprochen, sobald sie da ist.
        Wenn die Antwort während eines Fakts fertig wird, wird der
        Fakt zu Ende gespielt, dann kommt die Überleitung, dann die Antwort.
      - Niemals kommt ein "zusätzlicher" Fakt nach dem Zeitpunkt, wo
        die Antwort bereits da ist – wir prüfen VOR jedem neuen Fakt.
    """
    fakten, _ = finde_relevante_fakten(benutzer_eingabe, wissensdatenbank)
    system_instructions = wissensdatenbank.get("systemprompt", "")

    # LLM-Anfrage sofort starten
    result_queue: queue.Queue = queue.Queue()
    llm_thread = threading.Thread(
        target=frage_phi_threaded,
        args=((benutzer_eingabe, fakten, system_instructions, list(verlauf)), result_queue),
        daemon=True
    )
    llm_thread.start()
    t_start = time.time()

    bereits_gespielt: set = set()

    # ── Schritt 1: Filler-Satz spielt IMMER ───────────────────────
    try:
        filler_cache.play_random_filler()
    except Exception as e:
        print(f"[Filler-Fehler] {e}")

    # ── Schritt 2: mindestens EIN Fakt spielt IMMER ───────────────
    try:
        played = filler_cache.play_random_fakt(exclude=bereits_gespielt)
        if played is not None:
            bereits_gespielt.add(played)
    except Exception as e:
        print(f"[Fakt-Fehler] {e}")

    # ── Schritt 3: Schleife – weitere Fakten nur wenn Antwort noch fehlt ──
    while result_queue.empty() and (time.time() - t_start) < OLLAMA_TIMEOUT:
        # Kurze Atempause zwischen Fakten
        try:
            # Während der Pause regelmäßig prüfen, ob Antwort reingekommen ist;
            # wenn ja, sofort raus und Antwort sprechen statt neuer Fakt.
            pause_ende = time.time() + PAUSE_ZWISCHEN_FAKTEN
            while time.time() < pause_ende:
                if not result_queue.empty():
                    break
                time.sleep(0.05)
        except Exception:
            pass

        # Nach der Pause nochmal prüfen – falls Antwort in der Pause kam, raus
        if not result_queue.empty():
            break

        # Nächster Fakt (keine Wiederholung innerhalb eines Turns)
        try:
            played = filler_cache.play_random_fakt(exclude=bereits_gespielt)
            if played is None:
                # Alle Fakten einmal durch → Pool freigeben (bei sehr langer Wartezeit)
                bereits_gespielt.clear()
                played = filler_cache.play_random_fakt(exclude=bereits_gespielt)
            if played is not None:
                bereits_gespielt.add(played)
        except Exception as e:
            print(f"[Fakt-Fehler] {e}")
            break

    # ── Schritt 4: auf LLM-Ergebnis warten (Sicherheitsnetz) ──────
    restzeit = max(1.0, OLLAMA_TIMEOUT - (time.time() - t_start))
    llm_thread.join(timeout=restzeit + 5)

    if result_queue.empty():
        # Worst-Case: LLM hat komplett gehangen
        antwort = "Entschuldige, ich konnte gerade keine Antwort formulieren. Versuch es bitte noch einmal."
        status  = "timeout"
    else:
        status, antwort = result_queue.get()

    # ── Schritt 5: Überleitung + Antwort sprechen ────────────────
    # Überleitung nur spielen, wenn wir tatsächlich Filler/Fakt gespielt haben
    # (was auf dem Pi faktisch immer der Fall ist). Signalisiert klar:
    # "Jetzt kommt die echte Antwort."
    try:
        filler_cache.play_random_ueberleitung()
    except Exception as e:
        print(f"[Überleitungs-Fehler] {e}")

    # Kleine Pause zwischen Überleitung und Antwort
    try:
        time.sleep(PAUSE_VOR_ANTWORT)
    except Exception:
        pass

    print(f"🤖 Expodroide ({status}): {antwort}")
    speak(antwort)
    return antwort

# ─────────────────────────────────────────────
#  HAUPTSCHLEIFE
# ─────────────────────────────────────────────
if __name__ == "__main__":
    wissensdatenbank = lade_wissen()
    if not wissensdatenbank:
        exit(1)

    # Filler einmalig vor-synthetisieren
    filler_cache.build()

    verlauf = deque(maxlen=MAX_MEMORY)

    print("\n🤖 Expodroide bereit! Sprich mit mir (oder tippe 'ende').")
    speak("Hallo! Ich bin der DUAL-E. Wie kann ich dir helfen?")

    while True:
        try:
            benutzer_eingabe = listen()
        except Exception as e:
            print(f"[STT-Fehler] {e} – Bitte Text eingeben:")
            try:
                benutzer_eingabe = input("Besucher: ").strip()
            except (EOFError, KeyboardInterrupt):
                break

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

        try:
            antwort = dialog_turn(benutzer_eingabe, wissensdatenbank, verlauf)
            verlauf.append({"frage": benutzer_eingabe, "antwort": antwort})
        except Exception as e:
            # Niemals die Hauptschleife sterben lassen (Messe-Robustheit)
            print(f"[Dialog-Fehler] {e}")
            speak("Da ist etwas schiefgelaufen. Lass uns einfach weitermachen.")