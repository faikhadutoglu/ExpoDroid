/**
* @author 
*
* "Any fool can write code that a computer can understand. Good programmers write code that humans can understand." ~ Martin Fowler, 2008
*
*/
#include "actuatorController.h"

/******************************************************************************
* Private Declarations
******************************************************************************/
#define AUTOPILOT_LED_BLINK_RATE_AVAILABLE_AND_ACTIVATED 25
#define AUTOPILOT_LED_BLINK_RATE_AVAILABLE               5
#define AUTOPILOT_LED_BLINK_RATE_AUOTOPILOT_OFF          5000

// EXPODROID: Diese Defines werden nicht mehr benötigt
// #define ELEVATOR 0
// #define AILERON  1
// #define RUDDER   2
// #define ENGINE   3
// #define FLAP     4
// #define NUMBER_OF_SERVOS 5

#define MINIMUM_VALID_PULSE_LENGTH_SERVO 1000
#define MAXIMUM_VALID_PULSE_LENGTH_SERVO 2000

// Failsafe-Parameter
#define SIGNAL_TIMEOUT_MS 200
#define NEUTRAL_PULSE_LENGTH 1500

// Exponentialfilter-Parameter
#define ACCELERATION_FILTER_ALPHA 0.15

// Ultraschall-Parameter
#define ULTRASONIC_TIMEOUT_US 30000 // 30ms für max. 5m Reichweite
#define DISTANCE_STOP_CM   10       // Unter 10cm: Stopp
#define DISTANCE_SLOW_1_CM 25       // Unter 25cm: Faktor 0.2
#define DISTANCE_SLOW_2_CM 50       // Unter 50cm: Faktor 0.4
#define DISTANCE_SLOW_3_CM 100      // Unter 100cm: Faktor 0.7

// Winkfunktion-Parameter
#define WAVE_ARM_UP_ANGLE          120 // Arm hochgehoben
#define WAVE_ARM_DOWN_ANGLE        20  // Arm unten
#define WAVE_FINGER_OPEN_ANGLE     90  // Finger offen
#define WAVE_FINGER_CLOSED_ANGLE   30  // Finger geschlossen
#define WAVE_ARM_SPEED             2   // Langsame Armbewegung (Grad pro Zyklus)
#define WAVE_FINGER_SPEED          30  // Schnelle Fingerbewegung (Grad pro Zyklus)
#define WAVE_FINGER_CLICK_DELAY_MS 150 // Pause zwischen Klicks

// RC-Schwellen für Winkkanal (Pulsbreiten)
#define WAVE_PULSE_RIGHT_MIN 1800  // ~100
#define WAVE_PULSE_LEFT_MAX  1200  // ~-100

// Winkfunktion States
enum WaveState {
  WAVE_IDLE,
  WAVE_ARM_RAISING,
  WAVE_FINGER_CLICK_1_CLOSE,
  WAVE_FINGER_CLICK_1_OPEN,
  WAVE_FINGER_CLICK_2_CLOSE,
  WAVE_FINGER_CLICK_2_OPEN,
  WAVE_ARM_LOWERING
};

/******************************************************************************
* Private Variables
******************************************************************************/
static bool moduleIsInitialised = false;

// Failsafe-Variablen
static uint32_t lastValidSignalTime_ms = 0;
static bool signalValid = false;

// Exponentialfilter-Variablen
static float filteredLeftMotorSpeed = 0.0;
static float filteredRightMotorSpeed = 0.0;

// Ultraschall-Variablen
static float currentSpeedLimitFactor = 1.0;
static uint32_t lastUltrasonicMeasurement_ms = 0;
#define ULTRASONIC_MEASUREMENT_INTERVAL_MS 50 // Alle 50ms messen

// Servo-Objekte für Winkfunktion
static Servo shoulderServoRight;
static Servo shoulderServoLeft;
static Servo wristServoRight;
static Servo wristServoLeft;

// Winkfunktion-Variablen
static WaveState waveStateRight = WAVE_IDLE;
static WaveState waveStateLeft  = WAVE_IDLE;
static float currentShoulderAngleRight = WAVE_ARM_DOWN_ANGLE;
static float currentShoulderAngleLeft  = WAVE_ARM_DOWN_ANGLE;
static float currentWristAngleRight    = WAVE_FINGER_OPEN_ANGLE;
static float currentWristAngleLeft     = WAVE_FINGER_OPEN_ANGLE;
static uint32_t waveTimerRight = 0;
static uint32_t waveTimerLeft  = 0;

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
    if(duration == 0) return 999.0; // Kein Echo = weit weg
    return duration * 0.034f / 2.0f; // cm
  };

  // Alle 3 Sensoren messen
  float frontDist = measureDistance(PIN_ULTRASONIC_FRONT_TRIG, PIN_ULTRASONIC_FRONT_ECHO);
  float leftDist  = measureDistance(PIN_ULTRASONIC_LEFT_TRIG,  PIN_ULTRASONIC_LEFT_ECHO);
  float rightDist = measureDistance(PIN_ULTRASONIC_RIGHT_TRIG, PIN_ULTRASONIC_RIGHT_ECHO);

  // Minimum-Distanz finden (kritischster Sensor)
  float minDistance = min(frontDist, min(leftDist, rightDist));

  // Debug-Ausgabe
  Serial.print(" US-F:"); Serial.print(frontDist);
  Serial.print(" L:");     Serial.print(leftDist);
  Serial.print(" R:");     Serial.print(rightDist);

  // Faktor bestimmen
  float factor = 1.0f;
  if(minDistance <= DISTANCE_STOP_CM){
    factor = 0.0f; // STOPP!
  } else if(minDistance <= DISTANCE_SLOW_1_CM){
    factor = 0.2f;
  } else if(minDistance <= DISTANCE_SLOW_2_CM){
    factor = 0.4f;
  } else if(minDistance <= DISTANCE_SLOW_3_CM){
    factor = 0.7f;
  }

  return factor;
}

// Nicht-blockierende Winkfunktion für eine Hand
static void updateWaveAnimation(WaveState &waveState,
                                float &shoulderAngle,
                                float &wristAngle,
                                uint32_t &timer,
                                Servo &shoulderServo,
                                Servo &wristServo) {
  switch(waveState) {
    case WAVE_IDLE:
      // Nichts tun
      break;

    case WAVE_ARM_RAISING:
      if(shoulderAngle < WAVE_ARM_UP_ANGLE) {
        shoulderAngle += WAVE_ARM_SPEED;
        if(shoulderAngle >= WAVE_ARM_UP_ANGLE) {
          shoulderAngle = WAVE_ARM_UP_ANGLE;
          waveState = WAVE_FINGER_CLICK_1_CLOSE;
          timer = millis();
        }
      }
      shoulderServo.write((int)shoulderAngle);
      break;

    case WAVE_FINGER_CLICK_1_CLOSE:
      if(wristAngle > WAVE_FINGER_CLOSED_ANGLE) {
        wristAngle -= WAVE_FINGER_SPEED;
        if(wristAngle <= WAVE_FINGER_CLOSED_ANGLE) {
          wristAngle = WAVE_FINGER_CLOSED_ANGLE;
          timer = millis();
        }
      } else {
        if(millis() - timer >= WAVE_FINGER_CLICK_DELAY_MS) {
          waveState = WAVE_FINGER_CLICK_1_OPEN;
        }
      }
      wristServo.write((int)wristAngle);
      break;

    case WAVE_FINGER_CLICK_1_OPEN:
      if(wristAngle < WAVE_FINGER_OPEN_ANGLE) {
        wristAngle += WAVE_FINGER_SPEED;
        if(wristAngle >= WAVE_FINGER_OPEN_ANGLE) {
          wristAngle = WAVE_FINGER_OPEN_ANGLE;
          timer = millis();
        }
      } else {
        if(millis() - timer >= WAVE_FINGER_CLICK_DELAY_MS) {
          waveState = WAVE_FINGER_CLICK_2_CLOSE;
        }
      }
      wristServo.write((int)wristAngle);
      break;

    case WAVE_FINGER_CLICK_2_CLOSE:
      if(wristAngle > WAVE_FINGER_CLOSED_ANGLE) {
        wristAngle -= WAVE_FINGER_SPEED;
        if(wristAngle <= WAVE_FINGER_CLOSED_ANGLE) {
          wristAngle = WAVE_FINGER_CLOSED_ANGLE;
          timer = millis();
        }
      } else {
        if(millis() - timer >= WAVE_FINGER_CLICK_DELAY_MS) {
          waveState = WAVE_FINGER_CLICK_2_OPEN;
        }
      }
      wristServo.write((int)wristAngle);
      break;

    case WAVE_FINGER_CLICK_2_OPEN:
      if(wristAngle < WAVE_FINGER_OPEN_ANGLE) {
        wristAngle += WAVE_FINGER_SPEED;
        if(wristAngle >= WAVE_FINGER_OPEN_ANGLE) {
          wristAngle = WAVE_FINGER_OPEN_ANGLE;
          waveState = WAVE_ARM_LOWERING;
        }
      }
      wristServo.write((int)wristAngle);
      break;

    case WAVE_ARM_LOWERING:
      if(shoulderAngle > WAVE_ARM_DOWN_ANGLE) {
        shoulderAngle -= WAVE_ARM_SPEED;
        if(shoulderAngle <= WAVE_ARM_DOWN_ANGLE) {
          shoulderAngle = WAVE_ARM_DOWN_ANGLE;
          waveState = WAVE_IDLE;
        }
      }
      shoulderServo.write((int)shoulderAngle);
      break;
  }
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

  // RC-Eingänge
  pinMode(PIN_SPEED_RECEIVED_RC,   INPUT); // Lenkung
  pinMode(PIN_AILERON_RECEIVED_RC, INPUT); // Geschwindigkeit
  pinMode(PIN_FLAP_RECEIVED_RC,    INPUT); // Winkbefehl -100/0/100

  pinMode(PIN_AUTOPILOT_LED, OUTPUT);

  // Failsafe initialisieren
  lastValidSignalTime_ms = millis();
  signalValid = false;

  // Ultraschall-Pins konfigurieren
  pinMode(PIN_ULTRASONIC_FRONT_TRIG, OUTPUT);
  pinMode(PIN_ULTRASONIC_FRONT_ECHO, INPUT);
  pinMode(PIN_ULTRASONIC_LEFT_TRIG,  OUTPUT);
  pinMode(PIN_ULTRASONIC_LEFT_ECHO,  INPUT);
  pinMode(PIN_ULTRASONIC_RIGHT_TRIG, OUTPUT);
  pinMode(PIN_ULTRASONIC_RIGHT_ECHO, INPUT);

  // Servos für Winkfunktion initialisieren
  shoulderServoRight.attach(PIN_SHOULDER_SERVO_RIGHT);
  shoulderServoLeft.attach(PIN_SHOULDER_SERVO_LEFT);
  wristServoRight.attach(PIN_WRIST_SERVO_RIGHT);
  wristServoLeft.attach(PIN_WRIST_SERVO_LEFT);

  // Startposition
  shoulderServoRight.write(WAVE_ARM_DOWN_ANGLE);
  shoulderServoLeft.write(WAVE_ARM_DOWN_ANGLE);
  wristServoRight.write(WAVE_FINGER_OPEN_ANGLE);
  wristServoLeft.write(WAVE_FINGER_OPEN_ANGLE);

  moduleIsInitialised = true;
}

void actuatorControllerForwardSignals(){
  static uint8_t  autopilotLEDState   = 0;
  static uint32_t autopilotLEDCounter = 0;
  uint8_t autopilotState = flightguidanceCommunicatorGetAutopilotState();

  // EXPODROID: Variablen für Motor-Steuerung
  static int currentSteeringPulse = NEUTRAL_PULSE_LENGTH;
  static int currentSpeedPulse    = NEUTRAL_PULSE_LENGTH;
  static int currentWavePulse     = NEUTRAL_PULSE_LENGTH;

  // Temporäre Variablen für neue Signale
  int  newSteeringPulse  = 0;
  int  newSpeedPulse     = 0;
  int  newWavePulse      = 0;
  bool receivedSteering  = false;
  bool receivedSpeed     = false;
  bool receivedWave      = false;

  autopilotLEDCounter++;

  // Autopilot LED Steuerung
  if(autopilotState == AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE &&
     autopilotLEDCounter >= AUTOPILOT_LED_BLINK_RATE_AVAILABLE){
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

  // RC-Kanäle lesen
  // Lenkung
  uint16_t steeringPulse = pulseIn(PIN_SPEED_RECEIVED_RC, HIGH, TIMEOUT_SERVO_SIGNAL_HIGH_US);
  if(steeringPulse >= MINIMUM_VALID_PULSE_LENGTH_SERVO &&
     steeringPulse <= MAXIMUM_VALID_PULSE_LENGTH_SERVO){
    newSteeringPulse = steeringPulse;
    receivedSteering = true;
  }

  // Geschwindigkeit
  uint16_t speedPulse = pulseIn(PIN_AILERON_RECEIVED_RC, HIGH, TIMEOUT_SERVO_SIGNAL_HIGH_US);
  if(speedPulse >= MINIMUM_VALID_PULSE_LENGTH_SERVO &&
     speedPulse <= MAXIMUM_VALID_PULSE_LENGTH_SERVO){
    newSpeedPulse = speedPulse;
    receivedSpeed = true;
  }

  // Winkbefehl (−100 / 0 / 100)
  uint16_t wavePulse = pulseIn(PIN_FLAP_RECEIVED_RC, HIGH, TIMEOUT_SERVO_SIGNAL_HIGH_US);
  if(wavePulse >= MINIMUM_VALID_PULSE_LENGTH_SERVO &&
     wavePulse <= MAXIMUM_VALID_PULSE_LENGTH_SERVO){
    newWavePulse = wavePulse;
    receivedWave = true;
  }

  // Failsafe-Mechanismus: Nur Lenkung + Speed sind kritisch
  if(receivedSteering && receivedSpeed){
    currentSteeringPulse = newSteeringPulse;
    currentSpeedPulse    = newSpeedPulse;
    if(receivedWave) currentWavePulse = newWavePulse;
    lastValidSignalTime_ms = millis();
    signalValid = true;
  } else {
    if((millis() - lastValidSignalTime_ms) > SIGNAL_TIMEOUT_MS){
      currentSteeringPulse = NEUTRAL_PULSE_LENGTH;
      currentSpeedPulse    = NEUTRAL_PULSE_LENGTH;
      // Winkkanal wird im Zweifel ignoriert
      currentWavePulse     = NEUTRAL_PULSE_LENGTH;
      signalValid = false;
      Serial.println("FAILSAFE ACTIVATED - Signal lost!");
    }
  }

  // --- Winkbefehle aus RC-Wert ableiten ---
  // Rechte Hand winkt, wenn Kanal ~100 (hoher Puls)
  if(currentWavePulse >= WAVE_PULSE_RIGHT_MIN &&
     waveStateRight == WAVE_IDLE && waveStateLeft == WAVE_IDLE){
    waveStateRight = WAVE_ARM_RAISING;
    Serial.println("Wave: RIGHT hand");
  }

  // Beide Hände winken, wenn Kanal ~-100 (niedriger Puls)
  if(currentWavePulse <= WAVE_PULSE_LEFT_MAX &&
     waveStateRight == WAVE_IDLE && waveStateLeft == WAVE_IDLE){
    waveStateRight = WAVE_ARM_RAISING;
    waveStateLeft  = WAVE_ARM_RAISING;
    Serial.println("Wave: BOTH hands");
  }

  // Wichtig: Bei ~0 (Mitte = ca. 1500 µs) wird keine neue Animation gestartet,
  // laufende Animationen werden aber fertig abgespielt.

  // Nicht-blockierende Animationsupdates
  updateWaveAnimation(waveStateRight,
                      currentShoulderAngleRight,
                      currentWristAngleRight,
                      waveTimerRight,
                      shoulderServoRight,
                      wristServoRight);

  updateWaveAnimation(waveStateLeft,
                      currentShoulderAngleLeft,
                      currentWristAngleLeft,
                      waveTimerLeft,
                      shoulderServoLeft,
                      wristServoLeft);

  // --- Fahrlogik wie im Original ---
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

  int leftMotorSpeed  = baseSpeed;
  int rightMotorSpeed = baseSpeed;

  if(steering > 0) {
    rightMotorSpeed = baseSpeed * (100 - steering) / 100;
  } else if(steering < 0) {
    leftMotorSpeed = baseSpeed * (100 + steering) / 100;
  }

  // Ultraschall-Limitierung
  currentSpeedLimitFactor = measureDistanceAndGetSpeedFactor();

  if(leftMotorSpeed > 0){
    leftMotorSpeed = constrain(leftMotorSpeed, 0, (int)(150 * currentSpeedLimitFactor));
  } else {
    leftMotorSpeed = constrain(leftMotorSpeed, -70, 0);
  }

  if(rightMotorSpeed > 0){
    rightMotorSpeed = constrain(rightMotorSpeed, 0, (int)(150 * currentSpeedLimitFactor));
  } else {
    rightMotorSpeed = constrain(rightMotorSpeed, -70, 0);
  }

  Serial.print(" Factor:"); Serial.print(currentSpeedLimitFactor);

  // Exponentialfilter
  if(abs(leftMotorSpeed) > abs(filteredLeftMotorSpeed)){
    filteredLeftMotorSpeed = filteredLeftMotorSpeed +
      ACCELERATION_FILTER_ALPHA * (leftMotorSpeed - filteredLeftMotorSpeed);
  } else {
    filteredLeftMotorSpeed = leftMotorSpeed;
  }

  if(abs(rightMotorSpeed) > abs(filteredRightMotorSpeed)){
    filteredRightMotorSpeed = filteredRightMotorSpeed +
      ACCELERATION_FILTER_ALPHA * (rightMotorSpeed - filteredRightMotorSpeed);
  } else {
    filteredRightMotorSpeed = rightMotorSpeed;
  }

  int finalLeftMotorSpeed  = (int)filteredLeftMotorSpeed;
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
  Serial.print(" AIL:");  Serial.print(currentSpeedPulse);
  Serial.print(" ENG:");  Serial.print(currentSteeringPulse);
  Serial.print(" WAVE_P:"); Serial.print(currentWavePulse);
  Serial.print(" Base:"); Serial.print(baseSpeed);
  Serial.print(" Steer:"); Serial.print(steering);
  Serial.print(" L:");    Serial.print(finalLeftMotorSpeed);
  Serial.print(" R:");    Serial.print(finalRightMotorSpeed);
  Serial.print(" Valid:");Serial.println(signalValid ? "YES" : "NO");
}

#ifdef DEBUG_PRINT_RECEIVED_ANGLES
void actuatorControllerPrintAngles(){
  // Debug-Funktion aus Original – bei Bedarf anpassen
}
#endif

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