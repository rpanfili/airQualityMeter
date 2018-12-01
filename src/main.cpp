#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>

/* 
 *  NOTICE: all secrets and passwords (WIFI, MQTT, ETC) are
 *  stored into Secrets.h 
 *  This file is not included into this repository so please copy
 *  the example (Secrets.h.dist) and modify it
 */
#include "Secrets.h"

#define DEBUG True

WiFiClient espClient;
WiFiManager wifiManager;
PubSubClient mqtt_client(MQTT_SERVER, MQTT_PORT, espClient);

#define WIFI_AP_SSID_PREFIX "AQM"
#ifndef WIFI_AP_SSID_PWD
  #define WIFI_AP_SSID_PWD "stop_a1r_p0llution"
#endif

#define HTTP_PORT 80
ESP8266WebServer server(HTTP_PORT);


void debug(String str) {
#ifdef DEBUG
  uint32_t now = millis();
  Serial.printf("%07u.%03u: %s\n", now / 1000, now % 1000, str.c_str());
#endif  // DEBUG
}

void configModeCallback (WiFiManager *wiFiManager) {
  debug("Entered AP mode");
}

void setup_wifi() {
  debug("Trying to connect to WiFi network");
  delay(10);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setTimeout(300);  // Timeout 5 mins.
  
  String ssid = String(WIFI_AP_SSID_PREFIX) + "_" + String(ESP.getChipId(),HEX);
  
  if (!wifiManager.autoConnect(ssid.c_str(), WIFI_AP_SSID_PWD)) {
    debug("Wifi failed to connect and hit timeout.");
    delay(3000);
    ESP.reset();
    delay(5000);
  }
}

void setup() {
  
  #ifdef DEBUG
  Serial.begin(115200);
  #endif
  
  setup_wifi();
  
}

void loop() {
  // put your main code here, to run repeatedly:
  delay(100);
}