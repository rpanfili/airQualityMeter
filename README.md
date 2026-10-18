# Air Quality Meter
> "There's so much pollution in the air now that if it weren't for our lungs there'd be no place to put it all."
> \- Robert Orben
 
Detects **air particulate matter** (PM - pm1, pm2.5, pm10) concentrations with a **Plantower PMS7003** sensor and sends data to an **MQTT topic**.

An alternative firmware for _ESP8266 devices_ like the [NodeMCU](https://github.com/nodemcu/nodemcu-firmware) board written for [**Arduino**](https://www.arduino.cc/) on [**PlatformIO**](https://platformio.org/)

[![License](https://img.shields.io/github/license/xtianpaiva/airQualityMeter.svg)](LICENSE.txt) [![Build Status](https://travis-ci.com/xtianpaiva/airQualityMeter.svg?branch=master)](https://travis-ci.com/xtianpaiva/airQualityMeter)

### Disclaimer

We don't take any responsibility nor liability for using this software nor for the installation or any tips, advice, videos, etc. given by any member of this site or any related site.

## Hardware
### NodeMCU
An **opensource IoT platform**, tiny and powerful, based on the **ESP-12** module.
It's very affordable (you can buy _them_ for a few bucks) and you can develop custom firmwares like this one using _Lua_ scription language or in _C/C++_ with the _arduino_ tools!

![nodeMCU board](resources/NodeMCU_DEVKIT_1.0.jpg)

[NodeMCU on Wikipedia](https://en.wikipedia.org/wiki/NodeMCU)

### Plantower PMS7003
A **digital particle concentration sensor** that count the number of suspended particles in the air using a laser and the scattering principle and output them as digital data.
It's not expensive and you can find it on almost every marketplace like AliExpress.

The PMS7003 reports particle concentrations and particle counts. It does not measure formaldehyde, temperature or humidity.

## Installing / Getting started

A quick introduction of the minimal setup you need to get all up and
running. Remember that you *can* do all the setup steps without the PMS sensor connected!

#### Get [PlatformIO](https://platformio.org/) up and running
If you are used to this kind of software you already know how easier is to manage project dependencies, custom build routine, etc but if you are not don't worry! It's really easy and this can be a good starting point to learn something new :) You only need to [install it](https://platformio.org/platformio-ide) for your favorite IDE between Atom and VSCode.

#### [NOT REQUIRED] - Edit the configuration file
If you would like it you can copy `data/config.example.json` to `data/config.json` and edit it to **preload all parameters** and skip the access-point mode configuration step later. The local `config.json` file is ignored by Git so that Wi-Fi, MQTT and OTA passwords are not committed accidentally.
If you want you can configure only some of the parameters. Remove the property or leave it empty to use default project values.
Then you will need to upload this file into the SPI filesystem using the PlatformIO command:
```shell
pio run -t uploadfs
```
or by using the IDE top menu: **"PlatformIO" >> "Run other target..." >> "PIO Upload File System Image"**

#### Flash the module
Connect the NodeMCU via usb and from the top menu press **"PlatformIO" >> "Build"** and hopefully that's all! 

### Initial Configuration

After being flashed this application needs almost no configuration. Just to setup the desired wifi network.
To do that power the device up using the module micro-usb port. It'll boot in Access-point mode and a new wifi ssid will be available:
```
ssid: AQM-xxxxxx
pass: printed on the serial monitor at startup
```
On the first boot, the firmware generates a unique access-point password and prints it at 115200 baud. Connect to the access point using that password; a configuration page will be displayed. If it does not open automatically, go to [http://192.168.4.1](http://192.168.4.1).

Follow the instructions and you will configure the sensor "everyday" network!

If you want you can skip this part using the `data/config.json` **configuration file**. See [Edit the configuration file](#not-required---edit-the-configuration-file).

### Security notes

- Set a unique OTA password of at least eight characters to enable OTA updates. OTA remains disabled until then.
- MQTT credentials and measurements are sent without transport encryption. Use a trusted, isolated network until TLS support is configured for your broker.
- The device stores its configuration in SPIFFS. Treat physical access to the device as access to its Wi-Fi, MQTT and OTA credentials.

## Development

Run the hardware-independent tests before building firmware:

```shell
pio test -e native
pio run
```

The native tests cover port parsing, PMS7003 frame parsing and the Home Assistant discovery registry. The firmware build still requires a compatible ESP8266 toolchain; deploy it only after a successful build.

## Usage

This are the minimal wiring that this project needs:

![Fritzing schematics](resources/airQualitySensor_bb.png)

([fritzing schematics](resources/airQualitySensor.fzz) attached)

| PMS PIN | NodeMCU PIN | PMS wire color | 
| ------ | ------ | ------ |
| 1 (VCC) | VIN | <span style="color:purple">purple</span> |
| 2  (GND)| GND | <span style="color:orange">orange</span> |
| 3 (SET) |  | <span style="color:white; background-color: black">white</span> |
| 4 (RX)| D8 | <span style="color:blue">blue</span> |
| 5 (TX)| D7 | <span style="color:green">green</span> |
| 6 (RESET)|  | <span style="color:yellow">yellow</span> |
| 7 (NC)|  | <span style="color:black">black</span> |
| 8 (NC)|  | <span style="color:red">red</span> |

## (extra) HomeAssistant

The firmware uses MQTT discovery with the standard `homeassistant` discovery prefix. When it connects to the broker, Home Assistant automatically creates 12 entities for PM concentrations and particle counts, groups them under one device and tracks its MQTT availability. No YAML configuration is required.

The configured `mqtt_topic` remains the state topic. The firmware publishes `online` and `offline` status to `<mqtt_topic>/availability`. Discovery configuration messages are retained, while measurement values are not retained.

I personally collect all these data on my **HomeAssistant** instance. It's an opensource home automation software that runs perfectly on micropc like **RaspberryPI** or local servers (NAS, nuc, etc)

Take a look at [https://www.home-assistant.io/](https://www.home-assistant.io/)

This is the sensor configuration:
```yaml
homeassistant:
  sensor:
    - platform: mqtt
      state_topic: "my_house/entryway/air_quality_meter"
      name: "air_quality_meter"
      value_template: '{{ value_json }}'
      json_attributes:
        - pm10
        - pm25
        - pm100
        - pe10
        - pe25
        - pe100
        - pt03
        - pt05
        - pt10
        - pt25
        - pt50
        - pt100
        - sensor_version

    - platform: template
      sensors:
        air_quality_pm10:
          friendly_name: PM1.0
          value_template: '{{ states.sensor.air_quality_meter.attributes.pm10 }}'
          unit_of_measurement: 'μg/m3'
        air_quality_pm25:
          friendly_name: PM2.5
          value_template: '{{ states.sensor.air_quality_meter.attributes.pm25 }}'
          unit_of_measurement: 'μg/m3'
        air_quality_pm100:
          friendly_name: PM10
          value_template: '{{ states.sensor.air_quality_meter.attributes.pm100 }}'
          unit_of_measurement: 'μg/m3'        
        air_quality_pm10_env:
          friendly_name: PM1.0 (atmospheric environment)
          value_template: '{{ states.sensor.air_quality_meter.attributes.pe10 }}'
          unit_of_measurement: 'μg/m3'
        air_quality_pm25_env:
          friendly_name: PM2.5 (atmospheric environment)
          value_template: '{{ states.sensor.air_quality_meter.attributes.pe25 }}'
          unit_of_measurement: 'μg/m3' 
        air_quality_pm100_env:
          friendly_name: PM10 (atmospheric environment)
          value_template: '{{ states.sensor.air_quality_meter.attributes.pe100 }}'
          unit_of_measurement: 'μg/m3'

        air_quality_pt03:
          friendly_name: Particles > 0.3um / 0.1L air
          value_template: '{{ states.sensor.air_quality_meter.attributes.pt03 }}'
        air_quality_pt05:
          friendly_name: Particles > 0.5um / 0.1L air
          value_template: '{{ states.sensor.air_quality_meter.attributes.pt05 }}'        
        air_quality_pt10:
          friendly_name: Particles > 1.0um / 0.1L air
          value_template: '{{ states.sensor.air_quality_meter.attributes.pt10 }}'
        air_quality_pt25:
          friendly_name: Particles > 2.5um / 0.1L air
          value_template: '{{ states.sensor.air_quality_meter.attributes.pt25 }}'        
        air_quality_pt50:
          friendly_name: Particles > 5.0um / 0.1L air
          value_template: '{{ states.sensor.air_quality_meter.attributes.pt50 }}'
        air_quality_pt100:
          friendly_name: Particles > 10.0um / 0.1L air
          value_template: '{{ states.sensor.air_quality_meter.attributes.pt100 }}'        

```

and this is an example Lovelace UI view:

```yaml
title: Home
views: 

 - title: Air quality
    id: air-quality
    icon: mdi:speedometer
    cards:
    - type: glance
      title: Concentration Units (standard)
      show_header_toggle: false
      entities:
        - sensor.air_quality_pm10
        - sensor.air_quality_pm25
        - sensor.air_quality_pm100
    - type: glance
      title: Concentration Units (environmental)
      show_header_toggle: false
      entities:
        - name: PM1.0
          entity: sensor.air_quality_pm10_env
        - name: PM2.5
          entity: sensor.air_quality_pm25_env
        - name: PM10
          entity: sensor.air_quality_pm100_env
    - type: glance
      title: Number of particles
      show_header_toggle: false
      columns: 6
      entities:
        - name: "> 0.3μg"
          entity: sensor.air_quality_pt03
        - name: "> 0.5μg"
          entity: sensor.air_quality_pt05
        - name: "> 1.0μg"
          entity: sensor.air_quality_pt10 
        - name: "> 2.5μg"
          entity: sensor.air_quality_pt25
        - name: "> 5.0μg"
          entity: sensor.air_quality_pt50 
        - name: "> 10.0μg"
          entity: sensor.air_quality_pt100           
        
    - type: history-graph
      title: 'Concentration Units (standard)'
      entities:
        - sensor.air_quality_pm10
        - sensor.air_quality_pm25
        - sensor.air_quality_pm100  
        
    - type: history-graph
      title: 'Concentration Units (environmental)'
      entities:
        - sensor.air_quality_pm10
        - sensor.air_quality_pm25
        - sensor.air_quality_pm100  
```

to get something like this:
![homeassistant](resources/homeassistant.png)

### Contribute
You can contribute to this project by
- providing Pull Requests (Features, Proof of Concepts, Language files or Fixes)
- testing new released features and report issues
- donating to acquire hardware for testing and implementing or out of gratitude

### Credits

#### Libraries Used
Libraries used with this project are:
- [ESP8266 core for Arduino](https://github.com/esp8266/Arduino)
- [PubSubClient](https://github.com/knolleary/pubsubclient)
- [WiFiManger](https://github.com/tzapu/WiFiManager)
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)

### License

This program is licensed under GPL-3.0
