"""
Voice transformation module for robot voice effects.
Applies audio processing to transform voice into robotic sound.
"""

import numpy as np
from scipy import signal
from typing import Optional


class VoiceTransformer:
    """Applies voice transformation effects for robot voice synthesis."""
    
    def __init__(self, sample_rate: int = 44100):
        """
        Initialize voice transformer.
        
        Args:
            sample_rate: Audio sample rate in Hz
        """
        self.sample_rate = sample_rate
    
    def ring_modulation(self, audio: np.ndarray, carrier_freq: float = 1000.0) -> np.ndarray:
        """
        Apply ring modulation for classic robot/alien voice effect.
        Multiplies audio with a sine wave carrier.
        
        Args:
            audio: Input audio samples
            carrier_freq: Carrier frequency in Hz
        
        Returns:
            Ring-modulated audio
        """
        t = np.arange(len(audio)) / self.sample_rate
        carrier = np.sin(2 * np.pi * carrier_freq * t)
        modulated = audio * carrier
        
        # Normalize
        max_val = np.max(np.abs(modulated))
        if max_val > 0:
            modulated = modulated / max_val * 0.9
        
        return modulated.astype(np.float32)
    
    def bitcrush(self, audio: np.ndarray, bit_depth: int = 8) -> np.ndarray:
        """
        Apply bitcrushing for digital/lo-fi robot effect.
        
        Args:
            audio: Input audio samples
            bit_depth: Number of bits to reduce to (lower = more robotic)
        
        Returns:
            Bitcrushed audio
        """
        # Quantize to lower bit depth
        levels = 2 ** bit_depth
        quantized = np.round(audio * (levels / 2)) / (levels / 2)
        quantized = np.clip(quantized, -1.0, 1.0)
        
        return quantized.astype(np.float32)
    
    def vocoder_effect(self, audio: np.ndarray, carrier_freq: float = 800.0) -> np.ndarray:
        """
        Apply vocoder-like effect for synthetic robot voice.
        Extracts envelope and applies to carrier signal.
        
        Args:
            audio: Input audio samples
            carrier_freq: Carrier frequency in Hz
        
        Returns:
            Vocoded audio
        """
        # Extract envelope using low-pass filter on absolute value
        envelope = np.abs(audio)
        
        # Design lowpass filter for envelope extraction
        nyquist = self.sample_rate / 2
        cutoff = 200 / nyquist  # Extract envelope below 200 Hz
        cutoff = max(0.01, min(0.99, cutoff))
        
        b, a = signal.butter(3, cutoff, btype='low')
        envelope = signal.filtfilt(b, a, envelope)
        
        # Generate carrier signal (sine wave)
        t = np.arange(len(audio)) / self.sample_rate
        carrier = np.sin(2 * np.pi * carrier_freq * t)
        
        # Modulate carrier with envelope
        vocoded = carrier * envelope
        
        # Normalize
        max_val = np.max(np.abs(vocoded))
        if max_val > 0:
            vocoded = vocoded / max_val * 0.9
        
        return vocoded.astype(np.float32)
    
    def pitch_shift(self, audio: np.ndarray, shift_ratio: float = 1.5) -> np.ndarray:
        """
        Shift pitch of audio using simple resampling.
        
        Args:
            audio: Input audio samples
            shift_ratio: Pitch shift ratio (e.g., 1.5 = 1.5x pitch, higher = higher pitch)
        
        Returns:
            Pitch-shifted audio
        """
        if shift_ratio <= 0:
            return audio
        
        # Resample to change pitch
        new_length = int(len(audio) / shift_ratio)
        if new_length == 0:
            return audio
        
        indices = np.linspace(0, len(audio) - 1, new_length)
        shifted = np.interp(indices, np.arange(len(audio)), audio)
        return shifted.astype(np.float32)
    
    def transform_to_robot_voice(self, audio: np.ndarray, 
                                pitch_shift_semitones: float = 0,
                                effect_intensity: float = 1.0,
                                robot_type: str = 'vocoder') -> np.ndarray:
        """
        Transform voice to robot voice with multiple effect options.
        
        Args:
            audio: Input audio samples
            pitch_shift_semitones: Pitch shift in semitones (12 = one octave up)
            effect_intensity: Effect intensity (0.0-1.0)
            robot_type: Type of robot effect ('vocoder', 'ring_mod', 'bitcrush', 'formant', or 'none')
        
        Returns:
            Robot voice audio
        """
        # Normalize input
        max_val = np.max(np.abs(audio))
        if max_val == 0:
            return audio
        
        audio = audio / max_val
        
        # Apply pitch shift if requested
        if pitch_shift_semitones != 0:
            shift_ratio = 2 ** (pitch_shift_semitones / 12.0)
            audio = self.pitch_shift(audio, shift_ratio=shift_ratio)
        
        # Apply robot effect based on type
        if robot_type == 'vocoder':
            # Vocoder effect - most "robot" sounding
            carrier_freq = 800 + 400 * effect_intensity  # 800-1200 Hz
            transformed = self.vocoder_effect(audio, carrier_freq=carrier_freq)
        
        elif robot_type == 'ring_mod':
            # Ring modulation - metallic/alien sound
            carrier_freq = 500 + 500 * effect_intensity  # 500-1000 Hz
            transformed = self.ring_modulation(audio, carrier_freq=carrier_freq)
        
        elif robot_type == 'bitcrush':
            # Bitcrushing - digital/8-bit sound
            bit_depth = max(4, int(12 - 8 * effect_intensity))  # 4-12 bits
            transformed = self.bitcrush(audio, bit_depth=bit_depth)
        
        else:  # 'none'
            # No effect
            transformed = audio
        
        # Final normalization
        max_val = np.max(np.abs(transformed))
        if max_val > 0:
            transformed = transformed / max_val * 0.9
        
        return transformed.astype(np.float32)


if __name__ == "__main__":
    # Test voice transformer
    transformer = VoiceTransformer(sample_rate=44100)
    
    # Create a test signal (1 second of 440 Hz sine wave)
    duration = 1.0
    t = np.arange(int(44100 * duration)) / 44100
    test_signal = np.sin(2 * np.pi * 440 * t).astype(np.float32)
    
    print("Original signal stats:")
    print(f"  Min: {test_signal.min():.3f}, Max: {test_signal.max():.3f}")
    
    # Transform
    robot_voice = transformer.transform_to_robot_voice(test_signal)
    
    print("\nRobot voice stats:")
    print(f"  Min: {robot_voice.min():.3f}, Max: {robot_voice.max():.3f}")
