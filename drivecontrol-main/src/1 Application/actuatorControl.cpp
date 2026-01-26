/**
 * @author Maximilian Wendt
 * 
 * "Any fool can write code that a computer can understand. Good programmers write code that humans can understand."  ~ Martin Fowler, 2008
 *
 */
#include "actuatorController.h"

/******************************************************************************
 * Private Declarations
 ******************************************************************************/
#define AUTOPILOT_LED_BLINK_RATE_AVAILABLE_AND_ACTIVATED 25
#define AUTOPILOT_LED_BLINK_RATE_AVAILABLE 5
#define AUTOPILOT_LED_BLINK_RATE_AUOTOPILOT_OFF 5000

// EXPODROID: Diese Defines werden nicht mehr benötigt
// #define ELEVATOR 0
// #define AILERON 1
// #define RUDDER 2
// #define ENGINE 3
// #define FLAP 4
// #define NUMBER_OF_SERVOS 5

#define MINIMUM_VALID_PULSE_LENGTH_SERVO 1000
#define MAXIMUM_VALID_PULSE_LENGTH_SERVO 2000

// Failsafe-Parameter
#define SIGNAL_TIMEOUT_MS 200
#define NEUTRAL_PULSE_LENGTH 1500

// Exponentialfilter-Parameter
#define ACCELERATION_FILTER_ALPHA 0.15






// Ultraschall-Parameter
#define ULTRASONIC_TIMEOUT_US 30000  // 30ms für max. 5m Reichweite
#define DISTANCE_STOP_CM 10          // Unter 10cm: Stopp
#define DISTANCE_SLOW_1_CM 25        // Unter 25cm: Faktor 0.2
#define DISTANCE_SLOW_2_CM 50        // Unter 50cm: Faktor 0.4
#define DISTANCE_SLOW_3_CM 100       // Unter 100cm: Faktor 0.7

/******************************************************************************
 * Private Variables
 ******************************************************************************/
static bool moduleIsInitialised = false;

// EXPODROID: Servo-Arrays nicht mehr benötigt
// static Servo servo[NUMBER_OF_SERVOS];
// static uint8_t servoSignalPinsESP[NUMBER_OF_SERVOS] = {...};
// static uint8_t servoSignalPinsRC[NUMBER_OF_SERVOS] = {...};

// Failsafe-Variablen
static uint32_t lastValidSignalTime_ms = 0;
static bool signalValid = false;

// Exponentialfilter-Variablen
static float filteredLeftMotorSpeed = 0.0;
static float filteredRightMotorSpeed = 0.0;


// Ultraschall-Variablen
static float currentSpeedLimitFactor = 1.0;
static uint32_t lastUltrasonicMeasurement_ms = 0;
#define ULTRASONIC_MEASUREMENT_INTERVAL_MS 50  // Alle 50ms messen




/******************************************************************************
 * Private Helper Functions
 ******************************************************************************/
static float measureDistanceAndGetSpeedFactor(){
    // Nur alle 50ms neue Messung
    if((millis() - lastUltrasonicMeasurement_ms) < ULTRASONIC_MEASUREMENT_INTERVAL_MS){
        return currentSpeedLimitFactor;
    }
    lastUltrasonicMeasurement_ms = millis();
    
    // Funktion zum Messen eines Sensors
    auto measureDistance = [](uint8_t trigPin, uint8_t echoPin) -> float {
        digitalWrite(trigPin, LOW);
        delayMicroseconds(2);
        digitalWrite(trigPin, HIGH);
        delayMicroseconds(10);
        digitalWrite(trigPin, LOW);
        
        long duration = pulseIn(echoPin, HIGH, ULTRASONIC_TIMEOUT_US);
        if(duration == 0) return 999.0;  // Kein Echo = weit weg
        return duration * 0.034 / 2.0;   // cm
    };
    
    // Alle 3 Sensoren messen
    float frontDist = measureDistance(PIN_ULTRASONIC_FRONT_TRIG, PIN_ULTRASONIC_FRONT_ECHO);
    float leftDist = measureDistance(PIN_ULTRASONIC_LEFT_TRIG, PIN_ULTRASONIC_LEFT_ECHO);
    float rightDist = measureDistance(PIN_ULTRASONIC_RIGHT_TRIG, PIN_ULTRASONIC_RIGHT_ECHO);
    
    // Minimum-Distanz finden (kritischster Sensor)
    float minDistance = min(frontDist, min(leftDist, rightDist));
    
    // Debug-Ausgabe
    Serial.print(" US-F:"); Serial.print(frontDist);
    Serial.print(" L:"); Serial.print(leftDist);
    Serial.print(" R:"); Serial.print(rightDist);
    
    // Faktor bestimmen
    float factor = 1.0;
    if(minDistance <= DISTANCE_STOP_CM){
        factor = 0.0;  // STOPP!
    } else if(minDistance <= DISTANCE_SLOW_1_CM){
        factor = 0.2;
    } else if(minDistance <= DISTANCE_SLOW_2_CM){
        factor = 0.4;
    } else if(minDistance <= DISTANCE_SLOW_3_CM){
        factor = 0.7;
    }
    
    return factor;
}
/******************************************************************************
 * Public Functions
 ******************************************************************************/
void actuatorControllerInit(){
    // EXPODROID PWM-Pins konfigurieren
    pinMode(PIN_BTS1_RPWM, OUTPUT);
    pinMode(PIN_BTS1_LPWM, OUTPUT);
    pinMode(PIN_BTS2_RPWM, OUTPUT);
    pinMode(PIN_BTS2_LPWM, OUTPUT);

    // Nur die 2 benötigten RC-Eingänge
    pinMode(PIN_SPEED_RECEIVED_RC, INPUT);    // Für Lenkung
    pinMode(PIN_AILERON_RECEIVED_RC, INPUT);  // Für Geschwindigkeit

    pinMode(PIN_AUTOPILOT_LED, OUTPUT);

    // Failsafe initialisieren
    lastValidSignalTime_ms = millis();
    signalValid = false;



     // Ultraschall-Pins konfigurieren
    pinMode(PIN_ULTRASONIC_FRONT_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_FRONT_ECHO, INPUT);
    pinMode(PIN_ULTRASONIC_LEFT_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_LEFT_ECHO, INPUT);
    pinMode(PIN_ULTRASONIC_RIGHT_TRIG, OUTPUT);
    pinMode(PIN_ULTRASONIC_RIGHT_ECHO, INPUT);
    
    moduleIsInitialised = true;
}

void actuatorControllerForwardSignals(){
    static uint8_t autopilotLEDState = 0;
    static uint32_t autopilotLEDCounter = 0;
    uint8_t autopilotState = flightguidanceCommunicatorGetAutopilotState();

    // EXPODROID: Variablen für Motor-Steuerung
    static int currentSteeringPulse = NEUTRAL_PULSE_LENGTH;
    static int currentSpeedPulse = NEUTRAL_PULSE_LENGTH;
    
    // Temporäre Variablen für neue Signale
    int newSteeringPulse = 0;
    int newSpeedPulse = 0;
    bool receivedSteeringSignal = false;
    bool receivedSpeedSignal = false;

    autopilotLEDCounter++;

    // Autopilot LED Steuerung
    if(autopilotState == AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE && autopilotLEDCounter >= AUTOPILOT_LED_BLINK_RATE_AVAILABLE){
        autopilotLEDState ^= 1;
        digitalWrite(PIN_AUTOPILOT_LED, autopilotLEDState);
        autopilotLEDCounter = 0;
    }
    else if(autopilotState == AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE_AND_ACTIVATED){
        digitalWrite(PIN_AUTOPILOT_LED, HIGH);
    }
    else if(autopilotState == AUTOPILOT_COMMUNICATOR_AUTOPILOT_NOT_AVAILABLE){
        digitalWrite(PIN_AUTOPILOT_LED, LOW);
    }

    // EXPODROID: Direkte Abfrage der 2 benötigten RC-Kanäle
    // Lenkung (früher ENGINE/SPEED_RC)
    uint16_t steeringPulse = pulseIn(PIN_SPEED_RECEIVED_RC, HIGH, TIMEOUT_SERVO_SIGNAL_HIGH_US);
    if(steeringPulse >= MINIMUM_VALID_PULSE_LENGTH_SERVO && steeringPulse <= MAXIMUM_VALID_PULSE_LENGTH_SERVO){
        newSteeringPulse = steeringPulse;
        receivedSteeringSignal = true;
    }
    
    // Geschwindigkeit (früher AILERON_RC)
    uint16_t speedPulse = pulseIn(PIN_AILERON_RECEIVED_RC, HIGH, TIMEOUT_SERVO_SIGNAL_HIGH_US);
    if(speedPulse >= MINIMUM_VALID_PULSE_LENGTH_SERVO && speedPulse <= MAXIMUM_VALID_PULSE_LENGTH_SERVO){
        newSpeedPulse = speedPulse;
        receivedSpeedSignal = true;
    }

    // Failsafe-Mechanismus
    if(receivedSteeringSignal && receivedSpeedSignal){
        currentSteeringPulse = newSteeringPulse;
        currentSpeedPulse = newSpeedPulse;
        lastValidSignalTime_ms = millis();
        signalValid = true;
    }
    else{
        if((millis() - lastValidSignalTime_ms) > SIGNAL_TIMEOUT_MS){
            currentSteeringPulse = NEUTRAL_PULSE_LENGTH;
            currentSpeedPulse = NEUTRAL_PULSE_LENGTH;
            signalValid = false;
            Serial.println("FAILSAFE ACTIVATED - Signal lost!");
        }
    }

    // EXPODROID: Motor-Steuerung
    int baseSpeed = 0;
    if(currentSpeedPulse < 1450) {
        baseSpeed = map(currentSpeedPulse, 1100, 1450, -70, 0);
    } else if(currentSpeedPulse > 1550) {
        baseSpeed = map(currentSpeedPulse, 1550, 1900, 0, 150);
    } else {
        baseSpeed = 0;
    }
    
    int steering = 0;
    if(currentSteeringPulse < 1450) {
        steering = map(currentSteeringPulse, 1000, 1450, -100, 0);
    } else if(currentSteeringPulse > 1550) {
        steering = map(currentSteeringPulse, 1550, 2000, 0, 100);
    }
    
    // Differential Berechnung
    int leftMotorSpeed = baseSpeed;
    int rightMotorSpeed = baseSpeed;
    
    if(steering > 0) {
        rightMotorSpeed = baseSpeed * (100 - steering) / 100;
    } else if(steering < 0) {
        leftMotorSpeed = baseSpeed * (100 + steering) / 100;
    }
    

//Ultraschall Werte auslesen
currentSpeedLimitFactor = measureDistanceAndGetSpeedFactor();

// Begrenzung: Nur bei Vorwärtsfahrt (positive Geschwindigkeit)
if(leftMotorSpeed > 0){
    leftMotorSpeed = constrain(leftMotorSpeed, 0, 150 * currentSpeedLimitFactor);
} else {
    leftMotorSpeed = constrain(leftMotorSpeed, -70, 0);  // Rückwärts wie gemappt
}

if(rightMotorSpeed > 0){
    rightMotorSpeed = constrain(rightMotorSpeed, 0, 150 * currentSpeedLimitFactor);
} else {
    rightMotorSpeed = constrain(rightMotorSpeed, -70, 0);  // Rückwärts wie gemappt
}

Serial.print(" Factor:"); Serial.print(currentSpeedLimitFactor);






    // Exponentialfilter
    if(abs(leftMotorSpeed) > abs(filteredLeftMotorSpeed)){
        filteredLeftMotorSpeed = filteredLeftMotorSpeed + ACCELERATION_FILTER_ALPHA * (leftMotorSpeed - filteredLeftMotorSpeed);
    } else {
        filteredLeftMotorSpeed = leftMotorSpeed;
    }
    
    if(abs(rightMotorSpeed) > abs(filteredRightMotorSpeed)){
        filteredRightMotorSpeed = filteredRightMotorSpeed + ACCELERATION_FILTER_ALPHA * (rightMotorSpeed - filteredRightMotorSpeed);
    } else {
        filteredRightMotorSpeed = rightMotorSpeed;
    }
    
    int finalLeftMotorSpeed = (int)filteredLeftMotorSpeed;
    int finalRightMotorSpeed = (int)filteredRightMotorSpeed;
    
    // Motor 1 (Links)
    if(finalLeftMotorSpeed > 0) {
        analogWrite(PIN_BTS1_RPWM, abs(finalLeftMotorSpeed));
        analogWrite(PIN_BTS1_LPWM, 0);
    } else if(finalLeftMotorSpeed < 0) {
        analogWrite(PIN_BTS1_RPWM, 0);
        analogWrite(PIN_BTS1_LPWM, abs(finalLeftMotorSpeed));
    } else {
        analogWrite(PIN_BTS1_RPWM, 0);
        analogWrite(PIN_BTS1_LPWM, 0);
    }
    
    // Motor 2 (Rechts)
    if(finalRightMotorSpeed > 0) {
        analogWrite(PIN_BTS2_RPWM, abs(finalRightMotorSpeed));
        analogWrite(PIN_BTS2_LPWM, 0);
    } else if(finalRightMotorSpeed < 0) {
        analogWrite(PIN_BTS2_RPWM, 0);
        analogWrite(PIN_BTS2_LPWM, abs(finalRightMotorSpeed));
    } else {
        analogWrite(PIN_BTS2_RPWM, 0);
        analogWrite(PIN_BTS2_LPWM, 0);
    }
    
    // Debug-Ausgabe
    Serial.print("AIL:"); Serial.print(currentSpeedPulse);
    Serial.print(" ENG:"); Serial.print(currentSteeringPulse);
    Serial.print(" Base:"); Serial.print(baseSpeed);
    Serial.print(" Steer:"); Serial.print(steering);
    Serial.print(" L:"); Serial.print(finalLeftMotorSpeed);
    Serial.print(" R:"); Serial.print(finalRightMotorSpeed);
    Serial.print(" Valid:"); Serial.println(signalValid ? "YES" : "NO");
}

#ifdef DEBUG_PRINT_RECEIVED_ANGLES
void actuatorControllerPrintAngles(){
    
    #define INITIAL_VALUE 90

    Servo servo[NUMBER_OF_SERVOS];
    uint32_t maximumAngleLimit[NUMBER_OF_SERVOS+1] = {INITIAL_VALUE,INITIAL_VALUE,INITIAL_VALUE,INITIAL_VALUE,INITIAL_VALUE,INITIAL_VALUE};
    uint32_t minimumAngleLimit[NUMBER_OF_SERVOS+1] = {INITIAL_VALUE,INITIAL_VALUE,INITIAL_VALUE,INITIAL_VALUE,INITIAL_VALUE,INITIAL_VALUE};
    uint32_t currentValues[NUMBER_OF_SERVOS+1];
    uint32_t currentPulseLengths[NUMBER_OF_SERVOS+1];

    //Elevator,Aileron,Rudder,Engine,Flap, A_E

    uint16_t looper=0 ;
    
    for(;;looper++){
        for(int i = 0; i < NUMBER_OF_SERVOS; i++){
            currentPulseLengths[i] = pulseIn(servoSignalPinsRC[i], HIGH, TIMEOUT_SERVO_SIGNAL_HIGH_US);
            currentValues[i] = map(currentPulseLengths[i],MINIMUM_VALID_PULSE_LENGTH_SERVO,MAXIMUM_VALID_PULSE_LENGTH_SERVO,MINIMUM_ANGLE_SERVO,MAXIMUM_ANGLE_SERVO);
            if(currentValues[i] >  maximumAngleLimit[i] && currentValues[i] <= MAXIMUM_ANGLE_SERVO) maximumAngleLimit[i] = currentValues[i];//check for max values
            if(currentValues[i] <  minimumAngleLimit[i] && currentValues[i] >= MINIMUM_ANGLE_SERVO) minimumAngleLimit[i] = currentValues[i]; //check for min values
        }
        //for autopilot enable
        currentPulseLengths[5] = pulseIn(PIN_AUTOPILOT_ENABLE, HIGH, TIMEOUT_SERVO_SIGNAL_HIGH_US);
        currentValues[5] = map(currentPulseLengths[5],MINIMUM_VALID_PULSE_LENGTH_SERVO,MAXIMUM_VALID_PULSE_LENGTH_SERVO,MINIMUM_ANGLE_SERVO,MAXIMUM_ANGLE_SERVO);
        if(currentValues[5] >  maximumAngleLimit[5] && currentValues[5] <= MAXIMUM_ANGLE_SERVO) maximumAngleLimit[5] = currentValues[5];//check for max values
        if(currentValues[5] <  minimumAngleLimit[5] && currentValues[5] >= MINIMUM_ANGLE_SERVO) minimumAngleLimit[5] = currentValues[5]; //check for min values

        
        if(looper > 10){
            looper = 0;
            Serial.print("const static uint8_t maximumAngleLimit[NUMBER_OF_SERVOS] = {");
            for(int i = 0; i< NUMBER_OF_SERVOS+1; i++){
                Serial.print(maximumAngleLimit[i]);
                if(i != NUMBER_OF_SERVOS) Serial.print(",");
            }
            Serial.println("};");

            Serial.print("const static uint8_t minimumAngleLimit[NUMBER_OF_SERVOS] = {");
            for(int i = 0; i< NUMBER_OF_SERVOS+1; i++){
                Serial.print(minimumAngleLimit[i]);
                if(i != NUMBER_OF_SERVOS) Serial.print(",");
            }
            Serial.println("};");

            Serial.print("const static uint8_t neutralAngle[NUMBER_OF_SERVOS] = {");
            for(int i = 0; i< NUMBER_OF_SERVOS+1; i++){
                Serial.print(currentValues[i]);
                if(i != NUMBER_OF_SERVOS) Serial.print(",");
            }
            Serial.println("};\n");

            Serial.print("currentPulseLengths = {");
            for(int i = 0; i< NUMBER_OF_SERVOS+1; i++){
                Serial.print(currentPulseLengths[i]);
                if(i != NUMBER_OF_SERVOS) Serial.print(",");
            }
            Serial.println("};\n");

        }

    }
}
#endif