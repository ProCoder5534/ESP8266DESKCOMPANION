#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <EEPROM.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define TIMEZONE_OFFSET_SECONDS 19800  // India Standard Time UTC+5:30
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", TIMEZONE_OFFSET_SECONDS);

bool wifiConnected = false;
unsigned long lastWiFiCheck = 0;
unsigned long lastTimeUpdate = 0;
unsigned long lastWeatherUpdate = 0;

#define DHTPIN 0           // GPIO0
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#define BUTTON_PIN 14      // GPIO14
#define NAV_BUTTON_PIN 12  // GPIO12 (D6) - Navigation button
#define OLED_SDA 4         // GPIO4
#define OLED_SCL 5         // GPIO5

enum State { CLOCK, MENU, STOCK_PRICES, WEATHER, TIMEZONE, WIFI_SETUP };
State currentState = CLOCK;

int menuIndex = 0;
int timezoneIndex = 0;
int selectedAdditionalTimezone = -1; // -1 means none selected
String menuItems[] = {"Stock Prices", "Weather", "Timezone", "WiFi Setup"};
bool justSelected = false;
bool navButtonPressed = false;
bool lastNavButtonState = HIGH;
unsigned long lastNavPress = 0;

// Weather data cache
float lastTemp = NAN;
float lastHumidity = NAN;

// Timezone data with city names and DST support
struct TimezoneInfo {
  const char* cityName;
  const char* displayName;
  int offsetSeconds;
  bool observesDST;
  int dstOffsetSeconds; // Offset during DST
};

TimezoneInfo timezones[] = {
  {"Honolulu", "UTC-10:00 HST", -36000, false, -36000},
  {"Anchorage", "UTC-09:00 AKST", -32400, true, -28800},
  {"Los Angeles", "UTC-08:00 PST", -28800, true, -25200},
  {"Denver", "UTC-07:00 MST", -25200, true, -21600},
  {"Phoenix", "UTC-07:00 MST", -25200, false, -25200}, // Arizona doesn't observe DST
  {"Chicago", "UTC-06:00 CST", -21600, true, -18000},
  {"New York", "UTC-05:00 EST", -18000, true, -14400},
  {"Caracas", "UTC-04:00", -14400, false, -14400},
  {"Buenos Aires", "UTC-03:00", -10800, false, -10800},
  {"London", "UTC+00:00 GMT", 0, true, 3600},
  {"Paris", "UTC+01:00 CET", 3600, true, 7200},
  {"Cairo", "UTC+02:00 EET", 7200, false, 7200},
  {"Moscow", "UTC+03:00", 10800, false, 10800},
  {"Dubai", "UTC+04:00", 14400, false, 14400},
  {"Karachi", "UTC+05:00", 18000, false, 18000},
  {"Delhi", "UTC+05:30 IST", 19800, false, 19800},
  {"Dhaka", "UTC+06:00", 21600, false, 21600},
  {"Bangkok", "UTC+07:00", 25200, false, 25200},
  {"Beijing", "UTC+08:00 CST", 28800, false, 28800},
  {"Tokyo", "UTC+09:00 JST", 32400, false, 32400},
  {"Sydney", "UTC+10:00", 36000, true, 39600},
  {"Auckland", "UTC+12:00", 43200, true, 46800}
};
const int TIMEZONE_COUNT = sizeof(timezones) / sizeof(timezones[0]);

// EEPROM addresses
#define EEPROM_SIZE 512
#define EEPROM_TIMEZONE_ADDR 0
#define EEPROM_MAGIC_ADDR 4
#define EEPROM_MAGIC_VALUE 0xABCD

// Menu navigation constants
#define DEBOUNCE_DELAY 200  // 200ms debounce for button presses
#define MENU_ITEMS_COUNT 4

// Animation variables
unsigned long animationTime = 0;
int scrollOffset = 0;

// DST calculation function
bool isDST(int year, int month, int day, int hour, bool isNorthern) {
  if (isNorthern) {
    // Northern Hemisphere DST (US/Europe style)
    // DST starts second Sunday in March, ends first Sunday in November
    if (month < 3 || month > 11) return false;
    if (month > 3 && month < 11) return true;
    
    if (month == 3) {
      // March - DST starts second Sunday
      int secondSunday = 14 - ((1 + year * 5 / 4) % 7);
      return day > secondSunday || (day == secondSunday && hour >= 2);
    }
    
    if (month == 11) {
      // November - DST ends first Sunday
      int firstSunday = 7 - ((1 + year * 5 / 4) % 7);
      return day < firstSunday || (day == firstSunday && hour < 2);
    }
  } else {
    // Southern Hemisphere DST (Australia/NZ style)
    // DST starts first Sunday in October, ends first Sunday in April
    if (month > 4 && month < 10) return false;
    if (month < 4 || month > 10) return true;
    
    if (month == 10) {
      // October - DST starts first Sunday
      int firstSunday = 7 - ((1 + year * 5 / 4) % 7);
      return day > firstSunday || (day == firstSunday && hour >= 2);
    }
    
    if (month == 4) {
      // April - DST ends first Sunday
      int firstSunday = 7 - ((1 + year * 5 / 4) % 7);
      return day < firstSunday || (day == firstSunday && hour < 3);
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(NAV_BUTTON_PIN, INPUT_PULLUP);
  dht.begin();

  // Initialize EEPROM
  EEPROM.begin(EEPROM_SIZE);
  loadSettings();

  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  
  // Show startup screen
  showStartupScreen();

  // Try connecting to WiFi
  WiFiManager wm;
  wm.setConnectTimeout(10); // 10 second timeout
  wifiConnected = wm.autoConnect("ESP-Clock-Setup");

  if (wifiConnected) {
    timeClient.begin();
    timeClient.update();
    Serial.println("WiFi Connected!");
  } else {
    Serial.println("WiFi not connected");
    currentState = WIFI_SETUP;
  }
  
  // Initial weather reading
  updateWeatherData();
}

void loadSettings() {
  // Check if EEPROM has valid data
  int magic = 0;
  EEPROM.get(EEPROM_MAGIC_ADDR, magic);
  
  if (magic == EEPROM_MAGIC_VALUE) {
    EEPROM.get(EEPROM_TIMEZONE_ADDR, selectedAdditionalTimezone);
    Serial.println("Settings loaded from EEPROM");
  } else {
    // First time setup
    selectedAdditionalTimezone = -1;
    saveSettings();
    Serial.println("EEPROM initialized with default settings");
  }
}

void saveSettings() {
  EEPROM.put(EEPROM_TIMEZONE_ADDR, selectedAdditionalTimezone);
  EEPROM.put(EEPROM_MAGIC_ADDR, EEPROM_MAGIC_VALUE);
  EEPROM.commit();
  Serial.println("Settings saved to EEPROM");
}

void updateWeatherData() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  
  if (!isnan(temp) && !isnan(hum)) {
    lastTemp = temp;
    lastHumidity = hum;
  }
  lastWeatherUpdate = millis();
}

void showStartupScreen() {
  // Animated startup screen with progress bar
  const int totalSteps = 50;
  const int animationDuration = 3000; // 3 seconds
  const int stepDelay = animationDuration / totalSteps;
  
  for (int step = 0; step <= totalSteps; step++) {
    display.clearDisplay();
    
    // Animated border that draws progressively
    int borderProgress = map(step, 0, totalSteps, 0, 256);
    if (borderProgress > 0) {
      // Draw animated rounded border
      for (int i = 0; i < min(borderProgress, 128); i++) {
        display.drawPixel(i, 0, SSD1306_WHITE); // Top
        if (i < 64) display.drawPixel(127, i, SSD1306_WHITE); // Right
      }
      if (borderProgress > 128) {
        for (int i = 0; i < min(borderProgress - 128, 128); i++) {
          display.drawPixel(127 - i, 63, SSD1306_WHITE); // Bottom
          if (i < 64) display.drawPixel(0, 63 - i, SSD1306_WHITE); // Left
        }
      }
    }
    
    // Title animation - fade in effect
    if (step > 10) {
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(30, 10);
      display.println("ESP DESK");
      display.setCursor(25, 22);
      display.println("COMPANION");
    }
    
    // Main logo animation - bouncing effect
    if (step > 20) {
      int bounce = sin((step - 20) * 0.3) * 2;
      display.setTextSize(2);
      display.setCursor(45, 32 + bounce);
      display.println("EDC");
    }
    
    // Progress bar animation
    if (step > 5) {
      int progressWidth = map(step - 5, 0, totalSteps - 5, 0, 100);
      
      // Progress bar background
      display.drawRect(14, 50, 100, 8, SSD1306_WHITE);
      
      // Progress bar fill with animation
      if (progressWidth > 0) {
        display.fillRect(15, 51, progressWidth - 2, 6, SSD1306_WHITE);
      }
      
      // Progress percentage
      display.setTextSize(1);
      display.setCursor(120, 50);
      int percentage = map(step, 0, totalSteps, 0, 100);
      display.print(percentage);
      display.print("%");
      
      // Loading text with dots animation
      display.setCursor(30, 40);
      display.print("Loading");
      int dots = (step / 10) % 4;
      for (int i = 0; i < dots; i++) {
        display.print(".");
      }
    }
    
    // Decorative elements - spinning dots around the main text
    if (step > 15) {
      float angle = (step - 15) * 0.2;
      int centerX = 64, centerY = 30;
      int radius = 25;
      
      for (int i = 0; i < 6; i++) {
        float dotAngle = angle + (i * PI / 3);
        int x = centerX + cos(dotAngle) * radius;
        int y = centerY + sin(dotAngle) * radius;
        if (x >= 0 && x < 128 && y >= 0 && y < 64) {
          display.fillCircle(x, y, 1, SSD1306_WHITE);
        }
      }
    }
    
    // Pulsing effect for the final steps
    if (step > 45) {
      int pulse = (step - 45) * 20;
      display.drawCircle(64, 32, 30 + pulse, SSD1306_WHITE);
      display.drawCircle(64, 32, 35 + pulse, SSD1306_WHITE);
    }
    
    display.display();
    delay(stepDelay);
  }
  
  // Final static screen for a moment
  display.clearDisplay();
  display.drawRoundRect(2, 2, 124, 60, 8, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 15);
  display.println("ESP DESK COMPANION");
  display.setTextSize(2);
  display.setCursor(45, 30);
  display.println("EDC");
  display.setTextSize(1);
  display.setCursor(40, 45);
  display.println("Ready!");
  display.display();
  delay(1000);
}

void drawWiFiIcon(bool connected) {
  // Draw small WiFi icon at top right (120, 2)
  if (connected) {
    // WiFi connected icon - 3 curved lines
    display.drawPixel(120, 5, SSD1306_WHITE);
    display.drawPixel(121, 4, SSD1306_WHITE);
    display.drawPixel(122, 3, SSD1306_WHITE);
    display.drawPixel(123, 4, SSD1306_WHITE);
    display.drawPixel(124, 5, SSD1306_WHITE);
    
    display.drawPixel(121, 6, SSD1306_WHITE);
    display.drawPixel(122, 7, SSD1306_WHITE);
    display.drawPixel(123, 6, SSD1306_WHITE);
    
    display.drawPixel(122, 8, SSD1306_WHITE);
  } else {
    // Exclamation mark
    display.drawLine(122, 2, 122, 6, SSD1306_WHITE);
    display.drawPixel(122, 8, SSD1306_WHITE);
    display.drawCircle(122, 5, 3, SSD1306_WHITE);
  }
}

void drawHomeButton() {
  // This function is no longer used - removed the overlapping box
}

String formatTime12Hour(int hours, int minutes, int seconds = -1) {
  String ampm = (hours >= 12) ? "PM" : "AM";
  int displayHours = hours % 12;
  if (displayHours == 0) displayHours = 12;
  
  String timeStr = "";
  if (displayHours < 10) timeStr += " ";
  timeStr += String(displayHours) + ":";
  if (minutes < 10) timeStr += "0";
  timeStr += String(minutes);
  
  if (seconds >= 0) {
    timeStr += ":";
    if (seconds < 10) timeStr += "0";
    timeStr += String(seconds);
  }
  
  timeStr += " " + ampm;
  return timeStr;
}

String getAdditionalTimezoneTime() {
  if (selectedAdditionalTimezone == -1 || !wifiConnected) return "";
  
  // Get current UTC time
  time_t utcTime = timeClient.getEpochTime();
  struct tm * utcTimeInfo = gmtime(&utcTime);
  
  // Get the timezone info
  TimezoneInfo tz = timezones[selectedAdditionalTimezone];
  
  // Determine if DST is active for this timezone
  bool dstActive = false;
  if (tz.observesDST) {
    // Check if it's a northern or southern hemisphere location based on city
    bool isNorthern = true;
    if (strcmp(tz.cityName, "Sydney") == 0 || strcmp(tz.cityName, "Auckland") == 0) {
      isNorthern = false; // Southern hemisphere
    }
    
    dstActive = isDST(utcTimeInfo->tm_year + 1900, utcTimeInfo->tm_mon + 1, 
                      utcTimeInfo->tm_mday, utcTimeInfo->tm_hour, isNorthern);
  }
  
  // Use appropriate offset
  int offsetToUse = dstActive ? tz.dstOffsetSeconds : tz.offsetSeconds;
  
  // Calculate target time
  time_t targetTime = utcTime + offsetToUse;
  struct tm * targetTimeInfo = gmtime(&targetTime);
  
  return formatTime12Hour(targetTimeInfo->tm_hour, targetTimeInfo->tm_min);
}

void loop() {
  // Read button states
  int selectButtonState = digitalRead(BUTTON_PIN);
  int navButtonState = digitalRead(NAV_BUTTON_PIN);
  
  // Navigation button debouncing
  if (navButtonState == LOW && lastNavButtonState == HIGH && millis() - lastNavPress > DEBOUNCE_DELAY) {
    navButtonPressed = true;
    lastNavPress = millis();
  }
  lastNavButtonState = navButtonState;
  
  // Check WiFi status periodically
  if (millis() - lastWiFiCheck > 5000) {
    wifiConnected = (WiFi.status() == WL_CONNECTED);
    lastWiFiCheck = millis();
  }
  
  // Update time periodically if connected
  if (wifiConnected && millis() - lastTimeUpdate > 30000) {
    timeClient.update();
    lastTimeUpdate = millis();
  }
  
  // Update weather data periodically
  if (millis() - lastWeatherUpdate > 10000) { // Every 10 seconds
    updateWeatherData();
  }
  
  // Navigation logic using buttons
  if (currentState == CLOCK) {
    // From clock, nav button opens menu
    if (navButtonPressed) {
      currentState = MENU;
      navButtonPressed = false;
    }
    // Quick home access - select button on clock does nothing special
  } else if (currentState == MENU) {
    // Menu navigation - nav button cycles through items
    if (navButtonPressed) {
      menuIndex = (menuIndex + 1) % MENU_ITEMS_COUNT;
      navButtonPressed = false;
    }
    
    // Select button chooses current item
    if (selectButtonState == LOW && !justSelected) {
      justSelected = true;
      if (menuIndex == 0) currentState = STOCK_PRICES;
      else if (menuIndex == 1) currentState = WEATHER;
      else if (menuIndex == 2) currentState = TIMEZONE;
      else if (menuIndex == 3) currentState = WIFI_SETUP;
    }
  } else if (currentState == TIMEZONE) {
    // Timezone selection - nav button cycles through timezones
    if (navButtonPressed) {
      timezoneIndex = (timezoneIndex + 1) % (TIMEZONE_COUNT + 1); // +1 for "None" option
      navButtonPressed = false;
    }
    
    // Select button applies timezone and auto-exits after 2 seconds
    if (selectButtonState == LOW && !justSelected) {
      justSelected = true;
      if (timezoneIndex == 0) {
        selectedAdditionalTimezone = -1; // None selected
      } else {
        selectedAdditionalTimezone = timezoneIndex - 1;
      }
      saveSettings();
      
      // Auto-exit to home after 2 seconds
      delay(2000);
      currentState = CLOCK;
    }
  } else {
    // Other states - nav button returns to menu, select button goes home
    if (navButtonPressed) {
      currentState = MENU;
      navButtonPressed = false;
    }
    
    // Quick home access from any non-menu page
    if (selectButtonState == LOW && !justSelected) {
      currentState = CLOCK;
      justSelected = true;
    }
  }
  
  // Button release detection
  if (selectButtonState == HIGH) {
    justSelected = false;
  }
  
  // Check for home button press - Long press NAV button from menu
  static unsigned long buttonPressStart = 0;
  static bool longPressDetected = false;
  
  if (navButtonState == LOW && currentState == MENU) {
    if (buttonPressStart == 0) {
      buttonPressStart = millis();
      longPressDetected = false;
    }
    // Long press detected (2 seconds) and not already processed
    if (millis() - buttonPressStart > 2000 && !longPressDetected) {
      currentState = CLOCK;
      longPressDetected = true;
    }
  } else if (navButtonState == HIGH) {
    buttonPressStart = 0;
    longPressDetected = false;
  }
  
  // Render current screen
  display.clearDisplay();
  
  switch (currentState) {
    case CLOCK:
      showClock();
      break;
    case MENU:
      showMenu();
      break;
    case STOCK_PRICES:
      showStockPrices();
      break;
    case WEATHER:
      showWeather();
      break;
    case TIMEZONE:
      showTimezoneSelection();
      break;
    case WIFI_SETUP:
      showWiFiSetup();
      break;
  }
  
  // Always draw WiFi icon (except on clock)
  drawWiFiIcon(wifiConnected);
  if (currentState != CLOCK && currentState != MENU) {
    // Quick home instruction for non-menu pages only
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(45, 56);
    display.print("SEL: HOME");
  }
  
  display.display();
  delay(50);
}

void showClock() {
  // Draw decorative border
  display.drawRoundRect(2, 2, 124, 60, 8, SSD1306_WHITE);
  
  if (!wifiConnected) {
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(15, 15);
    display.println("WiFi Disconnected");
    display.setCursor(25, 30);
    display.println("Press NAV for");
    display.setCursor(35, 40);
    display.println("WiFi Setup");
    return;
  }

  // Get current time
  int hours = timeClient.getHours();
  int minutes = timeClient.getMinutes();
  int seconds = timeClient.getSeconds();

  // Format time in 12-hour format
  String timeStr = formatTime12Hour(hours, minutes);
  
  // Large time display
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 12);
  display.println(timeStr);
  
  // Seconds in smaller text - shifted left to stay in bounds
  display.setTextSize(1);
  display.setCursor(105, 16);
  display.printf(":%02d", seconds);
  
  // Date
  time_t rawTime = timeClient.getEpochTime();
  struct tm * timeInfo = localtime(&rawTime);
  String dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  display.setCursor(8, 32);
  display.printf("%s %02d/%02d/%02d", dayNames[timeInfo->tm_wday], 
                 timeInfo->tm_mday, timeInfo->tm_mon + 1, timeInfo->tm_year % 100);
  
  // Weather data (small)
  if (!isnan(lastTemp) && !isnan(lastHumidity)) {
    display.setCursor(8, 42);
    display.printf("%.1fC  %.0f%%", lastTemp, lastHumidity);
  }
  
  // Additional timezone if selected
  if (selectedAdditionalTimezone >= 0) {
    String additionalTime = getAdditionalTimezoneTime();
    if (additionalTime.length() > 0) {
      display.setCursor(8, 52);
      display.print(timezones[selectedAdditionalTimezone].cityName);
      display.print(":");
      
      // Calculate position for time to ensure AM/PM stays on screen
      int cityNameLength = strlen(timezones[selectedAdditionalTimezone].cityName);
      int timeStartPos = 8 + (cityNameLength * 6) + 6; // 6 pixels per char + 6 for ":"
      
      // Ensure we don't go beyond screen width (leave space for AM/PM)
      if (timeStartPos > 85) timeStartPos = 85;
      
      display.setCursor(timeStartPos, 52);
      display.print(additionalTime);
    }
  }
}

void showMenu() {
  // Title with underline
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(35, 5);
  display.println("MENU");
  display.drawLine(35, 22, 93, 22, SSD1306_WHITE);
  
  // Menu items with proper spacing - no overlap
  display.setTextSize(1);
  for (int i = 0; i < MENU_ITEMS_COUNT; i++) {
    int y = 26 + i * 10; // Increased spacing from 8 to 10
    
    if (i == menuIndex) {
      // Highlighted item with rounded rectangle
      display.fillRoundRect(8, y - 1, 112, 9, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(12, y);
      display.print("> ");
      display.print(menuItems[i]);
    } else {
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(12, y);
      display.print("  ");
      display.print(menuItems[i]);
    }
  }
}

void showStockPrices() {
  // Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 5);
  display.println("STOCK PRICES");
  display.drawLine(25, 13, 103, 13, SSD1306_WHITE);
  
  if (!wifiConnected) {
    display.setCursor(20, 25);
    display.println("WiFi Required!");
    display.setCursor(15, 35);
    display.println("Connect to WiFi first");
    return;
  }
  
  // Placeholder content - you can add real stock API later
  display.setCursor(10, 20);
  display.println("AAPL  $150.25  +2.1%");
  display.setCursor(10, 30);
  display.println("GOOGL $125.80  -0.8%");
  display.setCursor(10, 40);
  display.println("MSFT  $380.45  +1.2%");
  
  display.setCursor(15, 50);
  display.println("Feature coming soon!");
}

void showWeather() {
  // Header
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 5);
  display.println("WEATHER");
  display.drawLine(20, 22, 108, 22, SSD1306_WHITE);
  
  display.setTextSize(1);
  
  if (isnan(lastTemp) || isnan(lastHumidity)) {
    display.setCursor(25, 35);
    display.println("Sensor Error!");
    display.setCursor(15, 45);
    display.println("Check DHT11 wiring");
  } else {
    // Temperature with degree symbol
    display.setTextSize(2);
    display.setCursor(10, 28);
    display.printf("%.1f", lastTemp);
    display.setTextSize(1);
    display.setCursor(65, 28);
    display.print("o");
    display.setCursor(70, 32);
    display.print("C");
    
    // Humidity
    display.setTextSize(1);
    display.setCursor(10, 45);
    display.printf("Humidity: %.1f%%", lastHumidity);
    
    // Simple weather icons based on temperature
    if (lastTemp > 30) {
      // Hot - sun
      display.drawCircle(100, 35, 8, SSD1306_WHITE);
      display.drawLine(100, 20, 100, 24, SSD1306_WHITE);
      display.drawLine(100, 46, 100, 50, SSD1306_WHITE);
      display.drawLine(85, 35, 89, 35, SSD1306_WHITE);
      display.drawLine(111, 35, 115, 35, SSD1306_WHITE);
    } else if (lastTemp < 15) {
      // Cold - snowflake
      display.drawLine(95, 35, 105, 35, SSD1306_WHITE);
      display.drawLine(100, 30, 100, 40, SSD1306_WHITE);
      display.drawLine(97, 32, 103, 38, SSD1306_WHITE);
      display.drawLine(97, 38, 103, 32, SSD1306_WHITE);
    }
  }
}

void showTimezoneSelection() {
  // Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 5);
  display.println("Select Additional Timezone");
  display.drawLine(8, 13, 120, 13, SSD1306_WHITE);
  
  // Current selection (like menu style)
  String currentOption;
  if (timezoneIndex == 0) {
    currentOption = "None";
  } else {
    currentOption = String(timezones[timezoneIndex - 1].cityName) + " " + 
                   String(timezones[timezoneIndex - 1].displayName);
  }
  
  // Show as highlighted selection
  display.fillRoundRect(8, 18, 112, 9, 3, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(12, 19);
  display.print("> ");
  display.print(currentOption.substring(0, 16)); // Limit length
  
  // Show previous and next options (greyed out)
  display.setTextColor(SSD1306_WHITE);
  
  // Previous option
  int prevIndex = (timezoneIndex - 1 + TIMEZONE_COUNT + 1) % (TIMEZONE_COUNT + 1);
  String prevOption = (prevIndex == 0) ? "None" : timezones[prevIndex - 1].cityName;
  display.setCursor(12, 8);
  display.print("  ");
  display.print(prevOption.substring(0, 16));
  
  // Next option
  int nextIndex = (timezoneIndex + 1) % (TIMEZONE_COUNT + 1);
  String nextOption = (nextIndex == 0) ? "None" : timezones[nextIndex - 1].cityName;
  display.setCursor(12, 29);
  display.print("  ");
  display.print(nextOption.substring(0, 16));
  
  // Instructions
  display.setCursor(8, 42);
  display.print("NAV: Next  SEL: Apply");
  
  // Show current status
  display.setCursor(8, 52);
  if (selectedAdditionalTimezone == -1) {
    display.print("Current: None");
  } else {
    display.print("Current: ");
    display.print(timezones[selectedAdditionalTimezone].cityName);
  }
}

void showWiFiSetup() {
  // Header
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(35, 5);
  display.println("WiFi SETUP");
  display.drawLine(35, 13, 93, 13, SSD1306_WHITE);
  
  if (wifiConnected) {
    display.setCursor(20, 20);
    display.println("WiFi Connected!");
    display.setCursor(15, 30);
    display.print("IP: ");
    display.println(WiFi.localIP());
    display.setCursor(15, 40);
    display.print("SSID: ");
    display.println(WiFi.SSID());
  } else {
    display.setCursor(15, 18);
    display.println("1. Connect to WiFi:");
    display.setCursor(20, 28);
    display.println("ESP-Clock-Setup");
    display.setCursor(15, 38);
    display.println("2. Open: 192.168.4.1");
    display.setCursor(15, 48);
    display.println("3. Enter WiFi details");
    
    // Blinking indicator
    if ((millis() / 500) % 2) {
      display.fillCircle(10, 22, 2, SSD1306_WHITE);
    }
  }
}
