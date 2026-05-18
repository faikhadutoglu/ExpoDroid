/**
 * 
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
/******************************************************************************
* Constants and Defines
******************************************************************************/
#define AUTOPILOT_COMMUNICATOR_AUTOPILOT_NOT_AVAILABLE 0
#define AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE 1
#define AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE_AND_ACTIVATED 2
#define TIMEOUT_SERVO_SIGNAL_HIGH_US  25000 // After this time, the reading of pulseIn is aborted because the autopilot is not properly connected and therefore no more pulses are coming (servo high signals can only be a maximum of 2200us long).
/******************************************************************************
* Public Functions
******************************************************************************/
/**
 * @brief Initializes the autopilot communicator module
 * 
 */
void flightguidanceCommunicatorInit();

/**
 * @brief Returns information if the pilot has activated the autpilot via remote control and if the autopilot is active
 * 
 * @return uint8_t AUTOPILOT_COMMUNICATOR_AUTOPILOT_AVAILABLE if autopilot is activated and active, else AUTOPILOT_COMMUNICATOR_AUTOPILOT_NOT_AVAILABLE
 */
uint8_t flightguidanceCommunicatorGetAutopilotState();

/**
 * @brief Task which evaluates if autopilot is active
 * 
 * @param parameter Task period
 */
void flightguidanceCommunicatorCheckAutopilotResponse(void *parameter);

/**
 * @brief Task which evaluates if autopilot is activated by the pilot via remote control
 * 
 * @param parameter Task period
 */
void flightguidanceCommunicatorCheckAutopilotEnable(void *parameter);


