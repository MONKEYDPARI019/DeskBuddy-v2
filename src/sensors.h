#ifndef SENSORS_H
#define SENSORS_H

// Phase 4 — buttons, touch pad and status LEDs.
// Runs its own 50 Hz task and only *posts events*; what a press means is
// decided by the app task in main.cpp.
//
//   BTN1 (32)  click: next mood / next notification     hold: clear notifications
//   BTN2 (33)  click: next screen                       hold: toggle silent (DND)
//   BTN3 (27)  click: action (pomodoro start/pause)     hold: push-to-talk (mic)
//   Touch (4)  pat: Mochi reacts

#include <Arduino.h>

void          sensorsInit();          // starts the input task
touch_value_t sensorsReadTouch();
bool          sensorsButtonDown(uint8_t btn);   // raw state, 1..3
void          sensorsSetLedOverride(bool on);   // for `test leds`
void          sensorsLedWrite(uint8_t red, uint8_t yellow, uint8_t green);

#endif // SENSORS_H
