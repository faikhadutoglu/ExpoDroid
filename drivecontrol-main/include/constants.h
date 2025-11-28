/**
 * @author Maximilian Wendt    
 * 
 * "Any fool can write code that a computer can understand. Good programmers write code that humans can understand."  ~ Martin Fowler, 2008
 * 
 */
#pragma once
/******************************************************************************
* Table of used pins
******************************************************************************/
// Actuators 
//Signals from pilot
#define PIN_SPEED_RECEIVED_ESP 23
#define PIN_ELEVATOR_RECEIVED_ESP 19
#define PIN_AILERON_RECEIVED_ESP 21
#define PIN_RUDDER_RECEIVED_ESP 22
#define PIN_FLAP_RECEIVED_ESP 2

//Signals from remote control
#define PIN_SPEED_RECEIVED_RC 32
#define PIN_ELEVATOR_RECEIVED_RC 35 
#define PIN_AILERON_RECEIVED_RC 34
#define PIN_RUDDER_RECEIVED_RC 36
#define PIN_FLAP_RECEIVED_RC 39

//Signals to motor controllers
#define PIN_SPEED_CONTROLLER 25
#define PIN_ELEVATOR_CONTROLLER 18
#define PIN_AILERON_CONTROLLER 5 //Strapping Pin(toggles during boot sequence)
#define PIN_RUDDER_CONTROLLER 17
#define PIN_FLAP_CONTROLLER 26

//Autopilot Enable
#define PIN_AUTOPILOT_ENABLE 16

//Pilot Status
#define PIN_PILOT_STATUS_RX 4
#define PIN_AUTOPILOT_LED 27

//Debugger
#define PIN_JTAG_TDI 12
#define PIN_JTAG_TCK 13
#define PIN_JTAG_TMS 14
#define PIN_JTAG_TDO 15




