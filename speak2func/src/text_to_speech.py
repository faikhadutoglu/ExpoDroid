"""
Simple text-to-speech output using Windows PowerShell Speak-String.
Falls back to pyttsx3 on non-Windows systems.
"""

import os
import sys
import platform
import threading
import queue


class TextToSpeech:
    """Text-to-speech engine using PowerShell on Windows, pyttsx3 on others."""
    
    def __init__(self, rate: int = 150, volume: float = 1.0):
        """Initialize TTS."""
        self.rate = rate
        self.volume = volume
        self.is_windows = platform.system() == "Windows"
        
        if not self.is_windows:
            import pyttsx3
            self.engine = pyttsx3.init()
            self.engine.setProperty('rate', rate)
            self.engine.setProperty('volume', volume)
        
        self.speak_queue = queue.Queue()
        self.worker_thread = threading.Thread(target=self._worker, daemon=True)
        self.worker_thread.start()
    
    def speak(self, text: str) -> None:
        """Queue text to be spoken."""
        if not text or not text.strip():
            return
        self.speak_queue.put(text)
    
    def _worker(self) -> None:
        """Worker thread that processes speech queue."""
        while True:
            try:
                text = self.speak_queue.get(timeout=1)
                self._speak_safe(text)
            except queue.Empty:
                continue
            except Exception as e:
                print(f"[TTS ERROR] {e}", file=sys.stderr)
    
    def _speak_safe(self, text: str) -> None:
        """Speak text safely."""
        if self.is_windows:
            # Use PowerShell Speak-String on Windows
            escaped_text = text.replace('"', '\\"')
            cmd = f'powershell -Command "Add-Type -AssemblyName System.Speech; (New-Object System.Speech.Synthesis.SpeechSynthesizer).Speak(\'{escaped_text}\')"'
            os.system(cmd)
        else:
            # Use pyttsx3 on other platforms
            try:
                self.engine.say(text)
                self.engine.runAndWait()
            except Exception as e:
                print(f"[TTS ERROR] {e}", file=sys.stderr)
