import requests
import json
import os
from collections import deque

# ══════════════════════════════════════════════
# EINSTELLUNGEN - hier anpassen!
# ══════════════════════════════════════════════
MODELL          = "phi3.5" # can be:  "phi3.5" oder "gemma3:1b"
MAX_MEMORY      = 3      # Anzahl letzter Nachrichten im Gedaechtnis
MAX_TOKENS      = 100     # Maximale Antwortlaenge (hoeher = langsamer)
TEMPERATUR      = 0.2    # 0.0 = strikt, 1.0 = kreativ
OLLAMA_URL      = "http://localhost:11434/api/generate"

# ══════════════════════════════════════════════
# 1. Wissensbasis laden
# ══════════════════════════════════════════════
def lade_wissen():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    json_pfad = os.path.join(script_dir, "wissen.json")
    try:
        with open(json_pfad, "r", encoding="utf-8") as file:
            return json.load(file)
    except FileNotFoundError:
        print("Fehler: wissen.json nicht gefunden!")
        return None

# ══════════════════════════════════════════════
# 2. Fakten suchen
# ══════════════════════════════════════════════
def finde_relevante_fakten(benutzer_eingabe, daten, max_fakten=4):
    eingabe_lower = benutzer_eingabe.lower()
    beste_treffer = []

    for eintrag in daten["fakten"]:
        score = 0
        for keyword in eintrag["keywords"]:
            if keyword in eingabe_lower:
                score += 1
            elif len(keyword) >= 5 and keyword[:5] in eingabe_lower:
                score += 0.5
        if score > 0 and eintrag["antwort"].strip():
            beste_treffer.append((score, eintrag["antwort"]))

    beste_treffer.sort(key=lambda x: x[0], reverse=True)
    return [antwort for _, antwort in beste_treffer[:max_fakten]], beste_treffer

# ══════════════════════════════════════════════
# 3. Phi via Ollama anfragen
# ══════════════════════════════════════════════
def frage_phi(benutzer_eingabe, fakten_liste, system_instructions, verlauf):

    # -- Memory: letzte N Gespraeche als Text aufbauen --
    memory_text = ""
    if verlauf:
        memory_text = "Bisheriger Gespraechsverlauf:\n"
        for eintrag in verlauf:
            memory_text += "Besucher: " + eintrag["frage"] + "\n"
            memory_text += "Expodroide: " + eintrag["antwort"] + "\n"
        memory_text += "\n"

    # -- Fakten aufbauen --
    if fakten_liste:
        fakten_text = "\n".join(
            ["Fakt " + str(i+1) + ": " + f for i, f in enumerate(fakten_liste)]
        )
        kontext_prompt = (
            memory_text
            + fakten_text
            + "\n\nFrage: " + benutzer_eingabe
            + "\n\nAntworte in maximal 2-3 Saetzen NUR basierend auf den Fakten. Erfinde nichts."
        )
    else:
        kontext_prompt = memory_text + "Frage: " + benutzer_eingabe

    payload = {
        "model": MODELL,
        "system": system_instructions,
        "prompt": kontext_prompt,
        "stream": False,
        "options": {
            "temperature": TEMPERATUR,
            "num_predict": MAX_TOKENS,
            "stop": ["Besucher:", "Frage des Nutzers:", "\nFrage:"]
        }
    }

    try:
        response = requests.post(OLLAMA_URL, json=payload, timeout=60)
        antwort = response.json().get("response", "").strip()
        if not antwort:
            return "Das weiss ich leider nicht - frag einen Kollegen am Stand!"
        return antwort
    except Exception as e:
        return "Verbindungsfehler: " + str(e)

# ══════════════════════════════════════════════
# 4. Hauptprogramm
# ══════════════════════════════════════════════
if __name__ == "__main__":
    wissensdatenbank = lade_wissen()
    if not wissensdatenbank:
        exit(1)

    # deque begrenzt automatisch auf MAX_MEMORY Eintraege
    verlauf = deque(maxlen=MAX_MEMORY)

    print("Expodroide bereit! (Tippe ende zum Beenden)\n")

    while True:
        benutzer_eingabe = input("Besucher: ").strip()

        if not benutzer_eingabe:
            continue
        if benutzer_eingabe.lower() == "ende":
            print("Expodroide: Bis bald!")
            break

        fakten, alle_treffer = finde_relevante_fakten(benutzer_eingabe, wissensdatenbank)

             # -- DEBUG --
        # Token-Schaetzung: 1 Token ~ 4 Zeichen (Faustregel)
        tokens_memory   = sum(len(e["frage"]) + len(e["antwort"]) for e in verlauf) // 4
        tokens_fakten   = sum(len(a) for a in fakten) // 4
        tokens_frage    = len(benutzer_eingabe) // 4
        tokens_system   = len(wissensdatenbank["system_prompt"]) // 4
        tokens_gesamt   = tokens_memory + tokens_fakten + tokens_frage + tokens_system
        pi_wartezeit    = round(MAX_TOKENS / 3.4, 1)

        print("\n[DEBUG] Memory-Eintraege : " + str(len(verlauf)) + "/" + str(MAX_MEMORY))
        print("[DEBUG] Modelname        : " + MODELL)
        print("[DEBUG] System-Prompt    : " + ("aktiv" if "system_prompt" in wissensdatenbank else "FEHLT"))
        print("[DEBUG] Gefundene Fakten : " + str(len(fakten)))
        if alle_treffer:
            for score, antwort in alle_treffer[:2]:
                print("  Score " + str(score) + ": " + antwort[:60] + "...")
        else:
            print("  Kein Fakt - nur System-Prompt + Memory aktiv")
        print("[DEBUG] Tokens gesendet  : ~" + str(tokens_gesamt) + " (System:" + str(tokens_system) + " Memory:" + str(tokens_memory) + " Fakten:" + str(tokens_fakten) + " Frage:" + str(tokens_frage) + ")")
        print("[DEBUG] Max Tokens Antw. : " + str(MAX_TOKENS))
         # NEU - Prefill(die Zeit die man für ) + Output:
        pi_prefill      = round(tokens_gesamt / 80, 1)
        pi_output       = round(MAX_TOKENS / 3.4, 1)
        pi_wartezeit    = round(pi_prefill + pi_output, 1)
        ...
        print("[DEBUG] Pi 5 Wartezeit   : ~" + str(pi_wartezeit) + "s (Prefill:" + str(pi_prefill) + "s + Output:" + str(pi_output) + "s, worst case)")



        antwort = frage_phi(benutzer_eingabe, fakten, wissensdatenbank["system_prompt"], verlauf)

        # Gespraech ins Memory speichern
        verlauf.append({"frage": benutzer_eingabe, "antwort": antwort})

        print("Expodroide: " + antwort + "\n")
