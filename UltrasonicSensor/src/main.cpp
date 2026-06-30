#include <Arduino.h>
#include <avr/pgmspace.h>

// --- MySensors Configuration ---
#define MY_NODE_ID 100
#define MY_RADIO_RF24 
#define MY_BAUD_RATE 115200

#include <MySensors.h>

// --- Pin Assignments ---
const int TRIGGER_PIN = 5; // Connected to Arduino Pin D5 (PD5)
const int ECHO_PIN    = 6; // Connected to Arduino Pin D6 (PD6)

// --- Child Sensor IDs ---
#define CHILD_ID_DIST 181
#define CHILD_ID_VOL  182
#define CHILD_ID_VCC  99

// --- MySensors Packet Containers ---
MyMessage msgDist(CHILD_ID_DIST, V_DISTANCE);
MyMessage msgVol(CHILD_ID_VOL, V_VOLUME);
MyMessage msgVcc(CHILD_ID_VCC, V_VOLTAGE);

// --- Global Tracking Variables ---
long lastDistance = -999;
unsigned long vccTimer = 0;
const unsigned long ONE_HOUR_MS = 3600000UL; 

// --- Pre-calculated Volume Table (PROGMEM keeps this out of precious RAM) ---
const float volumeTable[] PROGMEM = {
    4.300, 4.300, 4.300, 4.300, 4.300, 4.300, 4.300, 4.300, 4.300, 4.300, // 0-9cm
    4.300, 4.300, 4.300, 4.300, 4.300, 4.300, 4.300, 4.300, 4.300, 4.300, // 10-19cm
    4.300, 4.300, 4.300, 4.300, 4.294, 4.283, 4.269, 4.252, 4.233, 4.212, // 20-29cm
    4.190, 4.165, 4.140, 4.113, 4.085, 4.056, 4.025, 3.994, 3.961, 3.928, // 30-39cm
    3.894, 3.859, 3.823, 3.786, 3.749, 3.711, 3.672, 3.633, 3.593, 3.552, // 40-49cm
    3.511, 3.470, 3.428, 3.385, 3.342, 3.299, 3.255, 3.211, 3.166, 3.121, // 50-59cm
    3.076, 3.030, 2.984, 2.938, 2.891, 2.845, 2.798, 2.751, 2.703, 2.656, // 60-69cm
    2.608, 2.560, 2.512, 2.464, 2.416, 2.368, 2.320, 2.272, 2.224, 2.176, // 70-79cm
    2.129, 2.081, 2.034, 1.987, 1.940, 1.894, 1.847, 1.801, 1.756, 1.710, // 80-89cm
    1.666, 1.621, 1.577, 1.533, 1.490, 1.447, 1.405, 1.363, 1.321, 1.280, // 90-99cm
    1.240, 1.200, 1.161, 1.122, 1.084, 1.047, 1.010, 0.974, 0.938, 0.903, // 100-109cm
    0.869, 0.835, 0.803, 0.771, 0.739, 0.709, 0.679, 0.650, 0.621, 0.593, // 110-119cm
    0.566, 0.540, 0.514, 0.489, 0.465, 0.441, 0.418, 0.396, 0.375, 0.354, // 120-129cm
    0.334, 0.315, 0.297, 0.279, 0.262, 0.245, 0.000, 0.000, 0.000, 0.000, // 130-139cm
    0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, 0.000, // 140-149cm
    0.000                                                                  // 150cm
};

// Internal Bandgap Hardware Voltmeter Function
long readVccInternal() {
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); 
  ADCSRA |= _BV(ADSC); 
  while (bit_is_set(ADCSRA, ADSC));
  uint8_t low  = ADCL;
  uint8_t high = ADCH;
  long result = (high << 8) | low;
  result = 1126400L / result; 
  return result;
}

void before() {
  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void setup() {
  // First-boot immediate VCC check
  send(msgVcc.set(readVccInternal()));
  vccTimer = millis();
}

void presentation() {
  sendSketchInfo("Ultrasonic Sensor", "1.0");
  present(CHILD_ID_DIST, S_DISTANCE, "Canister distance");
  present(CHILD_ID_VOL,  S_DUST,     "Canister water volume"); 
  present(CHILD_ID_VCC,  S_MULTIMETER, "VCC [mV]");
}

void loop() {
  // Fixes transient radio startup power spikes
  wait(100); 

  // --- 1. Measure Ultrasonic Echo ---
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); 
  
  if (duration > 0) {
    long distanceCm = duration / 29 / 2;
    
    // Bounds control
    if (distanceCm > 150) distanceCm = 150;
    if (distanceCm < 0) distanceCm = 0;

    // Report ONLY if level physically changes to reduce noise
    if (distanceCm != lastDistance) {
      float volume = pgm_read_float(&volumeTable[distanceCm]);
      
      send(msgDist.set(distanceCm));
      send(msgVol.set(volume, 3)); 
      
      lastDistance = distanceCm;
    }
  } else {
    // Hard failure timeout execution logic
    if (lastDistance != -1) {
      send(msgDist.set(-1)); // Broadcast out-of-bounds flag
      lastDistance = -1;
    }
  }

  // --- 2. Hourly VCC Evaluation ---
  if (millis() - vccTimer >= ONE_HOUR_MS) {
    send(msgVcc.set(readVccInternal()));
    vccTimer = millis(); // Refresh timestamp marker
  }

  // --- 3. Moderate Update Delay ---
  // Sleep execution pacing reduced to 15 seconds to limit framework congestion
  sleep(15000); 
}
