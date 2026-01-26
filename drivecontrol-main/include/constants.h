/**
 * constants.h 
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
//OLD PINS

//Signals from pilot
//#define PIN_SPEED_RECEIVED_ESP 23
//#define PIN_ELEVATOR_RECEIVED_ESP 19
//#define PIN_AILERON_RECEIVED_ESP 21
//#define PIN_RUDDER_RECEIVED_ESP 22
//#define PIN_FLAP_RECEIVED_ESP 2

//Signals from remote control
#define PIN_SPEED_RECEIVED_RC 32            // Für die Geschwindigkeitsregelung
//#define PIN_ELEVATOR_RECEIVED_RC 35 
#define PIN_AILERON_RECEIVED_RC 34          //Für Lenkung
//#define PIN_RUDDER_RECEIVED_RC 36
#define PIN_FLAP_RECEIVED_RC 39

//Signals to motor controllers
//#define PIN_SPEED_CONTROLLER 25
//#define PIN_ELEVATOR_CONTROLLER 18
//#define PIN_AILERON_CONTROLLER 5 //Strapping Pin(toggles during boot sequence)
//#define PIN_RUDDER_CONTROLLER 17
//#define PIN_FLAP_CONTROLLER 26

//Autopilot Enable
//#define PIN_AUTOPILOT_ENABLE 16

//Pilot Status
//#define PIN_PILOT_STATUS_RX 4
#define PIN_AUTOPILOT_LED 27

//Debugger
//#define PIN_JTAG_TDI 12
//#define PIN_JTAG_TCK 13
//#define PIN_JTAG_TMS 14
//#define PIN_JTAG_TDO 15




//EXPODROID PINSS
// BTS7960 Module 1 
#define PIN_BTS1_RPWM  33    // Forward PWM
#define PIN_BTS1_LPWM  0     // Reverse PWM
// R_EN und L_EN fest auf 5V

// BTS7960 Module 2 
#define PIN_BTS2_RPWM  1     // Forward PWM (TX0)
#define PIN_BTS2_LPWM  3     // Reverse PWM (RX0)

// Ultraschall-Sensoren (SEN-US01)
#define PIN_ULTRASONIC_FRONT_TRIG   23    // Vorne Mitte
#define PIN_ULTRASONIC_FRONT_ECHO   19
#define PIN_ULTRASONIC_LEFT_TRIG    21    // Links vorne
#define PIN_ULTRASONIC_LEFT_ECHO    22
#define PIN_ULTRASONIC_RIGHT_TRIG   2     // Rechts vorne
#define PIN_ULTRASONIC_RIGHT_ECHO   25