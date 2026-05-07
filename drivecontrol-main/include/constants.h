/**
* constants.h
* @author Maximilian Wendt
*
* "Any fool can write code that a computer can understand. Good programmers write code that humans can understand." ~ Martin Fowler, 2008
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
#define PIN_SPEED_RECEIVED_RC 32 // ( LENKUNG!) schaltplan angepasst
//#define PIN_ELEVATOR_RECEIVED_RC 
#define PIN_AILERON_RECEIVED_RC 34 //(FÜR SPEED!!) schaltplan angepasst
//#define PIN_RUDDER_RECEIVED_RC 36
#define PIN_FLAP_RECEIVED_RC 35 // Für Winkfunktion

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

//EXPODROID PINS
// BTS7960 Module 1 LINKS
#define PIN_BTS1_RPWM 33 // Forward PWM (bleibt gleich, ist sicher) schaltplan nicht angepasst
#define PIN_BTS1_LPWM 26 //  (ersetzt Pin 0) Schaltplan angepasst!

// BTS7960 Module 2 RECHTS
#define PIN_BTS2_RPWM 13 //  (ersetzt Pin 1) Schaltplan angepasst
#define PIN_BTS2_LPWM 14 //  (ersetzt Pin 3) Schaltplan angepasst

// Ultraschall-Sensoren (SEN-US01)
#define PIN_ULTRASONIC_FRONT_TRIG 23 // Vorne Mitte
#define PIN_ULTRASONIC_FRONT_ECHO 19
#define PIN_ULTRASONIC_LEFT_TRIG 21 // Links vorne
#define PIN_ULTRASONIC_LEFT_ECHO 22
#define PIN_ULTRASONIC_RIGHT_TRIG 25 // Rechts vorne (ALT: war Pin 15) schaltplan angepasst
#define PIN_ULTRASONIC_RIGHT_ECHO 36 // Rechts vorne (ALT: war Pin 25) schaltplan angepasst

// Servo-Pins für Winkfunktion
#define PIN_SHOULDER_SERVO_RIGHT 18 // Rechte Schulter (Arm)
#define PIN_SHOULDER_SERVO_LEFT 17 // Linke Schulter (Arm)
#define PIN_WRIST_SERVO_RIGHT 16 // Rechtes Handgelenk (Finger)
#define PIN_WRIST_SERVO_LEFT 4 // Linkes Handgelenk (Finger)


