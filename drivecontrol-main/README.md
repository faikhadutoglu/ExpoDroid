# Units
All variables store values in standard metric units except when explicitly
stated (e.g. speed_kmph)

# Setup
In order to be able to compile and flash the code to the drive computers you
must complete all of the following steps.

    For driveControl and driveGuidance software
    0. Read the provided study paper to get a bare understanding for the
    architecture and model drive regulations
    1. Install VS Code (https://code.visualstudio.com/download)
    2. Install PlatformIO as Extension of VS Code
    3. Download the provided code
        - The software of driveControl and driveGuidance are two separate
            PlatformIO Projects!
    4. Download (MIT licensed) library
        - ServoESP32-1.0.2 (https://github.com/RoboticsBrno/ServoESP32)
    5. Copy ServoESP32 in driveControl/lib
    6. Open project driveControl via VSCode->PlatformIO(Alien
    Emoji on the left)->Open->Open Project->Navigate to the directory->Open
    "driveControl"
    7. For Uploading: Click on the Arrow Button at the bottom of your screen

# Utilization of the software
The code was developed just for the fun of it and is only to be seen as a result
of the "Studienarbeit" Module of DHBW. It is not meant for publishing, or any other
purpose! (Which is why the open source libraries are NOT included)