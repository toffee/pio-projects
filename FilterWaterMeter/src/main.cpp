#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>

int PIN_SENSOR = D5;

//include/Config.h.sample needs to be copied to include/Config.h and filled with values that suite your environment 
#include "config.h"

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

byte sensorInterrupt = 0;
volatile long pulseCount = 0;
unsigned long currentMillis = 0;
unsigned long previousMillis = 0;
unsigned long publishMillis = 0;
const long intervalReading = 1000;
const long intervalPublish = 5000;
float flowRate = 0;
float oldFlowRate = 0;
float totalVolume = 0;
float oldTotalVolume = 0;

void pulse();
void connectToMqtt();

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

/*void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}*/

void onMqttPublish(uint16_t packetId) {
  //Serial.print("Publish acknowledged.");
  //Serial.print("  packetId: ");
  //Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  char new_payload[len+1];
  new_payload[len] = '\0';
  strncpy(new_payload, payload, len);
  totalVolume = atof(new_payload);
  Serial.printf("Set totalVolume value to: %.4f", totalVolume);  
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);  //baudrate
  
  //connectWifi(WIFI_SSID, WIFI_PASS);
  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);
  
  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.onMessage(onMqttMessage);
  mqttClient.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  // If your broker requires authentication (username and password), set them below
  //mqttClient.setCredentials("REPlACE_WITH_YOUR_USER", "REPLACE_WITH_YOUR_PASSWORD");
  connectToWifi();

  while(!mqttClient.connected()) {
    delay(1000);
  }

  mqttClient.subscribe(MQTT_SUB_RESPONSE, 1);  

  // Publish an MQTT message on topic request - this will triger the retrieval of volume value form openHAB
  mqttClient.publish(MQTT_PUB_REQUEST, 1, true, String("volume").c_str());

  pinMode(PIN_SENSOR, INPUT_PULLUP);
  sensorInterrupt = digitalPinToInterrupt(PIN_SENSOR);

  // Configured to trigger on a FALLING state change (transition from HIGH
  // state to LOW state)
  attachInterrupt(sensorInterrupt, pulse, FALLING);
}

void loop() {
  // put your main code here, to run repeatedly:
  long timePassed;
  currentMillis = millis();
  timePassed = currentMillis - previousMillis;
  if (timePassed > intervalReading) 
  {
    Serial.printf("Pulses: %li\t", pulseCount);

    // Disable the interrupt while calculating flow rate and sending the value to
    // the host
    detachInterrupt(sensorInterrupt);
        
    // Because this loop may not complete in exactly 1 second intervals we calculate
    // the number of milliseconds that have passed since the last execution and use
    // that to scale the output. We also apply the calibrationFactor to scale the output
    // based on the number of pulses per second per units of measure (litres/minute in
    // this case) coming from the sensor.
    float volume = float(pulseCount) / 5880; // [l]
    Serial.printf("Volume: %.3f l\t", volume);
    totalVolume += volume;
    flowRate = volume / timePassed; // [l/ms]
    flowRate = flowRate * 1000; // [l/s]
    flowRate = flowRate * 60;  // [l/min]
    
    // Note the time this processing pass was executed. Note that because we've
    // disabled interrupts the millis() function won't actually be incrementing right
    // at this point, but it will still return the value it was set to just before
    // interrupts went away.
    previousMillis = millis();
    
    // Print the flow rate for this second in litres / minute
    Serial.printf("Flow rate: %.3f l/min\t", flowRate);

    // Print the cumulative total of litres flowed since starting
    Serial.printf("Total volume: %.3f l\t%0.f ml\n", totalVolume, totalVolume*1000);        
    
    // Reset the pulse counter so we can start incrementing again
    pulseCount = 0;
    
    // Enable the interrupt again now that we've finished sending output
    attachInterrupt(sensorInterrupt, pulse, FALLING);
  }

  bool sendNow = ((unsigned long)(currentMillis - publishMillis) >= intervalPublish);
  if (sendNow && (totalVolume != oldTotalVolume || flowRate != oldFlowRate)) {
    // Publish an MQTT message on topic flow
    uint16_t packetIdPubFlow = mqttClient.publish(MQTT_PUB_FLOW, 1, true, String(flowRate, 3).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId: %i", MQTT_PUB_FLOW, packetIdPubFlow);
    Serial.printf("Message: %.4f \n", flowRate);

    // Publish an MQTT message on topic volume
    uint16_t packetIdPubVolume = mqttClient.publish(MQTT_PUB_VOLUME, 1, true, String(totalVolume, 3).c_str());
    Serial.printf("Publishing on topic %s at QoS 1, packetId: %i", MQTT_PUB_VOLUME, packetIdPubVolume);
    Serial.printf("Message: %.4f \n", totalVolume);

    oldTotalVolume = totalVolume;
    oldFlowRate	= flowRate;
		publishMillis = currentMillis;
  }
}

/**
Interrupt Service Routine
 */
ICACHE_RAM_ATTR void pulse() {
  pulseCount++;
}
