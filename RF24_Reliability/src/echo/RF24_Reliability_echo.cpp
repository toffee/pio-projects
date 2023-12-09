/****
  RF24_Reliability_echo
  This sketch is ment to work in conjunktion with another unit running RF24_Reliability_main
  Ron Miller (RonM9) July 2015
***/

#include <SPI.h>
#include <ESP8266WiFi.h>
#include "RF24.h"

//include/Config.h.sample needs to be copied to include/Config.h and filled with values that suite your environment 
#include "config.h"

WiFiClient client;

#define RF_CS 4
#define RF_CSN 15

RF24 radio(RF_CS, RF_CSN);

const uint64_t pipe = 0xE8E8F0F0E1LL;

struct MyData {
  byte dummy[4];
  unsigned long timestamp;
};
MyData data;

// ==================================================
void setup(void)
{
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  radio.begin();
  radio.setDataRate(RF24_250KBPS);
  radio.enableAckPayload();
  radio.openReadingPipe(1,pipe);
  radio.startListening();
  radio.setPALevel(RF24_PA_HIGH);  // Testing with default power level = HIGH Not MAX
  Serial.println("Power set to HIGH, Level: 3");
}

// ==================================================
void loop(void)
{
  while ( radio.available() )
  {
    radio.writeAckPayload( 1, &data, sizeof(MyData) );
    radio.read( &data, sizeof(MyData) );
    Serial.print("Acking: ");
    Serial.println(data.timestamp);
  }
  //----------------- Change Power level via Serial Commands
  if ( Serial.available() )
  {
    char c = toupper(Serial.read());
    if      ( c == '4') radio.setPALevel(RF24_PA_MAX); 
    else if ( c == '3') radio.setPALevel(RF24_PA_HIGH); 
    else if ( c == '2') radio.setPALevel(RF24_PA_LOW); 
    else if ( c == '1') radio.setPALevel(RF24_PA_MIN); 
    Serial.print("*** Changing Power to Level: ");
    Serial.println(c);
  }
}
