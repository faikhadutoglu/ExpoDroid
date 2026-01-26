/**
 * Motor Filter Test - Terminal Version
 * 
 * Steuerung:
 * Tasten 0-9: Verschiedene Geschwindigkeiten
 * f: Filter ein/aus
 * r: Reset
 * q: Beenden
 * 
 
 IN_TERMINAL:
 # Kompilieren
g++ -o motor_filter_test motor_filter_test.cpp -std=c++11

# Ausführen
./motor_filter_test

 */

#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>
#include <termios.h>
#include <unistd.h>

using namespace std;

// Farb-Codes für Terminal
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"

// Filter-Parameter
#define ACCELERATION_FILTER_ALPHA 0.1
#define BATTERY_FACTOR 0.91
#define MAX_PWM 255

// Globale Variablen
float filteredValue = 0.0;
bool filterEnabled = true;

// Tastatur-Eingabe ohne Enter
char getch() {
    char buf = 0;
    struct termios old = {0};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror ("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror ("tcsetattr ~ICANON");
    return (buf);
}

// Berechne Motor-Geschwindigkeit aus Pulse
float calculateSpeed(int pulse) {
    float speed = 0.0;
    
    if (pulse < 1450) {
        // Rückwärts: 1100-1450 -> -255 bis 0
        speed = ((float)(pulse - 1100) / (1450.0 - 1100.0)) * MAX_PWM - MAX_PWM;
    } else if (pulse > 1550) {
        // Vorwärts: 1550-1900 -> 0 bis 255
        speed = ((float)(pulse - 1550) / (1900.0 - 1550.0)) * MAX_PWM;
    } else {
        // Deadzone: 1450-1550 = Stop
        speed = 0.0;
    }
    
    // Begrenzung und Batterie-Faktor
    float maxSpeed = MAX_PWM * BATTERY_FACTOR;
    float minSpeed = -MAX_PWM * BATTERY_FACTOR;
    if (speed > maxSpeed) speed = maxSpeed;
    if (speed < minSpeed) speed = minSpeed;
    return speed;
}

// Exponentialfilter anwenden
float applyFilter(float targetSpeed, float currentFiltered) {
    float absTarget = (targetSpeed >= 0) ? targetSpeed : -targetSpeed;
    float absCurrent = (currentFiltered >= 0) ? currentFiltered : -currentFiltered;
    
    if (absTarget > absCurrent) {
        // Beschleunigen - Filter anwenden
        return currentFiltered + ACCELERATION_FILTER_ALPHA * (targetSpeed - currentFiltered);
        // Warten für 300 ms
       // usleep(900000); // usleep erwartet Mikrosekunden das gibts nicht in hauptcode nur probe!!
    } else {
        // Bremsen - direkter Durchgriff
        return targetSpeed;
    }
}

// Pulse-Wert für Taste ermitteln
int getPulseForKey(char key) {
    switch(key) {

        case '1': return 1100;  // Max rückwärts
        case '2': return 1150;
        case '3': return 1250;
        case '4': return 1350;
        case '5': return 1500;  // Deadzone Start
        case '6': return 1650;  // Deadzone Ende
        case '7': return 1800;
        case '8': return 1950;
        case '9': return 2030;  // Max vorwärts
        case '0': return 2099;  
        default: return -1;
    }
}

// Fortschrittsbalken zeichnen
string drawBar(float percent, int width = 40) {
    string bar = "[";
    int pos = width / 2;
    int fillPos = (int)(abs(percent) / 100.0 * (width / 2));
    
    for (int i = 0; i < width; i++) {
        if (percent >= 0) {
            // Vorwärts
            if (i >= pos && i < pos + fillPos) {
                bar += "█";
            } else if (i == pos) {
                bar += "|";
            } else {
                bar += " ";
            }
        } else {
            // Rückwärts
            if (i > pos - fillPos && i <= pos) {
                bar += "█";
            } else if (i == pos) {
                bar += "|";
            } else {
                bar += " ";
            }
        }
    }
    bar += "]";
    return bar;
}

// Bildschirm löschen
void clearScreen() {
    cout << "\033[2J\033[1;1H";
}

// Hauptanzeige
void displayStatus(int pulse, float unfiltered, float filtered) {
    clearScreen();
    
    float unfPercent = (unfiltered / (MAX_PWM * BATTERY_FACTOR)) * 100.0;
    float fltPercent = (filtered / (MAX_PWM * BATTERY_FACTOR)) * 100.0;
    float difference = abs(unfPercent - fltPercent);
    
    cout << COLOR_BOLD << "╔════════════════════════════════════════════════════════════════╗\n";
    cout << "║         MOTOR FILTER TEST - TERMINAL VERSION                  ║\n";
    cout << "╚════════════════════════════════════════════════════════════════╝" << COLOR_RESET << "\n\n";
    
    // Aktueller Pulse
    cout << COLOR_BLUE << "AKTUELLER PULSE: " << COLOR_BOLD << pulse << " μs" << COLOR_RESET << "\n\n";
    
    // Ohne Filter (ROT)
    cout << COLOR_RED << COLOR_BOLD << "OHNE FILTER:" << COLOR_RESET << "\n";
    cout << COLOR_RED << "  Geschwindigkeit: " << fixed << setprecision(1) 
         << setw(7) << unfPercent << " %  (" << setw(7) << unfiltered << " PWM)" << COLOR_RESET << "\n";
    cout << "  " << drawBar(unfPercent) << "\n\n";
    
    // Mit Filter (GRÜN)
    cout << COLOR_GREEN << COLOR_BOLD << "MIT FILTER:" << COLOR_RESET;
    if (!filterEnabled) cout << COLOR_YELLOW << " [DEAKTIVIERT]" << COLOR_RESET;
    cout << "\n";
    cout << COLOR_GREEN << "  Geschwindigkeit: " << fixed << setprecision(1) 
         << setw(7) << fltPercent << " %  (" << setw(7) << filtered << " PWM)" << COLOR_RESET << "\n";
    cout << "  " << drawBar(fltPercent) << "\n\n";
    
    // Differenz
    cout << COLOR_YELLOW << "DIFFERENZ: " << setw(6) << difference << " %" << COLOR_RESET << "\n\n";
    
    // Vergleichszeile
    cout << "─────────────────────────────────────────────────────────────────\n";
    cout << COLOR_RED << "OHNE: " << setw(7) << unfPercent << "%" << COLOR_RESET 
         << "  │  "
         << COLOR_GREEN << "MIT: " << setw(7) << fltPercent << "%" << COLOR_RESET
         << "  │  "
         << COLOR_YELLOW << "Δ: " << setw(6) << difference << "%" << COLOR_RESET << "\n";
    cout << "─────────────────────────────────────────────────────────────────\n\n";
    
    // Tastenbelegung
    cout << COLOR_BOLD << "TASTENBELEGUNG:" << COLOR_RESET << "\n";
    cout << "  1: -100%  2: -71%  3: -43%  4: -14%  5: 0% (Deadzone)\n";
    cout << "  6: 0%     7: +43%  8: +86%  9: +100% 0: +43%\n\n";
    cout << "  " << COLOR_BLUE << "f" << COLOR_RESET << ": Filter ein/aus  |  "
         << COLOR_BLUE << "r" << COLOR_RESET << ": Reset  |  "
         << COLOR_BLUE << "q" << COLOR_RESET << ": Beenden\n\n";
    
    // Filter-Status
    cout << "Filter-Parameter: α=" << ACCELERATION_FILTER_ALPHA 
         << ", Batterie=" << BATTERY_FACTOR 
         << ", Status=" << (filterEnabled ? COLOR_GREEN "EIN" : COLOR_RED "AUS") << COLOR_RESET << "\n";
}

int main() {
    int currentPulse = 1500;  // Start in Neutral
    char key;
    
    cout << "Motor Filter Test gestartet...\n";
    cout << "Drücke eine Taste um zu beginnen...\n";
    
    while (true) {
        // Berechne Werte
        float unfilteredSpeed = calculateSpeed(currentPulse);
        float filteredSpeed;
        
        if (filterEnabled) {
            filteredSpeed = applyFilter(unfilteredSpeed, filteredValue);
            filteredValue = filteredSpeed;
        } else {
            filteredSpeed = unfilteredSpeed;
            filteredValue = unfilteredSpeed;
        }
        
        // Anzeige aktualisieren
        displayStatus(currentPulse, unfilteredSpeed, filteredSpeed);
        
        // Warte auf Eingabe
        key = getch();
        
        // Verarbeite Eingabe
        if (key == 'q' || key == 'Q') {
            clearScreen();
            cout << "Programm beendet.\n";
            break;
        } else if (key == 'f' || key == 'F') {
            filterEnabled = !filterEnabled;
        } else if (key == 'r' || key == 'R') {
            currentPulse = 1500;
            filteredValue = 0.0;
        } else {
            int newPulse = getPulseForKey(key);
            if (newPulse != -1) {
                currentPulse = newPulse;
            }
        }
        
        // Kleine Verzögerung für sanftere Animation
        usleep(50000);  // 50ms
    }
    
    return 0;
}
