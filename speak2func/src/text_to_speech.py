"""
Simple text-to-speech output.
Speaks recognized text in real-time.
"""

import pyttsx3
import threading


class TextToSpeech:
    """Text-to-speech engine."""
    
    def __init__(self, rate: int = 150, volume: float = 1.0):
        """Initialize TTS."""
        self.engine = pyttsx3.init()
        self.engine.setProperty('rate', rate)
        self.engine.setProperty('volume', volume)
        self.speak_lock = threading.Lock()
    
    def speak(self, text: str) -> None:
        """Speak text (non-blocking with thread lock)."""
        if not text or not text.strip():
            return
        
        # Speak in a thread to not block listening
        thread = threading.Thread(
            target=self._speak_safe,
            args=(text,),
            daemon=True
        )
        thread.start()
    
    def _speak_safe(self, text: str) -> None:
        """Speak with thread safety."""
        with self.speak_lock:
            try:
                self.engine.say(text)
                self.engine.runAndWait()
            except:
                import traceback
                print(f"[TTS ERROR] {traceback.format_exc()}")
