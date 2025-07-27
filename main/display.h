#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include <Adafruit_SSD1306.h>

// Display object
extern Adafruit_SSD1306 display;

// Display functions
void initDisplay();
void displayCentered(const char* text, int y = 20, int size = 1);
void drawWiFiIcon(int x, int y);
void drawAnalogClock(int centerX, int centerY, int radius, int hour, int minute, int second);
void displayStartupAnimation();
void displayAPInstructions(IPAddress ip);
void showFetchingProgress();
void showClockScreen();
void showTimezoneMenu();

#endif