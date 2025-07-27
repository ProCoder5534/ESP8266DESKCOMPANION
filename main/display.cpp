#include "display.h"
#include "timeManager.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void initDisplay() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED initialization failed"));
    while (true);
  }
}

void displayCentered(const char* text, int y, int size) {
  display.clearDisplay();
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, y);
  display.print(text);
  display.display();
}

void drawWiFiIcon(int x, int y) {
  display.drawCircle(x, y, 2, SSD1306_WHITE);
  display.drawCircle(x, y, 4, SSD1306_WHITE);
  display.drawCircle(x, y, 6, SSD1306_WHITE);
  display.fillCircle(x, y, 1, SSD1306_WHITE);
}

void drawAnalogClock(int centerX, int centerY, int radius, int hour, int minute, int second) {
  // Draw clock face
  display.drawCircle(centerX, centerY, radius, SSD1306_WHITE);
  
  // Draw hour markers (12, 3, 6, 9)
  for (int i = 0; i < 4; i++) {
    float angle = i * 90 * PI / 180;
    int x1 = centerX + (radius - 3) * cos(angle - PI/2);
    int y1 = centerY + (radius - 3) * sin(angle - PI/2);
    int x2 = centerX + (radius - 1) * cos(angle - PI/2);
    int y2 = centerY + (radius - 1) * sin(angle - PI/2);
    display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
  }
  
  // Hour hand
  float hourAngle = ((hour % 12) * 30 + minute * 0.5) * PI / 180;
  int hourX = centerX + (radius - 8) * cos(hourAngle - PI/2);
  int hourY = centerY + (radius - 8) * sin(hourAngle - PI/2);
  display.drawLine(centerX, centerY, hourX, hourY, SSD1306_WHITE);
  display.drawLine(centerX-1, centerY, hourX-1, hourY, SSD1306_WHITE);
  
  // Minute hand
  float minuteAngle = minute * 6 * PI / 180;
  int minuteX = centerX + (radius - 4) * cos(minuteAngle - PI/2);
  int minuteY = centerY + (radius - 4) * sin(minuteAngle - PI/2);
  display.drawLine(centerX, centerY, minuteX, minuteY, SSD1306_WHITE);
  
  // Second hand
  float secondAngle = second * 6 * PI / 180;
  int secondX = centerX + (radius - 2) * cos(secondAngle - PI/2);
  int secondY = centerY + (radius - 2) * sin(secondAngle - PI/2);
  display.drawLine(centerX, centerY, secondX, secondY, SSD1306_WHITE);
  
  // Center dot
  display.fillCircle(centerX, centerY, 1, SSD1306_WHITE);
}

void displayStartupAnimation() {
  currentState = STATE_STARTUP;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  const char* title = "ESP DESK COMPANION";
  for (int i = 0; i <= strlen(title); i++) {
    display.clearDisplay();
    display.setCursor(10, 20);
    display.print(">");
    for (int j = 0; j < i; j++) {
      display.print(title[j]);
    }
    display.display();
    delay(100);
  }
  delay(800);
}

void displayAPInstructions(IPAddress ip) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Connect to:");
  display.println("SSID: " AP_SSID);
  display.println("PASS: " AP_PASSWORD);
  display.println();
  display.print("Go to: ");
  display.println(ip);
  display.display();
}

void showFetchingProgress() {
  currentState = STATE_FETCHING_DATA;
  const char* stages[] = {
    "Fetching stock prices",
    "Fetching timezones", 
    "Reading sensor data",
    "Syncing weather",
    "Ready!"
  };

  for (int i = 0; i < 5; i++) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.print(stages[i]);

    int progress = 0;
    while (progress <= 100) {
      display.fillRect(0, 40, map(progress, 0, 100, 0, 128), 8, SSD1306_WHITE);
      display.display();
      delay(15);
      progress += 10;
    }
    delay(300);
  }
  
  initTimeCache();
  currentState = STATE_READY;
}

void showClockScreen() {
  display.clearDisplay();
  
  drawWiFiIcon(120, 8);
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  
  TimeInfo istTime = getTimeInfo("Asia/Kolkata", 0);
  String istTimeStr = formatTime12Hour(istTime.hour, istTime.minute, istTime.second);
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 12);
  display.print(istTimeStr);
  
  display.setCursor(5, 22);
  display.print("IST");
  
  int analogHour = istTime.hour % 12;
  drawAnalogClock(95, 20, 12, analogHour, istTime.minute, istTime.second);
  
  display.drawLine(5, 32, SCREEN_WIDTH - 5, 32, SSD1306_WHITE);
  
  int y = 37;
  int count = 0;
  for (int i = 0; i < TIMEZONE_COUNT; i++) {
    if (timezoneSelected[i] && count < 2) {
      invalidateTimeCache(i + 1);
      TimeInfo tzTime = getTimeInfo(timezoneLabels[i], i + 1);
      String timeStr = formatTime12Hour(tzTime.hour, tzTime.minute, tzTime.second);
      
      display.setCursor(8, y);
      display.print(timeStr);
      display.setCursor(SCREEN_WIDTH - 25, y);
      display.print(timezoneDisplayNames[i]);
      
      y += 12;
      count++;
    }
  }
  
  // Decorative corners
  display.fillRect(2, 2, 3, 3, SSD1306_WHITE);
  display.fillRect(SCREEN_WIDTH - 5, 2, 3, 3, SSD1306_WHITE);
  display.fillRect(2, SCREEN_HEIGHT - 5, 3, 3, SSD1306_WHITE);
  display.fillRect(SCREEN_WIDTH - 5, SCREEN_HEIGHT - 5, 3, 3, SSD1306_WHITE);
  
  display.display();
}

void showTimezoneMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  
  display.setCursor(25, 2);
  display.print("SELECT TIMEZONES");
  display.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);
  
  int visibleItems = 3;
  int startIndex = menuIndex - (menuIndex % visibleItems);
  
  for (int i = 0; i < visibleItems && (startIndex + i) < TIMEZONE_COUNT; i++) {
    int index = startIndex + i;
    int y = 16 + (i * 12);
    
    if (index == menuIndex) {
      display.fillRect(2, y - 1, SCREEN_WIDTH - 4, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.drawRect(2, y - 1, SCREEN_WIDTH - 4, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE);
    }
    
    display.setCursor(6, y);
    display.print(timezoneSelected[index] ? "✓" : " ");
    
    display.setCursor(18, y);
    display.print(timezoneDisplayNames[index]);
    
    display.setCursor(45, y);
    String fullName = String(timezoneLabels[index]);
    if (fullName.length() > 12) {
      fullName = fullName.substring(fullName.lastIndexOf('/') + 1);
      if (fullName.length() > 12) {
        fullName = fullName.substring(0, 9) + "...";
      }
    }
    display.print(fullName);
  }
  
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(2, 52);
  display.print("BTN1:Next BTN2:Select");
  
  display.setCursor(2, 60);
  display.print("Selected:");
  display.print(selectedCount);
  display.print("/2 Long:Exit");
  
  display.display();
}