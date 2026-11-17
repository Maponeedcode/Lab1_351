#include <WiFi.h>
#include <DHT.h>
#include <MQTT.h> 

#define DHTPIN  4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

const char* WIFI_SSID     = "";
const char* WIFI_PASSWORD = "";
const char* MQTT_BROKER   = "mqtt-dashboard.com"; 
const int   MQTT_PORT     = 1883;
const char* NODE_ID       = "p01";
const char* TOPIC_ENV     = "coe67351/p01/env";
const char* TOPIC_STATUS  = "coe67351/p01/status";

WiFiClient net;
MQTTClient mqttClient(256);
unsigned long lastSensorRead = 0;
unsigned long lastReconnectAttempt = 0;
const unsigned long SENSOR_INTERVAL    = 10000;
const unsigned long RECONNECT_INTERVAL = 5000;
uint32_t seqNumber = 0;

void maintainConnections() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      Serial.println("[WiFi] Disconnected. Attempting reconnect...");
      WiFi.reconnect();
    }
    return;
  }

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      Serial.println("[MQTT] Connecting to broker...");

      mqttClient.setWill(TOPIC_STATUS, "offline", true, 1);

      String clientId = "ESP32S3-" + String(NODE_ID);
      if (mqttClient.connect(clientId.c_str())) {
        Serial.println("[MQTT] Connected!");
        mqttClient.publish(TOPIC_STATUS, "online", true, 1);
      } else {
        Serial.println("[MQTT] Connect failed, will retry...");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  net.setTimeout(1);

  mqttClient.begin(MQTT_BROKER, MQTT_PORT, net);
  mqttClient.setOptions(10, true, 1000);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    mqttClient.loop();
  }
  maintainConnections();

  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;

    float temp_c = dht.readTemperature();
    float rh     = dht.readHumidity();

    if (isnan(temp_c) || isnan(rh)) {
      Serial.println("Failed to read from DHT22 sensor!");
      return;
    }

    seqNumber++;
    long rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -100;
    char payload[150];
    snprintf(payload, sizeof(payload),
             "{\"node\":\"%s\",\"seq\":%lu,\"temp_c\":%.1f,\"rh\":%.1f,\"rssi\":%ld}",
             NODE_ID, (unsigned long)seqNumber, temp_c, rh, rssi);

    Serial.println(payload);

    if (mqttClient.connected()) {
      mqttClient.publish(TOPIC_ENV, payload, false, 1);
    }
  }
}