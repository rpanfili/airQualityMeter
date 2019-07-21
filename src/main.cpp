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
#define CONFIG_FILE "/config.json"

struct configStruct
{
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

// Uncomment/comment to turn on/off debug output messages.
#define DEBUG_NOTICE
// Uncomment/comment to turn on/off error output messages.
#define DEBUG_ERROR

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

#define SENSORNAME "air_quality_meter"

#define HTTP_PORT 80
ESP8266WebServer server(HTTP_PORT);

#define PMS5003ST_SIZE 40
#define PMS5003ST_SIG1 0X42
#define PMS5003ST_SIG2 0X4d

struct pms5003STdata
{
  uint16_t pm10_standard, pm25_standard, pm100_standard;
  uint16_t pm10_env, pm25_env, pm100_env;
  uint16_t particles_03um, particles_05um, particles_10um, particles_25um, particles_50um, particles_100um;
  uint16_t hcho, temperature, humidity;
  uint16_t reserved;
  uint16_t unused;
  uint16_t checksum;
};

struct pms5003STdata pms_data;

void debug(String str)
{
#ifdef DEBUG
  uint32_t now = millis();
  Serial.printf("%07u.%03u: %s\n", now / 1000, now % 1000, str.c_str());
#endif // DEBUG
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

  Serial.println("mounting FS..");
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
        std::unique_ptr<char[]> buffer(new char[size]);
        configFile.readBytes(buffer.get(), size);
        DynamicJsonBuffer json_buffer;
        JsonObject &json = json_buffer.parseObject(buffer.get());
        if (json.success())
        {
          debug("json loaded");
          json.printTo(Serial);

          strlcpy(config.wifi_ssid, json.get<const char *>("wifi_ssid"), sizeof(config.wifi_ssid));
          strlcpy(config.wifi_password, json.get<const char *>("wifi_password"), sizeof(config.wifi_password));
          if ((config.wifi_ssid[0] != '\0'))
          {
            WiFi.mode(WIFI_STA);
            WiFi.begin(config.wifi_ssid, config.wifi_password);
          }

          /*
           * Safely-load configuration values from config.json (manages also 
           * undefined keys)
           */
          if (json.get<const char *>("access_point_prefix")[0] != '\0')
          {
            strlcpy(config.access_point_prefix, json["access_point_prefix"], sizeof(config.access_point_prefix));
          }
          if (json.get<const char *>("access_point_password")[0] != '\0')
          {
            strlcpy(config.access_point_password, json["access_point_password"], sizeof(config.access_point_password));
          }
          if (json.get<const char *>("mqtt_server")[0] != '\0')
          {
            strlcpy(config.mqtt_server, json["mqtt_server"], sizeof(config.mqtt_server));
          }
          if (json.get<const char *>("mqtt_port")[0] != '\0')
          {
            strlcpy(config.mqtt_port, json["mqtt_port"], sizeof(config.mqtt_port));
          }
          if (json.get<const char *>("mqtt_username")[0] != '\0')
          {
            strlcpy(config.mqtt_username, json["mqtt_username"], sizeof(config.mqtt_username));
          }
          if (json.get<const char *>("mqtt_password")[0] != '\0')
          {
            strlcpy(config.mqtt_password, json["mqtt_password"], sizeof(config.mqtt_password));
          }
          if (json.get<const char *>("mqtt_topic")[0] != '\0')
          {
            strlcpy(config.mqtt_topic, json["mqtt_topic"], sizeof(config.mqtt_topic));
          }
          if (json.get<const char *>("ota_password")[0] != '\0')
          {
            strlcpy(config.ota_password, json["ota_password"], sizeof(config.ota_password));
          }
          if (json.get<const char *>("ota_port")[0] != '\0')
          {
            strlcpy(config.ota_port, json["ota_port"], sizeof(config.ota_port));
          }
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
  wifi_manager.setSaveConfigCallback(unlock_save_config);
  wifi_manager.setAPCallback(config_mode_callback);

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
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

JsonObject &to_json(pms5003STdata data)
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

  json_data["hcho"] = float(data.hcho / 1000.0);
  json_data["tem"] = float(data.temperature / 10.0);
  json_data["hum"] = float(data.humidity / 10.0);
  return json_data;
}

boolean publish_data(pms5003STdata &pms_data)
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
  if (bytes_to_read < PMS5003ST_SIZE)
  {
    if (bytes_to_read)
    {
      while (bytes_to_read-- && stream->peek() != PMS5003ST_SIG1)
      {
        // flush invalid chars
        buffer_char = stream->read();
        DEBUG_PRINT("skip char 0x");
        DEBUG_PRINTLN(buffer_char, HEX);
      }
      /*if(stream->peek() == PMS5003ST_SIG1){ 
        DEBUG_PRINTLN("buffer ready!"); 
      }
      else { 
        DEBUG_PRINTLN("buffer empty!"); 
      }*/
    }
    delay(50);
    return false;
  }

  buffer_char = stream->read();
  //DEBUG_PRINT("0x"); DEBUG_PRINT(buffer_char, HEX); DEBUG_PRINTLN(); return false;

  // Read a byte at a time until we get to the special '0x42' start-byte
  //uint8_t buffer_char = stream->read();

  if (buffer_char != PMS5003ST_SIG1)
  {
    ERROR_PRINT("[1] skip char 0x");
    ERROR_PRINTLN(buffer_char, HEX);
    return false;
  }
  DEBUG_PRINTLN("found start char");

  buffer_char = stream->read();
  if (buffer_char != PMS5003ST_SIG2)
  {
    ERROR_PRINT("[2] next char is not SIG2, skip 0x");
    ERROR_PRINTLN(buffer_char, HEX);
    return false;
  }
  DEBUG_PRINTLN("found second char");

  uint8_t framelen_high = stream->read();
  uint8_t framelen_low = stream->read();
  uint16_t framelen = framelen_low | framelen_high << 8;

  if (framelen != (PMS5003ST_SIZE - 4))
  {
    ERROR_PRINT("PMS5003ST : invalid framelength - ");
    ERROR_PRINTLN(framelen, DEC);
    return false;
  }
  DEBUG_PRINTLN("Framelen OK"); //DEBUG_PRINTLN(framelen, DEC);

  uint8_t buffer[framelen];
  uint16_t sum = 0;
  sum += (uint16_t)PMS5003ST_SIG1 + (uint16_t)PMS5003ST_SIG2;
  sum += (uint16_t)framelen_low + (uint16_t)framelen_high;

  stream->readBytes(buffer, framelen);
  // get checksum ready (framelen - 16bit checksum)
  for (uint8_t i = 0; i < (framelen - 2); i++)
  {
    sum += (uint16_t)buffer[i];
  }

  /* debugging */
  DEBUG_PRINT("FRAME: ");
  for (uint8_t i = 0; i < framelen; i++)
  {
    DEBUG_PRINT("0x");
    DEBUG_PRINT(buffer[i], HEX);
    DEBUG_PRINT(", ");
  }
  DEBUG_PRINTLN();

  // The data comes in big endians
  int buffer_u16_length = framelen / 2;
  uint16_t buffer_u16[buffer_u16_length];
  for (uint8_t i = 0; i < buffer_u16_length; i++)
  {
    buffer_u16[i] = buffer[i * 2 + 1] | (buffer[i * 2] << 8);
  }
  //DEBUG_PRINTLN("buffer16 OK");
  //return false;

  // put it into a nice struct :)
  memcpy((void *)&pms_data, (void *)buffer_u16, sizeof(pms_data));
  //DEBUG_PRINTLN("Struct OK");

  if (sum != pms_data.checksum)
  {
    ERROR_PRINTLN("Checksum failure");
    //DEBUG_PRINTLN("Sum: "+ String(sum) + " - check: "+ String(pms_data.checksum));
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
  uint16_t port;
  sscanf(config.mqtt_port, "%d", &port);
  debug("mqtt server: " + String(config.mqtt_server) + " - port: " + String(port));
  mqtt_client.setServer(config.mqtt_server, port);
}

/**
 * Reconnects MQTT client
 */
void reconnect()
{
  // Loop until we're reconnected
  while (!mqtt_client.connected())
  {
    Serial.print("username : '");
    Serial.print(config.mqtt_username);
    Serial.print("' - pass: '");
    Serial.print(config.mqtt_password);
    Serial.println("'");
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    String sensorname = config.access_point_prefix + String(ESP.getChipId(), HEX);
    if (mqtt_client.connect(sensorname.c_str(), config.mqtt_username, config.mqtt_password))
    {
      Serial.println("connected");
    }
    else
    {
      Serial.print("failed, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void print_pms_data(pms5003STdata data)
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
  DEBUG_PRINT("HCHO: ");
  DEBUG_PRINTLN(float(data.hcho / 1000.0), 4);
  DEBUG_PRINT("Temperature: ");
  DEBUG_PRINTLN(float(data.temperature / 10.), 2);
  DEBUG_PRINT("Humidity: ");
  DEBUG_PRINTLN(float(data.humidity / 10.0), 2);
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
#ifdef DEBUG
  Serial.begin(115200);
#endif
  delay(1000);

  // uncomment to reset ESP8266
  //wifi_manager.resetSettings();

  debug("Setup device");

  load_config();
  setup_wifi();
  setup_mqtt_client();
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
