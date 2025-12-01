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

#define ELEVATOR 0
#define AILERON 1
#define RUDDER 2
#define ENGINE 3
#define FLAP 4
#define NUMBER_OF_SERVOS 5 //Elevator,Aileron,Rudder,Engine,Flap

#define MINIMUM_ANGLE_SERVO 0
#define MAXIMUM_ANGLE_SERVO 180
#define MINIMUM_VALID_PULSE_LENGTH_SERVO 1000
#define MAXIMUM_VALID_PULSE_LENGTH_SERVO 2000

#define FLAP_NEUTRAL_POSITION_PULSE_LENGTH_US 1091

/******************************************************************************
 * Private Variables
 ******************************************************************************/
static bool moduleIsInitialised = false;

static Servo servo[NUMBER_OF_SERVOS];
static uint8_t servoSignalPinsESP[NUMBER_OF_SERVOS] = {PIN_ELEVATOR_RECEIVED_ESP, PIN_AILERON_RECEIVED_ESP, PIN_RUDDER_RECEIVED_ESP, PIN_SPEED_RECEIVED_ESP, PIN_FLAP_RECEIVED_ESP};
static uint8_t servoSignalPinsRC[NUMBER_OF_SERVOS] = {PIN_ELEVATOR_RECEIVED_RC, PIN_AILERON_RECEIVED_RC, PIN_RUDDER_RECEIVED_RC, PIN_SPEED_RECEIVED_RC, PIN_FLAP_RECEIVED_RC};

/******************************************************************************
 * Public Functions
 ******************************************************************************/
void actuatorControllerInit(){
    // To read PWM Signals from ESP32/Autopilot
    // pinMode(PIN_ELEVATOR_RECEIVED_ESP, INPUT);
    // pinMode(PIN_SPEED_RECEIVED_ESP, INPUT);
    // pinMode(PIN_AILERON_RECEIVED_ESP, INPUT);
    // pinMode(PIN_RUDDER_RECEIVED_ESP, INPUT);
    // pinMode(PIN_FLAP_RECEIVED_ESP, INPUT); // Currently not used




    // EXPODROID PWM-Pins konfigurieren
    //Module 1 
    pinMode(PIN_BTS1_RPWM, OUTPUT);
    pinMode(PIN_BTS1_LPWM, OUTPUT);
    //Modul2
    pinMode(PIN_BTS2_RPWM, OUTPUT);
    pinMode(PIN_BTS2_LPWM, OUTPUT);




    // To read PWM Signals from Remote Control
    pinMode(PIN_ELEVATOR_RECEIVED_RC, INPUT);
    pinMode(PIN_SPEED_RECEIVED_RC, INPUT);
    pinMode(PIN_AILERON_RECEIVED_RC, INPUT);
    pinMode(PIN_RUDDER_RECEIVED_RC, INPUT);
    pinMode(PIN_FLAP_RECEIVED_RC, INPUT);
    
    // To write PWM signals to servos
    servo[ELEVATOR].attach(PIN_ELEVATOR_CONTROLLER);
    servo[AILERON].attach(PIN_AILERON_CONTROLLER);
    servo[RUDDER].attach(PIN_RUDDER_CONTROLLER);
    servo[ENGINE].attach(PIN_SPEED_CONTROLLER);
    servo[FLAP].attach(PIN_FLAP_CONTROLLER);

    pinMode(PIN_AUTOPILOT_LED, OUTPUT);

    moduleIsInitialised = true;
}

void actuatorControllerForwardSignals(){
    static uint32_t cnt = 0;
    static uint8_t autopilotLEDState = 0;
    static uint32_t autopilotLEDCounter = 0;
    uint16_t pulseLength_us = 0;
    uint8_t autopilotState = flightguidanceCommunicatorGetAutopilotState();

    // EXPODROID: Variablen für Motor-Steuerung
    static int lastSteeringPulse = 1500;
    static int lastSpeedPulse = 1500;

    autopilotLEDCounter++;

    if(autopilotState == AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE && autopilotLEDCounter >= AUTOPILOT_LED_BLINK_RATE_AVAILABLE){ //if autopilot is available
        autopilotLEDState ^= 1;
        digitalWrite(PIN_AUTOPILOT_LED,autopilotLEDState); //LED TOGGLES
        autopilotLEDCounter = 0;
    }
    else if(autopilotState == AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE_AND_ACTIVATED){ //if Autopilot is controlling
        digitalWrite(PIN_AUTOPILOT_LED,HIGH); //LED ON
    }
    else if(autopilotState == AUTOPILOT_COMMUNICATOR_AUTOPILOT_NOT_AVAILABLE){ //if autopilot was never enabled, or has a major error 
        digitalWrite(PIN_AUTOPILOT_LED,LOW); //LED OFF
    }

    for(int i = 0; i < NUMBER_OF_SERVOS-1; i++){ //Flap must be handled separately, because flight guidance does not send flap servo signals to flight control
        if(autopilotState == AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE_AND_ACTIVATED) pulseLength_us = pulseIn(servoSignalPinsESP[i], HIGH, TIMEOUT_SERVO_SIGNAL_HIGH_US ); // if Timeout is exceeded, 0 gets returned -> 0 is NOT the correct pulse Length
        else pulseLength_us = pulseIn(servoSignalPinsRC[i], HIGH, TIMEOUT_SERVO_SIGNAL_HIGH_US );
        if(pulseLength_us != 0){ 
            #ifdef DEBUG_CURRENT_SERVO_ANGLES
                #define PRINT_COUNTS 10
                if(cnt>PRINT_COUNTS){
                    switch(i){
                        case 0: 
                            Serial.println("ELE:"+String(pulseLength_us)+",");
                            break;
                        case 1: 
                            Serial.println("AIL:"+String(pulseLength_us)+",");
                            break;
                        case 2: 
                            Serial.println("RDR:"+String(pulseLength_us)+",");
                            break;
                        case 3: 
                            Serial.println("ENG:"+String(pulseLength_us)+",");
                            break;     
                    }
                }
            #endif

            // EXPODROID: Werte für Motor-Steuerung speichern (wir wollen dc Motoren ansteuern und keine Servos deswegen die Anpassung)
            if(i == ENGINE){
                lastSteeringPulse = pulseLength_us;
            }
            else if(i == AILERON){
                lastSpeedPulse = pulseLength_us;
            }
            
            // Normale Servos (ELEVATOR, RUDDER) direkt ansteuern signal was vom Sender kommt ohne änderung für Servos
            // ENGINE und AILERON werden später für Motoren verwendet
            if(i != ENGINE && i != AILERON){
                servo[i].writeMicroseconds(pulseLength_us);
            }
        }
    }

    // EXPODROID: Motor-Steuerung mit den gesammelten Werten
    // Berechne Basis-Geschwindigkeit (-255 bis +255)
    int baseSpeed = 0;
    if(lastSpeedPulse < 1450) {
        // Rückwärts: 1100-1450 -> -255 bis 0
        baseSpeed = map(lastSpeedPulse, 1100, 1450, -255, 0); //mappen bedeutet wert von einem bereich in einen anderen zu übertragen damit er von Motortreiber verstanden wird
    } else if(lastSpeedPulse > 1550) {
        // Vorwärts: 1550-1900 -> 0 bis 255
        baseSpeed = map(lastSpeedPulse, 1550, 1900, 0, 255);
    } else {
        // Deadzone: 1450-1550 = Stop
        baseSpeed = 0;
    }
    
    // Berechne Lenkung (-100 bis +100)
    int steering = 0;
    if(lastSteeringPulse < 1450) {
        // Links: 1000-1450 -> -100 bis 0
        steering = map(lastSteeringPulse, 1000, 1450, -100, 0);
    } else if(lastSteeringPulse > 1550) {
        // Rechts: 1550-2000 -> 0 bis +100
        steering = map(lastSteeringPulse, 1550, 2000, 0, 100);
    }
    
    // Differential Berechnung
    int leftMotorSpeed = baseSpeed;
    int rightMotorSpeed = baseSpeed;
    
    if(steering > 0) {
        // Rechts drehen: rechter Motor langsamer  //bissle: in der Praxis testen ob die Rädergeschwindigkeitsunterschied zu viel wird!!
        rightMotorSpeed = baseSpeed * (100 - steering) / 100;
    } else if(steering < 0) {
        // Links drehen: linker Motor langsamer
        leftMotorSpeed = baseSpeed * (100 + steering) / 100;
    }
    
    // Begrenzung auf -255 bis +255  
    const float batterie_factor = 0.91; //Begrenzung der Spannung // Spitzenspannung kann trzdem mehr als 12V sein??
    leftMotorSpeed = constrain(leftMotorSpeed, -255 * batterie_factor, 255 * batterie_factor);
    rightMotorSpeed = constrain(rightMotorSpeed, -255 * batterie_factor, 255 * batterie_factor);
    
    // Motor 1 (Links) ansteuern
    if(leftMotorSpeed > 0) {
        analogWrite(PIN_BTS1_RPWM, abs(leftMotorSpeed));
        analogWrite(PIN_BTS1_LPWM, 0);
    } else if(leftMotorSpeed < 0) {
        analogWrite(PIN_BTS1_RPWM, 0);
        analogWrite(PIN_BTS1_LPWM, abs(leftMotorSpeed));
    } else {
        analogWrite(PIN_BTS1_RPWM, 0);
        analogWrite(PIN_BTS1_LPWM, 0);
    }
    
    // Motor 2 (Rechts) ansteuern
    if(rightMotorSpeed > 0) {
        analogWrite(PIN_BTS2_RPWM, abs(rightMotorSpeed));
        analogWrite(PIN_BTS2_LPWM, 0);
    } else if(rightMotorSpeed < 0) {
        analogWrite(PIN_BTS2_RPWM, 0);
        analogWrite(PIN_BTS2_LPWM, abs(rightMotorSpeed));
    } else {
        analogWrite(PIN_BTS2_RPWM, 0);
        analogWrite(PIN_BTS2_LPWM, 0);
    }
    
    // Debug-Ausgabe für Motor-Steuerung (immer aktiv für Testen)
    Serial.print("AIL:"); Serial.print(lastSpeedPulse);
    Serial.print(" ENG:"); Serial.print(lastSteeringPulse);
    Serial.print(" Base:"); Serial.print(baseSpeed);
    Serial.print(" Steer:"); Serial.print(steering);
    Serial.print(" L:"); Serial.print(leftMotorSpeed);
    Serial.print(" R:"); Serial.println(rightMotorSpeed);

    #ifdef DEBUG_CURRENT_SERVO_ANGLES
    if(cnt++>PRINT_COUNTS){
        cnt = 0;
        Serial.println("");
    } 
    #endif

    if(autopilotState == AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE_AND_ACTIVATED){
        servo[FLAP].writeMicroseconds(FLAP_NEUTRAL_POSITION_PULSE_LENGTH_US);
    }
    else{
        pulseLength_us = pulseIn(servoSignalPinsRC[FLAP], HIGH, TIMEOUT_SERVO_SIGNAL_HIGH_US);
        servo[FLAP].writeMicroseconds(pulseLength_us);
    }
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