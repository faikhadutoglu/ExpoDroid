"""
Unit tests for voice transformation functionality.
"""

import numpy as np
import sys
import os

# Add src to path for imports
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src'))

from voice_transformer import VoiceTransformer


def test_voice_transformer_initialization():
    """Test that VoiceTransformer initializes correctly."""
    transformer = VoiceTransformer(sample_rate=44100)
    assert transformer.sample_rate == 44100


def test_pitch_shift():
    """Test pitch shifting functionality."""
    transformer = VoiceTransformer(sample_rate=44100)
    
    # Create test signal
    duration = 0.1
    t = np.arange(int(44100 * duration)) / 44100
    test_signal = np.sin(2 * np.pi * 440 * t).astype(np.float32)
    
    # Shift pitch
    shifted = transformer.pitch_shift(test_signal, shift_ratio=1.5)
    
    # Check output
    assert isinstance(shifted, np.ndarray)
    assert shifted.dtype == np.float32
    assert len(shifted) > 0


def test_robotics_effect():
    """Test robotics effect application."""
    transformer = VoiceTransformer(sample_rate=44100)
    
    # Create test signal
    test_signal = np.sin(2 * np.pi * 440 * np.linspace(0, 1, 1000)).astype(np.float32)
    
    # Apply effect
    effect = transformer.add_robotics_effect(test_signal)
    
    # Check output
    assert isinstance(effect, np.ndarray)
    assert effect.dtype == np.float32
    assert len(effect) == len(test_signal)


def test_lowpass_filter():
    """Test lowpass filter."""
    transformer = VoiceTransformer(sample_rate=44100)
    
    # Create test signal with high frequency
    t = np.linspace(0, 1, 44100)
    test_signal = np.sin(2 * np.pi * 8000 * t).astype(np.float32)
    
    # Apply filter
    filtered = transformer.apply_lowpass_filter(test_signal, cutoff_freq=3000)
    
    # Filtered signal should have lower amplitude than original (attenuated)
    assert np.max(np.abs(filtered)) < np.max(np.abs(test_signal))


def test_full_transformation():
    """Test complete voice transformation pipeline."""
    transformer = VoiceTransformer(sample_rate=44100)
    
    # Create test signal
    duration = 0.5
    t = np.arange(int(44100 * duration)) / 44100
    test_signal = np.sin(2 * np.pi * 440 * t).astype(np.float32)
    
    # Transform
    robot_voice = transformer.transform_to_robot_voice(test_signal)
    
    # Check output
    assert isinstance(robot_voice, np.ndarray)
    assert robot_voice.dtype == np.float32
    assert len(robot_voice) > 0
    assert np.max(np.abs(robot_voice)) <= 1.0  # Should be normalized


def test_transformation_with_silence():
    """Test transformation with silent input."""
    transformer = VoiceTransformer(sample_rate=44100)
    
    # Create silent signal
    silent_signal = np.zeros(1000, dtype=np.float32)
    
    # Transform
    result = transformer.transform_to_robot_voice(silent_signal)
    
    # Should remain silent (or near-silent)
    assert np.max(np.abs(result)) < 0.01


if __name__ == "__main__":
    # Run tests manually
    print("Running voice transformer tests...\n")
    
    test_voice_transformer_initialization()
    print("✓ Initialization test passed")
    
    test_pitch_shift()
    print("✓ Pitch shift test passed")
    
    test_robotics_effect()
    print("✓ Robotics effect test passed")
    
    test_lowpass_filter()
    print("✓ Lowpass filter test passed")
    
    test_full_transformation()
    print("✓ Full transformation test passed")
    
    test_transformation_with_silence()
    print("✓ Silence transformation test passed")
    
    print("\nAll tests passed!")
