#include <SPI.h>
#include <RF24.h>
#include <EEPROM.h>

const byte address[6] = "00002"; // radio sync number tx/rx must be same
RF24 radio(9, 10);               // select CE,CSN pin
const int min_input = 0;         // arduino joystick min value
const int max_input = 1023;      // arduino joystick max value 
const int trim_step = 5;         // trim adjustment per button press (microseconds)
const int max_trim = 100;        // max trim adjustment (microseconds)

// EEPROM addresses
const int addr_roll_trim = 0;    // Address for roll trim
const int addr_pitch_trim = 4;   // Address for pitch trim

struct PacketData {
  byte ch1_0;  byte ch1_1;
  byte ch2_0;  byte ch2_1;
  byte ch3_0;  byte ch3_1;
  byte ch4_0;  byte ch4_1;
  byte ch5_0;  byte ch5_1;
  byte ch6_0;  byte ch6_1;
  byte ch7_0;  byte ch7_1;
  byte ch8_0;  byte ch8_1;
};
PacketData data;

// Trim variables
int roll_trim = 0;  // Trim for roll (ch1)
int pitch_trim = 0; // Trim for pitch (ch2)

void setup() {
  radio.begin();
  radio.openWritingPipe(address);
  radio.stopListening(); 
  pinMode(2, INPUT_PULLUP);   // toggle switch 8ch
  pinMode(3, INPUT_PULLUP);   // toggle switch 7ch
  pinMode(4, INPUT_PULLUP);   // toggle switch 6ch
  pinMode(5, INPUT_PULLUP);   // 3 position toggle switch 5ch
  pinMode(6, INPUT_PULLUP);   // 3 position toggle switch 5ch
  pinMode(7, INPUT_PULLUP);   // roll trim up
  pinMode(8, INPUT_PULLUP);   // roll trim down
  pinMode(0, INPUT_PULLUP);   // pitch trim up
  pinMode(1, INPUT_PULLUP);  // pitch trim down

  // Load trim values from EEPROM
  EEPROM.get(addr_roll_trim, roll_trim);
  EEPROM.get(addr_pitch_trim, pitch_trim);
  // Validate loaded values
  if (roll_trim > max_trim || roll_trim < -max_trim) roll_trim = 0;
  if (pitch_trim > max_trim || pitch_trim < -max_trim) pitch_trim = 0;
}

int rtnSwitchValue(int value) {
  if (value == 0) {
    return 1000;
  } else {
    return 2000;
  }
}

int rtn3PosSwitchValue(int state1, int state2) {
  if (state1 == 0 && state2 == 0) {
    return 1000; // Stabilize mode
  } else if (state1 == 1 && state2 == 1) {
    return 1500; // Althold mode
  } else if (state1 == 1 && state2 == 0) {
    return 2000; // Loiter mode 
  } else {
    return 1000; // Default to stabilize for invalid states
  }
}

int reverse(int value, int max) {
  value = max - value; 
  if (value > max) { value = max; }
  return value;
}

void updateTrim() {
  int old_roll_trim = roll_trim;
  int old_pitch_trim = pitch_trim;

  // Roll trim up
  if (digitalRead(7) == LOW && roll_trim < max_trim) {
    roll_trim += trim_step;
    delay(200); // Debounce
  }
  // Roll trim down
  if (digitalRead(8) == LOW && roll_trim > -max_trim) {
    roll_trim -= trim_step;
    delay(200); // Debounce
  }
  // Pitch trim up
  if (digitalRead(0) == LOW && pitch_trim < max_trim) {
    pitch_trim += trim_step;
    delay(200); // Debounce
  }
  // Pitch trim down
  if (digitalRead(1) == LOW && pitch_trim > -max_trim) {
    pitch_trim -= trim_step;
    delay(200); // Debounce
  }

  // Save to EEPROM only if trim values changed
  if (roll_trim != old_roll_trim) {
    EEPROM.put(addr_roll_trim, roll_trim);
  }
  if (pitch_trim != old_pitch_trim) {
    EEPROM.put(addr_pitch_trim, pitch_trim);
  }
}

void loop() {
  int state1 = digitalRead(5);
  int state2 = digitalRead(6);
  
  updateTrim(); // Update trim values based on button presses

  // Apply trim to roll (ch1) and pitch (ch2)
  int roll_value = map(analogRead(A0), min_input, max_input, 1000, 2000) + roll_trim;
  int pitch_value = map(analogRead(A1), min_input, max_input, 1000, 2000) + pitch_trim;

  // Constrain values to valid PWM range (1000-2000)
  roll_value = constrain(roll_value, 1000, 2000);
  pitch_value = constrain(pitch_value, 1000, 2000);

  data.ch1_0 = roll_value / 256;
  data.ch1_1 = roll_value % 256;
  data.ch2_0 = pitch_value / 256;
  data.ch2_1 = pitch_value % 256;
  data.ch3_0 = map(reverse(analogRead(A2), (min_input + max_input)), min_input, max_input, 1000, 2000) / 256;
  data.ch3_1 = map(reverse(analogRead(A2), (min_input + max_input)), min_input, max_input, 1000, 2000) % 256;
  data.ch4_0 = map(analogRead(A3), min_input, max_input, 1000, 2000) / 256;
  data.ch4_1 = map(analogRead(A3), min_input, max_input, 1000, 2000) % 256;
  
  data.ch5_0 = rtn3PosSwitchValue(state1, state2) / 256;
  data.ch5_1 = rtn3PosSwitchValue(state1, state2) % 256;
  data.ch6_0 = rtnSwitchValue(!digitalRead(4)) / 256;
  data.ch6_1 = rtnSwitchValue(!digitalRead(4)) % 256;
  data.ch7_0 = rtnSwitchValue(!digitalRead(3)) / 256;
  data.ch7_1 = rtnSwitchValue(!digitalRead(3)) % 256;
  data.ch8_0 = rtnSwitchValue(!digitalRead(2)) / 256;
  data.ch8_1 = rtnSwitchValue(!digitalRead(2)) % 256;
  
  radio.write(&data, sizeof(PacketData));
  /*
  char inputVal[200];
  sprintf(inputVal, "%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d",
    data.ch1_0*256 + data.ch1_1, data.ch2_0*256 + data.ch2_1, data.ch3_0*256 + data.ch3_1,
    data.ch4_0*256 + data.ch4_1, data.ch5_0*256 + data.ch5_1, data.ch6_0*256 + data.ch6_1,
    data.ch7_0*256 + data.ch7_1, data.ch8_0*256 + data.ch8_1);
  Serial.println(inputVal); 
  */
}