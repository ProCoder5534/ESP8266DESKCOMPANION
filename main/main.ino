#include "config.h"
#include "display.h"
#include "network.h"
#include "timeManager.h"
#include "storage.h"

// Global variable definitions
DeviceState currentState = STATE_STARTUP;
int menuIndex = 0;
bool inMenu = false;
int selectedCount = 0;
bool timezoneSelected[10];

const char* timezoneLabels[] = {
  "America/New_York", "Europe/London", "Asia/Tokyo", "Australia/Sydney",
  "Europe/Paris", "Asia/Dubai", "Asia/Singapore", "America/Los_Angeles",
  "Africa/Johannesburg", "America/Sao_Paulo"
};

const char* timezoneDisplayNames[] = {
  "EST", "GMT", "JST", "AEDT", "CET", "GST", "SGT", "PST", "SAST", "BRT"
};

const int TIMEZONE_COUNT = 10;

void handleButtonInput() {
  if (currentState != STATE_READY) return;
  
  static bool lastScrollState = HIGH;
  static bool lastSelectState = HIGH;
  static unsigned long lastButtonPress = 0;
  static unsigned long selectPressStart = 0;
  static bool longPressDetected = false;
  
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPress < 200) return;

  bool scrollState = digitalRead(BTN_SCROLL);
  bool selectState = digitalRead(BTN_SELECT);

  if (!scrollState && lastScrollState) {
    if (!inMenu) {
      inMenu = true;
      menuIndex = 0;
    } else {
      menuIndex = (menuIndex + 1) % TIMEZONE_COUNT;
    }
    lastButtonPress = currentTime;
  }

  if (!selectState && lastSelectState) {
    selectPressStart = currentTime;
    longPressDetected = false;
  } else if (!selectState && !lastSelectState) {
    if (!longPressDetected && (currentTime - selectPressStart) > 1000) {
      longPressDetected = true;
      if (inMenu) {
        inMenu = false;
        lastButtonPress = currentTime;
      }
    }
  } else if (selectState && !lastSelectState) {
    if (!longPressDetected && (currentTime - selectPressStart) < 1000) {
      if (inMenu) {
        int current = menuIndex;
        if (timezoneSelected[current]) {
          timezoneSelected[current] = false;
          selectedCount--;
        } else {
          if (selectedCount < 2) {
            timezoneSelected[current] = true;
            selectedCount++;
          } else {
            for (int i = 0; i < TIMEZONE_COUNT; i++) {
              if (timezoneSelected[i]) {
                timezoneSelected[i] = false;
                break;
              }
            }
            timezoneSelected[current] = true;
          }
        }
        saveTimezoneSelection();
      } else {
        inMenu = true;
        menuIndex = 0;
      }
      lastButtonPress = currentTime;
    }
  }

  lastScrollState = scrollState;
  lastSelectState = selectState;
}

void setup() {
  Serial.begin(115200);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  initDisplay();
  pinMode(BTN_SCROLL, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  
  loadTimezoneSelection();
  displayStartupAnimation();
  startCaptivePortal();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();
  checkWiFiConnection();
  handleButtonInput();
  
  if (currentState == STATE_READY) {
    if (inMenu) {
      showTimezoneMenu();
    } else {
      showClockScreen();
    }
  }
}