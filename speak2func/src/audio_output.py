"""
Cross-platform audio output module for speaker playback.
Works on both Windows PC and Raspberry Pi 5.
"""

import pyaudio
import numpy as np
from typing import Optional


class AudioOutput:
    """Handles speaker output with cross-platform compatibility."""
    
    def __init__(self, 
                 sample_rate: int = 44100,
                 channels: int = 1):
        """
        Initialize audio output.
        
        Args:
            sample_rate: Audio sample rate in Hz (44100 is standard)
            channels: Number of audio channels (1 for mono)
        """
        self.sample_rate = sample_rate
        self.channels = channels
        
        self.audio = pyaudio.PyAudio()
        self.stream = None
    
    def start_playback(self) -> None:
        """Start the audio output stream."""
        if self.stream is None:
            self.stream = self.audio.open(
                format=pyaudio.paFloat32,
                channels=self.channels,
                rate=self.sample_rate,
                output=True
            )
            print(f"Playback started (Sample rate: {self.sample_rate} Hz)")
    
    def stop_playback(self) -> None:
        """Stop the audio output stream."""
        if self.stream is not None:
            self.stream.stop_stream()
            self.stream.close()
            self.stream = None
            print("Playback stopped")
    
    def play_audio(self, audio: np.ndarray) -> None:
        """
        Play audio data through speaker.
        
        Args:
            audio: numpy array of audio samples (float32, range -1.0 to 1.0)
        """
        if self.stream is None:
            raise RuntimeError("Playback not started. Call start_playback() first.")
        
        # Ensure audio is float32
        if audio.dtype != np.float32:
            audio = audio.astype(np.float32)
        
        # Clip to prevent distortion
        audio = np.clip(audio, -1.0, 1.0)
        
        # Write to stream
        self.stream.write(audio.tobytes())
    
    def list_devices(self) -> None:
        """Print list of available audio devices."""
        print("\nAvailable audio devices:")
        for i in range(self.audio.get_device_count()):
            info = self.audio.get_device_info_by_index(i)
            print(f"{i}: {info['name']}")
    
    def cleanup(self) -> None:
        """Clean up audio resources."""
        self.stop_playback()
        self.audio.terminate()
        print("Audio cleanup complete")


if __name__ == "__main__":
    # Test audio output
    audio_output = AudioOutput()
    audio_output.list_devices()
    
    try:
        audio_output.start_playback()
        
        # Generate a test tone (440 Hz sine wave, 1 second)
        duration = 1.0
        t = np.arange(int(44100 * duration)) / 44100
        test_tone = np.sin(2 * np.pi * 440 * t).astype(np.float32) * 0.3  # 30% amplitude
        
        print("Playing test tone (440 Hz, 1 second)...")
        audio_output.play_audio(test_tone)
        print("Done!")
    finally:
        audio_output.cleanup()
