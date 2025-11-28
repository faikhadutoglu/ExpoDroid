#include <Arduino.h>
#include "actuatorController.h"
#include "flightguidanceCommunicator.h"

// ================================================================
// ===                      TASKS                               ===
// ================================================================

// PERIODS
uint16_t flightguidanceCommunicatorCheckAutopilotResponsePeriod = 22;
uint16_t flightguidanceCommunicatorCheckAutopilotEnablePeriod = 200;

//TASK HANDLES
TaskHandle_t flightguidanceCommunicatorCheckAutopilotResponseHandle;
TaskHandle_t flightguidanceCommunicatorCheckAutopilotEnableHandle;

// ================================================================
// ===                      INITIAL SETUP                       ===
// ================================================================

void setup()
{
  #ifdef DEBUG
  Serial.begin(115200);
  #endif

  actuatorControllerInit();

  #ifdef DEBUG_PRINT_RECEIVED_ANGLES
    actuatorControllerPrintAngles();
  #endif

  xTaskCreatePinnedToCore(flightguidanceCommunicatorCheckAutopilotEnable, "ACCAE", 10000, &flightguidanceCommunicatorCheckAutopilotEnablePeriod, 1, &flightguidanceCommunicatorCheckAutopilotEnableHandle, 0);

  xTaskCreatePinnedToCore(flightguidanceCommunicatorCheckAutopilotResponse, "ACCAR", 10000, &flightguidanceCommunicatorCheckAutopilotResponsePeriod, 1, &flightguidanceCommunicatorCheckAutopilotResponseHandle, 1);
    
  #ifdef DEBUG
  Serial.println("SETUP DONE");
  #endif
}

// ================================================================
// ===                    MAIN PROGRAM LOOP                     ===
// ================================================================

void loop()
{
 actuatorControllerForwardSignals();
}



