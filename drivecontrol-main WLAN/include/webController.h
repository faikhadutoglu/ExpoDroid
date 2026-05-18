/**
 * webController.h
 *
 * Ersetzt die RC-Empfängeransteuerung durch eine Wifi-Steuerung
 * via Smartphone-Browser (Safari, Chrome, etc.).
 *
 * Der ESP32 öffnet einen Access Point. Das Handy verbindet sich
 * direkt mit diesem AP und erreicht die Steueroberfläche unter
 * http://192.168.4.1
 *
 * Die Steuerwerte werden ueber einen WebSocket bei /ws empfangen.
 * Bei Verbindungsabbruch oder fehlenden Nachrichten gilt automatisch
 * der Failsafe (siehe actuatorController.cpp).
 */
#pragma once
#include <Arduino.h>

void     webControllerInit();
void     webControllerLoop();

// Live-Werte vom Handy (-100..100 fuer Throttle/Steering, 0..180 fuer Kopf)
int      webControllerGetThrottle();
int      webControllerGetSteering();
int      webControllerGetHeadAngle();

// Verbindungs- / Failsafe-Status
bool     webControllerHasClient();
uint32_t webControllerGetLastMessageTime_ms();

// Wave-Trigger (geben true zurueck und loeschen das Flag)
bool     webControllerConsumeWaveRight();
bool     webControllerConsumeWaveBoth();
bool webControllerGetBoost();
