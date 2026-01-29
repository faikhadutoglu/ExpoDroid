"""
Cross-platform audio input module for microphone capture.
Works on both Windows PC and Raspberry Pi 5.
"""

import pyaudio
import numpy as np
from typing import Optional, Callable


class AudioInput:
    """Handles microphone input with cross-platform compatibility."""
    
    def __init__(self, 
                 sample_rate: int = 44100,
                 chunk_size: int = 1024,
                 channels: int = 1):
        """
        Initialize audio input.
        
        Args:
            sample_rate: Audio sample rate in Hz (44100 is standard)
            chunk_size: Number of frames per buffer
            channels: Number of audio channels (1 for mono)
        """
        self.sample_rate = sample_rate
        self.chunk_size = chunk_size
        self.channels = channels
        
        self.audio = pyaudio.PyAudio()
        self.stream = None
        self.is_recording = False
        
    def start_recording(self) -> None:
        """Start the audio input stream."""
        if self.stream is None:
            self.stream = self.audio.open(
                format=pyaudio.paFloat32,
                channels=self.channels,
                rate=self.sample_rate,
                input=True,
                frames_per_buffer=self.chunk_size
            )
            self.is_recording = True
            print(f"Recording started (Sample rate: {self.sample_rate} Hz)")
    
    def stop_recording(self) -> None:
        """Stop the audio input stream."""
        if self.stream is not None:
            self.stream.stop_stream()
            self.stream.close()
            self.stream = None
            self.is_recording = False
            print("Recording stopped")
    
    def read_chunk(self) -> np.ndarray:
        """
        Read one chunk of audio data from microphone.
        
        Returns:
            numpy array of audio samples (float32, range -1.0 to 1.0)
        """
        if not self.is_recording or self.stream is None:
            raise RuntimeError("Recording not started. Call start_recording() first.")
        
        data = self.stream.read(self.chunk_size, exception_on_overflow=False)
        audio_data = np.frombuffer(data, dtype=np.float32)
        return audio_data
    
    def list_devices(self) -> None:
        """Print list of available audio devices."""
        print("\nAvailable audio devices:")
        for i in range(self.audio.get_device_count()):
            info = self.audio.get_device_info_by_index(i)
            print(f"{i}: {info['name']}")
    
    def cleanup(self) -> None:
        """Clean up audio resources."""
        self.stop_recording()
        self.audio.terminate()
        print("Audio cleanup complete")


if __name__ == "__main__":
    # Test audio input
    audio_input = AudioInput()
    audio_input.list_devices()
    
    try:
        audio_input.start_recording()
        print("Listening for 5 seconds...")
        
        for _ in range(5 * 44100 // 1024):  # 5 seconds worth of chunks
            chunk = audio_input.read_chunk()
            print(f"Read chunk: min={chunk.min():.3f}, max={chunk.max():.3f}, mean={chunk.mean():.3f}")
    finally:
        audio_input.cleanup()
