import requests
import json

# 1. Persönlichkeit/Fakten aus der Textdatei laden
def lade_persoenlichkeit():
    try:
        with open("persoenlichkeit.txt", "r", encoding="utf-8") as file:
            return file.read()
    except FileNotFoundError:
        return "Du bist ein hilfreicher Assistent."

# 2. Funktion, um die Frage an Phi-3.5 zu schicken
def frage_phi(prompt, system_prompt):
    url = "http://localhost:11434/api/generate"
    payload = {
        "model": "phi3.5",  # Hier wurde das Modell auf Phi-3.5 geändert
        "system": system_prompt,
        "prompt": prompt,
        "stream": False,
        "options": {
            "temperature": 0.0,  # 0.0 bedeutet: Keine Fantasie, nur harte Fakten!
            "num_predict": 100   # Je KLEINER die Zahl, desto KÜRZER die Antwort (Maximal erlaubte Tokens)
        }
    }
    
    try:
        response = requests.post(url, json=payload)
        if response.status_code == 200:
            return response.json().get("response", "")
        else:
            return "Fehler bei der Verbindung."
    except Exception as e:
        return f"Ollama läuft anscheinend nicht: {e}"

# 3. Das Hauptprogramm (Der Chat-Loop)
if __name__ == "__main__":
    mein_system_prompt = lade_persoenlichkeit()
    print("🤖 Phi-3.5-mini ist bereit! (Tippe 'ende' zum Beenden)")
    print("-" * 50)
    
    while True:
        # Hier gibst du später deinen Text per Tastatur (oder Mikrofon) ein
        benutzer_eingabe = input("\nDu: ")
        
        if benutzer_eingabe.lower() == 'ende':
            print("Chat beendet. Tschüss!")
            break
            
        print("Phi-3.5 denkt nach...")
        
        # Die Antwort von Ollama abholen
        antwort = frage_phi(benutzer_eingabe, mein_system_prompt)
        
        # Die Antwort auf dem Bildschirm ausgeben
        print(f"\nPhi-3.5: {antwort}")
        
        # HIER kommt später der Code rein, der die Variable 'antwort' laut vorliest!