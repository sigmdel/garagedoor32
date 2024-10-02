// hardware header file

#pragma once

// Hardware abstraction
void initHardware(void);    // Initialize the hardware (relay, button, buzzer, temperature and light sensors)
void closeDoor(void);
void toggleAuto(void);
void setAuto(bool on);
