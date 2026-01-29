import speech_recognition as sr
import threading


class StreamingSpeechRecognizer:
    def __init__(self):
        self.recognizer = sr.Recognizer()
        self.microphone = sr.Microphone()
        self.is_listening = False
        self.listen_thread = None
    
    def start_listening(self, callback):
        if self.is_listening:
            return
        self.is_listening = True
        self.listen_thread = threading.Thread(target=self._listen_loop, args=(callback,), daemon=True)
        self.listen_thread.start()
    
    def _listen_loop(self, callback):
        with self.microphone as source:
            self.recognizer.adjust_for_ambient_noise(source, duration=0.5)
            while self.is_listening:
                try:
                    audio = self.recognizer.listen(source, timeout=1.0, phrase_time_limit=30)
                    try:
                        text = self.recognizer.recognize_google(audio)
                        if text.strip():
                            callback(text)
                    except (sr.UnknownValueError, sr.RequestError):
                        pass
                except sr.RequestError:
                    pass
                except Exception:
                    pass
    
    def stop_listening(self):
        self.is_listening = False
        if self.listen_thread:
            self.listen_thread.join(timeout=1)
