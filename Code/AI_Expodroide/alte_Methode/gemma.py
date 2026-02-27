import requests
import json

# 1. Persönlichkeit aus der Textdatei laden
def lade_persoenlichkeit():
    try:
        with open("persoenlichkeit.txt", "r", encoding="utf-8") as file:
            return file.read()
    except FileNotFoundError:
        return "Du bist ein hilfreicher Assistent."

# 2. Funktion, um die Frage an Gemma zu schicken
def frage_gemma(prompt, system_prompt):
    url = "http://localhost:11434/api/generate"
    payload = {
        "model": "gemma3:1b",
        "system": system_prompt,
        "prompt": prompt,
        "stream": False,
        "options": {
            "temperature": 0.0,  # 0.0 bedeutet: Keine Fantasie, nur harte Fakten!
            "num_predict": 80    # Zwingt das Modell, extrem kurze Antworten zu geben. je größer die Zahl, desto kürzer die Antwort.?
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
    print("🤖 Gemma 3:1b ist bereit! (Tippe 'ende' zum Beenden)")
    print("-" * 50)
    
    while True:
        # Hier gibst du später deinen Text per Tastatur (oder Mikrofon) ein
        benutzer_eingabe = input("\nDu: ")
        
        if benutzer_eingabe.lower() == 'ende':
            print("Chat beendet. Tschüss!")
            break
            
        print("Gemma denkt nach...")
        
        # Die Antwort von Ollama abholen
        antwort = frage_gemma(benutzer_eingabe, mein_system_prompt)
        
        # Die Antwort auf dem Bildschirm ausgeben
        print(f"\nGemma: {antwort}")
        
        # HIER kommt später der Code rein, der die Variable 'antwort' laut vorliest!