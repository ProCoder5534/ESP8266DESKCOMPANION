#include "network.h"
#include "display.h"
#include "webPages.h"

ESP8266WebServer server(80);
DNSServer dnsServer;

bool isTryingToConnect = false;
unsigned long connectStartTime = 0;
String connectSSID = "", connectPASS = "";

void initNetwork() {
  WiFi.mode(WIFI_STA);
}

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

void startCaptivePortal() {
  currentState = STATE_AP_MODE;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
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
    showFetchingProgress();
  } else if (millis() - connectStartTime > CONNECT_TIMEOUT) {
    displayCentered("Connection Failed!");
    delay(2000);
    resetToAPMode();
    isTryingToConnect = false;
  }
}