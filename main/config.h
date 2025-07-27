#ifndef CONFIG_H
#define CONFIG_H

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <time.h>
#include <TZ.h>

// Pin definitions
#define BTN_SCROLL 14
#define BTN_SELECT 12

// Display configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// EEPROM configuration
#define EEPROM_SIZE 64

// Network configuration
#define DNS_PORT 53
#define CONNECT_TIMEOUT 15000

// AP credentials
#define AP_SSID "ESPClock"
#define AP_PASSWORD "espclock"

// Time cache duration
#define TIME_CACHE_DURATION 1000

// Device states
enum DeviceState {
  STATE_STARTUP,
  STATE_AP_MODE,
  STATE_CONNECTING,
  STATE_FETCHING_DATA,
  STATE_READY
};

// Timezone data
extern const char* timezoneLabels[];
extern const char* timezoneDisplayNames[];
extern const int TIMEZONE_COUNT;

// Global variables
extern DeviceState currentState;
extern int menuIndex;
extern bool inMenu;
extern int selectedCount;
extern bool timezoneSelected[];

#endif