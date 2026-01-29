# Speak2Func - Robot Voice Transformer

A cross-platform Python project for real-time voice transformation that works on both PC and Raspberry Pi 5.

## Features

- Real-time microphone input capture
- Voice transformation with robot voice effects
- Cross-platform audio support (Windows PC and Raspberry Pi 5)
- Easy integration with robot control systems

## Project Structure

```
speak2func/
├── src/
│   ├── audio_input.py       # Microphone capture
│   ├── voice_transformer.py # Voice effect processing
│   ├── audio_output.py      # Speaker output
│   └── main.py              # Main application
├── tests/
│   └── test_voice.py        # Unit tests
├── requirements.txt         # Python dependencies
└── README.md               # This file
```

## Installation

### Prerequisites
- Python 3.8+
- Microphone connected to your PC/Raspberry Pi
- Speaker/USB speaker connected

### Setup

1. Clone/navigate to the project directory
2. Install dependencies:
   ```bash
   pip install -r requirements.txt
   ```

## Usage

Run the main application:
```bash
python src/main.py
```

## Testing

Run tests:
```bash
python -m pytest tests/
```

## Notes

- This project is designed to work on both Windows PC and Raspberry Pi 5 without code changes
- All platform-specific code is abstracted in the audio modules
- Audio processing uses platform-independent libraries

## License

DHBW
