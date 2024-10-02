#include <Arduino.h>
#include <WiFi.h>
#include "AsyncTCP.h"
#include "ESPAsyncWebServer.h"
#include "AsyncElegantOTA.h"
#include "logging.h"
#include "config.h"
#include "html.h"
#include "hardware.h"
#include "commands.h"
#include "webserver.h"
#include "webvars.h"


extern void espRestart(int level = 0);
extern bool wifiConnected;
extern bool accessPointUp;


// Webserver instance using default HTTP port 80
AsyncWebServer server(80);

// Create an Event Source on /events
AsyncEventSource events("/events");


// Web server template substitution functions

String preProcessor(const String& var){
  if (var == "TITLE") return String(config.devname);
  if (var == "DEVICENAME") return String(config.devname);
  return String(); // empty string
}

// Web server template substitution function for main page
String mainProcessor(const String& var){
  //addToLogPf(LOG_DEBUG, TAG_WEBSERVER, PSTR("Processing %s"), var.c_str());
  String result = preProcessor(var);
  if (!result.isEmpty()) return result;
  if (var == "DOORSTATE") return DoorStateString;
  if (var == "DOORACTION") return doorAction();
  if (var == "AUTOSTATE") return AutoStateString;
  if (var == "LOG") return logHistory();
  /*  // NOTE:   for debugging size of log {
    String test = logHistory();
    Serial.printf("logHistory.length: %d, starts with: %s\n", test.length(), test.substring(0, 64).c_str());
    return test;
  } */
  //if (var == "INFO") return String("Using AsyncWebServer, AJAX and Server-Sent Events (SSE)");
  return String(); // empty string
}

// Web server template substitution function for sensor page
String sensorProcessor(const String& var){
  //addToLogPf(LOG_DEBUG, TAG_WEBSERVER, PSTR("Processing %s"), var.c_str());
  String result = preProcessor(var);
  if (!result.isEmpty()) return result;
  if (var == "TEMPERATURE") return TemperatureString;
  if (var == "HUMIDITY") return HumidityString;
  if (var == "BRIGHTNESS") return BrightnessString;
  return String(); // empty string
}

// Web server template substitution function for access point page
String processor2(const String& var){
  //addToLogPf(LOG_DEBUG, TAG_WEBSERVER, PSTR("Processing %s"), var.c_str());
  String result = preProcessor(var);
  if (!result.isEmpty()) return result;
  if (var == "SSID") return String(config.wifiSsid);
  if ((var == "PASS") && strlen(config.wifiPswd)) return String("***********");
  if (var == "STAIP") return IPAddress(config.staStaticIP).toString();
  if (var == "GATE") return IPAddress(config.staGateway).toString();
  if (var == "MASK") return IPAddress(config.staNetmask).toString();
  return String(); // empty string
}

void webserversetup(void) {
  addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("Adding HTTP request handlers"));

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("GET /"));
    if (accessPointUp)
      request->send_P(200, "text/html", html_wm, processor2);
    else
      request->send_P(200, "text/html", html_index, mainProcessor);
  });

  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
    // log in to index page even if using access point - can control light with web interface
    // but .......
    addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("GET /index.html"));
    request->send_P(200, "text/html", html_index, mainProcessor);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request){
    addToLogPf(LOG_DEBUG, TAG_WEBSERVER, PSTR("GET /cmd with %d params"), request->params());
    if (request->params() == 1) {
      AsyncWebParameter* aParam = request->getParam(0);
      if ((aParam) && (aParam->name().equals("cmd")) && (aParam->value().length() > 0)) {
        doCommand(FROM_WEBC, aParam->value());
      }
    }
    request->send(200, "text/plain", "OK");
  });

  server.on("/close", HTTP_GET, [](AsyncWebServerRequest *request){
    addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("GET /close"));
    closeDoor();
    request->send(200, "text/plain", "OK");
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("GET /toggle"));
    toggleAuto();
    request->send(200, "text/plain", "OK");
  });

  server.on("/sensors", HTTP_GET, [](AsyncWebServerRequest *request){
    addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("GET /log"));
    request->send_P(200, "text/html", html_sensors, sensorProcessor);
  });

  server.on("/log", HTTP_GET, [](AsyncWebServerRequest *request){
    addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("GET /log"));
    request->send_P(200, "text/html", html_console, mainProcessor);
  });

  server.on("/rst", HTTP_GET, [](AsyncWebServerRequest *request){
    addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("GET /rst"));
    request->send(200, "text/plain", "Restart device");
    espRestart();
  });

  server.on("/creds", HTTP_POST, [](AsyncWebServerRequest *request) {
    #define PARAM_INPUT_1 "ssid"
    #define PARAM_INPUT_2 "pass"
    #define PARAM_INPUT_3 "staip"
    #define PARAM_INPUT_4 "gateway"
    #define PARAM_INPUT_5 "mask"
    String wifi_ssid = "";
    String wifi_pass = "";
    IPAddress ipa;
    IPAddress gateway;
    IPAddress mask;

    addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("POST /creds"));
    if (!accessPointUp) {
      request->send_P(404, "text/html", html_404, mainProcessor);
      return;
    }
    bool bad = true;
    if (request->hasParam(PARAM_INPUT_1, true))
      wifi_ssid = request->getParam(PARAM_INPUT_1, true)->value();
    if (request->hasParam(PARAM_INPUT_2, true))
      wifi_pass = request->getParam(PARAM_INPUT_2, true)->value();
    if (request->hasParam(PARAM_INPUT_3, true))
      ipa.fromString(request->getParam(PARAM_INPUT_3, true)->value());
    if (request->hasParam(PARAM_INPUT_4, true))
      gateway.fromString(request->getParam(PARAM_INPUT_4, true)->value());
    if (request->hasParam(PARAM_INPUT_5, true))
      mask.fromString(request->getParam(PARAM_INPUT_5, true)->value());

    if (wifi_ssid.isEmpty())
      addToLogP(LOG_ERR, TAG_COMMAND, PSTR("Empty SSID"));
    else if ((wifi_pass.length() > 0) && (wifi_pass.length() < 8))
      addToLogP(LOG_ERR, TAG_COMMAND, PSTR("Password too short"));
    else if (ipa && ((ipa & mask) != (gateway & mask)))
      addToLogP(LOG_ERR, TAG_COMMAND, PSTR("The station IP and gateway are not on the same subnet"));
    else
      bad = false;
    if (bad) {
      request->send_P(200, "text/html", html_wm_bad_creds, mainProcessor);
      return;
    }

    request->send_P(200, "text/html", html_wm_connect, mainProcessor);

    strlcpy(config.wifiSsid, wifi_ssid.c_str(), HOST_SZ);
    strlcpy(config.wifiPswd, wifi_pass.c_str(), PSWD_SZ);

    if (ipa) {
      config.staStaticIP = ipa;
      config.staGateway = gateway;
      config.staNetmask = mask;
    }
    espRestart();

  });

  server.onNotFound([] (AsyncWebServerRequest *request) {
    addToLogPf(LOG_INFO, TAG_WEBSERVER, PSTR( "URI \"%s\" from \"%s:%d\" not found."), request->url().c_str(),
      request->client()->remoteIP().toString().c_str(), request->client()->remotePort());
    request->send_P(404, "text/html", html_404, mainProcessor);
  });

  // add SSE handler
  addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("Adding SSE handler"));
  server.addHandler(&events);

  // Starting Async OTA web server AFTER all the server.on requests registered
  // addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("Add OTA server at /ota not /update"));
  // AsyncOTA.begin(&server);
  addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("Add OTA server at /update"));
  AsyncElegantOTA.begin(&server);

  // Start async web browser
  server.begin();
  addToLogP(LOG_INFO, TAG_WEBSERVER, PSTR("Web server started"));
}
