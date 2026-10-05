/*
  SIS401 - Programming Assignment 1
  Multi-Sensor IoT Monitoring System with Wireless Communication

  DHT22  (digital, GPIO4)   temperature + humidity
  LDR    (analog,  GPIO34)  light level
  BMP280 (I2C, 0x76)        atmospheric pressure
  SSD1306 OLED (I2C, 0x3C) shares the bus with the BMP280

  Readings are shown on the OLED and published to a Mosquitto MQTT broker.
*/

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>
#include <DHT.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define I2C_SDA   21
#define I2C_SCL   22
#define DHT_PIN   4
#define LDR_PIN   34

#define DHT_TYPE    DHT22
#define OLED_ADDR   0x3C
#define BMP_ADDR    0x76
#define SCREEN_W    128
#define SCREEN_H    64
#define OLED_RESET  -1

const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* MQTT_SERVER   = "YOUR_BROKER_IP";

const int   MQTT_PORT      = 1883;
const char* MQTT_CLIENT_ID = "SIS401_A1_ESP32";

const char* TOPIC_TEMP     = "assignment1/sensors/temperature";
const char* TOPIC_HUMID    = "assignment1/sensors/humidity";
const char* TOPIC_LIGHT    = "assignment1/sensors/light";
const char* TOPIC_PRESSURE = "assignment1/sensors/pressure";

// Most LDR modules drive AO lower as light rises, so the reading is inverted.
// Set false if covering the sensor makes the percentage go up.
const bool LDR_INVERTED = true;

// Each sensor has its own rate: the DHT22 cannot be read faster than 2 s,
// while the ADC and BMP280 can be read far more often.
const unsigned long LDR_INTERVAL     = 500;
const unsigned long BMP_INTERVAL     = 1000;
const unsigned long DHT_INTERVAL     = 2000;
const unsigned long DISPLAY_INTERVAL = 500;
const unsigned long PUBLISH_INTERVAL = 2000;

unsigned long lastLdr = 0, lastBmp = 0, lastDht = 0;
unsigned long lastDisplay = 0, lastPublish = 0;

float temperature = NAN;
float humidity    = NAN;
float pressure    = NAN;
int   lightRaw    = 0;
int   lightPct    = 0;

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);
Adafruit_BMP280  bmp;
DHT              dht(DHT_PIN, DHT_TYPE);
WiFiClient       wifiClient;
PubSubClient     mqtt(wifiClient);

void connectWiFi() {
  Serial.print("WiFi: connecting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print(" connected, IP ");
  Serial.println(WiFi.localIP());
}

// Returns immediately either way, so a broker outage never stalls sampling.
void ensureMqtt() {
  if (mqtt.connected()) return;

  if (mqtt.connect(MQTT_CLIENT_ID)) {
    Serial.println("MQTT: connected");
  } else {
    Serial.print("MQTT: failed, rc=");
    Serial.println(mqtt.state());
  }
}

void sampleLdr() {
  lightRaw = analogRead(LDR_PIN);
  int pct  = map(lightRaw, 0, 4095, 0, 100);
  lightPct = LDR_INVERTED ? (100 - pct) : pct;
}

void sampleBmp() {
  pressure = bmp.readPressure() / 100.0F;
}

// Keeps the last good values on a failed read rather than blanking the screen.
void sampleDht() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("DHT22: read failed");
    return;
  }

  temperature = t;
  humidity    = h;
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 2);
  display.println("SIS401 Sensor Hub");
  display.drawFastHLine(0, 12, SCREEN_W, SSD1306_WHITE);

  display.setCursor(0, 18);
  display.print("Temp : "); display.print(temperature, 1); display.println(" C");
  display.print("Hum  : "); display.print(humidity, 1);    display.println(" %");
  display.print("Light: "); display.print(lightPct);       display.println(" %");
  display.print("Pres : "); display.print(pressure, 1);    display.println(" hPa");

  display.drawFastHLine(0, 53, SCREEN_W, SSD1306_WHITE);
  display.setCursor(0, 56);
  display.print(mqtt.connected() ? "MQTT: online" : "MQTT: offline");

  display.display();
}

// MQTT payloads are raw bytes, so each value is sent as text.
void publishReadings() {
  if (!mqtt.connected()) return;

  mqtt.publish(TOPIC_TEMP,     String(temperature, 1).c_str());
  mqtt.publish(TOPIC_HUMID,    String(humidity, 1).c_str());
  mqtt.publish(TOPIC_LIGHT,    String(lightPct).c_str());
  mqtt.publish(TOPIC_PRESSURE, String(pressure, 1).c_str());
}

void printReadings() {
  Serial.printf("[%6.1fs] T=%.1fC  H=%.1f%%  L=%d%% (raw %4d)  P=%.1fhPa  %s\n",
                millis() / 1000.0,
                temperature, humidity, lightPct, lightRaw, pressure,
                mqtt.connected() ? "-> published" : "-> MQTT down");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== SIS401 Assignment 1 : Multi-Sensor IoT Monitor ===");

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED: NOT FOUND, halting");
    for (;;);
  }
  Serial.println("OLED: found at 0x3C");

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println("SIS401 Sensor Hub");
  display.println("Starting up...");
  display.display();

  dht.begin();

  if (!bmp.begin(BMP_ADDR)) {
    Serial.println("BMP280: NOT FOUND, halting");
    for (;;);
  }
  Serial.println("BMP280: found at 0x76");

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                  Adafruit_BMP280::SAMPLING_X2,
                  Adafruit_BMP280::SAMPLING_X16,
                  Adafruit_BMP280::FILTER_X16,
                  Adafruit_BMP280::STANDBY_MS_500);

  analogReadResolution(12);

  connectWiFi();
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  ensureMqtt();

  Serial.println("--- sampling: LDR 0.5s, BMP280 1s, DHT22 2s ---");
}

// No delay() here: a blocking wait would slow every sensor to the slowest
// rate and stall the MQTT keep-alive handled inside mqtt.loop().
void loop() {
  unsigned long now = millis();

  mqtt.loop();

  if (now - lastLdr >= LDR_INTERVAL) {
    lastLdr = now;
    sampleLdr();
  }

  if (now - lastBmp >= BMP_INTERVAL) {
    lastBmp = now;
    sampleBmp();
  }

  if (now - lastDht >= DHT_INTERVAL) {
    lastDht = now;
    sampleDht();
  }

  if (now - lastDisplay >= DISPLAY_INTERVAL) {
    lastDisplay = now;
    updateDisplay();
  }

  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;
    ensureMqtt();
    publishReadings();
    printReadings();
  }
}