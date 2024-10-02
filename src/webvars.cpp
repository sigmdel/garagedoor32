#include <Arduino.h>
#include "webvars.h"

const char *DOOR_STRINGS[] = {
    DOOR_CLOSED, DOOR_OPEN, DOOR_MOVING, DOOR_ERROR
    };


doorState_t doorState = dsMoving;


// Values displayed in main Web page
String DoorStateString = DOOR_MOVING;
String AutoStateString = AUTO_ON;

// Values displayed in Sensors Web page with initial values
String ContactSwitch = "??";
String TemperatureString = "??";
String HumidityString = "??";
String BrightnessString = "??";

String doorAction(void){
  if (DoorStateString.equals(DOOR_OPEN))
    return String("<button class=\"button\" onclick=\"closeDoor()\">Fermer</button>");
  else
    return String("<button class=\"button bgrey\">Fermer</button>");
}
