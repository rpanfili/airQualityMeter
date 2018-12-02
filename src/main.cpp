#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <ESP8266WebServer.h>
#include <ArduinoOTA.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

/* 
 *  NOTICE: all secrets and passwords (WIFI, MQTT, ETC) are
 *  stored into Secrets.h 
 *  This file is not included into this repository so please copy
 *  the example (Secrets.h.dist) and modify it
 */
#include "Secrets.h"

#define DEBUG True

SoftwareSerial pmsSerial(D7, D8);

WiFiClient espClient;
WiFiManager wifiManager;
PubSubClient mqtt_client(MQTT_SERVER, MQTT_PORT, espClient);

#define SENSORNAME "air_quality_meter"

#define WIFI_AP_SSID_PREFIX "AQM"
#ifndef WIFI_AP_SSID_PWD
  #define WIFI_AP_SSID_PWD "stop_a1r_p0llution"
#endif

#define HTTP_PORT 80
ESP8266WebServer server(HTTP_PORT);

#define PMS5003ST_SIZE 40
#define PMS5003ST_SIG1 0X42
#define PMS5003ST_SIG2 0X4d

struct pms5003STdata {
  uint16_t pm10_standard, pm25_standard, pm100_standard;
  uint16_t pm10_env, pm25_env, pm100_env;
  uint16_t particles_03um, particles_05um, particles_10um, particles_25um, particles_50um, particles_100um;
  uint16_t hcho, temperature, humidity;
  uint16_t reserved;
  uint16_t unused;
  uint16_t checksum;
};
 
struct pms5003STdata data;


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

/*
 * Setup (esp8266) OverTheAir update
 */
void setupArduinoOTA(){
  ArduinoOTA.setPort(OTAport);
  ArduinoOTA.setHostname(SENSORNAME);
  ArduinoOTA.setPassword((const char *)OTApassword);
  
  ArduinoOTA.onStart([]() {
    Serial.println("Starting OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd OTA");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

/*void setup_pms() {
  //pms.passiveMode();  
  pms.wakeUp();
  pms.activeMode();    
}

void readPMS() {
  if (pms.read(data))
  {
    Serial.print("WAAAAAA");
    debug("PM 1.0 (ug/m3): ");
    debug(String(data.PM_AE_UG_1_0));

    debug("PM 2.5 (ug/m3): ");
    debug(String(data.PM_AE_UG_2_5));

    debug("PM 10.0 (ug/m3): ");
    debug(String(data.PM_AE_UG_10_0));
  }

}
*/
boolean readPMSdata(Stream *s) {
  if (! s->available()) {
    return false;
  }
  
  // Read a byte at a time until we get to the special '0x42' start-byte
  if (s->peek() != PMS5003ST_SIG1) {
    uint8_t c = s->read();
    Serial.print("[1] skip char 0x");Serial.println(c, HEX);
    return false;
  }
  
  Serial.println("found start char");
 
  // Now read all 40 bytes
  if (s->available() < PMS5003ST_SIZE) {
    return false;
  }
  
  // drop SIG1
  s->read();
  
  uint8_t se = s->peek();
  if ( se != PMS5003ST_SIG2) {
    Serial.print("[2] next char is not SIG2, skip (0x"); 
    Serial.print(se, HEX); 
    Serial.println(")");
    return false;
  }
  Serial.println("found second char");
  // drop SIG2
  s->read();
  
  uint8_t framelen_high = s->read();
  uint8_t framelen_low = s->read();
  uint16_t framelen = framelen_low |= framelen_high << 8;
  
  if(framelen != (PMS5003ST_SIZE - 4)) {
     Serial.print("PMS5003ST : invalid framelength - "); 
     Serial.println(framelen, DEC);
     return false;
  }
  Serial.println("Framelen OK");//Serial.println(framelen, DEC);
    
  Serial.println("got "+ String(PMS5003ST_SIZE) + " chars stream");    
    
  uint8_t buffer[PMS5003ST_SIZE - 4];    
  uint16_t sum = 0;
  sum += (uint16_t)PMS5003ST_SIG1 + (uint16_t)PMS5003ST_SIG2;
  sum += (uint16_t)framelen_low + (uint16_t)framelen_high;
  
  s->readBytes(buffer, PMS5003ST_SIZE - 4);
  
  // get checksum ready
  for (uint8_t i=0; i<(PMS5003ST_SIZE-6); i++) {
    sum += (uint16_t)buffer[i];
  }
 
  /* debugging 
  for (uint8_t i=0; i<PMS5003ST_SIZE-2; i++) {
    Serial.print("0x"); Serial.print(buffer[i], HEX); Serial.print(", ");
  }
  Serial.println();*/
  
  
  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t buffer_u16[19];
  for (uint8_t i=0; i<19; i++) {
    buffer_u16[i] = buffer[i*2 + 1];
    buffer_u16[i] |= (buffer[i*2] << 8);
  }
  
  // put it into a nice struct :)
  memcpy((void *)&data, (void *)buffer_u16, (PMS5003ST_SIZE-2));

  if (sum != data.checksum) {
    Serial.println("Checksum failure");
    Serial.println("Sum: "+ String(sum) + " - check: "+ String(data.checksum));
    return false;
  }
  
  
  StaticJsonBuffer<512> json_buffer;
  JsonObject& json_data = json_buffer.createObject();
  
  json_data["pm10_standard"] = data.pm10_standard;
  json_data["pm25_standard"] = data.pm25_standard;
  json_data["pm100_standard"] = data.pm100_standard;
    
  json_data["pm10_env"] = data.pm10_env;
  json_data["pm25_env"] = data.pm25_env;
  json_data["pm100_env"] = data.pm100_env;
     
  json_data["particles_03um"] = data.particles_03um;
  json_data["particles_05um"] = data.particles_05um;
  json_data["particles_10um"] = data.particles_10um;
  json_data["particles_25um"] = data.particles_25um;
  json_data["particles_50um"] = data.particles_50um;
  json_data["particles_100um"] = data.particles_100um;
    
  json_data["hcho"] = float(data.hcho / 1000.0);
  json_data["temperature"] = float(data.temperature / 10.0);
  json_data["humidity"] = float(data.humidity / 10.0);

  json_data.prettyPrintTo(Serial);

  return true;
}

void setup() {
  
  pmsSerial.begin(9600);
  #ifdef DEBUG
  Serial.begin(115200);
  #endif
  
  //setup_wifi();
  
  //setup_pms();
  
  
}

void loop() {
  
    ArduinoOTA.handle();
    
    if (readPMSdata(&pmsSerial)) {
    // reading data was successful!
    Serial.println();
    Serial.println("---------------------------------------");
    Serial.println("Concentration Units (standard)");
    Serial.print("PM 1.0: "); Serial.print(data.pm10_standard);
    Serial.print("\t\tPM 2.5: "); Serial.print(data.pm25_standard);
    Serial.print("\t\tPM 10: "); Serial.println(data.pm100_standard);
    Serial.println("---------------------------------------");
    Serial.println("Concentration Units (environmental)");
    Serial.print("PM 1.0: "); Serial.print(data.pm10_env);
    Serial.print("\t\tPM 2.5: "); Serial.print(data.pm25_env);
    Serial.print("\t\tPM 10: "); Serial.println(data.pm100_env);
    Serial.println("---------------------------------------");
    Serial.print("Particles > 0.3um / 0.1L air:"); Serial.println(data.particles_03um);
    Serial.print("Particles > 0.5um / 0.1L air:"); Serial.println(data.particles_05um);
    Serial.print("Particles > 1.0um / 0.1L air:"); Serial.println(data.particles_10um);
    Serial.print("Particles > 2.5um / 0.1L air:"); Serial.println(data.particles_25um);
    Serial.print("Particles > 5.0um / 0.1L air:"); Serial.println(data.particles_50um);
    Serial.print("Particles > 10.0 um / 0.1L air:"); Serial.println(data.particles_100um);
    Serial.println("---------------------------------------");
    Serial.print("HCHO: "); Serial.println(float(data.hcho/1000.0), 4);
    Serial.print("Temperature: "); Serial.println(float(data.temperature/10.), 2);
    Serial.print("Humidity: "); Serial.println(float(data.humidity/10.0), 2);
    Serial.println("---------------------------------------");
  }
    //readPMS();
    
    
    
    delay(5000);
    
    
}