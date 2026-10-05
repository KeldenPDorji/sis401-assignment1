# SIS401 Assignment 1 - Multi-Sensor IoT Monitoring System

## What this system does

An ESP32 reads three sensors across three different hardware interfaces at the
same time. Every reading is shown on a local OLED display and published to a
Mosquitto MQTT broker over WiFi, where any subscriber can receive it.

The system monitors ambient conditions: temperature, humidity, light level and
atmospheric pressure.

## Sensors and interfaces

| Sensor | Interface | Measures | Pin |
|---|---|---|---|
| DHT22 | Digital, single-wire protocol | Temperature (°C), humidity (%) | GPIO 4 |
| LDR module | Analog, 12-bit ADC | Light level (%) | GPIO 34 |
| BMP280 | I2C | Atmospheric pressure (hPa) | GPIO 21 SDA, GPIO 22 SCL |

The SSD1306 OLED is also on the I2C bus at GPIO 21 and GPIO 22. Two devices
share one bus because they answer to different addresses: 0x3C for the display
and 0x76 for the sensor.

## Wireless protocol

MQTT over WiFi, publishing to a Mosquitto broker using the PubSubClient
library. Each measurement goes to its own topic within a hierarchy:

```
assignment1/sensors/temperature
assignment1/sensors/humidity
assignment1/sensors/light
assignment1/sensors/pressure
```

A subscriber can receive all four with a single wildcard subscription to
`assignment1/sensors/#`.

The DHT22 is one physical sensor providing two measurements, which is why
there are four topics for three sensors.

## Simultaneous operation

The three sensors have different minimum sampling periods, so each runs on its
own `millis()` schedule rather than sharing one blocking `delay()`:

| Task | Interval | Reason |
|---|---|---|
| LDR sample | 500 ms | ADC read is near-instant |
| BMP280 sample | 1000 ms | Pressure changes slowly |
| DHT22 sample | 2000 ms | Hardware minimum for this sensor |
| OLED refresh | 500 ms | Keeps the display responsive |
| MQTT publish | 2000 ms | One burst per full sensor cycle |

There is no `delay()` anywhere in `loop()`. A blocking delay would force every
sensor down to the slowest rate and would also stall the MQTT client's
keep-alive handling.

## Circuit

| Component | Module pin | ESP32 pin |
|---|---|---|
| DHT22 | VCC | 3.3 V |
| DHT22 | DATA | GPIO 4 |
| DHT22 | GND | GND |
| LDR module | VCC | 3.3 V |
| LDR module | AO | GPIO 34 |
| LDR module | GND | GND |
| LDR module | DO | not connected |
| BMP280 | VIN | 3.3 V |
| BMP280 | SDA | GPIO 21 |
| BMP280 | SCL | GPIO 22 |
| BMP280 | GND | GND |
| SSD1306 OLED | VCC | 3.3 V |
| SSD1306 OLED | SDA | GPIO 21 |
| SSD1306 OLED | SCL | GPIO 22 |
| SSD1306 OLED | GND | GND |

See `circuit.jpg` for the labelled hardware photograph.

Note: the LDR is a 4-pin breakout module with its own onboard divider, so the
analog output connects straight to GPIO 34 with no external resistor. Use the
AO pin, not DO, because DO only gives a threshold on/off signal and would not
satisfy the analog interface requirement.

## Required libraries

Install through Arduino IDE Library Manager:

- Adafruit GFX Library
- Adafruit SSD1306
- Adafruit Unified Sensor
- Adafruit BMP280 Library
- DHT sensor library (Adafruit)
- PubSubClient (Nick O'Leary)

`Wire.h` and `WiFi.h` ship with the ESP32 board package.

## Setup and run

**1. Install the ESP32 board package**

Arduino IDE → Settings → Additional boards manager URLs:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Then Boards Manager → search `esp32` → install *esp32 by Espressif Systems*.

**2. Start the MQTT broker**

On macOS:

```
brew install mosquitto
```

Add these two lines to `/opt/homebrew/etc/mosquitto/mosquitto.conf`:

```
listener 1883
allow_anonymous true
```

Without the `listener` line Mosquitto only accepts connections from the local
machine and the ESP32 will not be able to reach it.

Start it and find the broker's IP address:

```
brew services start mosquitto
ipconfig getifaddr en0
```

**3. Verify the I2C wiring**

Upload `i2c_scanner.ino` and open the Serial Monitor at 115200 baud. Two
addresses should be reported, 0x3C and 0x76.

**4. Configure the firmware**

Edit these three lines near the top of `assignment1.ino`:

```cpp
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* MQTT_SERVER   = "YOUR_BROKER_IP";
```

The ESP32 and the broker machine must be on the same network, and the network
must be 2.4 GHz because the ESP32 cannot see 5 GHz.

**5. Upload and run**

Select *ESP32 Dev Module* and the correct port, then upload. Open the Serial
Monitor at 115200 baud. Expect one line per two-second cycle:

```
[  12.0s] T=27.5C  H=72.1%  L=60% (raw 1705)  P=960.7hPa  -> published
```

**6. Verify the wireless transmission**

From any machine on the same network:

```
mosquitto_sub -h <broker-ip> -t 'assignment1/sensors/#' -v
```

All four topics should arrive every two seconds.

## Calibrating the light sensor

Most 4-pin LDR modules output a lower voltage as light increases, so the raw
ADC value is inverted before being converted to a percentage. If covering the
sensor makes the percentage rise instead of fall, change this line:

```cpp
const bool LDR_INVERTED = true;    // set to false if the reading is backwards
```

## Repository contents

| File | Purpose |
|---|---|
| `assignment1.ino` | Main firmware |
| `README.md` | This document |
| `REFLECTION.md` | Required reflection on the work |
| `circuit.jpg` | Labelled photograph of the hardware |