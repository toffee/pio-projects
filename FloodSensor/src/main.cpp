// https://www.saicharan.in/blog/2023/01/22/diy-water-leak-sensor-with-esp8266/
// https://github.com/scharan/homebridge-nodeMCU/blob/master/ESP8266-12F-NodeMCU/WatSen.ino

#include <Arduino.h>

const int analogInPin = A0;  // ESP8266 Analog Pin ADC0 = A0

enum LEAK_STATUS {
  NOT_DETECTED = 0,
  DETECTED = 1
};

int ledPin = 2; // GPIO2 & built-in, on-board LED
int buzzerPin = D1;

LEAK_STATUS getLeakStatus() {
  
  int sensorValue = analogRead(analogInPin);
  Serial.printf("Sensor = %d\n", sensorValue);

  LEAK_STATUS ls = LEAK_STATUS::NOT_DETECTED;
  // ESP2866 on ADC0 is 10 bit, so gets a range 0-1024
  // Here, we consider anything >25% of the range ((i.e., > 256) as a leak
  if (sensorValue <= 256) {
    ls = LEAK_STATUS::NOT_DETECTED;
    digitalWrite(ledPin, HIGH);
    digitalWrite(buzzerPin, HIGH);
  } else {
    ls = LEAK_STATUS::DETECTED;
    digitalWrite(ledPin, LOW);
    digitalWrite(buzzerPin, LOW);
  }

  return ls;
}

void setup() {
  Serial.begin(9600);
  // Pin 2 has an integrated LED - configure it, and turn it off
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(ledPin, HIGH);

  Serial.println("Ready");
}

void loop() {
  LEAK_STATUS ls = getLeakStatus();
  if (ls == LEAK_STATUS::DETECTED) {
      Serial.println("{\"LeakDetected\":1}");
  } else {
      Serial.println("{\"LeakDetected\":0}");
  }
  delay(500);
}
