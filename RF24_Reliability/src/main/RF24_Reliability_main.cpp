/*
   RF24_Reliability_main
   This sketch is for testing Tx-Rx Reliability, at a given range.

  This code tests and reports on Tx-Rx Reliability, 
  to/from an echoing remote unit running the RF24_Reliability_echo.ino code
  
  Statistics, packets/sec and % success attempts, are gathered for a period of time (default 60 secs)
  Then reported on the serial console. Which is running at 115,200 baud in order not to slow us down.
  The main unit transmits at 3 different power levels, to better test result resolution.

  I used this code on modified RF24 radio under-test with this 'main' code 
  and an unmodified/pretested radio module with the 'echo' code at a remote location.
  
  Optionally a 16x2 LCD can be used to view results of each individual second, plus running averages.
  An optional button attached to 'btnPin' can be used to start a new test round (without a restart).
  Refer to http://arduino-info.wikispaces.com/ for info connecting RF24 (using SPI) and 16x2 LCD (using I2C)
  Change pin assignments as needed, below.
  
  Ron Miller (RonM9) July 2015
  I started this sketch with some code which was used in this video https://www.youtube.com/watch?v=lR60toEjHl8
     
*/

#include <SPI.h>
#include <Wire.h>

#include <RF24.h>
#include <LiquidCrystal_I2C.h>

#define LCD_CONNECTED false
// I2C address
#define LCD1602_ADDRESS 0x27
LiquidCrystal_I2C lcd(LCD1602_ADDRESS, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

#define RF_CS 9
#define RF_CSN 10
const uint64_t pipeOut = 0xE8E8F0F0E1LL;
RF24 radio(RF_CS, RF_CSN);
byte pwrLevel;

struct MyData {
  byte dummy[4];
  unsigned long timestamp;
};
MyData data;
char buffer[32];

const int btnPin = 5;   // the number of the push-button pin
int parts, partSpec=6;  // the factor used in calculation running averages
                        // Running average takes about 12 secs for 98% settling, when doing "4" part proportions

byte dspMode = 1;
char thisline[64];

unsigned int packetsSent = 0;
unsigned int packetsAcked = 0;
unsigned int latency = 0;

unsigned long lastLatencyUpdate = 0;

float avgSpeed = 0;
float avgQuality = 0;
float avgLatency = 0;

int maxSpeed, maxQuality, maxLatency;
int minSpeed, minQuality, minLatency;

bool autoStop=true;   // if true, after 'numSamples' taken min, max and average will be displayed
int numSamples=60;    // # of packet/sec samples (taken at 1 sample per second)
int samples=0, sampleSum=0;
int qualitySum=0;

void latency_loop();

void displayMaxMins();

// ==================================================
void setup()
{
  maxSpeed = maxQuality = maxLatency = 0;
  minSpeed = minQuality = minLatency =999;

  Serial.begin(115200);
  if (LCD_CONNECTED) {
    // LCD initialization
    lcd.begin(16, 2);              // initialize the lcd
    lcd.home ();                   // go home
    lcd.print ("RF24 Reliability Test");
  }

  radio.begin();
  //radio.setAutoAck(false);
  radio.setDataRate(RF24_250KBPS);
  radio.enableAckPayload();

  radio.openWritingPipe(pipeOut);
  memset(&data, 0, sizeof(MyData));
    
  radio.setPALevel(RF24_PA_LOW);  // Testing with default HIGH Not MAX  (July 2014 rrm)
  Serial.println("Power set to HIGH (not Max)");  // High has tested as more reliable than Max power level

  parts = 0;  // will work its way up to partSpec; keeping the averages scaled right from the get go
 
  pinMode(btnPin, INPUT);
  digitalWrite ( btnPin, HIGH );
  delay(100);
}

// ==================================================
void loop()
{
  if (dspMode ==1) {
    latency_loop();  // real-time Tx/Rx, Calculate & Display
  }  // else display is froze with whats last on it

  if (digitalRead(btnPin) == LOW) {
    parts=0; // effectively resets the running averages
    if (dspMode!=2) {
      displayMaxMins();  // display summary Max/min averages
    }
    dspMode=2;
    maxSpeed = maxQuality = maxLatency = 0;
    minSpeed = minQuality = minLatency =999;
    latency = packetsSent = packetsAcked = 0;
    lastLatencyUpdate = millis();
    samples = 0;
    sampleSum = 0; qualitySum=0;
  } else if (autoStop && samples>=numSamples) {
    if (dspMode!=2) {
      displayMaxMins();  // display summary Max/min averages
    }
    dspMode=2;    
  } else {
    dspMode=1;
  }
}

// ---------------------------------------------------
void latency_loop()
{
  //  radio.powerUp();
  //  delay(3);
  unsigned long now = millis();
  
  data.timestamp = now;
  radio.write(&data, sizeof(MyData));
  
  ////////////////////
  // if you comment out the Serial. statments in this block of code the maximum possible through-put
  // goes up from ~400/s to almost 600/s; but I don't think that the corresponding resuts are anymore revealing 
  Serial.print("Sent: ");
  Serial.print(data.timestamp);
  packetsSent++;
  // Note: .write (above) does not return until its received an Ack or has done max reties
  while ( radio.isAckPayloadAvailable()) {
    radio.read(&data, sizeof(MyData));
    Serial.print("  Ack: ");
    Serial.print(data.timestamp);
    latency += now - data.timestamp - 1;
    packetsAcked++;
  }
  Serial.println(' ');
  /////////////////////
  
  if ( now - lastLatencyUpdate > 1000 ) {  // have we got one seconds worth of test data ?
    
    float thisQuality = packetsAcked / (float)packetsSent;
    float thisLatency = packetsAcked > 0 ? latency / (float)packetsAcked : 0;
    
    if (parts < partSpec) parts++;
    avgSpeed   = ((parts-1) * avgSpeed + packetsAcked)  / parts;
    avgQuality = ((parts-1) * avgQuality + thisQuality) / parts;      
    avgLatency = ((parts-1) * avgLatency + thisLatency) / parts;
    // update Max/Mins
    if (parts == partSpec) {  // need enough samples averaged before considering Mins/Maxs
      if (avgSpeed>maxSpeed) maxSpeed = (int) avgSpeed;
      if (avgQuality*100>maxQuality) maxQuality = avgQuality*100;
      if (avgLatency>maxLatency) maxLatency = (int) avgLatency;
      if (avgSpeed<minSpeed) minSpeed = (int) avgSpeed;
      if (avgQuality*100<minQuality) minQuality = avgQuality*100;
      if (avgLatency<minLatency) minLatency = (int) avgLatency;
      sampleSum += packetsAcked;
      samples++;
      qualitySum += thisQuality * 100;
      
      // test with a portion of Max/High/Low power settings to get a better measure of relative reliability
      //      pwrLevel = samples % 4;  // 0-3  PA_MIN to PA_MAX
      pwrLevel = samples % 3 + 1;    // don't use RF24_PA_MIN
      // if      (pwrLevel == 1) radio.setPALevel(RF24_PA_LOW);
      // else if (pwrLevel == 2) radio.setPALevel(RF24_PA_HIGH);
      // else if (pwrLevel == 3) radio.setPALevel(RF24_PA_MAX);
      // sprintf(thisline, "Power level now set to: %d   Running Avg: %d", pwrLevel, (int)avgSpeed);
      // Serial.println(thisline);
    }
        
    if (LCD_CONNECTED) {
      // clear the screen
      lcd.clear();
      lcd.setCursor(0, 0); //Start at character 0 on line 0
      sprintf(thisline, "%d  %d %% %d ms", packetsAcked, (int)(thisQuality*100), (int)thisLatency);
      lcd.print(thisline);
      lcd.setCursor(0, 1); //Start at character 0 on line 1
      sprintf(thisline, "%d  %d %% %d ms", (int)avgSpeed, (int)(avgQuality*100), (int)avgLatency);
      // display each character to the LCD
      lcd.print(thisline);
    }
        
    latency = 0;
    packetsSent = 0;
    packetsAcked = 0;
    lastLatencyUpdate = millis(); // use millis again instead of 'now' to ignore time taken for screen update
  }
}

// ---------------------------------------------------
void displayMaxMins() {
  Serial.println(" ");
  Serial.println("Recent Minimum & Maximum averages: ");
  sprintf(thisline, "Mins: %d/s  %d%% %d ms", minSpeed, minQuality, minLatency);
  Serial.println(thisline);
  sprintf(thisline, "Maxs: %d/s  %d%% %d ms", maxSpeed, maxQuality, maxLatency);
  Serial.println(thisline);
  Serial.print("The Average packets per second: ");  // Only seen on Serial Monitor
  Serial.print(sampleSum / samples);
  sprintf(thisline, "   Success rate: %d %%", qualitySum / samples);
  Serial.println(thisline);
  
  if (LCD_CONNECTED) {
    // clear the screen
    lcd.clear();
    lcd.setCursor(0, 0); //Start at character 0 on line 0
    //    sprintf(thisline, "%d  %d %% %d ms", minSpeed, minQuality, minLatency);
    sprintf(thisline, "Avg: %d/s  %d%%", sampleSum/samples, qualitySum/samples);
    lcd.print(thisline);
    lcd.setCursor(0, 1); //Start at character 0 on line 1
    //    sprintf(thisline, "%d  %d %% %d ms", maxSpeed, maxQuality, maxLatency);
    sprintf(thisline, "Max: %d/s  %d%%", maxSpeed, maxQuality);
    // display each character to the LCD
    lcd.print(thisline);
  }
}
