#pragma once

#include <Arduino.h>

enum cmndSource_t {FROM_UART, FROM_WEBC, FROM_MQTT};

void doCommand(cmndSource_t source, String cmnd);
void doCommand(cmndSource_t source, const char *cmnd);

// mqtt command example:
//    topic  "portegarage/cmd"
//    payload "help config ; status"
// where portegarage is config.hostname
