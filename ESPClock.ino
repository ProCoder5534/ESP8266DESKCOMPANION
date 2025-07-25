// Include libraries
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>
#include <time.h>
#include <TZ.h>

#define BTN_SCROLL 14
#define BTN_SELECT 12

const int EEPROM_SIZE = 64;
int menuIndex = 0;
bool inMenu = false;
int selectedCount = 0;
bool timezoneSelected[10]; // You can expand this depending on how many timezones you list

// Device state management
enum DeviceState {
  STATE_STARTUP,
  STATE_AP_MODE,
  STATE_CONNECTING,
  STATE_FETCHING_DATA,
  STATE_READY
};

DeviceState currentState = STATE_STARTUP;

const char* timezoneLabels[] = {
  "America/New_York",
  "Europe/London",
  "Asia/Tokyo",
  "Australia/Sydney",
  "Europe/Paris",
  "Asia/Dubai",
  "Asia/Singapore",
  "America/Los_Angeles",
  "Africa/Johannesburg",
  "America/Sao_Paulo"
};

const char* timezoneDisplayNames[] = {
  "EST",
  "GMT",
  "JST",
  "AEDT",
  "CET",
  "GST",
  "SGT",
  "PST",
  "SAST",
  "BRT"
};

// OLED config
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Server config
ESP8266WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;

// AP credentials
const char* apSSID = "ESPClock";
const char* apPassword = "espclock";

// Wi-Fi connection
bool isTryingToConnect = false;
unsigned long connectStartTime = 0;
const unsigned long connectTimeout = 15000;
String connectSSID = "", connectPASS = "";

// ---------------- DISPLAY ----------------

void displayCentered(const char* text, int y = 20, int size = 1) {
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
  // WiFi symbol with 3 arcs
  display.drawCircle(x, y, 2, SSD1306_WHITE);
  display.drawCircle(x, y, 4, SSD1306_WHITE);
  display.drawCircle(x, y, 6, SSD1306_WHITE);
  display.fillCircle(x, y, 1, SSD1306_WHITE);
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
  display.println("SSID: ESPClock");
  display.println("PASS: espclock");
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
  
  // Set state to ready after progress animation completes
  currentState = STATE_READY;
}

// ---------------- HTML FORM ----------------

String getWiFiFormHTML() {
  return R"rawliteral(
  <!DOCTYPE html><html><head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESPClock</title>
  <style>
    body {
      background: black;
      color: #33ff33;
      font-family: monospace;
      display: flex;
      align-items: center;
      justify-content: center;
      height: 100vh;
    }
    .card {
      background: #111;
      padding: 20px;
      border-radius: 10px;
      box-shadow: 0 0 10px #33ff33;
      text-align: center;
    }
    input {
      background: black;
      color: #33ff33;
      border: 1px solid #33ff33;
      padding: 10px;
      width: 100%;
      margin: 10px 0;
    }
    input[type=submit] {
      cursor: pointer;
    }
  </style>
  </head><body>
    <div class="card">
      <h2>Enter WiFi</h2>
      <form action="/connect" method="POST">
        <input name="ssid" placeholder="WiFi SSID" required><br>
        <input name="password" type="password" placeholder="WiFi Password" required><br>
        <input type="submit" value="Connect">
      </form>
    </div>
  </body></html>
  )rawliteral";
}

// ---------------- HANDLERS ----------------

void handleRoot() {
  server.send(200, "text/html", getWiFiFormHTML());
}

void handleConnect() {
  connectSSID = server.arg("ssid");
  connectPASS = server.arg("password");

  WiFi.begin(connectSSID.c_str(), connectPASS.c_str());
  connectStartTime = millis();
  isTryingToConnect = true;
  currentState = STATE_CONNECTING;

  displayCentered("Connecting...");
  server.send(200, "text/html", "<html><body><h2>Connecting...<br>Check OLED</h2></body></html>");
}

// ---------------- NETWORK ----------------

void startCaptivePortal() {
  currentState = STATE_AP_MODE;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  IPAddress ip = WiFi.softAPIP();

  dnsServer.start(DNS_PORT, "*", ip);
  server.on("/", handleRoot);
  server.on("/connect", HTTP_POST, handleConnect);
  server.begin();

  displayAPInstructions(ip);
}

void resetToAPMode() {
  WiFi.disconnect(true);
  dnsServer.stop();
  server.stop();
  delay(500);
  startCaptivePortal();
}

void checkWiFiConnection() {
  if (!isTryingToConnect) return;

  if (WiFi.status() == WL_CONNECTED) {
    displayCentered("Connected to WiFi");
    delay(1000);
    server.stop();
    dnsServer.stop();
    WiFi.softAPdisconnect(true);
    isTryingToConnect = false;
    showFetchingProgress(); // This will set currentState to STATE_READY when done
  } else if (millis() - connectStartTime > connectTimeout) {
    displayCentered("Connection Failed!");
    delay(2000);
    resetToAPMode();
    isTryingToConnect = false;
  }
}

void loadTimezoneSelection() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 10; i++) {
    timezoneSelected[i] = EEPROM.read(i) == 1;
  }
  EEPROM.end();
}

void saveTimezoneSelection() {
  EEPROM.begin(EEPROM_SIZE);
  for (int i = 0; i < 10; i++) {
    EEPROM.write(i, timezoneSelected[i] ? 1 : 0);
  }
  EEPROM.commit();
  EEPROM.end();
}

void showClockScreen() {
  display.clearDisplay();
  
  // Draw WiFi icon at top right
  drawWiFiIcon(120, 8);
  
  // Draw decorative border
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  
  // Main IST time - large and centered
  String istTime = getTimeStringWithSeconds("Asia/Kolkata");
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  
  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(istTime.c_str(), 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 12);
  display.print(istTime);
  
  // IST label
  display.setTextSize(1);
  display.setCursor((SCREEN_WIDTH - 18) / 2, 32);
  display.print("IST");
  
  // Draw separator line
  display.drawLine(5, 40, SCREEN_WIDTH - 5, 40, SSD1306_WHITE);
  
  // Show selected timezones
  int y = 45;
  int count = 0;
  for (int i = 0; i < 10; i++) {
    if (timezoneSelected[i] && count < 2) { // Limit to 2 additional timezones for space
      String timeStr = getTimeStringWithSeconds(timezoneLabels[i]);
      
      // Time on the left
      display.setCursor(8, y);
      display.print(timeStr);
      
      // Timezone label on the right
      display.setCursor(SCREEN_WIDTH - 25, y);
      display.print(timezoneDisplayNames[i]);
      
      y += 10;
      count++;
    }
  }
  
  // Add decorative corners
  display.fillRect(2, 2, 3, 3, SSD1306_WHITE);
  display.fillRect(SCREEN_WIDTH - 5, 2, 3, 3, SSD1306_WHITE);
  display.fillRect(2, SCREEN_HEIGHT - 5, 3, 3, SSD1306_WHITE);
  display.fillRect(SCREEN_WIDTH - 5, SCREEN_HEIGHT - 5, 3, 3, SSD1306_WHITE);
  
  display.display();
}

String getTimeString(const char* tz) {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", tz, 1);
  tzset();
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--:--";

  char buf[6];
  strftime(buf, sizeof(buf), "%H:%M", &timeinfo);
  return String(buf);
}

String getTimeStringWithSeconds(const char* tz) {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", tz, 1);
  tzset();
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "--:--:--";

  char buf[9];
  strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
  return String(buf);
}

void showTimezoneMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  
  // Title
  display.setCursor(25, 2);
  display.print("SELECT TIMEZONES");
  display.drawLine(0, 12, SCREEN_WIDTH, 12, SSD1306_WHITE);
  
  int visibleItems = 3; // Reduced from 4 to 3 to make room for instructions
  int startIndex = menuIndex - (menuIndex % visibleItems);
  
  for (int i = 0; i < visibleItems && (startIndex + i) < 10; i++) {
    int index = startIndex + i;
    int y = 16 + (i * 12);
    
    // Highlight current selection with a box
    if (index == menuIndex) {
      display.fillRect(2, y - 1, SCREEN_WIDTH - 4, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK); // Black text on white background
    } else {
      display.drawRect(2, y - 1, SCREEN_WIDTH - 4, 10, SSD1306_WHITE);
      display.setTextColor(SSD1306_WHITE); // White text on black background
    }
    
    // Checkbox
    display.setCursor(6, y);
    if (timezoneSelected[index]) {
      display.print("✓");
    } else {
      display.print(" ");
    }
    
    // Timezone display name
    display.setCursor(18, y);
    display.print(timezoneDisplayNames[index]);
    
    // Full timezone name (truncated if needed)
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
  
  // Instructions at bottom with button numbers
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(5, 52);
  display.print("BTN1: Next  BTN2: Select");
  
  display.display();
}

void handleButtonInput() {
  // Only allow button input when device is ready
  if (currentState != STATE_READY) return;
  
  static bool lastScrollState = HIGH;
  static bool lastSelectState = HIGH;
  static unsigned long lastButtonPress = 0;
  unsigned long currentTime = millis();
  
  // Debounce buttons
  if (currentTime - lastButtonPress < 200) return;

  bool scrollState = digitalRead(BTN_SCROLL);
  bool selectState = digitalRead(BTN_SELECT);

  if (!scrollState && lastScrollState) {
    if (!inMenu) {
      inMenu = true;
      menuIndex = 0; // Start from first item
    } else {
      menuIndex = (menuIndex + 1) % 10;
    }
    lastButtonPress = currentTime;
  }

  if (!selectState && lastSelectState) {
    if (inMenu) {
      int current = menuIndex;
      if (timezoneSelected[current]) {
        timezoneSelected[current] = false;
        selectedCount--;
      } else if (selectedCount < 2) {
        timezoneSelected[current] = true;
        selectedCount++;
      }
      saveTimezoneSelection();
      
      // Exit menu immediately after selection
      inMenu = false;
    } else {
      // Enter menu if in clock view
      inMenu = true;
      menuIndex = 0;
    }
    lastButtonPress = currentTime;
  }

  lastScrollState = scrollState;
  lastSelectState = selectState;
}

// ---------------- MAIN ----------------

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED failed"));
    while (true);
  }
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
  
  // Only show clock/menu screens when device is ready
  if (currentState == STATE_READY) {
    if (inMenu) {
      showTimezoneMenu();
    } else {
      showClockScreen();
    }
  }
}