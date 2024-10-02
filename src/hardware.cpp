//--- Hardware Drivers ---


// Contact switch
#ifndef GSW_PIN
#error "No GSW_PIN defined"
#endif

// Light sensor
#ifndef LDR_PIN
#error "No LDR_PIN defined"
#endif

// Relay (garage door opener)
#ifndef RELAY_PIN
#error "No RELAY_PIN defined"
#endif

// Status LED
#ifndef SLED_PIN
#error "No SLED_PIN defined"
#endif

// Relay (garage door opener)
#ifndef BUZZ_PIN
#error "No BUZZ_PIN defined"
#endif
#if !defined(BUZZ_ON) && (BUZZ_PIN < 255)
#error "BUZZ_ON not defined"
#endif

// Remote automatic status LEDs
#ifndef RLED_PIN
#error "No RLED_PIN defined"
#endif
#ifndef RLED_ON
#error "RLED_ON not defined"
#endif

#ifndef BLED_PIN
#error "No BLED_PIN defined"
#endif
#ifndef BLED_ON
#error "BLED_ON not defined"
#endif

// Remote automatic push button
#ifndef RSW_PIN
#error "No RSW_PIN defined"
#endif

// BUG reset timers in init functions


//--------------------------------

#include <Arduino.h>
#include <Ticker.h>
#include "ESPAsyncWebServer.h"  // for AsyncEventSource
#include "mdSimpleButton.h"
#include "config.h"
#include "logging.h"
#include "hardware.h"
#include "domoticz.h"
#include "DHT20.h"
#include "webvars.h"

uint8_t test = SDA;

extern AsyncEventSource events;

// The delay in milliseconds before the open garage door is closed
// if auto closure is enabled. This delay can be
//    config.closeDelayShort if it is dark or cold
//    config.closeDelayLong  if is is light and warm
//
uint32_t closeDelay = 450000;  // 7.5 minutes default

// calculates closeDelay on the basis of the measure light and
// temperature values and the set thresholds.
void calcCloseDelay(void) {
  // note that toInt() returns 0 if BrightnessString does not contain a number
  boolean isDark = (BrightnessString.toInt() < config.darkThreshold);
  boolean isCold = (TemperatureString.toFloat() < (float) config.coldThreshold);
  addToLogPf(LOG_DEBUG, TAG_HARDWARE, PSTR("isDark set to %s"), (isDark) ? "true" : "false");
  addToLogPf(LOG_DEBUG, TAG_HARDWARE, PSTR("isCold set to %s"), (isCold) ? "true" : "false");
  uint32_t newDelay = (isDark || isCold) ? config.closeDelayShort : config.closeDelayLong;
  if (newDelay != closeDelay) {
    closeDelay = newDelay;
    addToLogPf(LOG_INFO, TAG_HARDWARE, PSTR("Auto closure delay set to %d seconds"), closeDelay/1000);
  }
}


// Buzzer
// ------

unsigned long buzzTime = 0;
unsigned long buzzOut = 0;

void initBuzzer(void) {
  #if (BUZZ_PIN < 255)
    addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Initializing buzzer I/O pin."));
    pinMode(BUZZ_PIN, OUTPUT);
    digitalWrite(BUZZ_PIN, 1-BUZZ_ON);
    buzzTime = 0;
    buzzOut = 0;
  #else
    addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("No buzzer pin defined"));
  #endif
}

void setBuzzer(int on, unsigned long buz = 50) {
  #if (BUZZ_PIN < 255)
    addToLogPf(LOG_DEBUG, TAG_HARDWARE, PSTR("SetBuzzer(%s) at %lu."), (on) ? "ON" : "OFF"  , millis());
    digitalWrite(BUZZ_PIN, on);
    if (on == BUZZ_ON) {
      buzzOut = (buz < 50) ? 50 : buz;
      buzzTime = millis();
      if (!buzzTime) {
        buzzTime = 1;
        delay(2);
      }
      addToLogPf(LOG_INFO, TAG_HARDWARE, PSTR("Starting buzzer: buzzOut = %lu ms, buzzTime = %lu."), buzzOut, buzzTime);
    }
    else {
      addToLogPf(LOG_DEBUG, TAG_HARDWARE, PSTR("Stoping buzzer at millis() = %lu , setting buzzTime = 0"), millis());
      buzzTime = 0;
      buzzOut = 0;
    }
  #endif
}

void checkBuzzer(void) {
  #if (BUZZ_PIN < 255)
    if ((buzzTime) && (millis() - buzzTime > buzzOut))
      setBuzzer(1-BUZZ_ON);
  #endif
}


unsigned long doorTime;  // time of last change in state of door state
unsigned long openWarnTime;  // timer for warning about door open/error state
unsigned long errorWarnTime;

bool setDoorState(doorState_t newState) {
  //if ((doorState == newState) || (doorState == dsError))
  //  return false;
  if (doorState == newState) {
    addToLogPf(LOG_DEBUG, TAG_HARDWARE, PSTR("doorState already in state %d (%s)"), newState, DoorStateString.c_str());
    return false;
  }
  if (doorState == dsError) {
    addToLogPf(LOG_ERR, TAG_HARDWARE, PSTR("Cannot set doorState to %d because it's current state is %s"), DoorStateString.c_str());
    return false;
  }
  doorState = newState;
  doorTime = millis();
  openWarnTime = millis();
  errorWarnTime = millis();
  DoorStateString = DOOR_STRINGS[doorState];
  addToLogPf(LOG_INFO, TAG_HARDWARE, PSTR("Set doorState to %d (%s)"), doorState, DoorStateString.c_str());
  events.send(DoorStateString.c_str(),"doorstate");   // updates all Web clients
  events.send(doorAction().c_str(), "dooraction");
  updateDomoticzSwitch(config.dmtzContactIdx, (doorState==dsOpen) ? 1 : 0); // and Domoticz
  return true;
}

// Updates door state if it was in state dsMoving after waiting travelTime
// or else warns if door open or door is in error state
void checkDoorState() {
  if ((doorState == dsMoving) && (millis() - doorTime > config.travelTime)) {
    boolean sw = (digitalRead(GSW_PIN) == HIGH);
    setDoorState((doorState_t) sw);
  }
  if ((doorState == dsOpen) && (millis() - openWarnTime > 2*60*1000)) {
    addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Door open"));
    setBuzzer(BUZZ_ON, 100);
    openWarnTime = millis();
  }
  if ((doorState == dsError) && (millis() - errorWarnTime > 5*60*1000)) {
    addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Door state unknown"));
    setBuzzer(BUZZ_ON, 500);
    errorWarnTime = millis();
  }
}

// Relay to control garage door
// ----------------------------

boolean relayClosed = false;        // flag indicating relay is being closed
const int relayOn = LOW;
const int relayOff = HIGH;

void initDoorRelay(void) {
  addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Initializing door relay I/O pin."));
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, relayOff);
  relayClosed = false;
}

void activateDoorRelay(void) {
  // ignore command if door is moving or if door state is dsError
  if (doorState == dsError) {
    addToLogP(LOG_ERR, TAG_HARDWARE, PSTR("Door state is dsError, SHOULD NOT GET HERE"));
  }
  if (doorState == dsMoving) {
    float remaining = (float) (config.travelTime - (millis() - doorTime))/1000;
    addToLogPf(LOG_INFO, TAG_HARDWARE, PSTR("Door relay commands ignored for next %.1f seconds"), remaining);
  } else {
    digitalWrite(RELAY_PIN, relayOn);
    relayClosed = true;
    setBuzzer(BUZZ_ON, config.relayOnTime);
    addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Closing door relay"));
    setDoorState(dsMoving);
  }
}

void checkDoorRelay(void) {
  if ((relayClosed) && (millis() - doorTime > config.relayOnTime)) {
    digitalWrite(RELAY_PIN, relayOff);
    relayClosed = false;
    addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Releasing door relay"));
  }
}


void closeDoor(void) {
  if (doorState == dsOpen) activateDoorRelay();
}


// Contact switch
// --------------

/*
Keep a FIFO of times contact switch changed to test if too many changes are occuring
in a short while indicating a defective switch or wire connection.
*/

#define CS_FIFO  6 // sizeo of FIFO when checking if switch broken

unsigned long csTimes[CS_FIFO] {0};
int csIndex = 0;
int csCount = 0;

// Returns time elapsed between added time (csTime) and oldest
// recorded time in FIFO
unsigned long addcsTime(unsigned long csTime) {
  addToLogPf(LOG_DEBUG, TAG_HARDWARE, "Adding %lu to csTimes", csTime);
  csTimes[csIndex] = csTime;
  csIndex = (csIndex + 1) % CS_FIFO;
  if (csCount < CS_FIFO) {
    csCount++;
    return csTime - csTimes[0];  // 0 if csIndex = 0 and csCount < CS_FIFO which is ok
  } else {
    return csTime - csTimes[csIndex];
  }
}

boolean SWOpen = true;

void initContactSwitch(void) {
  addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Initializing contact switch I/O pin."));
  pinMode(GSW_PIN, INPUT_PULLUP);
  // Reset fifo
  csIndex = 0;
  csCount = 0;
  memset(csTimes, 0, CS_FIFO*sizeof(csTimes[0]));
  // Assume that the contact switch works
  doorState = dsMoving; // and make sure door state set correctly in next steps
  SWOpen = (digitalRead(GSW_PIN) == HIGH);
  setDoorState((doorState_t) SWOpen);
  // Leaving with either doorState == dsOpen or dsClosed
}

void checkContactSwitch(void) {
  if (doorState == dsError) return;

  boolean sw = (digitalRead(GSW_PIN) == HIGH);
  if (sw != SWOpen) {
    SWOpen = sw;
    addToLogP(LOG_DEBUG, TAG_HARDWARE, PSTR("Contact switch toggled"));
    // Constact switch state has changed
    // keep track of time of switch event
    unsigned long int eventTime = millis();
    // and time interval between eventTime and
    // oldest event time in fifo queue
    unsigned long fifoPeriod = addcsTime(eventTime);
        /*
        Serial.printf("\nCount: %d, index: %d\n", csCount, csIndex);
        for (int i = 0; i< csCount; i++) {
          Serial.printf("%d, %lu\n", i, csTimes[i]);
        }
        addToLogPf(LOG_DEBUG, TAG_HARDWARE, PSTR("Time since %d changes in door state: %lu"), csCount, fifoPeriod);
        */
    if (( fifoPeriod < config.travelTime) && (csCount >= CS_FIFO)) {
      addToLogP(LOG_ERR, TAG_HARDWARE, PSTR("Door status changed too often, contact switch defective?"));
      setDoorState(dsError);
      ///NOTE WARN domoticz
      return;
    }
    setDoorState((doorState_t) sw);
  }
}

// Remote auto closure button
extern void espRestart(int level = 0);

mdSimpleButton button = mdSimpleButton(RSW_PIN);

void setAuto(bool on) {
  if (on) {
    if (AutoStateString.equals(AUTO_ON)) {
      addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Auto state already on"));
      return;
    } else
      AutoStateString = AUTO_ON;
  } else { // on = false
    if (AutoStateString.equals(AUTO_OFF)) {
      addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Auto state already off"));
      return;
    } else
      AutoStateString = AUTO_OFF;
  }
  events.send(AutoStateString.c_str(),"autostate");        // updates all Web clients
  updateDomoticzAutoCloseSelector(config.dmtzAutoCloseIdx, AutoStateString.equals(AUTO_ON));   // and Domoticz
  addToLogPf(LOG_INFO, TAG_HARDWARE, PSTR("Auto state updated to %s"), AutoStateString.c_str());
}

void toggleAuto(void) {
  setAuto(!AutoStateString.equals(AUTO_ON));
}

void checkAutoClose(void) {
  // Only check when AutoClose is enabled
  if (AutoStateString.equals(AUTO_ON))  {
    if ( (doorState == dsOpen) && (millis() - doorTime > closeDelay) ) {
      addToLogPf(LOG_INFO, TAG_HARDWARE, PSTR("Automatically closing door after %.f minutes"), (float) closeDelay / 60000);
      closeDoor();
    }
  }
}

void checkButton(void) {
  switch( button.update()) {
    case BUTTON_LONGPRESS:
      if (button.presstime > 30000) { // more than 30 seconds
        addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Push-button long press - restart 7"));
        espRestart(7);
      } else if (button.presstime > 10000) { // more than 10 seconds
        addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Push-button long press - restart 3"));
        espRestart(3);
      } else {
        addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Push-button long press - restart 0"));
        espRestart(0);
      }
      break;
    case BUTTON_RELEASED:
      addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Push-button released"));
      toggleAuto();
      break;
    default:   // avoid unhandled BUTTON_UNCHANGED warning/error
      break;
  }
}

// Remote LEDS
// -----------

unsigned long RemoteLEDStime = 0;
boolean AutoCloseEnabled = false;

void initRemoteLEDS(void) {
  addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Initializing remote LED I/O pins."));
  pinMode(RLED_PIN, OUTPUT);
  digitalWrite(RLED_PIN, 1-RLED_ON);
  pinMode(BLED_PIN, OUTPUT);
  digitalWrite(BLED_PIN, 1-BLED_ON);
  AutoCloseEnabled = !(AutoStateString.equals(AUTO_ON));
}

void checkRemoteLEDS(void) {
  boolean AutoEnabled = AutoStateString.equals(AUTO_ON);
  if (AutoEnabled != AutoCloseEnabled) {
     AutoCloseEnabled = AutoEnabled;
     RemoteLEDStime = millis();
     if (AutoEnabled) {
       digitalWrite(BLED_PIN, BLED_ON);
       digitalWrite(RLED_PIN, 1-BLED_ON);
     } else {
      digitalWrite(BLED_PIN, 1-BLED_ON);
      digitalWrite(RLED_PIN, RLED_ON);
    }
  } else { // no change in state
    if ((!AutoEnabled) && (millis() - RemoteLEDStime > 250)) {
      digitalWrite(RLED_PIN, 1-digitalRead(RLED_PIN));
      RemoteLEDStime = millis();
    }
  }
}


// TemperatureString and HumidityString Sensor
// -------------------------------

unsigned long temptime;

// nothing to setup, just implement a pseudo radom walk around initial values

bool hasTempSensor = false;
int dhtErrCount = 0;


DHT20 DHT;

void initDHTSensor() {
  addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Initializing temperature and humidity sensor"));
  //addToLogPf(LOG_DEBUG, TAG_HARDWARE, PSTR("SDA = %d, SCL = %d"), SDA, SCL);

  Wire.begin();
  Wire.setClock(400000);
  delay(100);

  // try to get sucessful DHT.begin()
  for(int i=0; i<5; i++) {
    hasTempSensor = DHT.begin();  //  ESP32 default 21, 22
    addToLogPf(LOG_DEBUG, TAG_HARDWARE, PSTR("DHT.begin() %s"), (hasTempSensor) ? "success" : "failed");
    if (hasTempSensor) break;
    delay(1200);
  }

  // try to get correct status
  if (hasTempSensor) {
    delay(100);
    for (int i=0; i<5; i++) {
      int status = DHT.readStatus() & 0x18;
      hasTempSensor = (status == 0x18);
      addToLogPf(LOG_DEBUG, TAG_HARDWARE, PSTR("DHT.readStatus() = %d (%s)"), status, (hasTempSensor) ? "success" : "failed");
      if (hasTempSensor) break;
      delay(1200);
    }
  }

  if (hasTempSensor) {
    temptime = millis();
    addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Temperature and humidity sensor intialized"));
  } else {
    TemperatureString = "(no sensor)";
    HumidityString = "(no sensor)";
    addToLogP(LOG_ERR, TAG_HARDWARE, PSTR("Temperature and humidity sensor not initialized"));
  }
}

void readTemp(void) {
  if (!hasTempSensor) return;
  if (millis() - temptime > config.sensorUpdtTime) {
    temptime = millis();
    addToLogP(LOG_DEBUG, TAG_HARDWARE, PSTR( "Reading temperature and humidity data"));

    // asynchroneous data read
    DHT.requestData();
    delay(100);
    if ((DHT.readData() <= 0) || (DHT.convert() != DHT20_OK)) {
      addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Error reading DHT data"));
      dhtErrCount++;
      hasTempSensor = (dhtErrCount < 5);
      if (!hasTempSensor)
        addToLogP(LOG_ERR, TAG_HARDWARE, PSTR("DHT sensor assumed defective"));
      return;
    }
    dhtErrCount = 0;
    float temp = DHT.getTemperature();
    addToLogPf(LOG_INFO, TAG_HARDWARE, PSTR("TemperatureString %s --> %.1f"), TemperatureString.c_str(), temp);
    TemperatureString = String(temp, 1);

    float humid = DHT.getHumidity();
    addToLogPf(LOG_INFO, TAG_HARDWARE, PSTR("HumidityString %s --> %.1f"), HumidityString.c_str(), humid);
    HumidityString = String(humid, 1);

    String sval = String(TemperatureString + " " + HumidityString);
    events.send(sval.c_str(),"tempvalue");        // updates all Web clients
    updateDomoticzTemperatureHumiditySensor(config.dmtzTHSIdx, temp, humid); // and Domoticz
    addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Temperature and humidity data updated"));

    calcCloseDelay();
  }
}


// BrightnessString Sensor
// -----------------

#define LS_FIFO    10     // size of FIFO queue when calculating rolling average if < 2, then no averaging done
#define LS_READ    10000  // minimum time (in ms) between readings of the sensor data if averaging
#define RESOLUTION 12

//#define DEBUG_LS_FIFO

unsigned long brightnesstime = 0;
unsigned long lightreadtime = 0;

// variables used for rolling average LDR value
uint32_t lsValues[LS_FIFO] {0};
uint32_t lsSum = 0;
uint32_t lsAvg = 0;  // last read avg value
int lsIndex = 0;
int lsCount = 0;

void initBrightness(void) {
  pinMode(LDR_PIN, INPUT);
  // Reset fifo
  lsIndex = 0;
  lsCount = 0;
  memset(lsValues, 0, CS_FIFO*sizeof(lsValues[0]));
  //brightnesstime = millis();
  //lightreadtime = millis();
}

// adds the a new LS value and returns the new average
uint32_t addlsValue(uint32_t newValue) {
     lsSum = lsSum - lsValues[lsIndex] + newValue;
     lsValues[lsIndex] = newValue;
     if (lsCount < LS_FIFO) lsCount++;
     lsIndex = (lsIndex + 1) % LS_FIFO;
     return (uint32_t) lsSum / lsCount;
}

void readBrightness() {
  if (millis() - lightreadtime >= LS_READ) {
    #ifdef DEBUG_LS_FIFO
      uint32_t mvolt = analogReadMilliVolts(LDR_PIN);
      lsAvg = addlsValue(mvolt);
      addToLogPf(LOG_DEBUG, TAG_HARDWARE, PSTR("Raw light value: %d mv, avg: %d"), mvolt, lsAvg);
    #else
      //addToLogP(LOG_DEBUG, TAG_HARDWARE, PSTR("Reading brightness sensor data"));
      lsAvg = addlsValue(analogReadMilliVolts(LDR_PIN));
    #endif
    lightreadtime = millis();
  }

  if (millis() - brightnesstime >= config.sensorUpdtTime) {
    addToLogP(LOG_DEBUG, TAG_HARDWARE, PSTR("Updating average brightness value."));
    addToLogPf(LOG_INFO, TAG_HARDWARE, PSTR("BrightnessString %s --> %d"), BrightnessString.c_str(), lsAvg);
    BrightnessString = String(lsAvg);
    brightnesstime = millis();
    events.send(BrightnessString.c_str(),"brightvalue");      // updates all Web clients
    updateDomoticzBrightnessSensor(config.dmtzLSIdx, lsAvg);  // and Domoticz
    addToLogP(LOG_INFO, TAG_HARDWARE, PSTR("Average brightness data updated"));

    calcCloseDelay();
  }
}

void checkHardware(void) {
  checkDoorRelay();
  checkContactSwitch();
  checkAutoClose();
  checkDoorState();
  checkRemoteLEDS();
  checkBuzzer();
  checkButton();
  readTemp();
  readBrightness();
}

Ticker ticker;

void initHardware(void) {
  closeDelay = config.closeDelayShort;
  initBuzzer();
  initDoorRelay();
  initContactSwitch();
  initRemoteLEDS();
  initBrightness();
  initDHTSensor();
  // Initialize sensor timers so that a sensor is polled every config.sensorUpdtTime/2 ms
  // and wait 2 seconds before starting

  int HALF_DELAY = config.sensorUpdtTime/2;
  brightnesstime = millis() - HALF_DELAY + 2000;
  temptime = brightnesstime - HALF_DELAY;         // will start with temperature sensor
  ticker.attach_ms(config.hdwPollTime, checkHardware);
}

/*
void stopHardware(void) {
  ticker.detach();
}
*/
