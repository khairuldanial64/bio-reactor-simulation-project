#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>

// WIFI
const char* ssid = ""; //to be filled 
const char* password = "";//to be filled

//  MQTT 
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;

// Publish topic (ESP → Dashboard)
const char* pubTopic = "bioreactorgrp242025ug/dataresults";

// Subscribe topics (Dashboard → ESP)
const char* tempTopic = "/publish24kljk/settemp";
const char* phTopic   = "/publish24kljk/setph";
const char* rpmTopic  = "/publish24kljk/setrpm";

// Global variables for current setpoints
float setTemp = 0.0;
float setPH   = 0.0;
float   setRPM  = 0.0;

WiFiClient espClient;
PubSubClient client(espClient);

// I2C 
#define I2C_ADDR 0x08
char buffer[40];

void sendCommand(String s) {
  Wire.beginTransmission(I2C_ADDR);
  Wire.write((uint8_t*)s.c_str(), s.length());
  Wire.endTransmission();
  Serial.println(s + " command sent");
}

// MQTT CALLBACK
void callback(char* topic, byte* payload, unsigned int length) {
  String msg = "";

  for (int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.println("-------------------------");
  Serial.print("Message received on topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  Serial.println(msg);

  // ---------- Extract "value" ----------
  int v1 = msg.indexOf("\"value\":");
  if (v1 < 0) return;
  v1 += 8;
  int v2 = msg.indexOf("}", v1);
  String value = msg.substring(v1, v2);

  if (strcmp(topic, tempTopic) == 0) {
    setTemp = value.toFloat();
    sendCommand("SET_TEMP" + value);
    Serial.printf("✔ New Temperature Setpoint: %.2f°C\n", setTemp);
  }
  else if (strcmp(topic, phTopic) == 0) {
    setPH = value.toFloat();
    sendCommand("SET_PH" + value);
    Serial.printf("✔ New pH Setpoint: %.2f\n", setPH);
  }
  else if (strcmp(topic, rpmTopic) == 0) {
    setRPM = value.toFloat();
    sendCommand("SET_RPM" + value);
    Serial.printf("✔ New RPM Setpoint: %.1f\n", setRPM);
  }

  Serial.println("----- Current Setpoints ---------------------------");
  Serial.printf("Temp: %.2f°C | pH: %.2f | RPM: %.1f\n", setTemp, setPH, setRPM);
  Serial.println("--------------------------------------------------\n");
}

//  RECONNECT MQTT
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection... ");

    if (client.connect("ESP32_Bioreactor_Client")) {
      Serial.println("connected!");

      // Subscribe to dashboard → ESP topics
      client.subscribe(tempTopic);
      client.subscribe(phTopic);
      client.subscribe(rpmTopic);

      Serial.println("Subscribed to setpoint topics:");
      Serial.println(tempTopic);
      Serial.println(phTopic);
      Serial.println(rpmTopic);

    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" — retrying in 3 seconds...");
      delay(3000);
    }
  }
}

// SETUP 
void setup() {
  Serial.begin(115200);
  Wire.begin(27, 26);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");

  // MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// loop
void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

    // request current measurements 
  Wire.requestFrom(I2C_ADDR, 32);

  if (Wire.available()) {
    byte len = Wire.read();
    int i = 0;
    while (Wire.available() && i < len) {
      buffer[i++] = Wire.read();
    }
    buffer[i] = '\0';

    Serial.println(buffer);

    String data = String(buffer);

    int firstComma  = data.indexOf(',');
    int secondComma = data.indexOf(',', firstComma + 1);

    String rpmStr  = data.substring(0, firstComma);
    String phStr   = data.substring(firstComma + 1, secondComma);
    String tempStr = data.substring(secondComma + 1);

    float rpm  = rpmStr.toFloat();
    float ph   = phStr.toFloat();
    float temp = tempStr.toFloat();

    // Create JSON payload
    String payload = "{";
    payload += "\"temperature\":" + String(temp) + ",";
    payload += "\"ph\":" + String(ph) + ",";
    payload += "\"rpm\":" + String(rpm) + ",";
    payload += "\"setTemp\":" + String(setTemp) + ",";
    payload += "\"setPH\":" + String(setPH) + ",";
    payload += "\"setRPM\":" + String(setRPM);
    payload += "}";

    // Publish every 3 seconds
    client.publish(pubTopic, payload.c_str());
    Serial.println("Published: " + payload);
    }

  delay(3000);
}
