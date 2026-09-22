#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <WebServer.h>
#include "mqtt_client.h"   // ESP-IDF's MQTT client (esp-mqtt), bundled with the Arduino core
#include "ca_cert.h"

// Display resolution
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// SSD1306 on I2C, address 0x3C, no reset pin
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// DHT22 on GPIO15 (GPIO3 is UART0 RX and clashes with Serial)
#define DHT_PIN 15
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// MPU6050 registers (datasheet: MPU-6000/6050 Register Map)
#define MPU_ADDR        0x68  // 0x68 with AD0 tied to GND, 0x69 with AD0 at 3V3
#define REG_PWR_MGMT_1  0x6B  // Power management; the chip powers up in sleep mode
#define REG_ACCEL_X_H   0x3B  // First accelerometer register (6 bytes: X, Y, Z)
#define REG_WHO_AM_I    0x75  // Identity register, always reads 0x68
#define ACCEL_LSB_PER_G 16384.0  // Default ±2 g range: 1 g = 16384 LSB

bool mpuPresent = false;

// Wi-Fi: Wokwi's virtual access point, open, channel 6
const char *WIFI_SSID = "Wokwi-GUEST";
const char *WIFI_PASSWORD = "";
WebServer server(80);

// MQTT: public test broker (user/password public/public).
// The host network only allows ports 80/443; MQTT's own ports (1883, 8883) are blocked.
// So MQTT is tunnelled over a TLS WebSocket on 443: wss://
const char *MQTT_URI = "wss://public.cloud.shiftr.io:443";
// Topics on a public broker are visible to everyone, so use a unique prefix
#define TOPIC_PREFIX   "yavuz-iot-sensor/esp32"
#define TOPIC_DATA     TOPIC_PREFIX "/data"      // device publishes readings here
#define TOPIC_STATUS   TOPIC_PREFIX "/status"    // online / offline
#define TOPIC_COMMAND  TOPIC_PREFIX "/command"   // device subscribes to this
esp_mqtt_client_handle_t mqtt;

// esp-mqtt runs in its own FreeRTOS task, so these are touched by two tasks
volatile bool mqttConnected = false;
char lastCommand[32] = "";  // last message on the command topic, shown on the OLED
portMUX_TYPE commandLock = portMUX_INITIALIZER_UNLOCKED;  // guards lastCommand

// Latest readings, shared by the display, the web server and MQTT
float humidity = NAN, temperature = NAN;
float ax = 0, ay = 0, az = 0;

// Write one byte to a register
void writeRegister(uint8_t addr, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  Wire.endTransmission();
}

// Read count bytes starting at reg (the sensor auto-increments the register pointer)
bool readRegisters(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t count) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;  // false: repeated START instead of STOP
  if (Wire.requestFrom(addr, count) != count) return false;
  for (uint8_t i = 0; i < count; i++) buf[i] = Wire.read();
  return true;
}

// Probe every 7-bit address and print the ones that ACK
void scanI2C() {
  Serial.println("Scanning I2C bus...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {  // 0 = device ACKed
      Serial.printf("  Device found at 0x%02X\n", addr);
      found++;
    }
  }
  Serial.printf("%d device(s) found\n", found);
}

// GET /: a simple page that refreshes itself every 2 s
void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<meta http-equiv='refresh' content='2'>"
                "<meta name='viewport' content='width=device-width'>"
                "<title>ESP32 Sensor</title></head>"
                "<body style='font-family:sans-serif;text-align:center'>"
                "<h1>ESP32 Sensor</h1>";
  html += "<p style='font-size:2em'>🌡 " + String(temperature, 1) + " °C</p>";
  html += "<p style='font-size:2em'>💧 " + String(humidity, 1) + " %</p>";
  html += "<p>Uptime: " + String(millis() / 1000) + " s</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

// GET /data: the same readings as JSON, for other programs
void handleData() {
  String json = "{\"temperature\":" + String(temperature, 1) +
                ",\"humidity\":" + String(humidity, 1) +
                ",\"ax\":" + String(ax, 2) +
                ",\"ay\":" + String(ay, 2) +
                ",\"az\":" + String(az, 2) + "}";
  server.send(200, "application/json", json);
}

void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }
  Serial.print("\nConnected, IP: ");
  Serial.println(WiFi.localIP());
}

// esp-mqtt calls this from its own task for every event (connected, disconnected, data...).
// Reconnecting is handled by the library, so loop() does not need to check the link.
void onMqttEvent(void *arg, esp_event_base_t base, int32_t eventId, void *eventData) {
  esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)eventData;
  switch ((esp_mqtt_event_id_t)eventId) {
    case MQTT_EVENT_CONNECTED:
      mqttConnected = true;
      Serial.println("MQTT connected");
      // retain = 1: the broker keeps this message, so late subscribers see the status too
      esp_mqtt_client_publish(mqtt, TOPIC_STATUS, "online", 0, 1, 1);
      esp_mqtt_client_subscribe(mqtt, TOPIC_COMMAND, 0);
      break;

    case MQTT_EVENT_DISCONNECTED:
      mqttConnected = false;
      Serial.println("MQTT disconnected, the library will retry");
      break;

    case MQTT_EVENT_DATA: {
      // Payload is not null-terminated; its length is passed separately
      int n = min(event->data_len, (int)sizeof(lastCommand) - 1);
      portENTER_CRITICAL(&commandLock);
      memcpy(lastCommand, event->data, n);
      lastCommand[n] = '\0';
      portEXIT_CRITICAL(&commandLock);
      Serial.printf("MQTT [%.*s]: %.*s\n", event->topic_len, event->topic, event->data_len, event->data);
      break;
    }

    case MQTT_EVENT_ERROR:
      Serial.printf("MQTT error (type %d)\n", event->error_handle->error_type);
      break;

    default:
      break;
  }
}

void startMqtt() {
  esp_mqtt_client_config_t cfg = {};
  cfg.uri = MQTT_URI;
  cfg.username = "public";
  cfg.password = "public";
  cfg.cert_pem = CA_CERT;  // verify the server really is the broker, using this root CA
  // Last Will: if the device drops without saying goodbye, the broker publishes this for it
  cfg.lwt_topic = TOPIC_STATUS;
  cfg.lwt_msg = "offline";
  cfg.lwt_qos = 1;
  cfg.lwt_retain = 1;
  cfg.keepalive = 15;  // broker treats the device as gone after 15 s of silence

  mqtt = esp_mqtt_client_init(&cfg);
  esp_mqtt_client_register_event(mqtt, MQTT_EVENT_ANY, onMqttEvent, NULL);
  esp_mqtt_client_start(mqtt);
  Serial.println("MQTT client started");
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // SDA, SCL
  dht.begin();

  scanI2C();

  // Identify the MPU6050 and wake it from sleep
  uint8_t id = 0;
  if (readRegisters(MPU_ADDR, REG_WHO_AM_I, &id, 1) && id == 0x68) {
    writeRegister(MPU_ADDR, REG_PWR_MGMT_1, 0x00);
    mpuPresent = true;
    Serial.println("MPU6050 ready");
  } else {
    Serial.printf("MPU6050 not found (WHO_AM_I = 0x%02X)\n", id);
  }

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 not found"));
    for (;;);  // stop here
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println("Connecting Wi-Fi...");
  display.display();

  connectWiFi();

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  Serial.println("Web server started");

  startMqtt();
}

void loop() {
  // Answer pending HTTP requests; this is why loop() must never block for long
  server.handleClient();

  // Publish readings every 5 s
  static unsigned long lastPublish = 0;
  if (mqttConnected && millis() - lastPublish >= 5000) {
    lastPublish = millis();
    char json[128];
    snprintf(json, sizeof(json),
             "{\"temperature\":%.1f,\"humidity\":%.1f,\"ax\":%.2f,\"ay\":%.2f,\"az\":%.2f}",
             temperature, humidity, ax, ay, az);
    esp_mqtt_client_publish(mqtt, TOPIC_DATA, json, 0, 0, 0);
  }

  // Update sensors and display every 500 ms
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 500) return;
  lastUpdate = millis();

  // Acceleration: 6 bytes, each axis a big-endian signed 16-bit value
  uint8_t raw[6];
  if (mpuPresent && readRegisters(MPU_ADDR, REG_ACCEL_X_H, raw, 6)) {
    ax = (int16_t)(raw[0] << 8 | raw[1]) / ACCEL_LSB_PER_G;
    ay = (int16_t)(raw[2] << 8 | raw[3]) / ACCEL_LSB_PER_G;
    az = (int16_t)(raw[4] << 8 | raw[5]) / ACCEL_LSB_PER_G;
  }

  // The DHT22 can be read at most once every 2 s; keep the last value in between
  static unsigned long lastDht = 0;
  if (millis() - lastDht >= 2000) {
    lastDht = millis();
    humidity = dht.readHumidity();
    temperature = dht.readTemperature();
    if (isnan(humidity) || isnan(temperature)) Serial.println("DHT22 read error");
    // Print only on DHT reads so the serial log does not drown out MQTT messages
    Serial.printf("T: %.1f C | RH: %.1f %% | ax: %.2f ay: %.2f az: %.2f g\n",
                  temperature, humidity, ax, ay, az);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.printf("Temp:     %.1f C", temperature);
  display.setCursor(0, 12);
  display.printf("Humidity: %.1f %%", humidity);
  display.setCursor(0, 21);
  char command[sizeof(lastCommand)];
  portENTER_CRITICAL(&commandLock);
  strcpy(command, lastCommand);
  portEXIT_CRITICAL(&commandLock);
  if (command[0] != '\0') {
    display.printf("> %s", command);
  } else {
    display.print(WiFi.localIP());
    display.print(mqttConnected ? " MQTT" : "");
  }

  display.setCursor(0, 30);
  if (mpuPresent) {
    display.printf("ax: %+.2f g", ax);
    display.setCursor(0, 42);
    display.printf("ay: %+.2f g", ay);
    display.setCursor(0, 54);
    display.printf("az: %+.2f g", az);
  } else {
    display.print("MPU6050 missing");
  }
  display.display();
}
