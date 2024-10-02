#include <Arduino.h>
#include <WiFi.h>
#include "PubSubClient.h"
#include "ArduinoJson.h"
#include "hardware.h"
#include "logging.h"
#include "config.h"
#include "commands.h"
#include "webvars.h"
#include "mqtt.h"

#define MSG_SZ  441

extern bool wifiConnected;

WiFiClient mqttClient;
PubSubClient mqtt_client(mqttClient);

void mqttLogStatus(void) {
  if (!strlen(config.mqttHost))
    addToLogP(LOG_INFO, TAG_MQTT, PSTR("No MQTT broker defined"));
  else {
    String connected;
    connected = (mqtt_client.connected()) ? "Connected" : "Not connected";
    addToLogPf(LOG_INFO, TAG_MQTT, PSTR("%s to MQTT broker %s:%d"), connected.c_str(), config.mqttHost, config.mqttPort);
  }
}


void receivingDomoticzMQTT(String const payload) {
  #ifdef JSONLIB_V7 // see https://arduinojson.org/v7/how-to/upgrade-from-v6/#jsondocument
  JsonDocument doc;
  #else
  DynamicJsonDocument doc(config.mqttBufferSize);
  #endif
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    addToLogPf(LOG_ERR, TAG_MQTT, PSTR("deserializeJson() failed : %s with message %s"), err.c_str(), payload.substring(0, 80).c_str());
    return;
  }

  int idx = doc["idx"];  // default 0 - not valid
  if (!idx) {
    addToLogP(LOG_ERR, TAG_MQTT, PSTR("idx not found in MQTT message"));
    return;
  }

  if (idx == config.dmtzAutoCloseIdx) {
    setAuto((doc["nvalue"] > 0));
    addToLogPf(LOG_INFO, TAG_MQTT, PSTR("AutoClose selector set to %s in Domoticz"), AutoStateString.c_str());
    return;
  }

  if (idx == config.dmtzSwitchIdx) {
    int n = (int) doc["nvalue"];
    addToLogPf(LOG_INFO, TAG_MQTT, PSTR("Garage close switch set to %d in Domoticz"), n);
    if (!n) closeDoor();
    return;
  }

  if (idx == config.dmtzIsColdIdx) {
    int n = (int) doc["svalue1"];
    addToLogPf(LOG_INFO, TAG_MQTT, PSTR("Cold setpoint set to %d °C in Domoticz"), n);
    config.coldThreshold = n;
    return;
  }

  if (idx == config.dmtzIsDarkIdx) {
    int n = (int) doc["svalue1"];
    addToLogPf(LOG_INFO, TAG_MQTT, PSTR("Dark setpoint set to %d lux Domoticz"), n);
    config.darkThreshold = n;
    return;
  }

  //addToLogPf(LOG_DEBUG, TAG_MQTT, PSTR("%d is an unknown idx, message ignored"), idx);
}

// Callback function, when we receive an MQTT value on the topics
// subscribed this function is called
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  //byte* payload_copy = reinterpret_cast<byte*>(malloc(length + 1));
  char* payload_copy = reinterpret_cast<char*>(malloc(length + 1));
  if (payload_copy == NULL) {
    addToLogP(LOG_ERR, TAG_MQTT, PSTR("Can't allocate memory for MQTT payload. Message ignored"));
    return;
  }

  // Copy the payload to the new buffer
  memcpy(payload_copy, payload, length);
  // Conversion to a printable string
  payload_copy[length] = '\0';

////  addToLogPf(LOG_DEBUG, TAG_MQTT, PSTR("MQTT rx [%s] %s"), topic, payload_copy);

  String sTopic(topic);
  if (sTopic.indexOf(config.topicDmtzSub) > -1)
    //receivingDomoticzMQTT((char *)payload_copy); // launch the function to treat received data
    receivingDomoticzMQTT(payload_copy); // launch the function to treat received data
  else
    doCommand(FROM_MQTT, payload_copy);

  // Free the memory
  free(payload_copy);
}



void mqttClientSetup(void) {
  addToLogPf(LOG_DEBUG, TAG_MQTT, PSTR("Setting up MQTT server: %s:%d"), config.mqttHost, config.mqttPort);
  if (!mqtt_client.setBufferSize(config.mqttBufferSize))
    addToLogPf(LOG_ERR, TAG_MQTT, PSTR("Could not allocated %d byte MQTT buffer"), config.mqttBufferSize);
  mqtt_client.setServer(config.mqttHost, config.mqttPort);
  mqtt_client.setCallback(mqttCallback);
  //mqtt loop will take care of connecting to MQTT broker
}

void mqttDisconnect(void) {
  if (mqtt_client.connected())
    mqtt_client.disconnect();
}

unsigned long lastMqttConnectAttempt = 0;

void mqttReconnect(void) {
  if ((mqtt_client.connected()) || (!wifiConnected) || (millis() - lastMqttConnectAttempt < 5000) )
    return;
  if (!strlen(config.mqttUser) || !strlen(config.mqttPswd))
    mqtt_client.connect(config.hostname);
  else
    mqtt_client.connect(config.hostname, config.mqttUser, config.mqttPswd);
  lastMqttConnectAttempt = millis();
}


bool mqttConnected = false;


bool mqttPublish(String payload, char* topic = NULL) {
  if (!mqtt_client.connected()) {
    mqttReconnect();
    delay(10);
  }
  if (!mqtt_client.connected()) {
    return false;
  }

  char* theTopic;
  if (topic == NULL)
    theTopic = config.topicDmtzPub;
  else
    theTopic = topic;
  addToLogPf(LOG_DEBUG, TAG_MQTT, PSTR("MQTT update message: %s"), payload.c_str());
  return mqtt_client.publish(theTopic, payload.c_str());
}

bool mqttLog(String message) {
  String topic(config.topicLog);
  topic.replace("%h%", config.hostname);
  return mqttPublish(message, (char*) topic.c_str());
}

#define MQTT_JSON "{\"idx\":%idx%, \"nvalue\":%nval%, \"svalue\":\"%sval%\", \"parse\":false}"

String startPayload(int idx, int value) {
  String payload(MQTT_JSON);
  payload.replace("%idx%", String(idx).c_str());
  payload.replace("%nval%", String(value).c_str());
  return payload;
}

bool mqttUpdateDmtzSwitch(int idx, int value) {
  String payload(startPayload(idx, value));
  payload.replace("%sval%", String("").c_str());
  return mqttPublish(payload);
}

bool mqttUpdateDomoticzBrightnessSensor(int idx, int value) {
  String payload(startPayload(idx, value));
  payload.replace("%sval%", String(value).c_str());
  return mqttPublish(payload);
}

bool mqttUpdateDomoticzTemperatureHumiditySensor(int idx, float value1, float value2, int state) {
  String payload(startPayload(idx, 0));
  String svalue(value1, 1);
  svalue += ";";
  svalue += String(value2, 0);
  svalue += ";";
  svalue += state;
  payload.replace("%sval%", svalue.c_str());
  return mqttPublish(payload);
}

bool mqttUpdateDomoticzAutoCloseSelector(int idx, bool value) {
  //Serial.println("send_mqtt_domoticz_autoClose()");
  int nvalue = (value) ? 2 : 0;
  String payload(startPayload(idx, nvalue));
  char svalue[3];
  strlcpy(svalue, (value) ? "10" : "0", 3);
  payload.replace("%sval%", svalue);
  return mqttPublish(payload);
}

bool mqttUpdateDomoticzSetPoint(int idx, int value) {
  String payload(startPayload(idx, 0));
  payload.replace("%sval%", String(value).c_str());
  return mqttPublish(payload);
}

void mqttSubscribe(void) {
  // subscribe to topicDmtzSub = domoticz/out by default
  mqtt_client.subscribe(config.topicDmtzSub);
  addToLogPf(LOG_INFO, TAG_MQTT, PSTR("Subscribed to topic \"%s\""), config.topicDmtzSub);

  // subscribe to topicCmd = hostname/cmd by default
  String topic(config.topicCmd);
  topic.replace("%h%", config.hostname);
  mqtt_client.subscribe(topic.c_str());
  addToLogPf(LOG_INFO, TAG_MQTT, PSTR("Subscribed to topic \"%s\""), topic.c_str());

  // query autoclose status as set in Domoticz
  String payload("{\"command\":\"getdeviceinfo\", \"idx\":");
  payload += config.dmtzAutoCloseIdx;
  payload += "}";
  mqttPublish(payload);
  // if this does not work, will default to ON

  // query cold setpoint as set in Domoticz
  payload = "{\"command\":\"getdeviceinfo\", \"idx\":";
  payload += config.dmtzIsColdIdx;
  payload += "}";
  mqttPublish(payload);

  // query dark setpoint as set in Domoticz
  payload = "{\"command\":\"getdeviceinfo\", \"idx\":";
  payload += config.dmtzIsDarkIdx;
  payload += "}";
  mqttPublish(payload);
}

void mqttLoop(void) {
  if (mqttConnected != mqtt_client.connected()) {
    // MQTT status has changed
    //int seconds = (int) ((millis() - connectiontime)/1000);
    //mqttConnectiontime = millis();
    mqttConnected = !mqttConnected;
    if (mqttConnected) {
       addToLogPf(LOG_INFO, TAG_MQTT, PSTR("Reconnected to MQTT broker %s as %s"), config.mqttHost, config.hostname);
       mqttSubscribe();
    } else
      addToLogP(LOG_INFO, TAG_MQTT, PSTR("Disconnected from MQTT broker"));
  }
  if (mqtt_client.connected()) {
    mqtt_client.loop();
    lastMqttConnectAttempt = millis();
  } else
    mqttReconnect();
}
