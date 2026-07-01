#include <Arduino.h>

int counter = 0;

void setup() {
  // Start serial communication at 9600 baud
  Serial.begin(9600);
}

void loop() {
  Serial.print("Test Counter: ");
  Serial.println(counter);
  
  counter++;
  delay(1000); // Wait 1 second