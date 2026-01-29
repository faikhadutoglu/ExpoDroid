"""
Main application - continuous speech recognition and output.
Listens, prints recognized text, and speaks it back.
"""

import sys
import threading
from speech_recognizer import StreamingSpeechRecognizer
from text_to_speech import TextToSpeech


class Speak2FuncApp:
    """Main application."""
    
    def __init__(self):
        """Initialize app."""
        self.recognizer = StreamingSpeechRecognizer()
        self.tts = TextToSpeech(rate=150, volume=1.0)
        self.running = True
    
    def on_recognized(self, text: str) -> None:
        """Handle recognized speech."""
        print(f">> {text}")
        sys.stdout.flush()
        self.tts.speak(text)
    
    def run(self) -> None:
        """Run the application."""
        print("=" * 60)
        print("Speak2Func - Continuous Speech Recognition")
        print("=" * 60)
        print("\nListening... (type 'quit' to stop)\n")
        
        # Start listening in background
        self.recognizer.start_listening(self.on_recognized)
        
        # Wait for user input to quit
        try:
            while self.running:
                user_input = input()
                if user_input.lower() in ('quit', '1', 'exit', 'q'):
                    self.running = False
                    break
        except (EOFError, KeyboardInterrupt):
            pass
        finally:
            self.cleanup()
    
    def cleanup(self) -> None:
        """Stop and cleanup."""
        print("\nStopping...")
        self.recognizer.stop_listening()
        print("Done.")


if __name__ == "__main__":
    app = Speak2FuncApp()
    app.run()
