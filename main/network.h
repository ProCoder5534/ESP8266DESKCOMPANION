#ifndef NETWORK_H
#define NETWORK_H

#include "config.h"

// Network objects
extern ESP8266WebServer server;
extern DNSServer dnsServer;

// Network variables
extern bool isTryingToConnect;
extern unsigned long connectStartTime;
extern String connectSSID, connectPASS;

// Network functions
void initNetwork();
void startCaptivePortal();
void resetToAPMode();
void checkWiFiConnection();
void handleRoot();
void handleConnect();

#endif