/**
 * @author Maximilian Wendt
 * 
 * "Any fool can write code that a computer can understand. Good programmers write code that humans can understand."  ~ Martin Fowler, 2008
 *
 */

#include "flightguidanceCommunicator.h"
/******************************************************************************
 * Defines
 ******************************************************************************/
/** #define MINIMUM_HIGH_PULSE_LENGTH_FOR_AUTOPILOT_ACTIVATED 1100
#define MAXIMUM_SEMAPHORE_BLOCKED_COUNTS_BEFORE_RETURNING_CONTROL_TO_PILOT 10
#define WAIT_FOR_SEMAPHORE_MS 4 //It is waited WAIT_FOR_SEMAPHORE_MS for a semaphore if it is blocked 

#define HIGHEST_VALID_NON_RESPONSE_TIME 200 //in ms
#define WAITING_TIME_MS 20
#define MAXIMUM_NUMER_OF_RECEIVED_BYTES 1000
/******************************************************************************
 * Private Variables
 ******************************************************************************/
/**static bool moduleIsInitialised = false;
static bool autopilotResponds = false; 
static bool autopilotActivated = false;
static bool autopilotCrashed = false;
static bool isConnectionEstablished = false;

static uint32_t pulseDuration = 0; // Variable to store the length of a high pulse
static SemaphoreHandle_t pulseDurationSemaphore;
/******************************************************************************
 * Private Functions
 ******************************************************************************/
// clears RX Buffer(.flush() only clears TX Buffer)
/**
void serialFlush(){
    uint32_t counter = 0;
    while(Serial1.available() > 0) {
        if(++counter >= MAXIMUM_NUMER_OF_RECEIVED_BYTES){ // UART Communication Failure: Overproduction (Autopilot transmits messages faster than Autocopilot can process)
            autopilotCrashed = true;
        }
        char t = Serial1.read();
    }
}

//measures the length of a signal pulse
void handlePulse() {
    static uint32_t pulseStartTime; // Variable to store the start time of the pulse
    static uint32_t localPulseDuration; // Variable to store the length of the pulse locally

    if (digitalRead(PIN_AUTOPILOT_ENABLE) == HIGH) { // If the pin is now high, it's the start of a new pulse
    pulseStartTime = micros(); // Record the start time of the pulse
    } 
    else { // If the pin is now low, the pulse has ended
        localPulseDuration = micros() - pulseStartTime; //buffer to block semaphore shortest as possible
        
        if(xSemaphoreTakeFromISR(pulseDurationSemaphore,NULL) == pdTRUE){
            pulseDuration = localPulseDuration; 
            xSemaphoreGiveFromISR(pulseDurationSemaphore,NULL);
        }
    }
}

*/
/******************************************************************************
 * Public Functions
 ******************************************************************************/
/**
void flightguidanceCommunicatorInit(){
    pinMode(PIN_AUTOPILOT_ENABLE, INPUT);
    attachInterrupt(digitalPinToInterrupt(PIN_AUTOPILOT_ENABLE), handlePulse, CHANGE); // Attach interrupt to handle pulse changes
    pulseDurationSemaphore = xSemaphoreCreateMutex();

    Serial1.begin(9600, SERIAL_8N1, PIN_PILOT_STATUS_RX);
    
    moduleIsInitialised = true;
    
}

uint8_t flightguidanceCommunicatorGetAutopilotState(){
    uint8_t retVal = AUTOPILOT_COMMUNICATOR_AUTOPILOT_NOT_AVAILABLE;

    if(moduleIsInitialised && autopilotResponds && autopilotCrashed == false){
        if(autopilotActivated){
            retVal = AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE_AND_ACTIVATED;
        }
        else{
            retVal = AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE;
        }  
    }
    else{
        retVal = AUTOPILOT_COMMUNICATOR_AUTOPILOT_NOT_AVAILABLE;
    }

    return retVal;
}


void flightguidanceCommunicatorCheckAutopilotResponse(void *parameter){

    uint16_t msWaitingTime = *((uint16_t *)parameter);
    static uint32_t lastResponse = xTaskGetTickCount();
   
    for(;;){
        
        if (Serial1.available() > 0 && autopilotCrashed == false){
            char receivedChar = Serial1.read();
            #ifdef DEBUG_AUTOPILOT_COMMUNICATOR
            Serial.print(receivedChar);
            #endif
            if(receivedChar == 'A'){ //regular autopilot still alive signal-> autopilot is o.K. 
                serialFlush();
                autopilotResponds = true;
                isConnectionEstablished = true;
                lastResponse = xTaskGetTickCount();
            }
            else{ //autopilot transmits another char, which indicates a major error->autopilot must be deactivated forever
                autopilotResponds = false;
                if(isConnectionEstablished) autopilotCrashed = true; //Deactivate autopilot until reset only if the connection was established between autpilot and autocopilot
            }
        }
        else{ 
            if(xTaskGetTickCount() - lastResponse >= HIGHEST_VALID_NON_RESPONSE_TIME){ 
                if(isConnectionEstablished) autopilotCrashed = true; //if autopilot stops transmitting after transmitting, then a major error must have occured->autopilot must be deactivated forever
                autopilotResponds = false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(msWaitingTime));

    }
}

void flightguidanceCommunicatorCheckAutopilotEnable(void *parameter){
    flightguidanceCommunicatorInit();
    uint16_t msWaitingTime = *((uint16_t *)parameter);
    uint32_t localPulseDuration = 0; // Variable to store the length of the pulse locally
    uint32_t semaphoreBlockedCounter = 0;

    for(;;){
        vTaskDelay(pdMS_TO_TICKS(msWaitingTime));

        #ifdef DEBUG_AUTOPILOT_ENABLE_HIGH_LOW
            autopilotActivated = digitalRead(PIN_AUTOPILOT_ENABLE);
        #endif

        #ifndef DEBUG_AUTOPILOT_ENABLE_HIGH_LOW

            if(xSemaphoreTake(pulseDurationSemaphore, WAIT_FOR_SEMAPHORE_MS) == pdTRUE){ // See if the mutex can be obtained. If the mutex is not available wait WAIT_FOR_SEMAPHORE_MS to see if it becomes free. 
                localPulseDuration = pulseDuration;
                pulseDuration = 0;
                xSemaphoreGive(pulseDurationSemaphore);
                semaphoreBlockedCounter = 0;
            }
            else{
                if(semaphoreBlockedCounter > MAXIMUM_SEMAPHORE_BLOCKED_COUNTS_BEFORE_RETURNING_CONTROL_TO_PILOT){ //Deadlock
                    autopilotCrashed = true;
                }
                else{
                    semaphoreBlockedCounter++;
                }
            }

            if(localPulseDuration < MINIMUM_HIGH_PULSE_LENGTH_FOR_AUTOPILOT_ACTIVATED){  
                autopilotActivated = false;
            }
            else{
                autopilotActivated = true;
            }
             #ifdef DEBUG_PILOT_COMMUNICATOR
                Serial.println("High Pulse Length "+ String(localPulseDuration));
            #endif

        #endif
    }
}

*/

//Hier einf vereinfacht von chatgpt ohne fehler sichere Autopilot-Kommunikation für EXPODROID

/******************************************************************************
 * Private Variables
 ******************************************************************************/
static bool moduleIsInitialised = false;

/******************************************************************************
 * Public Functions
 ******************************************************************************/

void flightguidanceCommunicatorInit(){
    // Minimale Initialisierung - keine Autopilot-Hardware
    moduleIsInitialised = true;
}

uint8_t flightguidanceCommunicatorGetAutopilotState(){
    // Autopilot ist immer deaktiviert für EXPODROID
    return AUTOPILOT_COMMUNICATOR_AUTOPILOT_NOT_AVAILABLE;
}

// Dummy-Funktionen falls sie irgendwo aufgerufen werden
void flightguidanceCommunicatorCheckAutopilotResponse(void *parameter){
    // Leere Task - macht nichts
    for(;;){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void flightguidanceCommunicatorCheckAutopilotEnable(void *parameter){
    // Leere Task - macht nichts
    for(;;){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}