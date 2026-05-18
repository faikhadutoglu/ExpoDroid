/**
 * @file main.cpp (Test-Modus) - CLEAN TERMINAL VERSION
 * @brief Notbremsen-Test mit sauberem Terminal-Display
 * 
 * Steuerung:
 * 0-9: Geschwindigkeit ändern
 * q:   Beenden
 * 
 * HINWEIS: Baudrate 115200 verwenden!
 * die datei muss main.cpp heißen, damit sie in der Arduino IDE funktioniert. die eigentliche main.cpp main.txt nennen dann komplieren und flashen.
 */

#include <Arduino.h>

// ================================================================
// ===               FARBEN & TERMINAL DEFINITIONEN             ===
// ================================================================
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"

// ================================================================
// ===                     KONFIGURATION                        ===
// ================================================================
#define PIN_ULTRASONIC_FRONT_TRIG   23
#define PIN_ULTRASONIC_FRONT_ECHO   19

#define ULTRASONIC_TIMEOUT_US 30000
#define ULTRASONIC_MEASUREMENT_INTERVAL_MS 50

#define DISTANCE_STOP_CM 10          
#define DISTANCE_SLOW_1_CM 25        
#define DISTANCE_SLOW_2_CM 50       
#define DISTANCE_SLOW_3_CM 100       

#define NEUTRAL_PULSE_LENGTH 1500
#define MAX_SPEED_DISPLAY 150

// ================================================================
// ===                    GLOBALE VARIABLEN                     ===
// ================================================================
static float currentSpeedLimitFactor = 1.0;
static uint32_t lastUltrasonicMeasurement_ms = 0;
static int currentSpeedPulse = NEUTRAL_PULSE_LENGTH;
static bool firstRun = true;

// ================================================================
// ===                    HELFER FUNKTIONEN                     ===
// ================================================================

// Fortschrittsbalken zeichnen
String drawBar(int value, int maxValue, int width = 25) {
    String bar = "[";
    float percent = (float)abs(value) / maxValue;
    if (percent > 1.0) percent = 1.0;
    
    int fillLen = percent * width;
    
    for (int i = 0; i < width; i++) {
        if (i < fillLen) {
            bar += "=";  // Einfaches '=' für bessere Kompatibilität
        } else {
            bar += "-";
        }
    }
    bar += "]";
    return bar;
}

// Cursor zum Anfang bewegen (ohne Bildschirm zu löschen)
void resetCursor() {
    Serial.print("\033[H");  // Cursor nach oben links
}

// Bildschirm initial löschen
void clearScreenOnce() {
    Serial.print("\033[2J\033[H");
}

float measureDistanceAndGetSpeedFactor(float &measuredDistance) {
    if ((millis() - lastUltrasonicMeasurement_ms) < ULTRASONIC_MEASUREMENT_INTERVAL_MS) {
        return currentSpeedLimitFactor;
    }
    lastUltrasonicMeasurement_ms = millis();
    
    digitalWrite(PIN_ULTRASONIC_FRONT_TRIG, LOW);
    delayMicroseconds(2);
    digitalWrite(PIN_ULTRASONIC_FRONT_TRIG, HIGH);
    delayMicroseconds(10);
    digitalWrite(PIN_ULTRASONIC_FRONT_TRIG, LOW);
    
    long duration = pulseIn(PIN_ULTRASONIC_FRONT_ECHO, HIGH, ULTRASONIC_TIMEOUT_US);
    
    if (duration == 0) {
        measuredDistance = 999.0;
    } else {
        measuredDistance = duration * 0.034 / 2.0;
    }
    
    float factor = 1.0;
    if (measuredDistance <= DISTANCE_STOP_CM) factor = 0.0;
    else if (measuredDistance <= DISTANCE_SLOW_1_CM) factor = 0.2;
    else if (measuredDistance <= DISTANCE_SLOW_2_CM) factor = 0.4;
    else if (measuredDistance <= DISTANCE_SLOW_3_CM) factor = 0.7;
    
    return factor;
}

int calculateMotorSpeed(int pulseLenght) {
    int baseSpeed = 0;
    if (pulseLenght < 1450) baseSpeed = map(pulseLenght, 1100, 1450, -70, 0);
    else if (pulseLenght > 1550) baseSpeed = map(pulseLenght, 1550, 1900, 0, 150);
    return baseSpeed;
}

int applySpeedLimitFactor(int motorSpeed, float factor) {
    if (motorSpeed > 0) return constrain(motorSpeed, 0, 150 * factor);
    else return constrain(motorSpeed, -70, 0);
}

// ================================================================
// ===              ANZEIGE (CLEAN IN-PLACE UPDATE)             ===
// ================================================================

void displayStatus(int pulse, int baseSpeed, int limitedSpeed, float distance, float factor) {
    // Beim ersten Mal Bildschirm löschen, danach nur Cursor zurücksetzen
    if (firstRun) {
        clearScreenOnce();
        firstRun = false;
    } else {
        resetCursor();
    }
    
    // HEADER
    Serial.print(COLOR_BOLD COLOR_CYAN);
    Serial.println("┌────────────────────────────────────────────────────────┐");
    Serial.println("│        NOTBREMSE TEST - EXPODROID SENSOR SYSTEM        │");
    Serial.println("└────────────────────────────────────────────────────────┘");
    Serial.print(COLOR_RESET);
    Serial.println();

    // RC PULSE INPUT
    Serial.print(COLOR_BLUE "RC Pulse: " COLOR_RESET);
    Serial.print(COLOR_BOLD);
    Serial.print(pulse);
    Serial.println(" µs      ");
    Serial.print(COLOR_RESET);
    Serial.println();

    // DISTANZ MESSUNG
    Serial.print(COLOR_BOLD "DISTANZ:" COLOR_RESET);
    Serial.print("  ");
    if (distance >= 999.0) {
        Serial.print("---.-- cm");
    } else {
        char buf[10];
        sprintf(buf, "%6.1f", distance);
        Serial.print(buf);
        Serial.print(" cm");
    }
    
    // Status-Anzeige
    Serial.print("  │  ");
    if (factor == 0.0) {
        Serial.print(COLOR_RED COLOR_BOLD "  STOP!  " COLOR_RESET);
    } else if (factor < 1.0) {
        Serial.print(COLOR_YELLOW COLOR_BOLD " LANGSAM " COLOR_RESET);
    } else {
        Serial.print(COLOR_GREEN COLOR_BOLD "  FREI   " COLOR_RESET);
    }
    Serial.println();
    Serial.println();

    // GESCHWINDIGKEIT
    Serial.print(COLOR_YELLOW COLOR_BOLD "MOTOR:" COLOR_RESET);
    Serial.println();
    
    // Basis-Geschwindigkeit
    Serial.print("  Basis:  ");
    char speedBuf[8];
    sprintf(speedBuf, "%4d", baseSpeed);
    Serial.print(speedBuf);
    Serial.println(" PWM");
    
    // Limitierte Geschwindigkeit
    Serial.print("  Limit:  ");
    if (limitedSpeed < baseSpeed && baseSpeed > 0) {
        Serial.print(COLOR_RED);
    } else {
        Serial.print(COLOR_GREEN);
    }
    sprintf(speedBuf, "%4d", limitedSpeed);
    Serial.print(speedBuf);
    Serial.print(COLOR_RESET);
    Serial.print(" PWM  ");
    
    // Faktor anzeigen
    Serial.print("(");
    Serial.print((int)(factor * 100));
    Serial.println("%)      ");
    
    // Balken
    Serial.print("  ");
    if (limitedSpeed < baseSpeed && baseSpeed > 0) {
        Serial.print(COLOR_RED);
    } else {
        Serial.print(COLOR_GREEN);
    }
    Serial.print(drawBar(limitedSpeed, 150));
    Serial.println(COLOR_RESET);
    Serial.println();

    // TRENNER
    Serial.println("────────────────────────────────────────────────────────");
    
    // ZUSAMMENFASSUNG
    Serial.print("Eingabe: ");
    Serial.print(baseSpeed);
    Serial.print(" → Faktor: ");
    Serial.print((int)(factor * 100));
    Serial.print("% → Final: ");
    if (limitedSpeed < baseSpeed && baseSpeed > 0) {
        Serial.print(COLOR_RED COLOR_BOLD);
    }
    Serial.print(limitedSpeed);
    Serial.println(COLOR_RESET "       ");
    
    Serial.println("────────────────────────────────────────────────────────");
    Serial.println();

    // STEUERUNG
    Serial.print(COLOR_CYAN COLOR_BOLD "STEUERUNG:" COLOR_RESET);
    Serial.println();
    Serial.println("  [0] Max     [5] Neutral   [1] Rückwärts");
    Serial.println("  [q] Beenden [2-9] Stufen                ");
    Serial.println();
    
    // Leere Zeilen zum Füllen des Bildschirms (verhindert Flackern)
    Serial.println("                                                        ");
    Serial.println("                                                        ");
}

void handleSerialInput() {
    if (Serial.available() > 0) {
        char input = Serial.read();
        if (input == '\n' || input == '\r') return;

        switch (input) {
            case '0': currentSpeedPulse = 2000; break;
            case '9': currentSpeedPulse = 1900; break;
            case '8': currentSpeedPulse = 1800; break;
            case '7': currentSpeedPulse = 1700; break;
            case '6': currentSpeedPulse = 1600; break;
            case '5': currentSpeedPulse = 1500; break;
            case '4': currentSpeedPulse = 1400; break;
            case '3': currentSpeedPulse = 1300; break;
            case '2': currentSpeedPulse = 1200; break;
            case '1': currentSpeedPulse = 1100; break;
            case 'q': 
            case 'Q':
                clearScreenOnce();
                Serial.println();
                Serial.println(COLOR_GREEN COLOR_BOLD "TEST BEENDET." COLOR_RESET);
                Serial.println();
                while(1) delay(1000); 
                break;
        }
    }
}

// ================================================================
// ===                    MAIN SETUP & LOOP                     ===
// ================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    pinMode(PIN_ULTRASONIC_FRONT_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_FRONT_ECHO, INPUT);
    digitalWrite(PIN_ULTRASONIC_FRONT_TRIG, LOW);
    
    delay(500);
}

void loop() {
    static uint32_t lastPrintTime = 0;
    const uint32_t PRINT_INTERVAL_MS = 150; // Etwas langsamer für Stabilität
    
    handleSerialInput();
    
    float measuredDistance = 0.0;
    currentSpeedLimitFactor = measureDistanceAndGetSpeedFactor(measuredDistance);
    
    int baseMotorSpeed = calculateMotorSpeed(currentSpeedPulse);
    int limitedMotorSpeed = applySpeedLimitFactor(baseMotorSpeed, currentSpeedLimitFactor);
    
    if (millis() - lastPrintTime >= PRINT_INTERVAL_MS) {
        displayStatus(currentSpeedPulse, baseMotorSpeed, limitedMotorSpeed, 
                     measuredDistance, currentSpeedLimitFactor);
        lastPrintTime = millis();
    }
    
    delay(10);
}