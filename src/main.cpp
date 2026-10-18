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
#include <memory>

#include "config_utils.h"
#include "homeassistant_discovery.h"
#include "pms7003_frame.h"

/*
 * Modify this file to preload default configuration!
 */
#define CONFIG_FILE "/config.json"

struct configStruct
{
  char wifi_ssid[32] = "";
  char wifi_password[64] = "";
  char access_point_prefix[25] = "AQM-";
  char access_point_password[64] = "";
  char mqtt_server[40] = "";
  char mqtt_port[6] = "1883";
  char mqtt_username[32] = "";
  char mqtt_password[64] = "";
  char mqtt_topic[200] = "air_quality_meter/status";
  char ota_password[64] = "";
  char ota_port[6] = "8266";
};

configStruct config;

// Uncomment/comment to turn on/off debug output messages.
// #define DEBUG_NOTICE
// Uncomment/comment to turn on/off error output messages.
// #define DEBUG_ERROR

// Set where debug messages will be printed.
#define DEBUG_PRINTER Serial
// If using something like Zero or Due, change the above to SerialUSB

#ifdef DEBUG_NOTICE
#define DEBUG_PRINT(...)              \
  {                                   \
    DEBUG_PRINTER.print(__VA_ARGS__); \
  }
#define DEBUG_PRINTLN(...)              \
  {                                     \
    DEBUG_PRINTER.println(__VA_ARGS__); \
  }
#define DEBUG_PRINTBUFFER(buffer, len) \
  {                                    \
    printBuffer(buffer, len);          \
  }
#else
#define DEBUG_PRINT(...) \
  {                      \
  }
#define DEBUG_PRINTLN(...) \
  {                        \
  }
#define DEBUG_PRINTBUFFER(buffer, len) \
  {                                    \
  }
#endif

#ifdef DEBUG_ERROR
#define ERROR_PRINT(...)              \
  {                                   \
    DEBUG_PRINTER.print(__VA_ARGS__); \
  }
#define ERROR_PRINTLN(...)              \
  {                                     \
    DEBUG_PRINTER.println(__VA_ARGS__); \
  }
#define ERROR_PRINTBUFFER(buffer, len) \
  {                                    \
    printBuffer(buffer, len);          \
  }
#else
#define ERROR_PRINT(...) \
  {                      \
  }
#define ERROR_PRINTLN(...) \
  {                        \
  }
#define ERROR_PRINTBUFFER(buffer, len) \
  {                                    \
  }
#endif

SoftwareSerial pms_serial(D7, D8);

WiFiClient esp_client;
WiFiManager wifi_manager;
PubSubClient mqtt_client(esp_client);

// semaphore to save config file
bool proceed_and_save_configuration = false;
unsigned long last_mqtt_attempt = 0;

#define SENSORNAME "air_quality_meter"
#define FIRMWARE_VERSION "2.0.0"

#define HTTP_PORT 80
ESP8266WebServer server(HTTP_PORT);

pms7003data pms_data;

void debug(String str)
{
#ifdef DEBUG
  uint32_t now = millis();
  Serial.printf("%07u.%03u: %s\n", now / 1000, now % 1000, str.c_str());
#endif // DEBUG
}

bool copy_config_value(JsonObject &json, const char *key, char *target, size_t target_size)
{
  const char *value = json.get<const char *>(key);
  if (value == nullptr || value[0] == '\0')
  {
    return false;
  }

  strlcpy(target, value, target_size);
  return true;
}

/**
 * let the configuration save process begin
 */
void unlock_save_config()
{
  proceed_and_save_configuration = true;
}

/**
 * Load configuration from file ./config.json
 * 
 * Modify this file to preload default configuration!
 */
void load_config()
{

  debug("mounting FS..");
  if (SPIFFS.begin())
  {
    debug("filesystem mounted");
    if (SPIFFS.exists(CONFIG_FILE))
    {
      File configFile = SPIFFS.open(CONFIG_FILE, "r");
      if (!configFile)
      {
        debug("unable to open config file");
      }
      else
      {
        debug("Opened config file");
        /*
         * allocate a buffer to store data inside the file
         */
        size_t size = configFile.size();
        if (size == 0 || size > 2048)
        {
          debug("config file has an invalid size");
          configFile.close();
          return;
        }

        std::unique_ptr<char[]> buffer(new char[size + 1]);
        size_t bytes_read = configFile.readBytes(buffer.get(), size);
        buffer[bytes_read] = '\0';
        configFile.close();
        if (bytes_read != size)
        {
          debug("unable to read config file");
          return;
        }

        DynamicJsonBuffer json_buffer;
        JsonObject &json = json_buffer.parseObject(buffer.get());
        if (json.success())
        {
          debug("json loaded");
          copy_config_value(json, "wifi_ssid", config.wifi_ssid, sizeof(config.wifi_ssid));
          copy_config_value(json, "wifi_password", config.wifi_password, sizeof(config.wifi_password));
          if ((config.wifi_ssid[0] != '\0'))
          {
            WiFi.mode(WIFI_STA);
            WiFi.begin(config.wifi_ssid, config.wifi_password);
          }

          /*
           * Safely-load configuration values from config.json (manages also 
           * undefined keys)
           */
          copy_config_value(json, "access_point_prefix", config.access_point_prefix, sizeof(config.access_point_prefix));
          copy_config_value(json, "access_point_password", config.access_point_password, sizeof(config.access_point_password));
          copy_config_value(json, "mqtt_server", config.mqtt_server, sizeof(config.mqtt_server));
          copy_config_value(json, "mqtt_port", config.mqtt_port, sizeof(config.mqtt_port));
          copy_config_value(json, "mqtt_username", config.mqtt_username, sizeof(config.mqtt_username));
          copy_config_value(json, "mqtt_password", config.mqtt_password, sizeof(config.mqtt_password));
          copy_config_value(json, "mqtt_topic", config.mqtt_topic, sizeof(config.mqtt_topic));
          copy_config_value(json, "ota_password", config.ota_password, sizeof(config.ota_password));
          copy_config_value(json, "ota_port", config.ota_port, sizeof(config.ota_port));
        }
        else
        {
          debug("failed to load json");
        }
      }
    }
  }
}

void config_mode_callback(WiFiManager *wifi_manager)
{
  debug("Entered AP mode");
}

void setup_wifi()
{
  debug("Trying to connect to WiFi network");
  delay(200);

  if (strlen(config.access_point_password) < 8)
  {
    randomSeed(ESP.getCycleCount() ^ ESP.getChipId() ^ micros());
    snprintf(config.access_point_password, sizeof(config.access_point_password), "aqm-%08lx", static_cast<unsigned long>(random(0x7fffffff)));
    Serial.printf("Configuration portal password: %s\n", config.access_point_password);
  }

  wifi_manager.setSaveConfigCallback(unlock_save_config);
  wifi_manager.setAPCallback(config_mode_callback);

  WiFiManagerParameter custom_access_point_prefix("access_point_prefix", "AP mode - SSID prefix", config.access_point_prefix, sizeof(config.access_point_prefix) - 1);
  WiFiManagerParameter custom_access_point_password("access_point_password", "AP mode - password", config.access_point_password, sizeof(config.access_point_password) - 1);
  WiFiManagerParameter custom_mqtt_server("mqtt_server", "MQTT server address", config.mqtt_server, sizeof(config.mqtt_server) - 1);
  WiFiManagerParameter custom_mqtt_port("mqtt_port", "MQTT server port", config.mqtt_port, sizeof(config.mqtt_port) - 1);
  WiFiManagerParameter custom_mqtt_username("mqtt_username", "MQTT username", config.mqtt_username, sizeof(config.mqtt_username) - 1);
  WiFiManagerParameter custom_mqtt_password("mqtt_password", "MQTT password", config.mqtt_password, sizeof(config.mqtt_password) - 1);
  WiFiManagerParameter custom_mqtt_topic("mqtt_topic", "MQTT topic", config.mqtt_topic, sizeof(config.mqtt_topic) - 1);
  WiFiManagerParameter custom_ota_password("ota_password", "OTA password", config.ota_password, sizeof(config.ota_password) - 1);
  WiFiManagerParameter custom_ota_port("ota_port", "OTA port", config.ota_port, sizeof(config.ota_port) - 1);

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
  wifi_manager.setTimeout(300); // Timeout 5 mins.

  String ssid = config.access_point_prefix + String(ESP.getChipId(), HEX);

  if (!wifi_manager.autoConnect(ssid.c_str(), config.access_point_password))
  {
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
  if (proceed_and_save_configuration)
  {
    debug("Saving configuration");
    DynamicJsonBuffer json_buffer;
    JsonObject &json = json_buffer.createObject();

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
    if (!configFile)
    {
      debug("unable to open config file");
    }
    else
    {
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
void setup_arduino_ota()
{
  if (strlen(config.ota_password) < 8)
  {
    debug("OTA disabled: configure a password of at least 8 characters");
    return;
  }

  uint16_t port = parse_port(config.ota_port, 8266);
  ArduinoOTA.setPort(port);
  String hostname = String(SENSORNAME) + "-" + String(ESP.getChipId(), HEX);
  ArduinoOTA.setHostname(hostname.c_str());
  ArduinoOTA.setPassword(config.ota_password);

  ArduinoOTA.onStart([]() {
    debug("Starting OTA");
  });
  ArduinoOTA.onEnd([]() {
    debug("OTA complete");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (total > 0)
    {
      DEBUG_PRINT("Progress: ");
      DEBUG_PRINT((progress * 100) / total);
      DEBUG_PRINTLN("%");
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    DEBUG_PRINT("OTA Error[");
    DEBUG_PRINT(error);
    DEBUG_PRINT("]: ");
    if (error == OTA_AUTH_ERROR)
      DEBUG_PRINTLN("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      DEBUG_PRINTLN("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      DEBUG_PRINTLN("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      DEBUG_PRINTLN("Receive Failed");
    else if (error == OTA_END_ERROR)
      DEBUG_PRINTLN("End Failed");
  });
  ArduinoOTA.begin();
}

JsonObject &to_json(const pms7003data &data)
{
  StaticJsonBuffer<1024> json_buffer;
  JsonObject &json_data = json_buffer.createObject();

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

  json_data["sensor_version"] = data.version;
  return json_data;
}

boolean publish_data(const pms7003data &pms_data)
{
  DEBUG_PRINTLN("Prepare json object");
  JsonObject &data = to_json(pms_data);

  DEBUG_PRINTLN("Convert json object to string");
  size_t buffer_length = data.measureLength() + 1;
  char buffer[buffer_length];
  DEBUG_PRINT("Buffer length: ");
  DEBUG_PRINTLN(buffer_length);
  data.printTo(buffer, sizeof(buffer));
  DEBUG_PRINTLN(buffer);
  return mqtt_client.publish(config.mqtt_topic, buffer);
}

boolean has_pms_data(Stream *stream)
{

  int bytes_to_read = stream->available();
  uint8_t buffer_char;
  /*
   * Fastly flush characters from Serial buffer while waiting for start message
   * delimiters
   */
  if (bytes_to_read < PMS7003_FRAME_SIZE)
  {
    if (bytes_to_read)
    {
      while (bytes_to_read-- && stream->peek() != 0x42)
      {
        // flush invalid chars
        buffer_char = stream->read();
        DEBUG_PRINT("skip char 0x");
        DEBUG_PRINTLN(buffer_char, HEX);
      }
    }
    delay(50);
    return false;
  }

  uint8_t frame[PMS7003_FRAME_SIZE];
  frame[0] = stream->read();
  if (frame[0] != 0x42)
  {
    ERROR_PRINT("[1] skip char 0x");
    ERROR_PRINTLN(frame[0], HEX);
    return false;
  }
  DEBUG_PRINTLN("found start char");

  frame[1] = stream->read();
  if (frame[1] != 0x4D)
  {
    ERROR_PRINT("[2] next char is not SIG2, skip 0x");
    ERROR_PRINTLN(frame[1], HEX);
    return false;
  }
  DEBUG_PRINTLN("found second char");

  if (stream->readBytes(frame + 2, PMS7003_FRAME_SIZE - 2) != PMS7003_FRAME_SIZE - 2)
  {
    ERROR_PRINTLN("PMS7003 : incomplete frame");
    return false;
  }

  /* debugging */
  DEBUG_PRINT("FRAME: ");
  for (uint8_t i = 0; i < PMS7003_FRAME_SIZE; i++)
  {
    DEBUG_PRINT("0x");
    DEBUG_PRINT(frame[i], HEX);
    DEBUG_PRINT(", ");
  }
  DEBUG_PRINTLN();

  if (!parse_pms7003_frame(frame, sizeof(frame), pms_data))
  {
    ERROR_PRINTLN("PMS7003 : invalid frame");
    return false;
  }
  if (pms_data.error != 0)
  {
    ERROR_PRINT("PMS7003 error code: ");
    ERROR_PRINTLN(pms_data.error);
    return false;
  }
  DEBUG_PRINTLN("Checksum OK");

  return true;
}

/**
 * Setup MQTT PubSub client
 */
void setup_mqtt_client()
{
  uint16_t port = parse_port(config.mqtt_port, 1883);
  debug("mqtt server: " + String(config.mqtt_server) + " - port: " + String(port));
  mqtt_client.setServer(config.mqtt_server, port);
}

void availability_topic(char *topic, size_t topic_size)
{
  snprintf(topic, topic_size, "%s/availability", config.mqtt_topic);
}

bool publish_homeassistant_discovery_sensor(const char *device_id, const char *availability, const homeassistant_sensor &sensor)
{
  char discovery_topic[128];
  snprintf(discovery_topic, sizeof(discovery_topic), "homeassistant/sensor/%s/%s/config", device_id, sensor.object_id);

  char unique_id[80];
  snprintf(unique_id, sizeof(unique_id), "%s_%s", device_id, sensor.object_id);

  char value_template[64];
  snprintf(value_template, sizeof(value_template), "{{ value_json.%s }}", sensor.json_key);

  char device_name[80];
  snprintf(device_name, sizeof(device_name), "Air Quality Meter %06X", ESP.getChipId());

  StaticJsonBuffer<768> json_buffer;
  JsonObject &payload = json_buffer.createObject();
  payload["name"] = sensor.name;
  payload["unique_id"] = unique_id;
  payload["state_topic"] = config.mqtt_topic;
  payload["value_template"] = value_template;
  payload["unit_of_measurement"] = sensor.unit;
  payload["state_class"] = "measurement";
  payload["availability_topic"] = availability;
  payload["payload_available"] = "online";
  payload["payload_not_available"] = "offline";
  if (sensor.device_class != nullptr)
  {
    payload["device_class"] = sensor.device_class;
  }

  JsonObject &device = payload.createNestedObject("device");
  JsonArray &identifiers = device.createNestedArray("identifiers");
  identifiers.add(device_id);
  device["name"] = device_name;
  device["manufacturer"] = "airQualityMeter";
  device["model"] = "ESP8266 + PMS7003";
  device["sw_version"] = FIRMWARE_VERSION;

  size_t payload_size = payload.measureLength() + 1;
  if (payload_size + strlen(discovery_topic) + 10 > MQTT_MAX_PACKET_SIZE)
  {
    debug("Home Assistant discovery message is too large");
    return false;
  }

  char message[1024];
  payload.printTo(message, sizeof(message));
  return mqtt_client.publish(discovery_topic, message, true);
}

bool publish_homeassistant_discovery(const char *availability)
{
  char device_id[40];
  snprintf(device_id, sizeof(device_id), "%s_%06X", SENSORNAME, ESP.getChipId());

  bool published = true;
  for (size_t index = 0; index < homeassistant_sensor_count; index++)
  {
    if (!publish_homeassistant_discovery_sensor(device_id, availability, homeassistant_sensors[index]))
    {
      published = false;
    }
  }
  return published;
}

/**
 * Reconnects MQTT client
 */
void reconnect()
{
  const unsigned long retry_interval = 5000;
  if (mqtt_client.connected() || millis() - last_mqtt_attempt < retry_interval)
  {
    return;
  }

  last_mqtt_attempt = millis();
  String sensorname = String(SENSORNAME) + "-" + String(ESP.getChipId(), HEX);
  char availability[220];
  availability_topic(availability, sizeof(availability));
  if (mqtt_client.connect(sensorname.c_str(), config.mqtt_username, config.mqtt_password, availability, 0, true, "offline"))
  {
    debug("MQTT connected");
    mqtt_client.publish(availability, "online", true);
    if (!publish_homeassistant_discovery(availability))
    {
      debug("Home Assistant discovery failed");
    }
  }
  else
  {
    debug("MQTT connection failed, rc=" + String(mqtt_client.state()));
  }
}

void print_pms_data(const pms7003data &data)
{
  DEBUG_PRINTLN("---------------------------------------");
  DEBUG_PRINTLN("Concentration Units (standard)");
  DEBUG_PRINT("PM 1.0: ");
  DEBUG_PRINTLN(data.pm10_standard);
  DEBUG_PRINT("PM 2.5: ");
  DEBUG_PRINTLN(data.pm25_standard);
  DEBUG_PRINT("PM 10: ");
  DEBUG_PRINTLN(data.pm100_standard);
  DEBUG_PRINTLN("---------------------------------------");
  DEBUG_PRINTLN("Concentration Units (environmental)");
  DEBUG_PRINT("PM 1.0: ");
  DEBUG_PRINTLN(data.pm10_env);
  DEBUG_PRINT("PM 2.5: ");
  DEBUG_PRINTLN(data.pm25_env);
  DEBUG_PRINT("PM 10: ");
  DEBUG_PRINTLN(data.pm100_env);
  DEBUG_PRINTLN("---------------------------------------");
  DEBUG_PRINT("Particles > 0.3um / 0.1L air:");
  DEBUG_PRINTLN(data.particles_03um);
  DEBUG_PRINT("Particles > 0.5um / 0.1L air:");
  DEBUG_PRINTLN(data.particles_05um);
  DEBUG_PRINT("Particles > 1.0um / 0.1L air:");
  DEBUG_PRINTLN(data.particles_10um);
  DEBUG_PRINT("Particles > 2.5um / 0.1L air:");
  DEBUG_PRINTLN(data.particles_25um);
  DEBUG_PRINT("Particles > 5.0um / 0.1L air:");
  DEBUG_PRINTLN(data.particles_50um);
  DEBUG_PRINT("Particles > 10.0 um / 0.1L air:");
  DEBUG_PRINTLN(data.particles_100um);
  DEBUG_PRINTLN("---------------------------------------");
  DEBUG_PRINT("Sensor version: ");
  DEBUG_PRINTLN(data.version);
  DEBUG_PRINTLN("---------------------------------------");
}

void publishDatas(JsonObject &datas)
{

  char buffer[datas.measureLength() + 1];
  datas.printTo(buffer, sizeof(buffer));
  mqtt_client.publish(config.mqtt_topic, buffer);
}

void setup()
{
  pms_serial.begin(9600);
  Serial.begin(115200);
  delay(1000);

  // uncomment to reset ESP8266
  //wifi_manager.resetSettings();

  debug("Setup device");

  load_config();
  setup_wifi();
  setup_mqtt_client();
  setup_arduino_ota();
}

void loop()
{

  ArduinoOTA.handle();

  if (!mqtt_client.connected())
  {
    reconnect();
  }
  mqtt_client.loop();

  if (has_pms_data(&pms_serial))
  {
    print_pms_data(pms_data);
    publish_data(pms_data);
  }

  //delay(5000);
}
