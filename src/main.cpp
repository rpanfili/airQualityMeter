#include <FS.h>
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
 * Modify this file to preload default configuration!
 */
#define CONFIG_FILE  "/config.json"

struct configStruct {
  char wifi_ssid[32] = "";
  char wifi_password[64] = "";
  char access_point_prefix[25] = "AQM-";
  char access_point_password[64] = "stop_air_pollution";
  char mqtt_server[40] = "";
  char mqtt_port[6] = "1883";
  char mqtt_username[32] = "";
  char mqtt_password[64] = "";
  char mqtt_topic[200] = "air_quality_meter/status";
  char ota_password[64] = "ota_password";
  char ota_port[6] = "8266";
};

configStruct config;

#define DEBUG True

SoftwareSerial pms_serial(D7, D8);

WiFiClient esp_client;
WiFiManager wifi_manager;
PubSubClient mqtt_client(esp_client);

// semaphore to save config file
bool proceed_and_save_configuration = false;

#define SENSORNAME "air_quality_meter"

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

/**
 * let the configuration save process begin
 */
void unlockSaveConfig()
{
  proceed_and_save_configuration = true;
}

/**
 * Load configuration from file ./config.json
 * 
 * Modify this file to preload default configuration!
 */
void loadConfig() {
  
  Serial.println("mounting FS..");
  if (SPIFFS.begin()) {
    debug("filesystem mounted");
    if(SPIFFS.exists(CONFIG_FILE)) {
      File configFile = SPIFFS.open(CONFIG_FILE, "r");
      if(!configFile) {
        debug("unable to open config file");
      } else {
        debug("Opened config file");
        /*
         * allocate a buffer to store data inside the file
         */
        size_t size = configFile.size();
        std::unique_ptr<char[]> buffer(new char[size]);
        configFile.readBytes(buffer.get(), size);
        DynamicJsonBuffer json_buffer;
        JsonObject& json = json_buffer.parseObject(buffer.get());
        if (json.success()) {
          debug("json loaded");
          json.printTo(Serial);
          
          strlcpy(config.wifi_ssid, json.get<const char*>("wifi_ssid"), sizeof(config.wifi_ssid));
          strlcpy(config.wifi_password, json.get<const char*>("wifi_password"), sizeof(config.wifi_password));
          if((config.wifi_ssid[0] != '\0')) {
            WiFi.mode(WIFI_STA);
            WiFi.begin(config.wifi_ssid, config.wifi_password);
          }
          
          /*
           * Safely-load configuration values from config.json (manages also 
           * undefined keys)
           */
          if(json.get<const char*>("access_point_prefix")[0] != '\0') {
            strlcpy(config.access_point_prefix, json["access_point_prefix"], sizeof(config.access_point_prefix));  
          }
          if(json.get<const char*>("access_point_password")[0] != '\0') {
            strlcpy(config.access_point_password, json["access_point_password"], sizeof(config.access_point_password));  
          }
          if(json.get<const char*>("mqtt_server")[0] != '\0') {
            strlcpy(config.mqtt_server, json["mqtt_server"], sizeof(config.mqtt_server));  
          }
          if(json.get<const char*>("mqtt_port")[0] != '\0') {
            strlcpy(config.mqtt_port, json["mqtt_port"], sizeof(config.mqtt_port));  
          }
          if(json.get<const char*>("mqtt_username")[0] != '\0') {
            strlcpy(config.mqtt_username, json["mqtt_username"], sizeof(config.mqtt_username));  
          }
          if(json.get<const char*>("mqtt_password")[0] != '\0') {
            strlcpy(config.mqtt_password, json["mqtt_password"], sizeof(config.mqtt_password));  
          }
          if(json.get<const char*>("mqtt_topic")[0] != '\0') {
            strlcpy(config.mqtt_topic, json["mqtt_topic"], sizeof(config.mqtt_topic));  
          }
          if(json.get<const char*>("ota_password")[0] != '\0') {
            strlcpy(config.ota_password, json["ota_password"], sizeof(config.ota_password));  
          }
          if(json.get<const char*>("ota_port")[0] != '\0') {
            strlcpy(config.ota_port, json["ota_port"], sizeof(config.ota_port));  
          }
        } else {
          debug("failed to load json");
        }
      }
      
    }
  }
}



void configModeCallback (WiFiManager *wifi_manager) {
  debug("Entered AP mode");
}

void setup_wifi() {
  debug("Trying to connect to WiFi network");
  delay(200);
  wifi_manager.setSaveConfigCallback(unlockSaveConfig);
  wifi_manager.setAPCallback(configModeCallback);
  
  WiFiManagerParameter custom_access_point_prefix("access_point_prefix", "AP mode - SSID prefix", config.access_point_prefix, 25);
  WiFiManagerParameter custom_access_point_password("access_point_password", "AP mode - password", config.access_point_password, 64);
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT server address", config.mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "MQTT server port", config.mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_username("mqtt_username", "MQTT username", config.mqtt_username, 6);
  WiFiManagerParameter custom_mqtt_password("mqtt_password", "MQTT password", config.mqtt_password, 200);
  WiFiManagerParameter custom_mqtt_topic("mqtt_topic", "MQTT topic", config.mqtt_topic, 200);
  WiFiManagerParameter custom_ota_password("ota_password", "OTA password", config.ota_password, 200);
  WiFiManagerParameter custom_ota_port("ota_port", "OTA port", config.ota_port, 200);
  
  wifi_manager.addParameter(&custom_access_point_prefix);
  wifi_manager.addParameter(&custom_access_point_password);
  wifi_manager.addParameter(&custom_mqtt_server);
  wifi_manager.addParameter(&custom_mqtt_port);
  wifi_manager.addParameter(&custom_mqtt_username);  
  wifi_manager.addParameter(&custom_mqtt_password);
  wifi_manager.addParameter(&custom_mqtt_topic); 
  wifi_manager.addParameter(&custom_ota_password);
  wifi_manager.addParameter(&custom_ota_port);
  
  /*
   * Configuration portal will remain active for 5 minutes.
   * Then it will reboot himself
   */
  wifi_manager.setTimeout(300);  // Timeout 5 mins.
  
  String ssid = config.access_point_prefix + String(ESP.getChipId(),HEX);
  
  if (!wifi_manager.autoConnect(ssid.c_str(), config.access_point_password)) {
    debug("Wifi failed to connect and hit timeout.");
    delay(3000);
    ESP.restart();
    delay(5000);
  }
  debug("Connected");
  
  strlcpy(config.access_point_prefix, custom_access_point_prefix.getValue(), sizeof(config.access_point_prefix));
  strlcpy(config.access_point_password, custom_access_point_password.getValue(), sizeof(config.access_point_password));
  strlcpy(config.mqtt_server, custom_mqtt_server.getValue(), sizeof(config.mqtt_server));
  strlcpy(config.mqtt_port, custom_mqtt_port.getValue(), sizeof(config.mqtt_port));
  strlcpy(config.mqtt_username, custom_mqtt_username.getValue(), sizeof(config.mqtt_username));
  strlcpy(config.mqtt_password, custom_mqtt_password.getValue(), sizeof(config.mqtt_password));
  strlcpy(config.mqtt_topic, custom_mqtt_topic.getValue(), sizeof(config.mqtt_topic));
  strlcpy(config.ota_password, custom_ota_password.getValue(), sizeof(config.ota_password));
  strlcpy(config.ota_port, custom_ota_port.getValue(), sizeof(config.ota_port));
  
  // save user configuration to filesystem
  if (proceed_and_save_configuration) {
    debug("Saving configuration");
    DynamicJsonBuffer json_buffer;
    JsonObject& json = json_buffer.createObject();

    json["access_point_prefix"] = config.access_point_prefix;
    json["access_point_password"] = config.access_point_password;
    json["mqtt_username"] = config.mqtt_username;
    json["mqtt_password"] = config.mqtt_password;
    json["mqtt_server"] = config.mqtt_server;
    json["mqtt_port"] = config.mqtt_port;
    json["mqtt_topic"] = config.mqtt_topic;  
    json["ota_password"] = config.ota_password;
    json["ota_port"] = config.ota_port;  

    File configFile = SPIFFS.open(CONFIG_FILE, "w");
    if(!configFile) {
      debug("unable to open config file");
    } else {
      debug("Opened config file");
      json.printTo(configFile);
      debug("User configuration saved");
      configFile.close();
      debug("Config file closed");
    }
    //end save
  }
  
}

/**
 * Setup (esp8266) OverTheAir update
 */
void setupArduinoOTA(){
  uint16_t port; 
  sscanf(config.ota_port, "%d", &port);
  ArduinoOTA.setPort(port);
  ArduinoOTA.setHostname(SENSORNAME);
  ArduinoOTA.setPassword(config.ota_password);
  
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

void publishDatas(JsonObject& datas) {
  
  char buffer[datas.measureLength() + 1];
  datas.printTo(buffer, sizeof(buffer));
  //Serial.println(MQTT_TOPIC);
  //Serial.println(buffer);
  mqtt_client.publish(config.mqtt_topic, buffer);
  //Serial.println("Buffer size: " + String(sizeof(buffer)));
  //Serial.print("success: "); Serial.println(success);
}

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
  
  json_data["pm10"] = data.pm10_standard;
  json_data["pm25"] = data.pm25_standard;
  json_data["pm100"] = data.pm100_standard;
    
  json_data["pe10"] = data.pm10_env;
  json_data["pe25"] = data.pm25_env;
  json_data["pe100"] = data.pm100_env;
     
  json_data["pt03"] = data.particles_03um;
  json_data["pt05"] = data.particles_05um;
  json_data["pt10"] = data.particles_10um;
  json_data["pt25"] = data.particles_25um;
  json_data["pt50"] = data.particles_50um;
  json_data["pt100"] = data.particles_100um;
    
  json_data["hcho"] = float(data.hcho / 1000.0);
  json_data["tem"] = float(data.temperature / 10.0);
  json_data["hum"] = float(data.humidity / 10.0);

  publishDatas(json_data);

  return true;
}

/**
 * Setup MQTT PubSub client
 */
void setupMqttClient() {
  uint16_t port; 
  sscanf(config.mqtt_port, "%d", &port);
  debug("mqtt server: "+ String(config.mqtt_server) + " - port: " + String(port));
  mqtt_client.setServer(config.mqtt_server, port);
} 

/**
 * Reconnects MQTT client
 */
void reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("username : '");Serial.print( config.mqtt_username);Serial.print("' - pass: '"); 
    Serial.print(config.mqtt_password); Serial.println("'");
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    String sensorname = config.access_point_prefix + String(ESP.getChipId(),HEX);
    if (mqtt_client.connect(sensorname.c_str(), config.mqtt_username, config.mqtt_password)) {
      Serial.println("connected");
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}


void setup() {
  pms_serial.begin(9600);
  #ifdef DEBUG
  Serial.begin(115200);
  #endif
  delay(1000);
  
  // uncomment to reset ESP8266
  //wifi_manager.resetSettings();
  
  debug("Setup device");
  
  loadConfig();
  setup_wifi();
  setupMqttClient();
}

void loop() {
  
    ArduinoOTA.handle();
    
    if (!mqtt_client.connected()) {
      reconnect();
    }
    mqtt_client.loop();
  
    
    if (readPMSdata(&pms_serial)) {
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
