/**
 * //accutatorController.h
 * @author Maximilian Wendt   
 * 
 * "Any fool can write code that a computer can understand. Good programmers write code that humans can understand."  ~ Martin Fowler, 2008
 *  
 */
#pragma once
/******************************************************************************
* Used Libaries
******************************************************************************/
#include <Arduino.h>
#include "constants.h"
#include "flightguidanceCommunicator.h"
#include "Servo.h"
/******************************************************************************
* Public Functions
******************************************************************************/
/**
 * @brief Initializes the actuator control module
 * 
 */
void actuatorControllerInit();

/**
 * @brief Depending on flightguidanceCommunicatorGetAutopilotState, actuatorControllerForwardSignals forwards the Servo Signals from remote control or from autopilot to the individual servos
 * 
 */
void actuatorControllerForwardSignals();



#ifdef DEBUG_PRINT_RECEIVED_ANGLES
/**
 * @warning ONLY FOR TESTING PURPOSES!
 * @brief Prints out all servos min, max and current angles. Before using the
 * values, command RC to min and max and let go the controller
 * 
 */
void actuatorControllerPrintAngles();
#endif

