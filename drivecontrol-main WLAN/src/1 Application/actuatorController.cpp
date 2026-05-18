/**
 * actuatorController.cpp
 *
 * EXPODROID-Variante OHNE RC-Empfaenger und OHNE Ultraschall.
 * Steuerung erfolgt jetzt ueber webController (WiFi/WebSocket).
 *
 * Was bleibt:
 *  - Motorlogik (BTS7960), Differential-Steering
 *  - Exponentialfilter fuer Beschleunigung (jetzt ruckelfrei)
 *  - Failsafe (Timeout 200 ms => alles auf 0)
 *  - Wave-Animation (jetzt durch Web-Buttons getriggert)
 *  - Kopfservo (PIN_HEAD_SERVO) wird proportional zum Slider angesteuert
 */

#include "actuatorController.h"
#include "webController.h"

/******************************************************************************
 *  Konfiguration
 ******************************************************************************/
#define AUTOPILOT_LED_BLINK_RATE_AVAILABLE_AND_ACTIVATED 25
#define AUTOPILOT_LED_BLINK_RATE_AVAILABLE                5
#define AUTOPILOT_LED_BLINK_RATE_AUOTOPILOT_OFF        5000

// Failsafe
#define SIGNAL_TIMEOUT_MS  200

// Exponentialfilter
#define ACCELERATION_FILTER_ALPHA  0.15f

// Motor-Limits
#define MAX_PWM_FORWARD   200
#define MAX_PWM_REVERSE    60

// Kopfservo
#define HEAD_SERVO_MIN_ANGLE   0
#define HEAD_SERVO_MAX_ANGLE 180

// Wave (Winkfunktion)
#define WAVE_ARM_UP_ANGLE        120
#define WAVE_ARM_DOWN_ANGLE       20
#define WAVE_FINGER_OPEN_ANGLE    90
#define WAVE_FINGER_CLOSED_ANGLE  30
#define WAVE_ARM_SPEED             5
#define WAVE_FINGER_SPEED         20
#define WAVE_FINGER_CLICK_DELAY_MS 150

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
 *  Interne Variablen
 ******************************************************************************/
static bool moduleIsInitialised = false;

static float    filteredLeftMotorSpeed  = 0.0f;
static float    filteredRightMotorSpeed = 0.0f;

// Wave-Servos
static Servo shoulderServoRight, shoulderServoLeft;
static Servo wristServoRight,    wristServoLeft;

// Kopfservo
static Servo headServo;
static int   lastHeadAngleWritten = -1;

// Wave-State
static WaveState waveStateRight = WAVE_IDLE;
static WaveState waveStateLeft  = WAVE_IDLE;
static float currentShoulderAngleRight = WAVE_ARM_DOWN_ANGLE;
static float currentShoulderAngleLeft  = WAVE_ARM_DOWN_ANGLE;
static float currentWristAngleRight    = WAVE_FINGER_OPEN_ANGLE;
static float currentWristAngleLeft     = WAVE_FINGER_OPEN_ANGLE;
static uint32_t waveTimerRight = 0, waveTimerLeft = 0;

/******************************************************************************
 *  Helper: Wave-Animation (nicht-blockierend)
 ******************************************************************************/
static void updateWaveAnimation(WaveState &waveState,
                                float &shoulderAngle, float &wristAngle,
                                uint32_t &timer,
                                Servo &shoulderServo, Servo &wristServo){
  switch(waveState){
    case WAVE_IDLE: break;

    case WAVE_ARM_RAISING:
      if(shoulderAngle < WAVE_ARM_UP_ANGLE){
        shoulderAngle += WAVE_ARM_SPEED;
        if(shoulderAngle >= WAVE_ARM_UP_ANGLE){
          shoulderAngle = WAVE_ARM_UP_ANGLE;
          waveState = WAVE_FINGER_CLICK_1_CLOSE;
          timer = millis();
        }
      }
      shoulderServo.write((int)shoulderAngle);
      break;

    case WAVE_FINGER_CLICK_1_CLOSE:
      if(wristAngle > WAVE_FINGER_CLOSED_ANGLE){
        wristAngle -= WAVE_FINGER_SPEED;
        if(wristAngle <= WAVE_FINGER_CLOSED_ANGLE){ wristAngle = WAVE_FINGER_CLOSED_ANGLE; timer = millis(); }
      } else if(millis() - timer >= WAVE_FINGER_CLICK_DELAY_MS){
        waveState = WAVE_FINGER_CLICK_1_OPEN;
      }
      wristServo.write((int)wristAngle);
      break;

    case WAVE_FINGER_CLICK_1_OPEN:
      if(wristAngle < WAVE_FINGER_OPEN_ANGLE){
        wristAngle += WAVE_FINGER_SPEED;
        if(wristAngle >= WAVE_FINGER_OPEN_ANGLE){ wristAngle = WAVE_FINGER_OPEN_ANGLE; timer = millis(); }
      } else if(millis() - timer >= WAVE_FINGER_CLICK_DELAY_MS){
        waveState = WAVE_FINGER_CLICK_2_CLOSE;
      }
      wristServo.write((int)wristAngle);
      break;

    case WAVE_FINGER_CLICK_2_CLOSE:
      if(wristAngle > WAVE_FINGER_CLOSED_ANGLE){
        wristAngle -= WAVE_FINGER_SPEED;
        if(wristAngle <= WAVE_FINGER_CLOSED_ANGLE){ wristAngle = WAVE_FINGER_CLOSED_ANGLE; timer = millis(); }
      } else if(millis() - timer >= WAVE_FINGER_CLICK_DELAY_MS){
        waveState = WAVE_FINGER_CLICK_2_OPEN;
      }
      wristServo.write((int)wristAngle);
      break;

    case WAVE_FINGER_CLICK_2_OPEN:
      if(wristAngle < WAVE_FINGER_OPEN_ANGLE){
        wristAngle += WAVE_FINGER_SPEED;
        if(wristAngle >= WAVE_FINGER_OPEN_ANGLE){
          wristAngle = WAVE_FINGER_OPEN_ANGLE;
          waveState = WAVE_ARM_LOWERING;
        }
      }
      wristServo.write((int)wristAngle);
      break;

    case WAVE_ARM_LOWERING:
      if(shoulderAngle > WAVE_ARM_DOWN_ANGLE){
        shoulderAngle -= WAVE_ARM_SPEED;
        if(shoulderAngle <= WAVE_ARM_DOWN_ANGLE){
          shoulderAngle = WAVE_ARM_DOWN_ANGLE;
          waveState = WAVE_IDLE;
        }
      }
      shoulderServo.write((int)shoulderAngle);
      break;
  }
}

/******************************************************************************
 *  Public API
 ******************************************************************************/
void actuatorControllerInit(){
  // BTS7960 Motortreiber
  pinMode(PIN_BTS1_RPWM, OUTPUT); pinMode(PIN_BTS1_LPWM, OUTPUT);
  pinMode(PIN_BTS2_RPWM, OUTPUT); pinMode(PIN_BTS2_LPWM, OUTPUT);

  // LED
  pinMode(PIN_AUTOPILOT_LED, OUTPUT);

  // Wave-Servos
  shoulderServoRight.attach(PIN_SHOULDER_SERVO_RIGHT);
  shoulderServoLeft .attach(PIN_SHOULDER_SERVO_LEFT);
  wristServoRight   .attach(PIN_WRIST_SERVO_RIGHT);
  wristServoLeft    .attach(PIN_WRIST_SERVO_LEFT);
  shoulderServoRight.write(WAVE_ARM_DOWN_ANGLE);
  shoulderServoLeft .write(WAVE_ARM_DOWN_ANGLE);
  wristServoRight   .write(WAVE_FINGER_OPEN_ANGLE);
  wristServoLeft    .write(WAVE_FINGER_OPEN_ANGLE);

  // Kopfservo
  headServo.attach(PIN_HEAD_SERVO);
  headServo.write(90);
  lastHeadAngleWritten = 90;

  moduleIsInitialised = true;
}

void actuatorControllerForwardSignals(){
  static uint8_t  autopilotLEDState   = 0;
  static uint32_t autopilotLEDCounter = 0;

  uint8_t autopilotState = flightguidanceCommunicatorGetAutopilotState();
  autopilotLEDCounter++;

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

  // ---------------------------------------------------------------
  // 1) Eingaben vom Web-Controller
  // ---------------------------------------------------------------
  int  throttleInput = webControllerGetThrottle();   // -100..100
  int  steeringInput = webControllerGetSteering();   // -100..100
  int  headAngle     = webControllerGetHeadAngle();  // 0..180

  uint32_t now           = millis();
  uint32_t lastMsgAge_ms = now - webControllerGetLastMessageTime_ms();
  bool     signalValid   = webControllerHasClient() && (lastMsgAge_ms < SIGNAL_TIMEOUT_MS);

  if(!signalValid){
    throttleInput = 0;
    steeringInput = 0;
    
    static uint32_t lastFailsafeLog = 0;
    if(now - lastFailsafeLog > 1000){
      Serial.println(F("[FAILSAFE] Kein gueltiges Signal vom Handy."));
      lastFailsafeLog = now;
    }
  }

  // ---------------------------------------------------------------
  // 2) Wave-Befehle vom Web triggern (nur wenn keine Animation laeuft)
  // ---------------------------------------------------------------
  bool waveR = webControllerConsumeWaveRight();
  bool waveB = webControllerConsumeWaveBoth();

  if(waveR && waveStateRight == WAVE_IDLE && waveStateLeft == WAVE_IDLE){
    waveStateRight = WAVE_ARM_RAISING;
    Serial.println(F("[WAVE] Right"));
  }
  if(waveB && waveStateRight == WAVE_IDLE && waveStateLeft == WAVE_IDLE){
    waveStateRight = WAVE_ARM_RAISING;
    waveStateLeft  = WAVE_ARM_RAISING;
    Serial.println(F("[WAVE] Both"));
  }

  updateWaveAnimation(waveStateRight, currentShoulderAngleRight, currentWristAngleRight,
                      waveTimerRight, shoulderServoRight, wristServoRight);
  updateWaveAnimation(waveStateLeft,  currentShoulderAngleLeft,  currentWristAngleLeft,
                      waveTimerLeft,  shoulderServoLeft,  wristServoLeft);

  // ---------------------------------------------------------------
  // 3) Kopfservo proportional setzen
  // ---------------------------------------------------------------
  headAngle = constrain(headAngle, HEAD_SERVO_MIN_ANGLE, HEAD_SERVO_MAX_ANGLE);
  if(headAngle != lastHeadAngleWritten){
    headServo.write(headAngle);
    lastHeadAngleWritten = headAngle;
  }

  // ---------------------------------------------------------------
  // 4) Geschwindigkeit/Lenkung -> Motor-PWM
  // ---------------------------------------------------------------
  // throttleInput: -100..100  ->  -MAX_REV..+MAX_FWD
  int baseSpeed;
  if(throttleInput >= 0){
    baseSpeed = map(throttleInput,    0, 100, 0, MAX_PWM_FORWARD);
  } else {
    baseSpeed = map(throttleInput, -100,   0, -MAX_PWM_REVERSE, 0);
  }

  int leftMotorSpeed  = baseSpeed;
  int rightMotorSpeed = baseSpeed;

  if(steeringInput > 0){
    rightMotorSpeed = baseSpeed * (100 - steeringInput) / 100;
  } else if(steeringInput < 0){
    leftMotorSpeed  = baseSpeed * (100 + steeringInput) / 100;
  }

  if(leftMotorSpeed > 0)
    leftMotorSpeed  = constrain(leftMotorSpeed,  0, (int)(MAX_PWM_FORWARD));
  else
    leftMotorSpeed  = constrain(leftMotorSpeed,  -MAX_PWM_REVERSE, 0);

  if(rightMotorSpeed > 0)
    rightMotorSpeed = constrain(rightMotorSpeed, 0, (int)(MAX_PWM_FORWARD));
  else
    rightMotorSpeed = constrain(rightMotorSpeed, -MAX_PWM_REVERSE, 0);

  // Exponentialfilter (nur beim Beschleunigen, nicht beim Bremsen)
  if(abs(leftMotorSpeed) > abs(filteredLeftMotorSpeed)){
    filteredLeftMotorSpeed += ACCELERATION_FILTER_ALPHA * (leftMotorSpeed - filteredLeftMotorSpeed);
  } else {
    filteredLeftMotorSpeed = leftMotorSpeed;
  }
  if(abs(rightMotorSpeed) > abs(filteredRightMotorSpeed)){
    filteredRightMotorSpeed += ACCELERATION_FILTER_ALPHA * (rightMotorSpeed - filteredRightMotorSpeed);
  } else {
    filteredRightMotorSpeed = rightMotorSpeed;
  }

  int finalLeftMotorSpeed  = signalValid ? (int)filteredLeftMotorSpeed  : 0;
  int finalRightMotorSpeed = signalValid ? (int)filteredRightMotorSpeed : 0;

  // ---------------------------------------------------------------
  // 5) PWM ausgeben
  // ---------------------------------------------------------------
  // Motor 1 (Links)
  if(finalLeftMotorSpeed > 0){
    analogWrite(PIN_BTS1_RPWM, abs(finalLeftMotorSpeed));
    analogWrite(PIN_BTS1_LPWM, 0);
  } else if(finalLeftMotorSpeed < 0){
    analogWrite(PIN_BTS1_RPWM, 0);
    analogWrite(PIN_BTS1_LPWM, abs(finalLeftMotorSpeed));
  } else {
    analogWrite(PIN_BTS1_RPWM, 0);
    analogWrite(PIN_BTS1_LPWM, 0);
  }

  // Motor 2 (Rechts)
  if(finalRightMotorSpeed > 0){
    analogWrite(PIN_BTS2_RPWM, abs(finalRightMotorSpeed));
    analogWrite(PIN_BTS2_LPWM, 0);
  } else if(finalRightMotorSpeed < 0){
    analogWrite(PIN_BTS2_RPWM, 0);
    analogWrite(PIN_BTS2_LPWM, abs(finalRightMotorSpeed));
  } else {
    analogWrite(PIN_BTS2_RPWM, 0);
    analogWrite(PIN_BTS2_LPWM, 0);
  }

  // Optional: Debug
  // Serial.printf("Th:%4d St:%4d Hd:%3d  L:%4d R:%4d Valid:%d\n",
  //               throttleInput, steeringInput, headAngle,
  //               finalLeftMotorSpeed, finalRightMotorSpeed,
  //               signalValid);
}