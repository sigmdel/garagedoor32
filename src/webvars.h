// webvars header file

#pragma once

#include <Arduino.h>

#define DOOR_ERROR   "??????"  // contact switch conflict
#define DOOR_CLOSED  "Fermée"
#define DOOR_OPEN    "Ouverte"
#define DOOR_MOVING  "~~~~~~"

#define AUTO_ON  "Oui"
#define AUTO_OFF "Non"

//#define CONTACT_CLOSED "fermé"
//#define CONTACT_OPEN "ouvert"

enum doorState_t { dsClosed,  // door closed -> contact switch closed -> pin is LOW
                   dsOpen,    // door open -> contact switch open -> pin is pulled HIGH
                   dsMoving,
                   dsError};

extern doorState_t doorState;
extern const char *DOOR_STRINGS[];

// The sensor data strings defined in webvars.cpp
extern String DoorStateString;
extern String AutoStateString;
extern String TemperatureString;
extern String HumidityString;
extern String BrightnessString;

// String to display Fermer in web interface 
String doorAction(void);
